# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3068/3235 lines (94.84%)

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
|       165 |   42 | `				*ppOut = &aLabel[n];` |
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
|    190013 |   68 | `PH7_PRIVATE int GenStateDeclIsConditional(ph7_gen_state *pGen)` |
|         5 |   69 | `{` |
|    190018 |   70 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    190018 |   71 | `	return pBlock != 0 && (pBlock->iFlags & GEN_BLOCK_GLOBAL) == 0;` |
|         5 |   72 | `}` |
|         - |   73 |  |
|     60015 |   74 | `PH7_PRIVATE GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|         5 |   75 | `{` |
|     60020 |   76 | `	GenBlock *pBlock = pCurrent;` |
|    136464 |   77 | `	for(;;){` |
|    273285 |   78 | `		if( pBlock->iFlags & iBlockType ){` |
|     60022 |   79 | `			iCount--; /* Decrement nesting level */` |
|     60022 |   80 | `			if( iCount < 1 ){` |
|         - |   81 | `				/* Block meet with the desired criteria */` |
|     59968 |   82 | `				return pBlock;` |
|         - |   83 | `			}` |
|        27 |   84 | `		}` |
|         - |   85 | `		/* Point to the upper block */` |
|    213322 |   86 | `		pBlock = pBlock->pParent;` |
|    213322 |   87 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|         - |   88 | `			/* Forbidden */` |
|        29 |   89 | `			break;` |
|         - |   90 | `		}` |
|         5 |   91 | `	}` |
|         - |   92 | `	/* No such block */` |
|        55 |   93 | `	return 0;` |
|     29974 |   94 | `}` |
|         - |   95 | `/*` |
|         - |   96 | ` * Initialize a freshly allocated block instance.` |
|         - |   97 | ` */` |
|   2054460 |   98 | `static void GenStateInitBlock(` |
|         - |   99 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  100 | `	GenBlock *pBlock,    /* Target block */` |
|         - |  101 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |  102 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |  103 | `	void *pUserData      /* Upper layer private data */` |
|         - |  104 | `	)` |
|         5 |  105 | `{` |
|         - |  106 | `	/* Initialize block fields */` |
|   2054465 |  107 | `	pBlock->nFirstInstr = nFirstInstr;` |
|   2054465 |  108 | `	pBlock->pUserData   = pUserData;` |
|   2054465 |  109 | `	pBlock->pGen        = pGen;` |
|   2054465 |  110 | `	pBlock->iFlags      = iType;` |
|   2054465 |  111 | `	pBlock->pParent     = 0;` |
|   2054465 |  112 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|   2054465 |  113 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|   2054465 |  114 | `}` |
|         - |  115 | `/*` |
|         - |  116 | ` * Allocate a new block instance.` |
|         - |  117 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |  118 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |  119 | ` * processing on failure.` |
|         - |  120 | ` */` |
|   2046009 |  121 | `PH7_PRIVATE sxi32 GenStateEnterBlock(` |
|         - |  122 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  123 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |  124 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |  125 | `	void *pUserData,      /* Upper layer private data */` |
|         - |  126 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |  127 | `	)` |
|         5 |  128 | `{` |
|         - |  129 | `	GenBlock *pBlock;` |
|         - |  130 | `	/* Allocate a new block instance */` |
|   2046014 |  131 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|   2046014 |  132 | `	if( pBlock == 0 ){` |
|         - |  133 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  134 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  135 | `		 */` |
|       ! 0 |  136 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |  137 | `		/* Abort processing immediately */` |
|       ! 0 |  138 | `		return SXERR_ABORT;` |
|         - |  139 | `	}` |
|         - |  140 | `	/* Zero the structure */` |
|   2046014 |  141 | `	SyZero(pBlock,sizeof(GenBlock));` |
|   2046014 |  142 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |  143 | `	/* Link to the parent block */` |
|   2046014 |  144 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |  145 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|         - |  146 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|   2046014 |  147 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    167592 |  148 | `		sxu32 nParent = pGen->nCurLoopId;` |
|    167592 |  149 | `		pGen->nLoopId++;` |
|    167592 |  150 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|    167592 |  151 | `		pBlock->nLoopId = pGen->nLoopId;` |
|    167592 |  152 | `		pBlock->nOuterLoopId = nParent;` |
|    167592 |  153 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|     83676 |  154 | `	}` |
|         - |  155 | `	/* A try/catch/finally block gets a scope id, and remembers the scope it nests inside,` |
|         - |  156 | `	 * so the chain between any two points can be walked after compilation (aScope). Every` |
|         - |  157 | `	 * other block simply inherits the scope in effect. */` |
|   2046014 |  158 | `	pBlock->nOuterScopeId = pGen->nCurScopeId;` |
|   2046014 |  159 | `	pBlock->nScopeId = pGen->nCurScopeId;` |
|   2046014 |  160 | `	if( iType & GEN_BLOCK_EXCEPTION ){` |
|         - |  161 | `		GenScope sScope;` |
|     11511 |  162 | `		sScope.nParent = pGen->nCurScopeId;` |
|     11511 |  163 | `		sScope.pUserData = pUserData;` |
|     11511 |  164 | `		if( iType & GEN_BLOCK_FINALLY ){` |
|       345 |  165 | `			sScope.iKind = GEN_SCOPE_FINALLY;` |
|     11341 |  166 | `		}else if( iType & GEN_BLOCK_DETACHED ){` |
|      5389 |  167 | `			sScope.iKind = GEN_SCOPE_DETACHED;` |
|      2693 |  168 | `		}else{` |
|         - |  169 | `			/* A try. pUserData is its ph7_exception, which every try-block site passes at` |
|         - |  170 | `			 * ENTRY precisely so this can classify it. */` |
|      5787 |  171 | `			sScope.iKind = GenStateInlineTryCatch(pGen) ? GEN_SCOPE_TRY_INLINE : GEN_SCOPE_TRY;` |
|         - |  172 | `		}` |
|     11511 |  173 | `		if( SySetPut(&pGen->aScope,(const void *)&sScope) == SXRET_OK ){` |
|     11511 |  174 | `			pBlock->nScopeId = SySetUsed(&pGen->aScope);` |
|     11511 |  175 | `			pGen->nCurScopeId = pBlock->nScopeId;` |
|      5745 |  176 | `		}` |
|      5745 |  177 | `	}` |
|         - |  178 | `	/* Mark as the current block */` |
|   2046014 |  179 | `	pGen->pCurrent = pBlock;` |
|   2046014 |  180 | `	if( ppBlock ){` |
|         - |  181 | `		/* Write a pointer to the new instance */` |
|    965529 |  182 | `		*ppBlock = pBlock;` |
|    482023 |  183 | `	}` |
|   2046014 |  184 | `	return SXRET_OK;` |
|   1021548 |  185 | `}` |
|         - |  186 | `/*` |
|         - |  187 | ` * Release block fields without freeing the whole instance.` |
|         - |  188 | ` */` |
|   2045991 |  189 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |  190 | `{` |
|   2045996 |  191 | `	SySetRelease(&pBlock->aPostContFix);` |
|   2045996 |  192 | `	SySetRelease(&pBlock->aJumpFix);` |
|   2045996 |  193 | `}` |
|         - |  194 | `/*` |
|         - |  195 | ` * Release a block.` |
|         - |  196 | ` */` |
|   2045985 |  197 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |  198 | `{` |
|   2045990 |  199 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|   2045990 |  200 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |  201 | `	/* Free the instance */` |
|   2045990 |  202 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|   2045990 |  203 | `}` |
|         - |  204 | `/*` |
|         - |  205 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |  206 | ` */` |
|   2045757 |  207 | `PH7_PRIVATE sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |  208 | `{` |
|   2045762 |  209 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   2045762 |  210 | `	if( pBlock == 0 ){` |
|         - |  211 | `		/* No more block to pop */` |
|       ! 0 |  212 | `		return SXERR_EMPTY;` |
|         - |  213 | `	}` |
|   2045762 |  214 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    167496 |  215 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|     83628 |  216 | `	}` |
|   2045762 |  217 | `	if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|     11511 |  218 | `		pGen->nCurScopeId = pBlock->nOuterScopeId;` |
|      5745 |  219 | `	}` |
|         - |  220 | `	/* Point to the upper block */` |
|   2045762 |  221 | `	pGen->pCurrent = pBlock->pParent;` |
|   2045762 |  222 | `	if( ppBlock ){` |
|         - |  223 | `		/* Write a pointer to the popped block */` |
|       ! 0 |  224 | `		*ppBlock = pBlock;` |
|       ! 0 |  225 | `	}else{` |
|         - |  226 | `		/* Safely release the block */` |
|   2045762 |  227 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |  228 | `	}` |
|   2045762 |  229 | `	return SXRET_OK;` |
|   1021422 |  230 | `}` |
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
|    189655 |  249 | `PH7_PRIVATE int GenStateUnconditionalTopLevel(ph7_gen_state *pGen)` |
|         5 |  250 | `{` |
|    189660 |  251 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    189896 |  252 | `	while( pBlock ){` |
|    189896 |  253 | `		if( pBlock->iFlags & (GEN_BLOCK_COND\|GEN_BLOCK_LOOP\|GEN_BLOCK_FUNC\|GEN_BLOCK_SWITCH\|GEN_BLOCK_EXCEPTION) ){` |
|       209 |  254 | `			return 0; /* conditional / nested */` |
|         - |  255 | `		}` |
|    189692 |  256 | `		if( pBlock->iFlags & GEN_BLOCK_GLOBAL ){` |
|    189456 |  257 | `			return 1; /* reached the global block with no conditional ancestor */` |
|         - |  258 | `		}` |
|       241 |  259 | `		pBlock = pBlock->pParent;` |
|         5 |  260 | `	}` |
|       ! 0 |  261 | `	return 1;` |
|     94711 |  262 | `}` |
|         - |  263 | `/*` |
|         - |  264 | ` * Guard a top-level function about to be installed. Same contract as the class` |
|         - |  265 | ` * guard above.` |
|         - |  266 | ` */` |
|    182451 |  267 | `PH7_PRIVATE sxi32 GenStateGuardFuncRedeclaration(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  268 | `{` |
|         - |  269 | `	SyHashEntry *pEntry;` |
|    182456 |  270 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|       ! 0 |  271 | `		return SXRET_OK;` |
|         - |  272 | `	}` |
|    182456 |  273 | `	pFunc->iFlags \|= VM_FUNC_BOUND;` |
|    182456 |  274 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|    177350 |  275 | `		return SXRET_OK;` |
|         - |  276 | `	}` |
|         - |  277 | `	/* An INTERNAL function of this name makes the declaration php's fatal, and every` |
|         - |  278 | `	 * one of them can be seen from here: PH7_VmInit registers the whole host table` |
|         - |  279 | `	 * before a program compiles, the way php has its own before it compiles. php` |
|         - |  280 | `	 * names no previous declaration for this arm -- an internal function has no file` |
|         - |  281 | `	 * and no line to name -- so the sentence is the short one.` |
|         - |  282 | `	 *` |
|         - |  283 | `	 * The bCompilingBuiltin early-return above keeps the prelude itself exempt: ~22` |
|         - |  284 | `	 * builtins ARE embedded PHP, and each of them declares its own name. */` |
|      5111 |  285 | `	if( PH7_VmNameIsInternalFunc(pGen->pVm,pFunc->sName.zString,pFunc->sName.nByte) ){` |
|        11 |  286 | `		PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|         3 |  287 | `			"Cannot redeclare function %z()",&pFunc->sName);` |
|         8 |  288 | `		return SXERR_ABORT;` |
|         - |  289 | `	}` |
|      5105 |  290 | `	pEntry = SyHashGet(&pGen->pVm->hFunction,(const void *)pFunc->sName.zString,pFunc->sName.nByte);` |
|      5105 |  291 | `	if( pEntry ){` |
|         7 |  292 | `		ph7_vm_func *pPrev = (ph7_vm_func *)pEntry->pUserData;` |
|         7 |  293 | `		while( pPrev ){` |
|         7 |  294 | `			if( pPrev->iFlags & VM_FUNC_BOUND ){` |
|         7 |  295 | `				if( pPrev->sFile.nByte > 0 ){` |
|        10 |  296 | `					PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|         - |  297 | `						"Cannot redeclare function %z() (previously declared in %.*s:%u)",` |
|         3 |  298 | `						&pFunc->sName,pPrev->sFile.nByte,pPrev->sFile.zString,pPrev->nLine);` |
|         4 |  299 | `				}else{` |
|       ! 0 |  300 | `					PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|       ! 0 |  301 | `						"Cannot redeclare function %z()",&pFunc->sName);` |
|         - |  302 | `				}` |
|         7 |  303 | `				return SXERR_ABORT;` |
|         - |  304 | `			}` |
|       ! 0 |  305 | `			pPrev = pPrev->pNextName;` |
|       ! 0 |  306 | `		}` |
|       ! 0 |  307 | `	}` |
|      5099 |  308 | `	return SXRET_OK;` |
|     91109 |  309 | `}` |
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
|   1173950 |  320 | `PH7_PRIVATE sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |  321 | `{` |
|         - |  322 | `	JumpFixup sJumpFix;` |
|         - |  323 | `	sxi32 rc;` |
|         - |  324 | `	/* Init the JumpFixup structure */` |
|   1173955 |  325 | `	sJumpFix.nJumpType = nJumpType;` |
|   1173955 |  326 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |  327 | `	/* Remember which bytecode array the emitted instruction lives in: the block whose` |
|         - |  328 | `	 * table this lands in may be resolved after a container swap (see JumpFixup). */` |
|   1173955 |  329 | `	sJumpFix.pContainer = PH7_VmGetByteCodeContainer(pBlock->pGen->pVm);` |
|         - |  330 | `	/* Insert in the jump fixup table */` |
|   1173955 |  331 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   1173955 |  332 | `	return rc;` |
|         5 |  333 | `}` |
|         - |  334 | `/*` |
|         - |  335 | ` * TRUE when the body being compiled has its try/catch/finally compiled INLINE` |
|         - |  336 | ` * (ROOT C, generator bodies) rather than into detached mini-programs.` |
|         - |  337 | ` */` |
|      5782 |  338 | `PH7_PRIVATE int GenStateInlineTryCatch(ph7_gen_state *pGen)` |
|         5 |  339 | `{` |
|      5787 |  340 | `	return pGen->bInGenerator && pGen->pVm->bInlineTryCatch;` |
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
|     60119 |  367 | `PH7_PRIVATE int GenStateJumpScope(ph7_gen_state *pGen,sxu32 nFrom,sxu32 nTo,int bEmitPops,` |
|         - |  368 | `	GenJumpScope *pScope)` |
|         5 |  369 | `{` |
|     60124 |  370 | `	GenScope *aScope = (GenScope *)SySetBasePtr(&pGen->aScope);` |
|     60124 |  371 | `	sxu32 nUsed = SySetUsed(&pGen->aScope);` |
|     60124 |  372 | `	sxu32 nCur = nFrom;` |
|     60124 |  373 | `	SyZero(pScope,sizeof(*pScope));` |
|     60238 |  374 | `	while( nCur != nTo ){` |
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
|        74 |  391 | `			if( pScope->nDet == 0 ){` |
|        70 |  392 | `				pScope->nTry = 0;    /* below the first boundary: not the landing pad's */` |
|        70 |  393 | `				pScope->nInline = 0;` |
|        33 |  394 | `			}` |
|        74 |  395 | `			pScope->nDet++;` |
|        74 |  396 | `		}else if( pScopeEnt->iKind == GEN_SCOPE_TRY_INLINE ){` |
|        11 |  397 | `			pScope->nInline++;` |
|        35 |  398 | `		}else if( pScope->nDet == 0 && bEmitPops ){` |
|         3 |  399 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pScopeEnt->pUserData,0);` |
|         2 |  400 | `		}else{` |
|        28 |  401 | `			pScope->nTry++;` |
|         - |  402 | `		}` |
|       119 |  403 | `		nCur = pScopeEnt->nParent;` |
|         5 |  404 | `	}` |
|     60120 |  405 | `	return TRUE;` |
|     30026 |  406 | `}` |
|         - |  407 | `/*` |
|         - |  408 | ` * Pick the jump opcode for a crossing described by GenStateJumpScope, and its iP1.` |
|         - |  409 | ` * Shared by break/continue (which know their target at emit time) and goto (which` |
|         - |  410 | ` * settles this in GenStateFixGoto, once the label fixes the counts).` |
|         - |  411 | ` */` |
|     60003 |  412 | `PH7_PRIVATE sxi32 GenStateScopeJumpOp(const GenJumpScope *pCross,sxi32 *piP1)` |
|         5 |  413 | `{` |
|     60008 |  414 | `	if( pCross->nDet > 0 \|\| pCross->nTry > 0 ){` |
|        84 |  415 | `		*piP1 = PH7_CATCH_JMP_P1(pCross->nDet,pCross->nTry);` |
|        84 |  416 | `		return PH7_OP_CATCH_JMP;` |
|         - |  417 | `	}` |
|     59928 |  418 | `	if( pCross->nInline > 0 ){` |
|        11 |  419 | `		*piP1 = (sxi32)pCross->nInline;` |
|        11 |  420 | `		return PH7_OP_SET_FINALLY_JMP;` |
|         - |  421 | `	}` |
|     59920 |  422 | `	*piP1 = 0;` |
|     59920 |  423 | `	return PH7_OP_JMP;` |
|     29968 |  424 | `}` |
|         - |  425 | `/*` |
|         - |  426 | ` * Resolve a recorded fixup to its VM instruction, in the container it was emitted` |
|         - |  427 | ` * into (see JumpFixup.pContainer) rather than whichever one is current now.` |
|         - |  428 | ` */` |
|   1191000 |  429 | `PH7_PRIVATE VmInstr * GenStateFixupInstr(const JumpFixup *pFix)` |
|         5 |  430 | `{` |
|   1191005 |  431 | `	return (VmInstr *)SySetAt(pFix->pContainer,pFix->nInstrIdx);` |
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
|   1647867 |  444 | `PH7_PRIVATE sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |  445 | `{` |
|         - |  446 | `	JumpFixup *aFix;` |
|         - |  447 | `	VmInstr *pInstr;` |
|         - |  448 | `	sxu32 nFixed;` |
|         - |  449 | `	sxu32 n;` |
|         - |  450 | `	/* Point to the jump fixup table */` |
|   1647872 |  451 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |  452 | `	/* Fix the desired jumps */` |
|   3784836 |  453 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|   2136969 |  454 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |  455 | `			/* Already fixed */` |
|    750440 |  456 | `			continue;` |
|         - |  457 | `		}` |
|   1386534 |  458 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |  459 | `			/* Not of our interest */` |
|    212672 |  460 | `			continue;` |
|         - |  461 | `		}` |
|         - |  462 | `		/* Point to the instruction to fix */` |
|   1173867 |  463 | `		pInstr = GenStateFixupInstr(&aFix[n]);` |
|   1173867 |  464 | `		if( pInstr ){` |
|   1173867 |  465 | `			pInstr->iP2 = nJumpDest;` |
|   1173867 |  466 | `			nFixed++;` |
|         - |  467 | `			/* Mark as fixed */` |
|   1173867 |  468 | `			aFix[n].nJumpType = -1;` |
|    586151 |  469 | `		}` |
|    586156 |  470 | `	}` |
|         - |  471 | `	/* Total number of fixed jumps */` |
|   1647872 |  472 | `	return nFixed;` |
|         5 |  473 | `}` |
|         - |  474 | `/*` |
|         - |  475 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |  476 | ` * The goto statement can be used to jump to another section` |
|         - |  477 | ` * in the program.` |
|         - |  478 | ` * Refer to the routine responsible of compiling the goto` |
|         - |  479 | ` * statement for more information.` |
|         - |  480 | ` */` |
|    231582 |  481 | `PH7_PRIVATE sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |  482 | `{` |
|         - |  483 | `	JumpFixup *pJump,*aJumps;` |
|         - |  484 | `	GenJumpScope sCross;` |
|         - |  485 | `	Label *pLabel;` |
|         - |  486 | `	VmInstr *pInstr;` |
|         - |  487 | `	sxi32 rc;` |
|         - |  488 | `	sxu32 n;` |
|         - |  489 | `	/* Point to the goto table */` |
|    231587 |  490 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |  491 | `	/* Fix */` |
|    231811 |  492 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|       165 |  511 | `		if( pLabel->nLoopId != 0 ){` |
|         8 |  512 | `			sxu32 *aParent = (sxu32 *)SySetBasePtr(&pGen->aLoopParent);` |
|         8 |  513 | `			sxu32 nCur = pJump->nLoopId;` |
|         8 |  514 | `			int bInside = 0;` |
|         8 |  515 | `			while( nCur != 0 ){` |
|         8 |  516 | `				if( nCur == pLabel->nLoopId ){` |
|         8 |  517 | `					bInside = 1;` |
|         8 |  518 | `					break;` |
|         - |  519 | `				}` |
|       ! 0 |  520 | `				nCur = (nCur <= SySetUsed(&pGen->aLoopParent)) ? aParent[nCur - 1] : 0;` |
|       ! 0 |  521 | `			}` |
|         8 |  522 | `			if( !bInside ){` |
|       ! 0 |  523 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,` |
|         - |  524 | `					"'goto' into loop or switch statement is disallowed");` |
|       ! 0 |  525 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  526 | `					return SXERR_ABORT;` |
|         - |  527 | `				}` |
|       ! 0 |  528 | `				continue;` |
|         - |  529 | `			}` |
|         3 |  530 | `		}` |
|         - |  531 | `		/* What the jump crosses, and whether it is legal at all: the label's scope must` |
|         - |  532 | `		 * ENCLOSE the goto. Jumping INTO a try/catch/finally is fine in php (its handlers` |
|         - |  533 | `		 * are instruction RANGES, so landing anywhere in the body is being in the try),` |
|         - |  534 | `		 * but PHL pushes a handler at the try's OP_LOAD_EXCEPTION and runs a catch body` |
|         - |  535 | `		 * as a mini-program entered at its first instruction — there is no way to arrive` |
|         - |  536 | `		 * mid-body with the handler live. Say so rather than jump nowhere in silence,` |
|         - |  537 | `		 * skip a finally, or land in a foreign array. */` |
|       165 |  538 | `		if( !GenStateJumpScope(&(*pGen),pJump->nScopeId,pLabel->nScopeId,FALSE,&sCross) ){` |
|         6 |  539 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,` |
|         - |  540 | `				"'goto' into a try, catch or finally block is disallowed");` |
|         6 |  541 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  542 | `				return SXERR_ABORT;` |
|         - |  543 | `			}` |
|         6 |  544 | `			continue;` |
|         - |  545 | `		}` |
|       161 |  546 | `		if( sCross.nFinally > 0 ){` |
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
|       159 |  563 | `		pInstr = GenStateFixupInstr(pJump);` |
|       159 |  564 | `		if( pInstr ){` |
|       159 |  565 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|       159 |  566 | `			if( pInstr->iOp == PH7_OP_CATCH_JMP ){` |
|         - |  567 | `				/* Emitted as a structure-crossing jump because the goto sits inside a` |
|         - |  568 | `				 * try or a detached body. Now that the crossing is known it may well` |
|         - |  569 | `				 * turn out to leave nothing, and downgrade to a plain OP_JMP. */` |
|        47 |  570 | `				sxi32 iP1 = 0;` |
|        47 |  571 | `				pInstr->iOp = (sxu8)GenStateScopeJumpOp(&sCross,&iP1);` |
|        47 |  572 | `				pInstr->iP1 = iP1;` |
|        22 |  573 | `			}` |
|        77 |  574 | `		}` |
|        82 |  575 | `	}` |
|         - |  576 | `	/* php says nothing about a label nobody jumps to — the old "defined but not` |
|         - |  577 | `	 * referenced" warning was a PH7-ism with no counterpart in the oracle. */` |
|    231585 |  578 | `	return SXRET_OK;` |
|    115647 |  579 | `}` |
|         - |  580 | `/*` |
|         - |  581 | ` * Check if a given token value is installed in the literal table.` |
|         - |  582 | ` */` |
|   2247119 |  583 | `PH7_PRIVATE sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |  584 | `{` |
|         - |  585 | `	SyHashEntry *pEntry;` |
|   2247124 |  586 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|   2247124 |  587 | `	if( pEntry == 0 ){` |
|    937778 |  588 | `		return SXERR_NOTFOUND;` |
|         - |  589 | `	}` |
|   1309351 |  590 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|   1309351 |  591 | `	return SXRET_OK;` |
|   1121391 |  592 | `}` |
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
|    937773 |  603 | `PH7_PRIVATE sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |  604 | `{` |
|    937778 |  605 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|    937778 |  606 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|    467948 |  607 | `	}` |
|    937778 |  608 | `	return SXRET_OK;` |
|         5 |  609 | `}` |
|         - |  610 | `/*` |
|         - |  611 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |  612 | ` * in the constant table.` |
|         - |  613 | ` */` |
|    985610 |  614 | `PH7_PRIVATE ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |  615 | `{` |
|         - |  616 | `	ph7_value *pObj;` |
|    985615 |  617 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |  618 | `	/* Reserve a new constant */` |
|    985615 |  619 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    985615 |  620 | `	if( pObj == 0 ){` |
|       ! 0 |  621 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  622 | `		return 0;` |
|         - |  623 | `	}` |
|    985615 |  624 | `	*pIdx = nIdx;` |
|         - |  625 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |  626 | `	 * the constant string iterals table [optimization purposes].` |
|         - |  627 | `	 */` |
|    985615 |  628 | `	return pObj;` |
|    492138 |  629 | `}` |
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
|   1328925 |  644 | `PH7_PRIVATE void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |  645 | `{` |
|         - |  646 | `	VmCallArgMap *pMap;` |
|   1328930 |  647 | `	if( !pGen->bStrictTypes ) return p3;` |
|      1333 |  648 | `	if( p3 == 0 ){` |
|       189 |  649 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|       189 |  650 | `		if( pMap == 0 ) return 0;` |
|       189 |  651 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|       189 |  652 | `		p3 = (void *)pMap;` |
|        92 |  653 | `	}` |
|      1333 |  654 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      1333 |  655 | `	return p3;` |
|    663224 |  656 | `}` |
|         - |  657 | `/* Forward declaration */` |
|         - |  658 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut);` |
|         - |  659 | `/* Forward declarations */` |
|         - |  660 | `/*` |
|         - |  661 | ` * Recover from a compile-time error. In other words synchronize` |
|         - |  662 | ` * the token stream cursor with the first semi-colon seen.` |
|         - |  663 | ` */` |
|        84 |  664 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|         2 |  665 | `{` |
|         - |  666 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|       224 |  667 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       140 |  668 | `		pGen->pIn++;` |
|         2 |  669 | `	}` |
|        86 |  670 | `	return SXRET_OK;` |
|         2 |  671 | `}` |
|         - |  672 | `/*` |
|         - |  673 | ` * Check if the given identifier name is reserved or not.` |
|         - |  674 | ` * Return TRUE if reserved.FALSE otherwise.` |
|         - |  675 | ` */` |
|       290 |  676 | `PH7_PRIVATE int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  677 | `{` |
|       295 |  678 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|        24 |  679 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  680 | `			return TRUE;` |
|        22 |  681 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         6 |  682 | `			return TRUE;` |
|         4 |  683 | `		}` |
|       282 |  684 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|        20 |  685 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  686 | `			return TRUE;` |
|         - |  687 | `		}` |
|         7 |  688 | `	}` |
|         - |  689 | `	/* Not a reserved constant */` |
|       287 |  690 | `	return FALSE;` |
|       150 |  691 | `}` |
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
|  10685007 |  717 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 |  718 | `{` |
|  10685012 |  719 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - |  720 | `	sxu32 nTarget;` |
|         - |  721 | `	sxu32 *aIdx;` |
|         - |  722 | `	sxu32 i;` |
|  10685012 |  723 | `	if( nCur <= nBaseline ){` |
|  10684804 |  724 | `		return;` |
|         - |  725 | `	}` |
|       213 |  726 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|       213 |  727 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|       429 |  728 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|       221 |  729 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|       221 |  730 | `		if( pInstr ){` |
|       221 |  731 | `			pInstr->iP2 = (sxi32)nTarget;` |
|       108 |  732 | `		}` |
|       113 |  733 | `	}` |
|       213 |  734 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|   5334400 |  735 | `}` |
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
|     10351 |  746 | `static int GenStateArgHasPropertyStep(ph7_expr_node *pNode)` |
|         5 |  747 | `{` |
|     10398 |  748 | `	while( pNode && pNode->pOp ){` |
|       130 |  749 | `		if( pNode->pOp->iOp == EXPR_OP_ARROW ){` |
|        80 |  750 | `			return 1;` |
|         - |  751 | `		}` |
|        51 |  752 | `		if( pNode->pOp->iOp != EXPR_OP_SUBSCRIPT ){` |
|         8 |  753 | `			return 0;` |
|         - |  754 | `		}` |
|        44 |  755 | `		pNode = pNode->pLeft;` |
|         2 |  756 | `	}` |
|     10272 |  757 | `	return 0;` |
|      5168 |  758 | `}` |
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
|   1203026 |  776 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
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
|   1203031 |  840 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|     36821 |  841 | `		return 0;` |
|         - |  842 | `	}` |
|  57788712 |  843 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  56634286 |  844 | `		if( pName->nByte == aByRef[i].nByte` |
|  29689065 |  845 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     11794 |  846 | `			return aByRef[i].mask;` |
|         - |  847 | `		}` |
|  28256082 |  848 | `	}` |
|   1154426 |  849 | `	return 0;` |
|    600356 |  850 | `}` |
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
|   1707105 |  875 | `static int GenStateArgShape(ph7_expr_node *pNode)` |
|         5 |  876 | `{` |
|   1707110 |  877 | `	if( pNode == 0 ){` |
|       ! 0 |  878 | `		return GEN_ARG_NONE;` |
|         - |  879 | `	}` |
|   1707110 |  880 | `	if( pNode->pOp == 0 ){` |
|         - |  881 | ``		/* A leaf: only the `$…` family is a variable. Everything else the parser`` |
|         - |  882 | ``		 * files here — a literal, an array/list constructor, a closure, `match`,`` |
|         - |  883 | ``		 * `clone` — is a temporary. */`` |
|   1385249 |  884 | `		return pNode->xCode == PH7_CompileVariable ? GEN_ARG_LVALUE : GEN_ARG_NONE;` |
|         - |  885 | `	}` |
|    321866 |  886 | `	switch( pNode->pOp->iOp ){` |
|     24606 |  887 | `	case EXPR_OP_SUBSCRIPT: /* $a[k], and any base: php accepts g()[0] and C::m()[0] */` |
|         - |  888 | `	case EXPR_OP_ARROW:     /* $o->p */` |
|     49158 |  889 | `		return GEN_ARG_LVALUE;` |
|       661 |  890 | `	case EXPR_OP_DC:` |
|         - |  891 | ``		/* `C::$s` is a static property (an lvalue); `C::K` is a class constant and`` |
|         - |  892 | ``		 * `C::CASE` an enum case, neither of which php will bind. The right operand`` |
|         - |  893 | `		 * tells them apart. */` |
|      1988 |  894 | `		return ( pNode->pRight && pNode->pRight->pOp == 0` |
|      1322 |  895 | `		      && pNode->pRight->xCode == PH7_CompileVariable )` |
|      1322 |  896 | `			? GEN_ARG_LVALUE : GEN_ARG_NONE;` |
|     47099 |  897 | `	case EXPR_OP_FUNC_CALL:` |
|         - |  898 | `	case EXPR_OP_NEW:` |
|     93991 |  899 | `		return GEN_ARG_TEMPCALL;` |
|         2 |  900 | `	case EXPR_OP_REF:` |
|         - |  901 | ``		/* `take($q = &$p)`: a reference ASSIGNMENT hands back the reference it`` |
|         - |  902 | `		 * made, so php passes it on to a by-ref parameter and all three names end` |
|         - |  903 | ``		 * up aliasing one slot. A plain `$q = $p` does not -- php's ASSIGN yields`` |
|         - |  904 | `		 * a temporary where ASSIGN_REF yields the VAR -- which is why only this` |
|         - |  905 | `		 * one arm moves. */` |
|         5 |  906 | `		return GEN_ARG_LVALUE;` |
|     88826 |  907 | `	default:` |
|         - |  908 | ``		/* Includes `?->` (php: "Cannot use nullsafe operator in write context"),`` |
|         - |  909 | ``		 * `@$x`, `$q = …`, `clone $o` and every arithmetic/logical operator. */`` |
|    177401 |  910 | `		return GEN_ARG_NONE;` |
|         - |  911 | `	}` |
|    851838 |  912 | `}` |
|         - |  913 | `/*` |
|         - |  914 | ` * Is this argument a string php's compiler can read -- a quoted literal with nothing` |
|         - |  915 | `` * interpolated? A double-quoted or heredoc body is one only when it names no `$` at`` |
|         - |  916 | `` * all (an escaped `\$` is refused too, which errs towards "not a constant").`` |
|         - |  917 | ` */` |
|   1707105 |  918 | `static int GenStateArgIsConstString(ph7_expr_node *pNode)` |
|         5 |  919 | `{` |
|   1707105 |  920 | `	if( pNode == 0 \|\| pNode->pOp != 0 \|\| pNode->pStart == 0` |
|   1385249 |  921 | `	 \|\| (pNode->iFlags & EXPR_NODE_SPREAD) != 0 ){` |
|    321866 |  922 | `		return 0;` |
|         - |  923 | `	}` |
|   1385249 |  924 | `	if( pNode->xCode == PH7_CompileSimpleString \|\| pNode->xCode == PH7_CompileNowDoc ){` |
|    239404 |  925 | `		return 1;` |
|         - |  926 | `	}` |
|   1145850 |  927 | `	if( pNode->xCode == PH7_CompileString \|\| pNode->xCode == PH7_CompileHereDoc ){` |
|     25026 |  928 | `		const SyString *pData = &pNode->pStart->sData;` |
|         - |  929 | `		sxu32 nPos;` |
|     25026 |  930 | `		return SyByteFind(pData->zString,pData->nByte,'$',&nPos) != SXRET_OK;` |
|         - |  931 | `	}` |
|   1120829 |  932 | `	return 0;` |
|    851838 |  933 | `}` |
|         - |  934 | `/*` |
|         - |  935 | ` * Can evaluating this argument expression RUN anything?` |
|         - |  936 | ` *` |
|         - |  937 | ` * php materializes every by-value argument where it is written, so an argument already` |
|         - |  938 | ` * pushed cannot see what a later one does. PHL pushes an aliasing view of the source's` |
|         - |  939 | ` * bytes instead, which is only equivalent while nothing between the two pushes can write.` |
|         - |  940 | ` * This is the question that decides it, asked of every argument that FOLLOWS the one in` |
|         - |  941 | `` * hand: a plain `$var` read and a scalar literal execute nothing, so the alias is safe`` |
|         - |  942 | `` * beside them; every other shape -- an assignment, a call, `new`, `++`, a property or`` |
|         - |  943 | ` * element fetch (which may reach __get / offsetGet), an interpolated string, an array` |
|         - |  944 | `` * constructor, `match`, a closure with a by-reference `use` -- either writes or hands`` |
|         - |  945 | ` * control to code that can, so the earlier arguments are copied first (PH7_OP_SNAPSHOT).` |
|         - |  946 | ` *` |
|         - |  947 | ` * Deliberately answered from the SHAPE and not from what the shape is likely to do: the` |
|         - |  948 | ` * cost of a false yes is one copy of an argument the callee was about to copy anyway,` |
|         - |  949 | ` * and the cost of a false no is a silent wrong value.` |
|         - |  950 | ` */` |
|   3048318 |  951 | `static int GenStateArgRunsCode(ph7_expr_node *pNode)` |
|         5 |  952 | `{` |
|   3048323 |  953 | `	if( pNode == 0 ){` |
|       ! 0 |  954 | `		return 0;` |
|         - |  955 | `	}` |
|   3048323 |  956 | `	if( pNode->pOp == 0 ){` |
|         - |  957 | ``		/* A leaf. Only these read without running: `$x` (and `$$x`, whose name`` |
|         - |  958 | `		 * comes off the stack and still runs nothing), a number, and a string with no` |
|         - |  959 | `		 * interpolation in it. A bare identifier is a constant lookup -- a table read` |
|         - |  960 | `		 * with no user code behind it in php either. */` |
|   4155973 |  961 | `		return !( pNode->xCode == PH7_CompileVariable` |
|   2077272 |  962 | `		       \|\| pNode->xCode == PH7_CompileLiteral` |
|   1397874 |  963 | `		       \|\| pNode->xCode == PH7_CompileNumLiteral` |
|    901411 |  964 | `		       \|\| pNode->xCode == PH7_CompileSimpleString` |
|    316841 |  965 | `		       \|\| pNode->xCode == PH7_CompileNowDoc );` |
|         - |  966 | `	}` |
|         - |  967 | ``	/* An operator node, and none of them is whitelisted: `.` and the arithmetic`` |
|         - |  968 | `	 * operators reach __toString, the fetches reach __get and offsetGet, and the rest` |
|         - |  969 | `	 * write outright. Answering yes for the whole family also spares this a subtree` |
|         - |  970 | `	 * walk -- a nested call is an operator node at its own root. */` |
|    472879 |  971 | `	return 1;` |
|   1521707 |  972 | `}` |
|         - |  973 | `/*` |
|         - |  974 | `` * Is this node the plain variable `$name`?`` |
|         - |  975 | ` *` |
|         - |  976 | `` * `$$name`, `${expr}`, `$a[0]`, `$o->p` and `C::$s` are all excluded: php materializes`` |
|         - |  977 | ` * every one of those where it is written and re-reads only a compiled variable, so the` |
|         - |  978 | ` * two halves of the operand rule below turn on exactly this question.` |
|         - |  979 | ` */` |
|    183324 |  980 | `static int GenStateNodeIsSimpleVar(ph7_expr_node *pNode)` |
|         5 |  981 | `{` |
|         - |  982 | `	SyToken *pTok;` |
|    183329 |  983 | `	if( pNode == 0 \|\| pNode->pOp != 0 \|\| pNode->xCode != PH7_CompileVariable ){` |
|    122932 |  984 | `		return 0;` |
|         - |  985 | `	}` |
|     60402 |  986 | `	pTok = pNode->pStart;` |
|     60402 |  987 | `	if( pTok == 0 \|\| pNode->pEnd != &pTok[2] ){` |
|         3 |  988 | `		return 0;` |
|         - |  989 | `	}` |
|     90555 |  990 | `	return (pTok[0].nType & PH7_TK_DOLLAR) != 0` |
|     60395 |  991 | `	    && (pTok[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0;` |
|     91540 |  992 | `}` |
|         - |  993 | `/*` |
|         - |  994 | ` * Does this operator take the VALUE of both operands, with nothing between them?` |
|         - |  995 | ` *` |
|         - |  996 | `` * The concatenation, arithmetic, shift, bitwise, comparison and `xor` operators, which`` |
|         - |  997 | ``  * evaluate their left operand, then their right, and then read both. `&&`, `\|\|` and `??` `` |
|         - |  998 | ` * are excluded because they may not evaluate the right operand at all -- and when they do,` |
|         - |  999 | ` * the left one has already been consumed by the short-circuit test. Everything else here` |
|         - | 1000 | ` * is either unary, an assignment (whose operands parse.c has swapped), or an access.` |
|         - | 1001 | ` */` |
|   4760260 | 1002 | `static int GenStateBinOpReadsBothOperands(sxi32 iVmOp)` |
|         5 | 1003 | `{` |
|   4760265 | 1004 | `	switch( iVmOp ){` |
|    738159 | 1005 | `	case PH7_OP_CAT:` |
|         - | 1006 | `	case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_MUL:` |
|         - | 1007 | `	case PH7_OP_DIV: case PH7_OP_MOD: case PH7_OP_POW:` |
|         - | 1008 | `	case PH7_OP_SHL: case PH7_OP_SHR:` |
|         - | 1009 | `	case PH7_OP_BAND: case PH7_OP_BOR: case PH7_OP_BXOR: case PH7_OP_LXOR:` |
|         - | 1010 | `	case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:` |
|         - | 1011 | `	case PH7_OP_EQ: case PH7_OP_NEQ: case PH7_OP_TEQ: case PH7_OP_TNE:` |
|         - | 1012 | `	case PH7_OP_SPACESHIP:` |
|   1474336 | 1013 | `		return 1;` |
|   1645521 | 1014 | `	default:` |
|   3285934 | 1015 | `		return 0;` |
|         - | 1016 | `	}` |
|   2376585 | 1017 | `}` |
|         - | 1018 | `/*` |
|         - | 1019 | ` * Can the value this expression leaves on the stack be a VIEW of storage user code can` |
|         - | 1020 | ` * still write to?` |
|         - | 1021 | ` *` |
|         - | 1022 | `` * A variable read of any spelling -- `$x`, `$$x`, `$a[0]`, `$o->p`, `C::$s` -- pushes the`` |
|         - | 1023 | ` * source's own bytes (PH7_MemObjLoad sets SXBLOB_RDONLY and points at them), and so does` |
|         - | 1024 | `` * anything that merely SELECTS one of those: a ternary, `??`, `@`, an assignment (which`` |
|         - | 1025 | `` * hands back what it stored), a short-circuit `&&`/`\|\|` (whose jump keeps the operand`` |
|         - | 1026 | ` * itself), and a CAST, which for a string already a string is a no-op that keeps the view.` |
|         - | 1027 | `` * An operator's own result, a call's or `new`'s return, a literal, an array constructor and`` |
|         - | 1028 | ` * a closure are values the expression owns and nobody can reach.` |
|         - | 1029 | ` *` |
|         - | 1030 | ` * Answered from the SHAPE, and the unknown shape answers YES: a needless copy costs one` |
|         - | 1031 | ` * instruction, a missed one is a silently wrong value.` |
|         - | 1032 | ` */` |
|    122929 | 1033 | `static int GenStateNodeMayAliasStorage(ph7_expr_node *pNode)` |
|         5 | 1034 | `{` |
|    122934 | 1035 | `	if( pNode == 0 ){` |
|       ! 0 | 1036 | `		return 0;` |
|         - | 1037 | `	}` |
|    122934 | 1038 | `	if( pNode->pOp == 0 ){` |
|     30588 | 1039 | `		return pNode->xCode == PH7_CompileVariable;` |
|         - | 1040 | `	}` |
|     92351 | 1041 | `	if( GenStateBinOpReadsBothOperands(pNode->pOp->iVmOp) ){` |
|     63768 | 1042 | `		return 0;` |
|         - | 1043 | `	}` |
|     28588 | 1044 | `	switch( pNode->pOp->iVmOp ){` |
|      5315 | 1045 | `	case PH7_OP_CALL: case PH7_OP_NEW: case PH7_OP_CLONE: case PH7_OP_IS_A:` |
|         - | 1046 | `	case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT: case PH7_OP_LNOT:` |
|     10611 | 1047 | `		return 0;` |
|      9001 | 1048 | `	default:` |
|     17982 | 1049 | `		return 1;` |
|         - | 1050 | `	}` |
|     61385 | 1051 | `}` |
|         - | 1052 | `/*` |
|         - | 1053 | ` * Re-emit an instruction that was popped, exactly as it stood.` |
|         - | 1054 | ` *` |
|         - | 1055 | ` * PH7_VmEmitInstr stamps the CURRENT token's line and strict_types mode, which for a` |
|         - | 1056 | ` * moved instruction is the wrong position -- the codegen cursor has walked past the` |
|         - | 1057 | ` * operand it belongs to. Overwriting the fresh entry with the saved one keeps the source` |
|         - | 1058 | ` * line a diagnostic will name.` |
|         - | 1059 | ` */` |
|     60395 | 1060 | `static void GenStateReEmitInstr(ph7_gen_state *pGen,const VmInstr *pSaved)` |
|         5 | 1061 | `{` |
|         - | 1062 | `	VmInstr *pNew;` |
|     60400 | 1063 | `	PH7_VmEmitInstr(pGen->pVm,pSaved->iOp,pSaved->iP1,pSaved->iP2,pSaved->p3,0);` |
|     60400 | 1064 | `	pNew = PH7_VmPeekInstr(pGen->pVm);` |
|     60400 | 1065 | `	if( pNew ){` |
|     60400 | 1066 | `		*pNew = *pSaved;` |
|     30155 | 1067 | `	}` |
|     60400 | 1068 | `}` |
|         - | 1069 | `/*` |
|         - | 1070 | ` * Recover the bare global-builtin name from a call's callee node.` |
|         - | 1071 | ` *` |
|         - | 1072 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|         - | 1073 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|         - | 1074 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|         - | 1075 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|         - | 1076 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|         - | 1077 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|         - | 1078 | ` */` |
|   3444153 | 1079 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 | 1080 | `{` |
|         - | 1081 | `	SyToken *p, *pEnd;` |
|   3444158 | 1082 | `	pOut->zString = 0;` |
|   3444158 | 1083 | `	pOut->nByte = 0;` |
|   3444158 | 1084 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 | 1085 | `		return;` |
|         - | 1086 | `	}` |
|   3444158 | 1087 | `	p = pLeft->pStart;` |
|   3444158 | 1088 | `	pEnd = pLeft->pEnd;` |
|         - | 1089 | `	/* Optional single leading namespace separator (absolute path). */` |
|   3444158 | 1090 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|       793 | 1091 | `		p++;` |
|       394 | 1092 | `	}` |
|   3444158 | 1093 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     90799 | 1094 | `		return;` |
|         - | 1095 | `	}` |
|         - | 1096 | `	/* Must be a single component: nothing follows the name token. */` |
|   3353364 | 1097 | `	if( p + 1 != pEnd ){` |
|       953 | 1098 | `		return;` |
|         - | 1099 | `	}` |
|   3352416 | 1100 | `	*pOut = p->sData;` |
|   1718753 | 1101 | `}` |
|         - | 1102 | `/*` |
|         - | 1103 | `` * Is this expression node the bare variable `$this`?`` |
|         - | 1104 | ` */` |
|   1125636 | 1105 | `PH7_PRIVATE int PH7_ExprNodeIsThis(ph7_expr_node *pNode)` |
|         5 | 1106 | `{` |
|         - | 1107 | `	SyToken *pTok;` |
|   1125641 | 1108 | `	if( pNode == 0 \|\| pNode->pOp != 0 \|\| pNode->xCode != PH7_CompileVariable ){` |
|    210313 | 1109 | `		return 0;` |
|         - | 1110 | `	}` |
|    915333 | 1111 | `	pTok = pNode->pStart;` |
|    915333 | 1112 | `	if( pTok == 0 \|\| pNode->pEnd == 0 \|\| pNode->pEnd < &pTok[2] ){` |
|       ! 0 | 1113 | `		return 0;` |
|         - | 1114 | `	}` |
|   1372368 | 1115 | `	return (pTok[0].nType & PH7_TK_DOLLAR) != 0` |
|    915328 | 1116 | `		&& (pTok[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|    915316 | 1117 | `		&& pTok[1].sData.nByte == sizeof("this")-1` |
|   1373621 | 1118 | `		&& SyMemcmp((const void *)pTok[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0;` |
|    562057 | 1119 | `}` |
|         - | 1120 | `/*` |
|         - | 1121 | ` * TRUE when codegen is inside a real FUNCTION body — php's` |
|         - | 1122 | `` * `CG(active_op_array)->function_name`. A synthetic block (a match() arm's`` |
|         - | 1123 | ` * throw-fixup) carries no ph7_vm_func and is not a scope.` |
|         - | 1124 | ` */` |
|        86 | 1125 | `static int GenStateInFunction(ph7_gen_state *pGen)` |
|         5 | 1126 | `{` |
|        91 | 1127 | `	GenBlock *pBlock = pGen->pCurrent;` |
|       191 | 1128 | `	while( pBlock ){` |
|       183 | 1129 | `		if( (pBlock->iFlags & GEN_BLOCK_FUNC) && pBlock->pUserData ){` |
|        83 | 1130 | `			return 1;` |
|         - | 1131 | `		}` |
|       105 | 1132 | `		pBlock = pBlock->pParent;` |
|         5 | 1133 | `	}` |
|        10 | 1134 | `	return 0;` |
|        48 | 1135 | `}` |
|         - | 1136 | `/*` |
|         - | 1137 | `` * php's SPECIALIZED builtins — the list behind `Cannot use result of built-in`` |
|         - | 1138 | `` * function in write context`.`` |
|         - | 1139 | ` *` |
|         - | 1140 | ` * The wording says "built-in function" but the rule is not about builtins: php` |
|         - | 1141 | ` * refuses the write when the call was compiled to an OPCODE of its own rather` |
|         - | 1142 | ` * than a real call, because a specialized opcode leaves a TMP where a call` |
|         - | 1143 | `` * leaves a VAR (`zend_separate_if_call_and_write`). So `strlen("x")[0] = 1` and`` |
|         - | 1144 | `` * `count([1])[0] = 1` are compile fatals while `array_values([1])[0] = 2`,`` |
|         - | 1145 | `` * `str_split("ab")[0] = "z"` and `get_object_vars($o)["k"] = 2` all RUN — the`` |
|         - | 1146 | `` * difference being php's `zend_try_compile_special_func_ex` table, reproduced`` |
|         - | 1147 | ` * here name for name with the ARITY each entry demands.` |
|         - | 1148 | ` *` |
|         - | 1149 | ` * Six of php's names are deliberately absent, and only the last pair is a` |
|         - | 1150 | ` * simplification — the other four are not refusals of php's at all:` |
|         - | 1151 | `` *   `chr`/`ord` gate on BP_VAR_R, so they are never special in a WRITE context;`` |
|         - | 1152 | `` *   `call_user_func`/`call_user_func_array` emit a REAL call, so their result is`` |
|         - | 1153 | ` *     a VAR and php does not refuse a write through it either;` |
|         - | 1154 | `` *   `in_array` and `array_slice` gate on the CONTENTS of a literal array`` |
|         - | 1155 | `` *     argument and on a `func_get_args()`-shaped first argument — value-dependent`` |
|         - | 1156 | ` *     shapes no program writes through, left out under the scope policy. Leaving them out` |
|         - | 1157 | ` *     ACCEPTS where php refuses, which is the direction that keeps running a` |
|         - | 1158 | ` *     program php runs.` |
|         - | 1159 | ` * Verified by sweeping every internal function of both engines at arities 0-3:` |
|         - | 1160 | ` * the two specialized sets are identical, 27 names at the same arities.` |
|         - | 1161 | ` */` |
|         - | 1162 | `#define SPECFN_LITERAL_ARG0 0x01 /* php gives up unless argument #1 is a literal */` |
|         - | 1163 | ``#define SPECFN_ANY_ARGS     0x02 /* …and `assert` is decided BEFORE php's unpack/named`` |
|         - | 1164 | ``                                  * bail, so it stays special even for `assert(...$a)` */`` |
|         - | 1165 | `#define SPECFN_IN_FUNC      0x04 /* php's gate reads CG(active_op_array)->function_name:` |
|         - | 1166 | `                                  * at GLOBAL scope it emits a real call, whose runtime` |
|         - | 1167 | `                                  * Error ("cannot be called from the global scope") is` |
|         - | 1168 | `                                  * what the program actually gets */` |
|         - | 1169 | ``#define SPECFN_FORMAT_ARG0  0x08 /* …and `sprintf` also needs php's format arithmetic`` |
|         - | 1170 | `                                  * (implies SPECFN_LITERAL_ARG0) */` |
|         - | 1171 | `static const struct {` |
|         - | 1172 | `	const char *zName;` |
|         - | 1173 | `	int nMinArg;   /* inclusive */` |
|         - | 1174 | `	int nMaxArg;   /* inclusive; -1 = variadic */` |
|         - | 1175 | `	int iFlags;` |
|         - | 1176 | `} aSpecialFunc[] = {` |
|         - | 1177 | `	{ "strlen",           1,  1, 0 },` |
|         - | 1178 | `	{ "is_null",          1,  1, 0 },  { "is_bool",          1,  1, 0 },` |
|         - | 1179 | `	{ "is_long",          1,  1, 0 },  { "is_int",           1,  1, 0 },` |
|         - | 1180 | `	{ "is_integer",       1,  1, 0 },  { "is_float",         1,  1, 0 },` |
|         - | 1181 | `	{ "is_double",        1,  1, 0 },  { "is_string",        1,  1, 0 },` |
|         - | 1182 | `	{ "is_array",         1,  1, 0 },  { "is_object",        1,  1, 0 },` |
|         - | 1183 | `	{ "is_resource",      1,  1, 0 },  { "is_scalar",        1,  1, 0 },` |
|         - | 1184 | `	{ "boolval",          1,  1, 0 },  { "intval",           1,  1, 0 },` |
|         - | 1185 | `	{ "floatval",         1,  1, 0 },  { "doubleval",        1,  1, 0 },` |
|         - | 1186 | `	{ "strval",           1,  1, 0 },` |
|         - | 1187 | `	{ "count",            1,  1, 0 },  { "sizeof",           1,  1, 0 },` |
|         - | 1188 | `	{ "get_class",        0,  1, 0 },  { "get_called_class", 0,  0, 0 },` |
|         - | 1189 | `	{ "gettype",          1,  1, 0 },` |
|         - | 1190 | `	{ "func_num_args",    0,  0, SPECFN_IN_FUNC },` |
|         - | 1191 | `	{ "func_get_args",    0,  0, SPECFN_IN_FUNC },` |
|         - | 1192 | `	{ "array_key_exists", 2,  2, 0 },` |
|         - | 1193 | `	{ "defined",          1,  1, SPECFN_LITERAL_ARG0 },` |
|         - | 1194 | `	{ "sprintf",          1, -1, SPECFN_LITERAL_ARG0\|SPECFN_FORMAT_ARG0 },` |
|         - | 1195 | `	/* php compiles assert() to its own opcode pair "independently of compiler` |
|         - | 1196 | `	 * flags", in zend_compile_call BEFORE the special-func table is consulted —` |
|         - | 1197 | `	 * so every arity counts and an unpacked argument does not exempt it. */` |
|         - | 1198 | `	{ "assert",           0, -1, SPECFN_ANY_ARGS },` |
|         - | 1199 | `};` |
|         - | 1200 | `/*` |
|         - | 1201 | ` * TRUE when this call node is one php compiles to an opcode of its own, so a` |
|         - | 1202 | ` * write THROUGH its result is php's built-in-function refusal. pName is the` |
|         - | 1203 | ` * callee's bare global name, already resolved by GenStateCallBuiltinName.` |
|         - | 1204 | ` */` |
|    906560 | 1205 | `static int GenStateCallIsSpecialized(ph7_gen_state *pGen,ph7_expr_node *pCall,SyString *pName)` |
|         5 | 1206 | `{` |
|         - | 1207 | `	ph7_expr_node **apArg;` |
|         - | 1208 | `	sxu32 nArg, n;` |
|         - | 1209 | `	sxu32 i;` |
|    906565 | 1210 | `	if( pName->nByte < 1 ){` |
|     36577 | 1211 | `		return 0;` |
|         - | 1212 | `	}` |
|    869993 | 1213 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pCall->aNodeArgs);` |
|    869993 | 1214 | `	nArg = SySetUsed(&pCall->aNodeArgs);` |
|  21554246 | 1215 | `	for( i = 0 ; i < SX_ARRAYSIZE(aSpecialFunc) ; ++i ){` |
|         - | 1216 | `		SyString sEntry;` |
|  20887145 | 1217 | `		SyStringInitFromBuf(&sEntry,aSpecialFunc[i].zName,SyStrlen(aSpecialFunc[i].zName));` |
|  20887140 | 1218 | `		if( sEntry.nByte != pName->nByte` |
|  11390561 | 1219 | `		 \|\| SyStrnicmp(sEntry.zString,pName->zString,pName->nByte) != 0 ){` |
|  20684258 | 1220 | `			continue;` |
|         - | 1221 | `		}` |
|    202887 | 1222 | `		if( (int)nArg < aSpecialFunc[i].nMinArg` |
|    202872 | 1223 | `		 \|\| (aSpecialFunc[i].nMaxArg >= 0 && (int)nArg > aSpecialFunc[i].nMaxArg) ){` |
|    101745 | 1224 | `			return 0;` |
|         - | 1225 | `		}` |
|         - | 1226 | `		/* php bails out of the whole table when any argument unpacks or is named` |
|         - | 1227 | ``		 * (`zend_args_contain_unpack_or_named`), so `strlen(...$a)[0] = 1` RUNS. */`` |
|    202582 | 1228 | `		if( (aSpecialFunc[i].iFlags & SPECFN_ANY_ARGS) == 0 ){` |
|    405521 | 1229 | `			for( n = 0 ; n < nArg ; ++n ){` |
|    203008 | 1230 | `				if( apArg[n] && (apArg[n]->iFlags & (EXPR_NODE_SPREAD\|EXPR_NODE_NAMED_ARG)) ){` |
|         3 | 1231 | `					return 0;` |
|         - | 1232 | `				}` |
|    101364 | 1233 | `			}` |
|    101115 | 1234 | `		}` |
|    202580 | 1235 | `		if( (aSpecialFunc[i].iFlags & SPECFN_IN_FUNC) && !GenStateInFunction(pGen) ){` |
|        10 | 1236 | `			return 0;` |
|         - | 1237 | `		}` |
|         - | 1238 | ``		/* `defined` and `sprintf` specialize only over a LITERAL first argument;`` |
|         - | 1239 | `		 * php gives up on a computed one and emits an ordinary call. */` |
|    202572 | 1240 | `		if( aSpecialFunc[i].iFlags & SPECFN_LITERAL_ARG0 ){` |
|       538 | 1241 | `			if( nArg < 1 \|\| apArg[0] == 0 \|\| apArg[0]->pOp != 0` |
|       535 | 1242 | `			 \|\| apArg[0]->pStart == 0` |
|       537 | 1243 | `			 \|\| (apArg[0]->pStart->nType & (PH7_TK_SSTR\|PH7_TK_DSTR)) == 0 ){` |
|        67 | 1244 | `				return 0;` |
|         - | 1245 | `			}` |
|       237 | 1246 | `		}` |
|    202508 | 1247 | `		if( (aSpecialFunc[i].iFlags & SPECFN_FORMAT_ARG0) && nArg >= 1 && apArg[0] ){` |
|         - | 1248 | `			/* php's own sprintf gate, and it is arithmetic: a format under 256` |
|         - | 1249 | ``			 * bytes carrying nothing but `%s`, `%d` and `%%`, with exactly one`` |
|         - | 1250 | ``			 * VALUE per placeholder. `sprintf("a","b")` fails it (no placeholder,`` |
|         - | 1251 | `			 * one value) and compiles to an ordinary call, which is why the write` |
|         - | 1252 | `			 * through it RUNS. */` |
|       323 | 1253 | `			const SyString *pFmt = &apArg[0]->pStart->sData;` |
|       323 | 1254 | `			sxu32 nPlace = 0, k;` |
|       323 | 1255 | `			if( pFmt->nByte >= 256 ){` |
|       ! 0 | 1256 | `				return 0;` |
|         - | 1257 | `			}` |
|         - | 1258 | `			/* An escape or an interpolation makes php's argument something other` |
|         - | 1259 | `			 * than a plain literal; leave those to the ordinary call. */` |
|      2439 | 1260 | `			for( k = 0 ; k < pFmt->nByte ; ++k ){` |
|      2128 | 1261 | `				if( pFmt->zString[k] == '\\'` |
|      2479 | 1262 | `				 \|\| ((apArg[0]->pStart->nType & PH7_TK_DSTR)` |
|      1411 | 1263 | `				  && (pFmt->zString[k] == '$' \|\| pFmt->zString[k] == '{')) ){` |
|        15 | 1264 | `					return 0;` |
|         - | 1265 | `				}` |
|      1063 | 1266 | `			}` |
|      1049 | 1267 | `			for( k = 0 ; k < pFmt->nByte ; ++k ){` |
|       961 | 1268 | `				if( pFmt->zString[k] != '%' ){` |
|       568 | 1269 | `					continue;` |
|         - | 1270 | `				}` |
|       397 | 1271 | `				if( k + 1 >= pFmt->nByte ){` |
|       ! 0 | 1272 | `					return 0; /* a trailing '%' */` |
|         - | 1273 | `				}` |
|       397 | 1274 | `				k++;` |
|       397 | 1275 | `				if( pFmt->zString[k] == 's' \|\| pFmt->zString[k] == 'd' ){` |
|       168 | 1276 | `					nPlace++;` |
|       315 | 1277 | `				}else if( pFmt->zString[k] != '%' ){` |
|       223 | 1278 | `					return 0; /* any other conversion */` |
|         - | 1279 | `				}` |
|        91 | 1280 | `			}` |
|        92 | 1281 | `			if( nPlace != nArg - 1 ){` |
|        11 | 1282 | `				return 0;` |
|         - | 1283 | `			}` |
|        40 | 1284 | `		}` |
|    202270 | 1285 | `		return 1;` |
|       ! 0 | 1286 | `	}` |
|    667106 | 1287 | `	return 0;` |
|    452324 | 1288 | `}` |
|         - | 1289 | `/*` |
|         - | 1290 | ` * The two write-target rules php decides at COMPILE time, in one place because` |
|         - | 1291 | ` * every write site has to make both of them.` |
|         - | 1292 | ` *` |
|         - | 1293 | `` * **`$this`** is not a variable a program may re-point: php refuses the`` |
|         - | 1294 | `` * assignment, the reference bind, a foreach/list target and `unset()` where they`` |
|         - | 1295 | `` * are WRITTEN. PHL performed all of them, so `$this = 5;` inside a method`` |
|         - | 1296 | ` * replaced the receiver with an int for the rest of the call and every later` |
|         - | 1297 | `` * `$this->x` failed somewhere else entirely.`` |
|         - | 1298 | ` *` |
|         - | 1299 | ``  * **A temporary** cannot be written THROUGH: `(new A)->p = 1` and `"s"->p = 1` `` |
|         - | 1300 | ` * modify an object/value that no longer exists after the statement, so php` |
|         - | 1301 | `` * refuses the whole chain — every write kind, including `+=`, `++`, `=&` and`` |
|         - | 1302 | `` * `unset()`. The base of the access chain decides: a variable and a userland`` |
|         - | 1303 | `` * CALL are writable (`f()->p = 1` is php-legal), a `new`, a literal and any`` |
|         - | 1304 | ` * other computed value are not, and an internal function's result gets php's own` |
|         - | 1305 | `` * separate wording — which is what `(clone $o)->p = 1` is, `clone` being a`` |
|         - | 1306 | ` * function in php 8.5.` |
|         - | 1307 | ` *` |
|         - | 1308 | ` * **A call is writable THROUGH but not writable INTO.** The distinction is` |
|         - | 1309 | `` * php's, and it is made in two different places: `zend_compile_var_inner` lets`` |
|         - | 1310 | `` * a call be the base of a chain, while `zend_ensure_writable_variable` refuses`` |
|         - | 1311 | ` * the call when it is the target ITSELF, with a wording that says which kind of` |
|         - | 1312 | `` * call it was. So `f()[0] = 5` compiles and `f() = 5` does not. The one write`` |
|         - | 1313 | `` * site that does not ask the second question is the SOURCE of `=&`, which is`` |
|         - | 1314 | `` * why `$r =& f()` is a runtime notice rather than a compile error.`` |
|         - | 1315 | ` */` |
|   1125048 | 1316 | `PH7_PRIVATE sxi32 GenStateWriteTargetCheck(ph7_gen_state *pGen,ph7_expr_node *pTarget,int iCtx)` |
|         5 | 1317 | `{` |
|   1125053 | 1318 | `	ph7_expr_node *pBase = pTarget;` |
|   1125053 | 1319 | `	const char *zMsg = 0;` |
|         - | 1320 | `	sxi32 rc;` |
|   1125053 | 1321 | `	if( pTarget == 0 ){` |
|       ! 0 | 1322 | `		return SXRET_OK;` |
|         - | 1323 | `	}` |
|   1125053 | 1324 | `	if( PH7_ExprNodeIsThis(pTarget) && (iCtx & (PH7_WTC_REFSRC\|PH7_WTC_RMW\|PH7_WTC_THISSRC)) == 0 ){` |
|         - | 1325 | ``		/* Only as the TARGET. php refuses `$this = …`, `$this =& …`, a`` |
|         - | 1326 | ``		 * foreach/list target and `unset($this)` -- but the SOURCE of a `=&` is`` |
|         - | 1327 | `		 * compiled in write context WITHOUT zend_ensure_writable_variable, and` |
|         - | 1328 | ``		 * that is the function that holds the $this rule. So `$t =& $this` binds`` |
|         - | 1329 | ``		 * the receiver, and so do `$a[] =& $this`, `$this->p =& $this` and`` |
|         - | 1330 | ``		 * `self::$s =& $this`; a $this that has no object behind it is the`` |
|         - | 1331 | `		 * ordinary RUNTIME "Using $this when not in object context". Refusing the` |
|         - | 1332 | ``		 * source here cost react/promise's `$target =& $this` -- Composer's whole`` |
|         - | 1333 | `		 * async download layer.` |
|         - | 1334 | `		 *` |
|         - | 1335 | `		 * And only for an ASSIGNMENT. php makes this rule in the assignment` |
|         - | 1336 | ``		 * compiler, so a READ-MODIFY-WRITE (`$this += 1`, `$this .= "x"`,`` |
|         - | 1337 | ``		 * `$this++`) compiles and raises the ordinary operand error at run time`` |
|         - | 1338 | ``		 * (`Unsupported operand types: C + int`, `Cannot increment C`) -- which`` |
|         - | 1339 | `		 * this engine already words for any other object. */` |
|        15 | 1340 | `		zMsg = (iCtx & PH7_WTC_UNSET) ? "Cannot unset $this" : "Cannot re-assign $this";` |
|   1125045 | 1341 | `	}else if( pTarget->pOp && pTarget->pOp->iOp == EXPR_OP_FUNC_CALL` |
|    104703 | 1342 | `	       && (iCtx & PH7_WTC_REFSRC) == 0 ){` |
|         - | 1343 | ``		/* The target is the call itself (`f() = 5`, `f()++`, `unset(f())`,`` |
|         - | 1344 | ``		 * `foreach (… as f())`). php names the kind of call: a METHOD callee —`` |
|         - | 1345 | ``		 * `$o->m()`, `C::m()` — reports "method", everything else "function".`` |
|         - | 1346 | `		 * A PARENTHESISED member callee is php's variable-invocation` |
|         - | 1347 | ``		 * (`($o->p)()` calls the property's VALUE), which is an ordinary`` |
|         - | 1348 | `		 * function call, exactly the distinction the OP_CALL codegen makes. */` |
|        27 | 1349 | `		int bMethod = pTarget->pLeft` |
|        12 | 1350 | `			&& pTarget->pLeft->pOp` |
|         9 | 1351 | `			&& (pTarget->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|         5 | 1352 | `			 \|\| pTarget->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|         3 | 1353 | `			 \|\| pTarget->pLeft->pOp->iOp == EXPR_OP_DC)` |
|        18 | 1354 | `			&& (pTarget->pLeft->iFlags & EXPR_NODE_PARENS) == 0;` |
|        15 | 1355 | `		zMsg = bMethod` |
|         - | 1356 | `			? "Can't use method return value in write context"` |
|         6 | 1357 | `			: "Can't use function return value in write context";` |
|   1125035 | 1358 | `	}else if( PH7_ExprContainsNullsafe(pTarget) ){` |
|         - | 1359 | ``		/* php asks this AFTER the call question (`$o?->m()++` is a method return`` |
|         - | 1360 | `` 		 * value, not a nullsafe chain) and BEFORE the base one (`(new A)?->p = 1` `` |
|         - | 1361 | `		 * is the nullsafe refusal, not the temporary). A reference SOURCE has its` |
|         - | 1362 | ``		 * own sentence for it. The `=`/`+=`/`unset()`/foreach paths screened this`` |
|         - | 1363 | ``		 * themselves; `++`/`--`, `??=` and `array(&…)` did not, so `$o?->p++` ran. */`` |
|        10 | 1364 | `		zMsg = (iCtx & PH7_WTC_REFSRC)` |
|         - | 1365 | `			? "Cannot take reference of a nullsafe chain"` |
|         4 | 1366 | `			: "Can't use nullsafe operator in write context";` |
|         6 | 1367 | `	}else{` |
|         - | 1368 | `		/* Walk to the base of the access chain; the links themselves are writable. */` |
|   1334929 | 1369 | `		while( pBase && pBase->pOp ){` |
|    210459 | 1370 | `			if( pBase->pOp->iOp == EXPR_OP_DC && !PH7_ExprNodeIsClassConst(pBase) ){` |
|         - | 1371 | `` 				/* A `::` left operand is a CLASS reference, not a value — `C::$s = 1` `` |
|         - | 1372 | ``				 * and even `(new C)::$s = 1` write class-level storage that outlives`` |
|         - | 1373 | `` 				 * any temporary, so the chain stops being about a base here. A `::` `` |
|         - | 1374 | ``				 * naming a CONSTANT is not storage, though: `A::K[0] = 5` subscripts`` |
|         - | 1375 | `				 * a COPY, so it falls through to the computed-base verdict below —` |
|         - | 1376 | `				 * php's "Cannot use temporary expression in write context", where` |
|         - | 1377 | `				 * PHL wrote into the copy and answered nothing. */` |
|       315 | 1378 | `				return SXRET_OK;` |
|         - | 1379 | `			}` |
|    210144 | 1380 | `			if( pBase->pOp->iOp != EXPR_OP_ARROW && pBase->pOp->iOp != EXPR_OP_NULLSAFE_ARROW` |
|    206925 | 1381 | `			 && pBase->pOp->iOp != EXPR_OP_SUBSCRIPT ){` |
|       241 | 1382 | `				break;` |
|         - | 1383 | `			}` |
|    209913 | 1384 | `			pBase = pBase->pLeft;` |
|         5 | 1385 | `		}` |
|   1124711 | 1386 | `		if( pBase == 0 \|\| pBase == pTarget ){` |
|         - | 1387 | `			/* No chain: a non-variable target of its own is the caller's business` |
|         - | 1388 | `			 * (php reports its parse error / "Assignments can only happen to` |
|         - | 1389 | `			 * writable values" there, and so does PHL). */` |
|    915489 | 1390 | `			return SXRET_OK;` |
|         - | 1391 | `		}` |
|    209227 | 1392 | `		if( pBase->pOp == 0 ){` |
|    209051 | 1393 | `			if( pBase->xCode != PH7_CompileVariable ){` |
|       ! 0 | 1394 | `				zMsg = "Cannot use temporary expression in write context";` |
|         5 | 1395 | `			}` |
|    104567 | 1396 | `		}else if( pBase->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|         - | 1397 | `			/* php refuses a write through the result of a call it SPECIALIZED into` |
|         - | 1398 | `			 * an opcode — see aSpecialFunc above. The old test asked whether the` |
|         - | 1399 | `			 * name was a host function AT ALL, which would have refused every` |
|         - | 1400 | `			 * builtin (php specializes 28 of them), and asked it of the CALL node` |
|         - | 1401 | `			 * where GenStateCallBuiltinName wants the CALLEE node — so it never` |
|         - | 1402 | ``			 * matched anything and `clone` below was the only arm that ever fired.`` |
|         - | 1403 | `			 * The name table IS the resolution here: php looks the callee up in a` |
|         - | 1404 | `			 * function table that is fully populated at compile time, and PHL's is` |
|         - | 1405 | `			 * not — the ~650 core builtins register in PH7_VmMakeReady, which runs` |
|         - | 1406 | `			 * AFTER compilation (see the redeclaration guard near the top of this` |
|         - | 1407 | ``			 * file), so hHostFunction has no `strlen` to find. */`` |
|         - | 1408 | `			SyString sName;` |
|       170 | 1409 | `			GenStateCallBuiltinName(pBase->pLeft,&sName);` |
|       170 | 1410 | `			if( GenStateCallIsSpecialized(&(*pGen),pBase,&sName) ){` |
|         6 | 1411 | `				zMsg = "Cannot use result of built-in function in write context";` |
|         6 | 1412 | `			}` |
|        95 | 1413 | `		}else if( pBase->pOp->iOp == EXPR_OP_CLONE ){` |
|         - | 1414 | ``			/* php 8.5 implements `clone` AS a function, so a write through its result`` |
|         - | 1415 | `			 * takes the internal-function wording rather than the temporary one. */` |
|         3 | 1416 | `			zMsg = "Cannot use result of built-in function in write context";` |
|         2 | 1417 | `		}else{` |
|         - | 1418 | ``			/* `new`, and every other computed base. */`` |
|        10 | 1419 | `			zMsg = "Cannot use temporary expression in write context";` |
|         - | 1420 | `		}` |
|         - | 1421 | `	}` |
|    209259 | 1422 | `	if( zMsg == 0 ){` |
|    209213 | 1423 | `		return SXRET_OK;` |
|         - | 1424 | `	}` |
|        50 | 1425 | `	rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|        46 | 1426 | `		pTarget->pStart ? pTarget->pStart->nLine : 0,"%s",zMsg);` |
|        50 | 1427 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_INVALID;` |
|    561763 | 1428 | `}` |
|         - | 1429 | `/*` |
|         - | 1430 | ` * What emitting a call's ARGUMENT LIST decided, handed back to the CALL codegen.` |
|         - | 1431 | ` * The arguments are emitted from their own routine because php evaluates them` |
|         - | 1432 | ` * AFTER the callee has been resolved, so this runs between the callee's emission` |
|         - | 1433 | ` * and the OP_CALL — see GenStateEmitCallArgs.` |
|         - | 1434 | ` */` |
|         - | 1435 | `typedef struct GenCallArgs GenCallArgs;` |
|         - | 1436 | `struct GenCallArgs {` |
|         - | 1437 | `	sxi32 iP1;      /* OP_CALL.iP1: the compile-time argument count */` |
|         - | 1438 | ``	sxu32 iP2;      /* OP_CALL.iP2: 1 if any argument unpacks (`...$a`) */`` |
|         - | 1439 | `	void *p3;       /* OP_CALL.p3: the VmCallArgMap, which may ALREADY carry the callee's` |
|         - | 1440 | `	                 * namespace qualification — the callee is emitted first now */` |
|         - | 1441 | ``	int bFcc;       /* First-class callable `f(...)`: no arguments, OP_LOAD_FCC follows */`` |
|         - | 1442 | ``	int bAnySpread; /* Any `...` argument (iP2 says the same; kept for the shape masks) */`` |
|         - | 1443 | ``	int bNewCallee; /* IN: the list is a `new`'s — the slot below it is a CLASS operand, whose`` |
|         - | 1444 | `	                 * constructor a named argument's send-time screen asks (PH7_ROT_NEW) */` |
|         - | 1445 | `};` |
|         - | 1446 | `static sxi32 GenStateEmitCallArgs(ph7_gen_state *pGen,ph7_expr_node *pNode,sxi32 iFlags,` |
|         - | 1447 | `	GenCallArgs *pArgs);` |
|         - | 1448 | `/*` |
|         - | 1449 | `` * TRUE when literal nLit is the keyword `self`, `static` or `parent` -- the`` |
|         - | 1450 | `` * class operand of `self::`, `new static`, `$x instanceof parent`. php resolves the`` |
|         - | 1451 | `` * three only where they are WRITTEN: the same string reaching a `::`, `new` or`` |
|         - | 1452 | `` * `instanceof` through a variable is an ordinary class name, and no class can be`` |
|         - | 1453 | `` * called that, so `$c = 'self'; $c::m()` is `Class "self" not found`. The run-time`` |
|         - | 1454 | ` * doors see a string either way, so the codegen marks the consuming instruction` |
|         - | 1455 | ` * (VmInstr::bDiscard's second meaning) and they resolve the keyword under that` |
|         - | 1456 | ` * mark only.` |
|         - | 1457 | ` */` |
|    147097 | 1458 | `static int GenStateLitIsScopeKeyword(ph7_gen_state *pGen,sxu32 nLit)` |
|         5 | 1459 | `{` |
|    147102 | 1460 | `	ph7_value *pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLit);` |
|         - | 1461 | `	const char *z;` |
|         - | 1462 | `	sxu32 n;` |
|    147102 | 1463 | `	if( pLit == 0 \|\| (pLit->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 1464 | `		return 0;` |
|         - | 1465 | `	}` |
|    147102 | 1466 | `	z = (const char *)SyBlobData(&pLit->sBlob);` |
|    147102 | 1467 | `	n = (sxu32)SyBlobLength(&pLit->sBlob);` |
|    147283 | 1468 | `	return (n == 4 && SyMemcmp(z,"self",4) == 0)` |
|    146805 | 1469 | `		\|\| (n == 6 && SyMemcmp(z,"static",6) == 0)` |
|    220742 | 1470 | `		\|\| (n == 6 && SyMemcmp(z,"parent",6) == 0);` |
|     73457 | 1471 | `}` |
|         - | 1472 | `/*` |
|         - | 1473 | `` * php's COMPILE-time refusal of a written `self`/`parent`/`static` class operand`` |
|         - | 1474 | ``  * (zend_ensure_valid_class_fetch_type), for literal nLit about to feed a `::`, a `new` `` |
|         - | 1475 | `` * or an `instanceof`. php asks only where the scope is KNOWN while the body compiles`` |
|         - | 1476 | ` * (zend_is_scope_known): a named function has none, even written inside a method, and` |
|         - | 1477 | `` * a method has its class -- unless that class is a trait, whose `self` is the user.`` |
|         - | 1478 | ` * Everywhere else the question waits for run time: top-level code (an include can run` |
|         - | 1479 | ` * inside a method), a closure or arrow function (it can be rebound), and a` |
|         - | 1480 | ` * const-expression -- a parameter, property or constant default, which php compiles` |
|         - | 1481 | ` * lazily. Those keep the run-time Error the member door already raises.` |
|         - | 1482 | ` */` |
|      1108 | 1483 | `static sxi32 GenStateScreenScopeKeyword(ph7_gen_state *pGen,sxu32 nLit,sxu32 nLine)` |
|         5 | 1484 | `{` |
|      1113 | 1485 | `	ph7_value *pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLit);` |
|      1113 | 1486 | `	GenBlock *pBlock = pGen->pCurrent;` |
|      1113 | 1487 | `	ph7_vm_func *pFunc = 0;` |
|      1113 | 1488 | `	ph7_class *pScope = 0;` |
|         - | 1489 | `	const char *zKw;` |
|      1113 | 1490 | `	if( pGen->iInMemberDefault > 0 \|\| pLit == 0 ){` |
|        61 | 1491 | `		return SXRET_OK;` |
|         - | 1492 | `	}` |
|      1057 | 1493 | `	zKw = (const char *)SyBlobData(&pLit->sBlob); /* not NUL-terminated */` |
|      2151 | 1494 | `	while( pBlock ){` |
|      2101 | 1495 | `		if( (pBlock->iFlags & GEN_BLOCK_FUNC) && pBlock->pUserData ){` |
|      1007 | 1496 | `			pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      1007 | 1497 | `			break;` |
|         - | 1498 | `		}` |
|      1099 | 1499 | `		pBlock = pBlock->pParent;` |
|         5 | 1500 | `	}` |
|         - | 1501 | `	/* VM_FUNC_CLOSURE is only set on a closure that captures: a capture-less static` |
|         - | 1502 | `	 * one carries VM_FUNC_STATIC_CL alone. */` |
|      1057 | 1503 | `	if( pFunc == 0 \|\| (pFunc->iFlags & (VM_FUNC_CLOSURE\|VM_FUNC_ARROW\|VM_FUNC_STATIC_CL)) ){` |
|       190 | 1504 | `		return SXRET_OK;` |
|         - | 1505 | `	}` |
|       871 | 1506 | `	if( pGen->pCurClass ){` |
|         - | 1507 | `		/* A function block at or above the class body's is OUTSIDE it: this is a` |
|         - | 1508 | `		 * class-body const-expression of a class declared inside that function. */` |
|       871 | 1509 | `		GenBlock *pUp = pGen->pCurClassBlock;` |
|      1741 | 1510 | `		while( pUp ){` |
|       875 | 1511 | `			if( pUp == pBlock ){` |
|       ! 0 | 1512 | `				return SXRET_OK;` |
|         - | 1513 | `			}` |
|       875 | 1514 | `			pUp = pUp->pParent;` |
|         5 | 1515 | `		}` |
|       433 | 1516 | `	}` |
|       871 | 1517 | `	if( pFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|       869 | 1518 | `		pScope = pGen->pCurClass;` |
|       869 | 1519 | `		if( pScope && (pScope->iFlags & PH7_CLASS_TRAIT) ){` |
|        83 | 1520 | `			return SXRET_OK;` |
|         - | 1521 | `		}` |
|       393 | 1522 | `	}` |
|       793 | 1523 | `	if( pScope == 0 ){` |
|         4 | 1524 | `		return PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 1525 | `			"Cannot use \"%.*s\" when no class scope is active",` |
|         2 | 1526 | `			(int)SyBlobLength(&pLit->sBlob),zKw);` |
|         - | 1527 | `	}` |
|       791 | 1528 | `	if( zKw[0] == 'p' && pGen->pCurBase == 0 && (pScope->iFlags & PH7_CLASS_LINT_UNBOUND) == 0 ){` |
|         9 | 1529 | `		return PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 1530 | `			"Cannot use \"parent\" when current class scope has no parent");` |
|         - | 1531 | `	}` |
|       785 | 1532 | `	return SXRET_OK;` |
|       559 | 1533 | `}` |
|         - | 1534 | `/*` |
|         - | 1535 | `` * TRUE when the `instanceof` SUBJECT that just compiled into the instruction`` |
|         - | 1536 | ` * stream starting at nFirst is what zend calls IS_CONST -- the shape whose` |
|         - | 1537 | ` * whole expression its compiler folds to FALSE, without ever compiling the` |
|         - | 1538 | ` * class operand.` |
|         - | 1539 | ` *` |
|         - | 1540 | `` * php decides this from its own constant FOLDER: `zend_compile_expr` on the`` |
|         - | 1541 | `` * subject comes back IS_CONST for a literal, for `null`/`true`/`false`, for an`` |
|         - | 1542 | ` * engine constant, for an array literal, and for arithmetic or concatenation` |
|         - | 1543 | `` * over any of those -- and `5 instanceof $x` is then false whatever $x holds,`` |
|         - | 1544 | `` * while `$v instanceof $x` with the same 5 in $v reaches the runtime opcode and`` |
|         - | 1545 | ` * is refused when $x is neither an object nor a string. The two spellings really` |
|         - | 1546 | ` * do answer differently, so OP_IS_A's screen has to be told which one it is.` |
|         - | 1547 | ` *` |
|         - | 1548 | ` * PHL has no constant folder, so the question is asked of the INSTRUCTIONS the` |
|         - | 1549 | ` * subject compiled to: a run built only from LITERAL loads and pure value` |
|         - | 1550 | ` * operators is a constant expression, and anything that reads a variable, names` |
|         - | 1551 | ` * a constant, calls something or touches an object is not. php's own folder` |
|         - | 1552 | `` * reaches two shapes further -- an ENGINE constant (`PHP_EOL`) and a builtin`` |
|         - | 1553 | `` * call it ct-evaluates (`strlen("a")`) -- where php answers false and this`` |
|         - | 1554 | ` * refuses; the pair is recorded under the constant-folding family.` |
|         - | 1555 | ` */` |
|     17592 | 1556 | `static int GenStateInstanceofFoldsLhs(ph7_gen_state *pGen,sxu32 nFirst)` |
|         5 | 1557 | `{` |
|         - | 1558 | `	sxu32 n;` |
|     17597 | 1559 | `	sxu32 nLen = PH7_VmInstrLength(pGen->pVm);` |
|     17899 | 1560 | `	for( n = nFirst ; n < nLen ; ++n ){` |
|     17883 | 1561 | `		VmInstr *pIn = PH7_VmGetInstr(pGen->pVm,n);` |
|     17883 | 1562 | `		if( pIn == 0 ){` |
|       ! 0 | 1563 | `			return 0;` |
|         - | 1564 | `		}` |
|     17883 | 1565 | `		switch( pIn->iOp ){` |
|       150 | 1566 | `		case PH7_OP_LOADC:` |
|         - | 1567 | `			/* A LOADC that still carries EXPAND is a NAME the runtime resolves --` |
|         - | 1568 | `			 * a constant -- and that is exactly what php does NOT fold: its` |
|         - | 1569 | `			 * compiler substitutes only the engine's own persistent constants, so` |
|         - | 1570 | ``			 * a userland `const OBJ = new C();` reaches the runtime opcode and`` |
|         - | 1571 | ``			 * `OBJ instanceof C` is a real question. Folding it answered FALSE for`` |
|         - | 1572 | `			 * every constant that holds an object. */` |
|       305 | 1573 | `			if( pIn->iP1 & PH7_LOADC_EXPAND ){` |
|         5 | 1574 | `				return 0;` |
|         - | 1575 | `			}` |
|       301 | 1576 | `			break;` |
|         3 | 1577 | `		case PH7_OP_LOAD_MAP:` |
|         - | 1578 | `		case PH7_OP_CAT:` |
|         - | 1579 | `		case PH7_OP_CVT_INT: case PH7_OP_CVT_STR: case PH7_OP_CVT_REAL:` |
|         - | 1580 | `		case PH7_OP_CVT_BOOL: case PH7_OP_CVT_NUMC: case PH7_OP_CVT_NULL:` |
|         - | 1581 | `		case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT: case PH7_OP_LNOT:` |
|         - | 1582 | `		case PH7_OP_MUL: case PH7_OP_DIV: case PH7_OP_MOD: case PH7_OP_POW:` |
|         - | 1583 | `		case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_SHL: case PH7_OP_SHR:` |
|         - | 1584 | `		case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:` |
|         - | 1585 | `		case PH7_OP_SPACESHIP: case PH7_OP_EQ: case PH7_OP_NEQ:` |
|         - | 1586 | `		case PH7_OP_TEQ: case PH7_OP_TNE:` |
|         - | 1587 | `		case PH7_OP_BAND: case PH7_OP_BXOR: case PH7_OP_BOR:` |
|         7 | 1588 | `			break;` |
|      8797 | 1589 | `		default:` |
|     17577 | 1590 | `			return 0;` |
|         - | 1591 | `		}` |
|       156 | 1592 | `	}` |
|        17 | 1593 | `	return nLen > nFirst;` |
|      8790 | 1594 | `}` |
|         - | 1595 | `/*` |
|         - | 1596 | ` * The child flags a subscript/property NAME is compiled under: whatever the access itself` |
|         - | 1597 | ` * was given, minus every context that belongs to the ACCESS rather than to the expression` |
|         - | 1598 | ` * that names it. Kept beside the LOAD_IDX arm that uses the same mask, since the two have` |
|         - | 1599 | ` * to agree -- a name parked ahead of the assigned value is compiled here and read back` |
|         - | 1600 | ` * there, and a difference between the two would compile one expression two ways.` |
|         - | 1601 | ` */` |
|         - | 1602 | `#define GEN_ACCESS_NAME_MASK  (~(EXPR_FLAG_LOAD_IDX_STORE \` |
|         - | 1603 | `	\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET \` |
|         - | 1604 | `	\|EXPR_FLAG_LOAD_IDX_UNSET_BASE \` |
|         - | 1605 | `	\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE \` |
|         - | 1606 | `	\|EXPR_FLAG_MEMBER_COALESCE \` |
|         - | 1607 | `	\|EXPR_FLAG_QUIET_VAR\|EXPR_FLAG_RMW_LOAD\|EXPR_FLAG_DEFER_ARG))` |
|         - | 1608 | `/*` |
|         - | 1609 | ` * The NAME expression of one access, or 0 when the access has none to run.` |
|         - | 1610 | ` *` |
|         - | 1611 | `` * A subscript names its element with its single index node; `->` and `?->` name their`` |
|         - | 1612 | `` * property with the right operand. `::` is left out on purpose: a static property's name`` |
|         - | 1613 | `` * is folded into the OP_MEMBER itself rather than pushed, and the append form `[]` names`` |
|         - | 1614 | ` * nothing.` |
|         - | 1615 | ` */` |
|    208550 | 1616 | `static ph7_expr_node * GenStateAccessNameNode(ph7_expr_node *pAccess)` |
|         5 | 1617 | `{` |
|    208555 | 1618 | `	if( pAccess == 0 \|\| pAccess->pOp == 0 ){` |
|       ! 0 | 1619 | `		return 0;` |
|         - | 1620 | `	}` |
|    208555 | 1621 | `	if( pAccess->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|    206001 | 1622 | `		ph7_expr_node **apArg = (ph7_expr_node **)SySetBasePtr(&pAccess->aNodeArgs);` |
|    206001 | 1623 | `		return SySetUsed(&pAccess->aNodeArgs) == 1 ? apArg[0] : 0;` |
|         - | 1624 | `	}` |
|      2559 | 1625 | `	if( pAccess->pOp->iOp == EXPR_OP_ARROW \|\| pAccess->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|      2559 | 1626 | `		return pAccess->pRight;` |
|         - | 1627 | `	}` |
|       ! 0 | 1628 | `	return 0;` |
|    104143 | 1629 | `}` |
|         - | 1630 | `/*` |
|         - | 1631 | ` * Collect an assignment TARGET's dynamic names, in the order php evaluates them.` |
|         - | 1632 | ` *` |
|         - | 1633 | `` * php compiles `$a[k()][j()] = v()` as: the two subscript expressions, then the assigned`` |
|         - | 1634 | ``  * value, and only then the fetches that use them -- so `k()` and `j()` run before `v()` `` |
|         - | 1635 | ` * does, while the array is still untouched by the write. PHL emits the value first and the` |
|         - | 1636 | ` * whole target after it, which reverses every one of those side effects.` |
|         - | 1637 | ` *` |
|         - | 1638 | `` * Only a name that can RUN is collected. php reads a plain `$var` or a literal name off the`` |
|         - | 1639 | `` * fetch opline's own operand, at the fetch, which is AFTER the value: `$k = 'A'; $a[$k] =`` |
|         - | 1640 | `` * f();` with an `f()` that assigns `'B'` to `$k` stores under `B` there, and under `B` here`` |
|         - | 1641 | `` * for the same reason. Parking one of those would answer `A`, so the same shape test the`` |
|         - | 1642 | ` * operand-snapshot rule turns on decides this too.` |
|         - | 1643 | ` *` |
|         - | 1644 | ` * Returns the count (0 = nothing to park, compile as before), or -1 when more than` |
|         - | 1645 | ` * PH7_STORE_KEY_MAX names in one target can run -- which compiles as before, in the old` |
|         - | 1646 | ` * order. The ceiling is a real one and it is set well past anything written: it takes NINE` |
|         - | 1647 | ` * running subscripts on a single assignment target to reach it.` |
|         - | 1648 | ` */` |
|    928231 | 1649 | `static int GenStateCollectStoreNames(ph7_expr_node *pTarget,ph7_expr_node **apOut)` |
|         5 | 1650 | `{` |
|         - | 1651 | `	ph7_expr_node *apChain[32];` |
|    928236 | 1652 | `	ph7_expr_node *p = pTarget;` |
|    928236 | 1653 | `	int nChain = 0;` |
|    928236 | 1654 | `	int nOut = 0;` |
|         - | 1655 | `	int n;` |
|   1241106 | 1656 | `	while( p && p->pOp && (p->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|    105779 | 1657 | `	    \|\| p->pOp->iOp == EXPR_OP_ARROW \|\| p->pOp->iOp == EXPR_OP_NULLSAFE_ARROW) ){` |
|    208555 | 1658 | `		if( nChain >= (int)SX_ARRAYSIZE(apChain) ){` |
|       ! 0 | 1659 | `			return -1;` |
|         - | 1660 | `		}` |
|    208555 | 1661 | `		apChain[nChain++] = p;` |
|    208555 | 1662 | `		p = p->pLeft;` |
|         5 | 1663 | `	}` |
|         - | 1664 | `	/* The chain was walked outermost-first; php names them base-first. */` |
|   1136786 | 1665 | `	for( n = nChain - 1 ; n >= 0 ; --n ){` |
|    208555 | 1666 | `		ph7_expr_node *pName = GenStateAccessNameNode(apChain[n]);` |
|    208555 | 1667 | `		if( pName == 0 \|\| !GenStateArgRunsCode(pName) ){` |
|    208083 | 1668 | `			continue;` |
|         - | 1669 | `		}` |
|       477 | 1670 | `		if( nOut >= PH7_STORE_KEY_MAX ){` |
|       ! 0 | 1671 | `			return -1;` |
|         - | 1672 | `		}` |
|       477 | 1673 | `		apOut[nOut++] = pName;` |
|       241 | 1674 | `	}` |
|    928236 | 1675 | `	return nOut;` |
|    463493 | 1676 | `}` |
|         - | 1677 | `/*` |
|         - | 1678 | ` * Where the parked name at index iSlot sits, counted down from the top of the stack, at the` |
|         - | 1679 | ` * moment the access that owns it is emitted.` |
|         - | 1680 | ` *` |
|         - | 1681 | ` * Above the parked run the stack holds exactly two things by then: the assigned value, and` |
|         - | 1682 | ` * the container this access is about to read -- every level of the chain consumes a` |
|         - | 1683 | ` * container and a name and leaves one element in their place, so the shape is the same at` |
|         - | 1684 | ` * every level. The parked names sit under that in push order, so the FIRST one parked is` |
|         - | 1685 | ` * the deepest.` |
|         - | 1686 | ` */` |
|         - | 1687 | `#define GEN_STORE_KEY_DEPTH(nParked,iSlot)  ((sxi32)((nParked) + 1 - (iSlot)))` |
|         - | 1688 | `/*` |
|         - | 1689 | ` * Generate bytecode for a given expression tree.` |
|         - | 1690 | ` * If something goes wrong while generating bytecode` |
|         - | 1691 | ` * for the expression tree (A very unlikely scenario)` |
|         - | 1692 | ` * this function takes care of generating the appropriate` |
|         - | 1693 | ` * error message.` |
|         - | 1694 | ` */` |
|         - | 1695 | `/*` |
|         - | 1696 | `` * php's `zend_is_variable_or_call`: what may sit on the right of a destructuring`` |
|         - | 1697 | ` * assignment whose target list binds BY REFERENCE. A variable, a property, a` |
|         - | 1698 | ` * static property, a subscript and a CALL can each hand a slot over; an array` |
|         - | 1699 | `` * literal, a string, `new`, and any computed value cannot, and php refuses those`` |
|         - | 1700 | ` * at compile time rather than binding to a temporary.` |
|         - | 1701 | ` */` |
|       406 | 1702 | `static int GenStateNodeIsRefSource(ph7_expr_node *pNode)` |
|         5 | 1703 | `{` |
|       411 | 1704 | `	if( pNode == 0 ){` |
|       ! 0 | 1705 | `		return 0;` |
|         - | 1706 | `	}` |
|       411 | 1707 | `	if( pNode->pOp ){` |
|       280 | 1708 | `		return pNode->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|       183 | 1709 | `		    \|\| pNode->pOp->iOp == EXPR_OP_ARROW` |
|       177 | 1710 | `		    \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|       172 | 1711 | `		    \|\| pNode->pOp->iOp == EXPR_OP_DC` |
|       275 | 1712 | `		    \|\| pNode->pOp->iOp == EXPR_OP_FUNC_CALL;` |
|         - | 1713 | `	}` |
|       227 | 1714 | `	return pNode->xCode == PH7_CompileVariable;` |
|       208 | 1715 | `}` |
|         - | 1716 | `/*` |
|         - | 1717 | `` * Is this call node's callee the `isset` KEYWORD itself?`` |
|         - | 1718 | ` *` |
|         - | 1719 | ` * isset() is a language construct, not a name: it reaches the call path as a` |
|         - | 1720 | ` * keyword token whose literal the compiler canonicalizes to "isset" (see` |
|         - | 1721 | ` * compile_node.c). A keyword used as a MEMBER name is excluded here for the same` |
|         - | 1722 | `` * reason it is excluded there — `$o->isset(...)` names a method — and so is any`` |
|         - | 1723 | ` * callee that is an operator node rather than a bare literal.` |
|         - | 1724 | ` */` |
|    460875 | 1725 | `static int GenStateCalleeIsIsset(ph7_expr_node *pCallee)` |
|         5 | 1726 | `{` |
|         - | 1727 | `	SyString *pName;` |
|    460880 | 1728 | `	if( pCallee == 0 \|\| pCallee->pOp != 0 \|\| pCallee->pStart == 0 ){` |
|      3079 | 1729 | `		return 0;` |
|         - | 1730 | `	}` |
|    457801 | 1731 | `	if( (pCallee->pStart->nType & PH7_TK_KEYWORD) == 0` |
|    228417 | 1732 | `	 \|\| (pCallee->pStart->nType & PH7_TK_MEMBER_NAME) ){` |
|    457712 | 1733 | `		return 0;` |
|         - | 1734 | `	}` |
|        99 | 1735 | `	pName = &pCallee->pStart->sData;` |
|       140 | 1736 | `	return pName->nByte == sizeof("isset")-1` |
|        94 | 1737 | `		&& SyStrnicmp(pName->zString,"isset",sizeof("isset")-1) == 0;` |
|    229904 | 1738 | `}` |
|         - | 1739 | `/*` |
|         - | 1740 | ` * Is this callee the keyword of a language CONSTRUCT that compiles to a call --` |
|         - | 1741 | `` * `isset`, `empty` or `eval`? (`unset`, `print` and the four file-inclusion words have`` |
|         - | 1742 | ` * their own codegen and mark their OP_CALL directly.)` |
|         - | 1743 | ` *` |
|         - | 1744 | ` * The answer rides the emitted call as PH7_CALL_CONSTRUCT, which is what lets the` |
|         - | 1745 | ` * construct's hidden host function answer it and nothing else: see` |
|         - | 1746 | ` * PH7_VmGetHostFunction. Read off the token's KEYWORD ID, never its text -- only a` |
|         - | 1747 | ` * keyword token can produce one of these calls, and that is what makes the mark` |
|         - | 1748 | ` * unforgeable by a program.` |
|         - | 1749 | ` */` |
|   2232843 | 1750 | `static int GenStateCalleeIsConstruct(ph7_expr_node *pCallee)` |
|         5 | 1751 | `{` |
|         - | 1752 | `	sxu32 nKw;` |
|   2232848 | 1753 | `	if( pCallee == 0 \|\| pCallee->pOp != 0 \|\| pCallee->pStart == 0 ){` |
|     29359 | 1754 | `		return 0;` |
|         - | 1755 | `	}` |
|   2203489 | 1756 | `	if( (pCallee->pStart->nType & PH7_TK_KEYWORD) == 0` |
|   1118537 | 1757 | `	 \|\| (pCallee->pStart->nType & PH7_TK_MEMBER_NAME) ){` |
|   2165679 | 1758 | `		return 0;` |
|         - | 1759 | `	}` |
|     37820 | 1760 | `	nKw = (sxu32)SX_PTR_TO_INT(pCallee->pStart->pUserData);` |
|     37820 | 1761 | `	return nKw == PH7_TKWRD_ISSET \|\| nKw == PH7_TKWRD_EMPTY \|\| nKw == PH7_TKWRD_EVAL;` |
|   1114265 | 1762 | `}` |
|  12364937 | 1763 | `static sxi32 GenStateEmitExprCode(` |
|         - | 1764 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 1765 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - | 1766 | `	sxi32 iFlags /* Control flags */` |
|         - | 1767 | `	)` |
|         5 | 1768 | `{` |
|         - | 1769 | `	VmInstr *pInstr;` |
|         - | 1770 | `	sxu32 nJmpIdx;` |
|  12364942 | 1771 | `	sxi32 iP1 = 0;` |
|  12364942 | 1772 | `	sxu32 iP2 = 0;` |
|  12364942 | 1773 | `	void *p3  = 0;` |
|         - | 1774 | `	sxi32 iVmOp;` |
|         - | 1775 | `	sxi32 rc;` |
|  12364942 | 1776 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  12364942 | 1777 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  12364942 | 1778 | `	int bScopeKw = 0;   /* The class operand is a literal self/static/parent (see` |
|         - | 1779 | `	                     * GenStateLitIsScopeKeyword): stamped on the emitted op */` |
|  12364942 | 1780 | `	sxu32 nRhsNsBase = 0;` |
|  12364942 | 1781 | `	sxi32 iRhsFlags = 0; /* control flags the RIGHT operand is compiled under */` |
|  12364942 | 1782 | `	sxu32 nLhsFirst = 0; /* instruction index the LEFT operand starts at */` |
|  12364942 | 1783 | `	int bMoveLhs = 0;    /* the LEFT operand's load was lifted past the RIGHT one */` |
|         - | 1784 | `	VmInstr sMovedLhs;   /* ...and this is it, verbatim */` |
|         - | 1785 | ``	/* Consumed here so it describes THIS node only — the direct operand of a `new` —`` |
|         - | 1786 | `	 * and never travels down into the operand's own sub-expressions. */` |
|  12364942 | 1787 | `	int bNewCallee = (iFlags & EXPR_FLAG_NEW_CALLEE) != 0;` |
|  12364942 | 1788 | `	int nParked = 0;                          /* assignment target names parked on the stack */` |
|  12364942 | 1789 | `	int nOuterParked = 0;                     /* ...and what an enclosing emission had parked */` |
|  12364942 | 1790 | `	ph7_expr_node **apOuterParked = 0;` |
|         - | 1791 | `	ph7_expr_node *apParked[PH7_STORE_KEY_MAX];` |
|  12364942 | 1792 | `	iFlags &= ~EXPR_FLAG_NEW_CALLEE;` |
|  12364942 | 1793 | `	if( pGen->nStoreKey > 0 ){` |
|         - | 1794 | `		/* This node is a target name that was already compiled and parked, ahead of the` |
|         - | 1795 | `		 * assigned value; its access is being emitted now, so read it back rather than` |
|         - | 1796 | `		 * running it a second time. */` |
|         - | 1797 | `		int iSlot;` |
|      2495 | 1798 | `		for( iSlot = 0 ; iSlot < pGen->nStoreKey ; ++iSlot ){` |
|      1505 | 1799 | `			if( pGen->apStoreKey[iSlot] == pNode ){` |
|       713 | 1800 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_PICK,` |
|       472 | 1801 | `					GEN_STORE_KEY_DEPTH(pGen->nStoreKey,iSlot),0,0,0);` |
|       477 | 1802 | `				return SXRET_OK;` |
|         - | 1803 | `			}` |
|       519 | 1804 | `		}` |
|       495 | 1805 | `	}` |
|  12364470 | 1806 | `	if( pNode->xCode ){` |
|         - | 1807 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - | 1808 | `		/* Compile node */` |
|   7648744 | 1809 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|   7648744 | 1810 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|   7648744 | 1811 | `		RE_SWAP_DELIMITER(pGen);` |
|   7648744 | 1812 | `		return rc;` |
|         - | 1813 | `	}` |
|   4715731 | 1814 | `	if( pNode->pOp == 0 ){` |
|       ! 0 | 1815 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 1816 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 | 1817 | `		return SXERR_ABORT;` |
|         - | 1818 | `	}` |
|   4715731 | 1819 | `	iVmOp = pNode->pOp->iVmOp;` |
|   4715731 | 1820 | `	if( iVmOp == PH7_OP_STORE_REF && pNode->pLeft && PH7_ExprNodeIsThis(pNode->pLeft) ){` |
|         - | 1821 | ``		/* `$t =& $this` is a VALUE assignment. php's `$this` is not a slot a`` |
|         - | 1822 | `		 * reference can name -- the receiver lives in the frame's own field, not` |
|         - | 1823 | `		 * in a variable -- so the bind quietly degrades to a copy of the object` |
|         - | 1824 | ``		 * HANDLE: `$t` gets a slot of its own, `$t->v = 9` still reaches the same`` |
|         - | 1825 | `` 		 * object (that is identity, not reference), and `$t = 5` leaves `$this` `` |
|         - | 1826 | `		 * an object. Binding the slot instead let a write through the alias` |
|         - | 1827 | `		 * REPLACE the receiver for the rest of the call. The operands were` |
|         - | 1828 | `		 * swapped in parse.c, so the source is pLeft. */` |
|        17 | 1829 | `		iVmOp = PH7_OP_STORE;` |
|         8 | 1830 | `	}` |
|   4715731 | 1831 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - | 1832 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - | 1833 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - | 1834 | `		 * and later errors are still reported. */` |
|         3 | 1835 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 1836 | `			"The (unset) cast is no longer supported");` |
|         3 | 1837 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1838 | `			return SXERR_ABORT;` |
|         - | 1839 | `		}` |
|         1 | 1840 | `	}` |
|   4715731 | 1841 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|       195 | 1842 | `		sxu32 nJmp = 0;` |
|         - | 1843 | `		sxu32 nNcNsBase;` |
|         - | 1844 | `		VmInstr *pInstrFix;` |
|         - | 1845 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|         - | 1846 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|         - | 1847 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|         - | 1848 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|         - | 1849 | `		 * stack slot carries a writable nIdx. */` |
|       195 | 1850 | `		if( pNode->pRight ){` |
|         - | 1851 | ``			/* `$a[] ??= v` READS its target before deciding to write, so php refuses the`` |
|         - | 1852 | ``			 * append form anywhere in that target's chain (`$a[][0] ??= v` too), even`` |
|         - | 1853 | ``			 * though the same tag makes every other `[]` on this path a legal write`` |
|         - | 1854 | ``			 * target. Only the container chain is walked — a `[]` inside an INDEX`` |
|         - | 1855 | `			 * expression is an ordinary read and the subscript codegen refuses it. */` |
|       195 | 1856 | `			ph7_expr_node *pTgt = pNode->pRight;` |
|       442 | 1857 | `			while( pTgt && pTgt->pOp && (pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|       112 | 1858 | `			      \|\| pTgt->pOp->iOp == EXPR_OP_ARROW \|\| pTgt->pOp->iOp == EXPR_OP_DC) ){` |
|       168 | 1859 | `				if( pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT && SySetUsed(&pTgt->aNodeArgs) < 1 ){` |
|       ! 0 | 1860 | `					break;` |
|         - | 1861 | `				}` |
|       168 | 1862 | `				pTgt = pTgt->pLeft;` |
|         4 | 1863 | `			}` |
|       190 | 1864 | `			if( pTgt && pTgt->pOp && pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|         6 | 1865 | `			 && SySetUsed(&pTgt->aNodeArgs) < 1 ){` |
|       ! 0 | 1866 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 1867 | `					pNode->pRight->pStart ? pNode->pRight->pStart->nLine : 0,` |
|         - | 1868 | `					"Cannot use [] for reading");` |
|       ! 0 | 1869 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 1870 | `			}` |
|         - | 1871 | `` 			/* …and only THEN the write-target rules, php's order: `strval(1)[] ??= 3` `` |
|         - | 1872 | ``			 * is the append refusal, not the specialized-builtin one. `??=` compiles`` |
|         - | 1873 | `			 * its own way and so never reached this check at all, which is why` |
|         - | 1874 | ``			 * `(new A)->p ??= 3` and `"lit"->p->q ??= 3` used to run. */`` |
|       195 | 1875 | `			rc = GenStateWriteTargetCheck(&(*pGen),pNode->pRight,0);` |
|       195 | 1876 | `			if( rc != SXRET_OK ){` |
|         3 | 1877 | `				return rc;` |
|         - | 1878 | `			}` |
|       192 | 1879 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       192 | 1880 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|       192 | 1881 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1882 | `				return rc;` |
|         - | 1883 | `			}` |
|       192 | 1884 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|         - | 1885 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|         - | 1886 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|         - | 1887 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|         - | 1888 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|         - | 1889 | `			 * the store, so the parent array does not need to be copied at` |
|         - | 1890 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|         - | 1891 | `			 * cascade for the actual write path stays correct. */` |
|       192 | 1892 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|       192 | 1893 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|       101 | 1894 | `				pInstrFix->iP2 = 3;` |
|        49 | 1895 | `			}` |
|        94 | 1896 | `		}` |
|         - | 1897 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|       192 | 1898 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|         - | 1899 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|       192 | 1900 | `		if( pNode->pLeft ){` |
|       192 | 1901 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       192 | 1902 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|       192 | 1903 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1904 | `				return rc;` |
|         - | 1905 | `			}` |
|       192 | 1906 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        94 | 1907 | `		}` |
|         - | 1908 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|       192 | 1909 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|         - | 1910 | `		/* Patch the short-circuit jump to land after the store. */` |
|       192 | 1911 | `		if( nJmp > 0 ){` |
|       192 | 1912 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|       192 | 1913 | `			if( pInstrFix ){` |
|       192 | 1914 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|        94 | 1915 | `			}` |
|        94 | 1916 | `		}` |
|       192 | 1917 | `		return SXRET_OK;` |
|         - | 1918 | `	}` |
|   4715541 | 1919 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - | 1920 | `		sxu32 nJz,nJmp;` |
|         - | 1921 | `		sxu32 nTernaryNsBase;` |
|         - | 1922 | `		/* Ternary operator require special handling */` |
|         - | 1923 | `		/* Phase#1: Compile the condition */` |
|     47191 | 1924 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|     47191 | 1925 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|     47191 | 1926 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1927 | `			return rc;` |
|         - | 1928 | `		}` |
|         - | 1929 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - | 1930 | `		 * compiling the condition must short-circuit to the end of the` |
|         - | 1931 | `		 * condition expression, not leak past the ternary. */` |
|     47191 | 1932 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|     47191 | 1933 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|     47191 | 1934 | `		if( pNode->pLeft ){` |
|         - | 1935 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - | 1936 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|     47021 | 1937 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 1938 | `			/* Phase#3: Compile the 'then' expression  */` |
|     47021 | 1939 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|     47021 | 1940 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|     47021 | 1941 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1942 | `				return rc;` |
|         - | 1943 | `			}` |
|     47021 | 1944 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|     23483 | 1945 | `		}else{` |
|         - | 1946 | `			/* Elvis operator: (expr) ?: (else)` |
|         - | 1947 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - | 1948 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|       174 | 1949 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       174 | 1950 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 1951 | `		}` |
|         - | 1952 | `		/* Phase#4: Emit the unconditional jump */` |
|     47191 | 1953 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - | 1954 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|     47191 | 1955 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|     47191 | 1956 | `		if( pInstr ){` |
|     47191 | 1957 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     23563 | 1958 | `		}` |
|     47191 | 1959 | `		if( !pNode->pLeft ){` |
|         - | 1960 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|       174 | 1961 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        85 | 1962 | `		}` |
|         - | 1963 | `		/* Phase#6: Compile the 'else' expression */` |
|     47191 | 1964 | `		if( pNode->pRight ){` |
|     47191 | 1965 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|     47191 | 1966 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|     47191 | 1967 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1968 | `				return rc;` |
|         - | 1969 | `			}` |
|     47191 | 1970 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|     23563 | 1971 | `		}` |
|     47191 | 1972 | `		if( nJmp > 0 ){` |
|         - | 1973 | `			/* Phase#7: Fix the unconditional jump */` |
|     47191 | 1974 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|     47191 | 1975 | `			if( pInstr ){` |
|     47191 | 1976 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     23563 | 1977 | `			}` |
|     23563 | 1978 | `		}` |
|         - | 1979 | `		/* All done */` |
|     47191 | 1980 | `		return SXRET_OK;` |
|         - | 1981 | `	}` |
|   4668355 | 1982 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|         - | 1983 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|         - | 1984 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|         - | 1985 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|         - | 1986 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|         - | 1987 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|         - | 1988 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|         - | 1989 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|         - | 1990 | `		sxu32 nPipeNsBase;` |
|        27 | 1991 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE` |
|         - | 1992 | `			\|EXPR_FLAG_MEMBER_REFSRC\|EXPR_FLAG_RDONLY_LOAD);` |
|        27 | 1993 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|       ! 0 | 1994 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 1995 | `				"'\|>': Missing operand");` |
|       ! 0 | 1996 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 1997 | `		}` |
|         - | 1998 | `		/* Argument: the LHS value. */` |
|        27 | 1999 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 2000 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|        27 | 2001 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 2002 | `			return rc;` |
|         - | 2003 | `		}` |
|        27 | 2004 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 2005 | `		/* Callable: the RHS. */` |
|        27 | 2006 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 2007 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|        27 | 2008 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 2009 | `			return rc;` |
|         - | 2010 | `		}` |
|        27 | 2011 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 2012 | `		/* Invoke the callable with the single piped argument. */` |
|        27 | 2013 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        27 | 2014 | `		return SXRET_OK;` |
|         - | 2015 | `	}` |
|         - | 2016 | `	/*` |
|         - | 2017 | ``	 * php compiles the multi-operand `isset($a, $b, ...)` as a short-circuit CHAIN --`` |
|         - | 2018 | ``	 * `isset($a) && isset($b) && ...` -- so nothing after the first operand that is not`` |
|         - | 2019 | `	 * set is ever evaluated. isset() is a host function here, and a call evaluates every` |
|         - | 2020 | `	 * argument before dispatching, so the later operands ran for real: the ordinary` |
|         - | 2021 | ``	 * `isset($info['k'], $data[$info['k']])` answered false through an `Undefined array`` |
|         - | 2022 | ``	 * key` warning and a null-offset deprecation php never raises, and`` |
|         - | 2023 | ``	 * `isset($a['no'], $b[side()])` CALLED side(). Doctrine's hydrator guards its`` |
|         - | 2024 | `	 * discriminator lookup in exactly that shape, so every hydrated row of every query` |
|         - | 2025 | `	 * carried two diagnostics php does not.` |
|         - | 2026 | `	 *` |
|         - | 2027 | `	 * Emit the chain the compiler owes: one SINGLE-operand isset() per argument, joined` |
|         - | 2028 | `	 * by a keep-the-value JZ to the end (the false it left IS the answer) and a POP on` |
|         - | 2029 | `	 * the fall-through. Each link is the ordinary call path below, re-entered with the` |
|         - | 2030 | `	 * argument set narrowed to one node, so every operand keeps the exact isset context` |
|         - | 2031 | `	 * it already had -- LOAD_IDX iP2=4, the quiet intermediates of an access chain,` |
|         - | 2032 | `	 * ArrayAccess::offsetExists -- and only the ORDER changes. A spread or named operand` |
|         - | 2033 | `	 * opts out: php refuses both in this position, and neither maps to one link.` |
|         - | 2034 | `	 */` |
|   4668324 | 2035 | `	if( iVmOp == PH7_OP_CALL && SySetUsed(&pNode->aNodeArgs) > 1` |
|    832111 | 2036 | `	 && GenStateCalleeIsIsset(pNode->pLeft) ){` |
|        84 | 2037 | `		ph7_expr_node **apIsset = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|        84 | 2038 | `		sxu32 nIsset = SySetUsed(&pNode->aNodeArgs);` |
|        84 | 2039 | `		SySet sSaved = pNode->aNodeArgs;` |
|         - | 2040 | `		SySet sJz;` |
|         - | 2041 | `		sxu32 n;` |
|        84 | 2042 | `		int bPlain = 1;` |
|       314 | 2043 | `		for( n = 0 ; n < nIsset ; ++n ){` |
|       230 | 2044 | `			if( apIsset[n] == 0` |
|       232 | 2045 | `			 \|\| (apIsset[n]->iFlags & (EXPR_NODE_SPREAD\|EXPR_NODE_NAMED_ARG)) ){` |
|       ! 0 | 2046 | `				bPlain = 0;` |
|       ! 0 | 2047 | `				break;` |
|         - | 2048 | `			}` |
|       117 | 2049 | `		}` |
|        84 | 2050 | `		if( bPlain ){` |
|        84 | 2051 | `			SySetInit(&sJz,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|        84 | 2052 | `			rc = SXRET_OK;` |
|       314 | 2053 | `			for( n = 0 ; n < nIsset ; ++n ){` |
|       232 | 2054 | `				pNode->aNodeArgs.pBase = (void *)&apIsset[n];` |
|       232 | 2055 | `				pNode->aNodeArgs.nUsed = 1;` |
|       232 | 2056 | `				pNode->aNodeArgs.nSize = 1;` |
|       232 | 2057 | `				pNode->aNodeArgs.nCursor = 0;` |
|       232 | 2058 | `				rc = GenStateEmitExprCode(&(*pGen),pNode,iFlags);` |
|       232 | 2059 | `				pNode->aNodeArgs = sSaved;` |
|       232 | 2060 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 2061 | `					break;` |
|         - | 2062 | `				}` |
|       232 | 2063 | `				if( n + 1 < nIsset ){` |
|       150 | 2064 | `					sxu32 nJz = 0;` |
|       150 | 2065 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,` |
|         - | 2066 | `						1 /* keep the false on the stack: it is the answer */,0,0,&nJz);` |
|       150 | 2067 | `					SySetPut(&sJz,(const void *)&nJz);` |
|         - | 2068 | `					/* Truthy link: drop it and ask the next operand. */` |
|       150 | 2069 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        74 | 2070 | `				}` |
|       117 | 2071 | `			}` |
|        84 | 2072 | `			if( rc == SXRET_OK ){` |
|        84 | 2073 | `				sxu32 *aJz = (sxu32 *)SySetBasePtr(&sJz);` |
|        84 | 2074 | `				sxu32 nEnd = PH7_VmInstrLength(pGen->pVm);` |
|       232 | 2075 | `				for( n = 0 ; n < SySetUsed(&sJz) ; ++n ){` |
|       150 | 2076 | `					VmInstr *pFix = PH7_VmGetInstr(pGen->pVm,aJz[n]);` |
|       150 | 2077 | `					if( pFix ){` |
|       150 | 2078 | `						pFix->iP2 = nEnd;` |
|        74 | 2079 | `					}` |
|        76 | 2080 | `				}` |
|        41 | 2081 | `			}` |
|        84 | 2082 | `			SySetRelease(&sJz);` |
|        84 | 2083 | `			return rc;` |
|         - | 2084 | `		}` |
|       ! 0 | 2085 | `	}` |
|         - | 2086 | `	/* php evaluates an assignment TARGET's dynamic subscript and property names before the` |
|         - | 2087 | `` 	 * assigned value, and performs the fetches they belong to after it -- `$a[k()] = v()` `` |
|         - | 2088 | `	 * runs k() first, and the array it is about to write is untouched while v() runs, so a` |
|         - | 2089 | ``	 * recursive memoization (`$cache[$k] = compute()` whose compute() asks whether $k is`` |
|         - | 2090 | `	 * already there) sees the truth. PHL emitted the value first and the whole target after` |
|         - | 2091 | ``	 * it, which reversed every side effect in the target and answered `v k`.`` |
|         - | 2092 | `	 *` |
|         - | 2093 | `	 * Both halves are wanted, and in a stack machine they need three pieces: the names are` |
|         - | 2094 | `	 * compiled HERE, ahead of the value; the value lands on top of them; and the access` |
|         - | 2095 | `	 * chain is emitted last, reading each name back from where it was parked (OP_PICK) so` |
|         - | 2096 | `	 * nothing it creates is visible to the value. The names are snapshotted for the same` |
|         - | 2097 | `	 * reason a call's earlier arguments are -- the value runs between the push and the` |
|         - | 2098 | `	 * consumer, and a pushed value only borrows the bytes it was loaded from. */` |
|   4668242 | 2099 | `	if( pNode->pOp->iPrec == 18 && pNode->pOp->iOp != EXPR_OP_REF` |
|    928637 | 2100 | `	 && pNode->pLeft && pNode->pRight` |
|    928637 | 2101 | `	 && pNode->pRight->xCode != PH7_CompileList` |
|    928613 | 2102 | `	 && pNode->pRight->xCode != PH7_CompileShortList ){` |
|    928236 | 2103 | `		nParked = GenStateCollectStoreNames(pNode->pRight,apParked);` |
|    928236 | 2104 | `		if( nParked > 0 ){` |
|         - | 2105 | `			int nAt;` |
|       941 | 2106 | `			for( nAt = 0 ; nAt < nParked ; ++nAt ){` |
|       477 | 2107 | `				sxu32 nParkNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|         - | 2108 | `				/* The same flags the access arm would have compiled this name under:` |
|         - | 2109 | `				 * the access's own contexts masked off, and the reference-source` |
|         - | 2110 | `				 * marker with them -- the target emission strips that before it` |
|         - | 2111 | `				 * reaches a name too, since a name is never the source of a bind. */` |
|       713 | 2112 | `				rc = GenStateEmitExprCode(&(*pGen),apParked[nAt],` |
|       236 | 2113 | `					(iFlags & GEN_ACCESS_NAME_MASK & ~EXPR_FLAG_MEMBER_REFSRC)` |
|       472 | 2114 | `						\|EXPR_FLAG_RDONLY_LOAD);` |
|       477 | 2115 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 2116 | `					return rc;` |
|         - | 2117 | `				}` |
|         - | 2118 | `				/* Each name is its own nullsafe scope, exactly as it is where the` |
|         - | 2119 | `				 * subscript arm compiles it. */` |
|       477 | 2120 | `				GenStatePatchNullsafeJumps(pGen, nParkNsBase);` |
|       241 | 2121 | `			}` |
|       469 | 2122 | `			if( GenStateArgRunsCode(pNode->pLeft) ){` |
|       251 | 2123 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_SNAPSHOT,nParked,0,0,0);` |
|       123 | 2124 | `			}` |
|       237 | 2125 | `		}else{` |
|    927772 | 2126 | `			nParked = 0; /* nothing to park, or a chain longer than the parking area */` |
|         - | 2127 | `		}` |
|    463488 | 2128 | `	}` |
|   4668247 | 2129 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|   4668247 | 2130 | `	nLhsFirst = PH7_VmInstrLength(pGen->pVm);` |
|         - | 2131 | `	/* Generate code for the left tree */` |
|   4668247 | 2132 | `	if( pNode->pLeft ){` |
|   4668247 | 2133 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|         - | 2134 | `		GenCallArgs sArgs;` |
|   4668247 | 2135 | `		int bArgsEmitted = 0;` |
|   4668247 | 2136 | ``		sxu32 nNewClassInstr = 0; /* index+1 of a `new` operand's class-name push */`` |
|   4668247 | 2137 | `		SyZero(&sArgs,sizeof(sArgs));` |
|         - | 2138 | `		{` |
|         - | 2139 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - | 2140 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - | 2141 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - | 2142 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - | 2143 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - | 2144 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - | 2145 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - | 2146 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|   4668247 | 2147 | `			sxi32 iLeftFlags = iFlags;` |
|         - | 2148 | `			/* The LHS chain whose subscript reads must be QUIET (LOAD_IDX iP2=8):` |
|         - | 2149 | ``			 * `??`'s left operand, and an isset()/empty() chain's intermediate`` |
|         - | 2150 | `			 * links -- php reads both silently and for the value. */` |
|   4668247 | 2151 | `			sxu32 nQuietLhsFirst = PH7_VmInstrLength(pGen->pVm);` |
|   4668247 | 2152 | `			int bQuietLhs = 0;` |
|         - | 2153 | `			/* D1 commit 2: a deferred element/property call arg records its lvalue chain, but` |
|         - | 2154 | `			 * that chain must be CONTIGUOUS. Only propagate DEFER_ARG to the base when the base` |
|         - | 2155 | `			 * is itself a continuable lvalue — a plain variable (an undefined base auto-defers),` |
|         - | 2156 | ``			 * another subscript, or a `->` member. If the base is anything else (most importantly`` |
|         - | 2157 | ``			 * a method CALL, e.g. `$o->items()->prop` or `$r->attributes->item(0)->nodeName`),`` |
|         - | 2158 | `			 * strip DEFER so that intermediate read is a NORMAL read, not a record-mode carrier. */` |
|   4668247 | 2159 | `			if( iLeftFlags & EXPR_FLAG_DEFER_ARG ){` |
|     63277 | 2160 | `				int bContinuable = pNode->pLeft` |
|     48074 | 2161 | `					&& ( (pNode->pLeft->pOp == 0 && pNode->pLeft->xCode == PH7_CompileVariable)` |
|     17058 | 2162 | `					  \|\| (pNode->pLeft->pOp != 0 && (pNode->pLeft->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|      1145 | 2163 | `					                              \|\| pNode->pLeft->pOp->iOp == EXPR_OP_ARROW)) );` |
|     31641 | 2164 | `				if( !bContinuable ){` |
|       637 | 2165 | `					iLeftFlags &= ~EXPR_FLAG_DEFER_ARG;` |
|       315 | 2166 | `				}` |
|     15799 | 2167 | `			}` |
|         - | 2168 | `			/*` |
|         - | 2169 | `			 * An isset()/empty() CHAIN reads its intermediate links for their` |
|         - | 2170 | ``			 * VALUE, not for a truth. php walks `isset($o->a->b)` by fetching`` |
|         - | 2171 | ``			 * `$o->a` in BP_VAR_IS mode -- silent, but a real read that runs`` |
|         - | 2172 | `			 * __isset AND THEN __get (or offsetExists and then offsetGet) --` |
|         - | 2173 | `			 * and only the LAST link answers the isset question. PHL gave every` |
|         - | 2174 | `			 * link the terminal context, so the intermediate pushed a bool and` |
|         - | 2175 | `` 			 * the final `->b` was a property of `true`: `isset($model->rel->id)` `` |
|         - | 2176 | `			 * was FALSE for every class with accessors, and so was` |
|         - | 2177 | ``			 * `isset($container['k']['j'])` over ArrayAccess -- a silently wrong`` |
|         - | 2178 | `			 * guard, not a diagnostic.` |
|         - | 2179 | `			 *` |
|         - | 2180 | ``			 * The intermediate context is `??`'s (PH7_MEMBER_COALESCE for a`` |
|         - | 2181 | `			 * member, LOAD_IDX iP2=8 for a subscript, patched over the emitted` |
|         - | 2182 | `			 * range below), which is exactly "silent, and the value": EMPTY's` |
|         - | 2183 | `			 * would read a shade differently, since a class declaring __get with` |
|         - | 2184 | `			 * no __isset is read through __get for an intermediate link and is` |
|         - | 2185 | `			 * NOT for a terminal isset()/empty().` |
|         - | 2186 | `			 */` |
|   4668242 | 2187 | `			if( (iLeftFlags & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY))` |
|   2339771 | 2188 | `				&& pNode->pOp && pNode->pLeft && pNode->pLeft->pOp` |
|      9201 | 2189 | `				&& GEN_IS_ACCESS_OP(pNode->pOp->iOp)` |
|       190 | 2190 | `				&& GEN_IS_ACCESS_OP(pNode->pLeft->pOp->iOp) ){` |
|       135 | 2191 | `				iLeftFlags &= ~(EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY);` |
|       135 | 2192 | `				iLeftFlags \|= EXPR_FLAG_MEMBER_COALESCE\|EXPR_FLAG_QUIET_VAR;` |
|       135 | 2193 | `				bQuietLhs = 1;` |
|        65 | 2194 | `			}` |
|   4668242 | 2195 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|   3803542 | 2196 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   1472930 | 2197 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   1457701 | 2198 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|     33669 | 2199 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   4651396 | 2200 | `			}else if( iLeftFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 2201 | ``				/* A SUBSCRIPT intermediate of an unset chain (`unset($a['k']['n'])`) keeps`` |
|         - | 2202 | `				 * the unset context — it must COW-separate the parent and must NOT vivify a` |
|         - | 2203 | `				 * missing key — but it is a READ of the container, not an unset of it. The` |
|         - | 2204 | ``				 * UNSET_BASE context says exactly that: `$a['k']` is loaded, where the`` |
|         - | 2205 | `				 * plain unset context would have removed the ELEMENT (and, for an` |
|         - | 2206 | `				 * ArrayAccess base, called offsetUnset() on the intermediate key). */` |
|       437 | 2207 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|       437 | 2208 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_UNSET_BASE;` |
|       216 | 2209 | `			}` |
|         - | 2210 | `			/* Only the OUTERMOST access of a reference SOURCE is the reference fetch;` |
|         - | 2211 | `` 			 * every container under it is php's ordinary write base (`$r =& $o->arr['k']` `` |
|         - | 2212 | ``			 * creates `arr` the way `$o->arr['k'] = v` does). So the flag never travels`` |
|         - | 2213 | `			 * down as itself -- it decays to the write-lvalue flag, which the strip just` |
|         - | 2214 | ``			 * below then applies its own `->`-intermediate rule to. */`` |
|   4668247 | 2215 | `			if( iLeftFlags & EXPR_FLAG_MEMBER_REFSRC ){` |
|       307 | 2216 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_REFSRC;` |
|       307 | 2217 | `				iLeftFlags \|= EXPR_FLAG_MEMBER_WRITE;` |
|       151 | 2218 | `			}` |
|         - | 2219 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - | 2220 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - | 2221 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - | 2222 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - | 2223 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - | 2224 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - | 2225 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|   4668242 | 2226 | `			if( pNode->pOp` |
|   6979064 | 2227 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|   4648493 | 2228 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   4628679 | 2229 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|     45853 | 2230 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|     22905 | 2231 | `			}` |
|         - | 2232 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - | 2233 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - | 2234 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - | 2235 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - | 2236 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - | 2237 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|   4668242 | 2238 | `			if( pNode->pOp` |
|   4668247 | 2239 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|         - | 2240 | ``				/* `(new A)->p++` writes through a temporary exactly as `= 1` does --`` |
|         - | 2241 | ``				 * but a `$this++` is a read-modify-write php leaves to run time. */`` |
|     77325 | 2242 | `				rc = GenStateWriteTargetCheck(&(*pGen),pNode->pLeft,PH7_WTC_RMW);` |
|     77325 | 2243 | `				if( rc != SXRET_OK ){` |
|        45 | 2244 | `					return rc;` |
|         - | 2245 | `				}` |
|     77317 | 2246 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE` |
|         - | 2247 | ``					\| EXPR_FLAG_RMW_LOAD /* php warns before seeding `$undef++` */;`` |
|     38604 | 2248 | `			}` |
|         - | 2249 | ``			/* The SOURCE of a `=&` (pLeft, the operands having been swapped in`` |
|         - | 2250 | `			 * parse.c) is compiled in WRITE context by php too —` |
|         - | 2251 | ``			 * `zend_compile_var(source, BP_VAR_W, 1)` — which is what makes`` |
|         - | 2252 | ``			 * `$r =& $undef` and `$r =& $a[5]` CREATE the thing they bind to,`` |
|         - | 2253 | `			 * silently. PHL READ it, so both warned about what was missing and then` |
|         - | 2254 | `			 * refused the bind outright, leaving $r undefined as well. No` |
|         - | 2255 | `			 * RMW_LOAD: a bind does not read the source's value, and no` |
|         - | 2256 | `			 * MEMBER_WRITE: a handler-backed native property has no pointer for` |
|         - | 2257 | ``			 * php to hand out either, so `$r =& $iv->s` must keep taking the`` |
|         - | 2258 | `			 * read COPY it takes in php. */` |
|   4668239 | 2259 | `			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_REF ){` |
|       463 | 2260 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_REFSRC;` |
|       229 | 2261 | `			}` |
|         - | 2262 | `			/* A destructuring target list that binds BY REFERENCE reads its SOURCE` |
|         - | 2263 | `			 * in write context for the same reason: the bind must reach the thing` |
|         - | 2264 | ``			 * the source NAMES. That is what keeps `[&$t] = $undef;` from warning`` |
|         - | 2265 | `			 * about what it is on the point of creating. */` |
|   4668234 | 2266 | `			if( iVmOp == PH7_OP_STORE && pNode->pRight && pNode->pRight->pStart` |
|    897378 | 2267 | `			 && (pNode->pRight->xCode == PH7_CompileList` |
|    897349 | 2268 | `			  \|\| pNode->pRight->xCode == PH7_CompileShortList)` |
|    448289 | 2269 | `			 && PH7_GenStateListSpanHasRef(pNode->pRight->pStart,pNode->pRight->pEnd) ){` |
|        45 | 2270 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_REFSRC;` |
|        22 | 2271 | `			}` |
|         - | 2272 | ``			/* `??` reads its LEFT operand in isset-context: an undefined or`` |
|         - | 2273 | `			 * UNINITIALIZED typed PROPERTY must yield the default rather than a` |
|         - | 2274 | `			 * warning/Error (php). Tag a member-access LHS so its OP_MEMBER takes` |
|         - | 2275 | `			 * the silent-lookup path (iP2 = ISSET), which still loads a present` |
|         - | 2276 | `			 * value. A SUBSCRIPT LHS is left untagged: LOAD_IDX's ISSET mode means` |
|         - | 2277 | ``			 * offsetExists (a bool), but `$o[$k] ?? d` needs the offsetGet value —`` |
|         - | 2278 | `			 * that path is already handled correctly by OP_NULLC. */` |
|   4668225 | 2279 | `			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC && pNode->pLeft ){` |
|         - | 2280 | ``				/* php reads the ENTIRE left operand of `??` in isset-context: no`` |
|         - | 2281 | ``				 * "Undefined variable" for `$x ?? d` OR for the base of`` |
|         - | 2282 | ``				 * `$x['k'] ?? d`. QUIET_VAR silences the variable read wherever it`` |
|         - | 2283 | `				 * sits in the chain. */` |
|       657 | 2284 | `				iLeftFlags \|= EXPR_FLAG_QUIET_VAR;` |
|       657 | 2285 | `				bQuietLhs = 1;` |
|       652 | 2286 | `				if( pNode->pLeft->pOp` |
|       805 | 2287 | `					&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|       490 | 2288 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|       406 | 2289 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|         - | 2290 | `					/* A member-access LHS additionally takes OP_MEMBER's SILENT` |
|         - | 2291 | `					 * lookup so an uninitialized typed property yields the default` |
|         - | 2292 | `					 * instead of an Error. It used to borrow isset()'s context for` |
|         - | 2293 | `					 * that, which is the same mistake the comment below records for` |
|         - | 2294 | `					 * subscripts: silence is shared, but isset() context makes every` |
|         - | 2295 | ``					 * ACCESSOR answer a truth, and `$o->p ?? d` needs the accessor's`` |
|         - | 2296 | ``					 * VALUE — so `??` has its own member context. A SUBSCRIPT LHS`` |
|         - | 2297 | `					 * still takes neither: LOAD_IDX's ISSET mode means offsetExists` |
|         - | 2298 | ``					 * (a bool), while `$o[$k] ?? d` needs the offsetGet value —`` |
|         - | 2299 | `					 * OP_NULLC already handles that path. */` |
|       184 | 2300 | `					iLeftFlags \|= EXPR_FLAG_MEMBER_COALESCE;` |
|        90 | 2301 | `				}` |
|       326 | 2302 | `			}` |
|   4668225 | 2303 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 2304 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 2305 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|     27429 | 2306 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|     13688 | 2307 | `			}` |
|   4668225 | 2308 | `			if( iVmOp == PH7_OP_NEW ){` |
|         - | 2309 | ``				/* Mark the direct operand so a call node under it (`new C($a)`) keeps the`` |
|         - | 2310 | `				 * emission order OP_NEW is assembled from — see the bNewCallee branch above. */` |
|    124015 | 2311 | `				iLeftFlags \|= EXPR_FLAG_NEW_CALLEE;` |
|     61924 | 2312 | `			}` |
|   4668225 | 2313 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|   4668225 | 2314 | `			if( rc == SXRET_OK && bQuietLhs ){` |
|         - | 2315 | `				/* Mark EVERY subscript read in the quiet left chain (iP2=8).` |
|         - | 2316 | `				 * Peeking at the next instruction only catches the OUTERMOST` |
|         - | 2317 | ``				 * access, so `$d['x']['y'] ?? $v` still warned for the inner one;`` |
|         - | 2318 | `				 * and a forward scan would be unsound, since an unrelated sibling` |
|         - | 2319 | ``				 * access can sit immediately before a coalesce (`f($a['k'],`` |
|         - | 2320 | ``				 * $b['j'] ?? 1)`). The compiler knows the real extent, so it marks`` |
|         - | 2321 | `				 * the range it just emitted. Write-context codes (1/3/5) belong to` |
|         - | 2322 | ``				 * `??=` and keep their meaning. */`` |
|       787 | 2323 | `				sxu32 nEnd = PH7_VmInstrLength(pGen->pVm);` |
|         - | 2324 | `				sxu32 nAt;` |
|      3415 | 2325 | `				for( nAt = nQuietLhsFirst ; nAt < nEnd ; ++nAt ){` |
|      2633 | 2326 | `					VmInstr *pFix = PH7_VmGetInstr(pGen->pVm,nAt);` |
|      2633 | 2327 | `					if( pFix && pFix->iOp == PH7_OP_LOAD_IDX && pFix->iP2 == 0 ){` |
|       405 | 2328 | `						pFix->iP2 = 8;` |
|       200 | 2329 | `					}` |
|      1319 | 2330 | `				}` |
|       391 | 2331 | `			}` |
|         - | 2332 | `		}` |
|   4668225 | 2333 | `		if( rc != SXRET_OK ){` |
|        65 | 2334 | `			return rc;` |
|         - | 2335 | `		}` |
|   4668165 | 2336 | `		if( !bIsChainOp ){` |
|         - | 2337 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 2338 | `			 * target the end of that LHS chain, which is right here. */` |
|   3034169 | 2339 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   1515035 | 2340 | `		}` |
|   4668165 | 2341 | `		if( iVmOp == PH7_OP_CALL ){` |
|   1204508 | 2342 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   1204508 | 2343 | `			if( pInstr ){` |
|   1204508 | 2344 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   1167524 | 2345 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 2346 | `					sxu32 nQual;` |
|   1167524 | 2347 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 2348 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 2349 | `					 * so the later NEW handler (if any) can see it. */` |
|   1167524 | 2350 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 2351 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 2352 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 2353 | `					 * imports — class imports must NOT affect function` |
|         - | 2354 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 2355 | `					 * before NEW; we store the original literal index in the` |
|         - | 2356 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 2357 | `					 * the unqualified name and re-qualify with class imports. */` |
|   1167524 | 2358 | `					if( bAbsolute ){` |
|       363 | 2359 | `						pInstr->iP2 = (sxi32)nOrig;` |
|       184 | 2360 | `					}else{` |
|   1167166 | 2361 | `						int fromImport = 0;` |
|   1167166 | 2362 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   1167166 | 2363 | `						pInstr->iP2 = (sxi32)nQual;` |
|   1167166 | 2364 | `						if( nQual != nOrig ){` |
|         - | 2365 | `							/* Record the original literal index in the arg map` |
|         - | 2366 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 2367 | `							 * flag) so the NEW handler can recover the` |
|         - | 2368 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 2369 | `							 * imports. */` |
|       941 | 2370 | `							if( p3 == 0 ){` |
|       941 | 2371 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       936 | 2372 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|       941 | 2373 | `								if( pMap ){` |
|       941 | 2374 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|       941 | 2375 | `									p3 = (void *)pMap;` |
|       468 | 2376 | `								}` |
|       468 | 2377 | `							}` |
|       941 | 2378 | `							if( p3 ){` |
|       941 | 2379 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|       941 | 2380 | `								if( !fromImport ){` |
|         - | 2381 | `									/* Mark as namespace-qualified */` |
|       909 | 2382 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|       452 | 2383 | `								}` |
|       468 | 2384 | `							}` |
|       468 | 2385 | `						}` |
|         - | 2386 | `					}` |
|    619613 | 2387 | `				}else if( (pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */` |
|     32910 | 2388 | `						&& !bNewCallee` |
|     28845 | 2389 | `						&& !(pNode->pLeft && (pNode->pLeft->iFlags & EXPR_NODE_PARENS)))` |
|     22571 | 2390 | `					\|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 2391 | `					/* Method call,flag that. But NOT when the callee was an explicitly` |
|         - | 2392 | ``					 * PARENTHESISED member access: `($o->p)(...)` / `($o::$p)(...)` invokes`` |
|         - | 2393 | `					 * the VALUE of the property (php's variable-invocation), so the OP_MEMBER` |
|         - | 2394 | `					 * must stay a plain property READ (leaving the callable on the stack for` |
|         - | 2395 | `					 * OP_CALL to invoke) rather than being rewritten into a method-name` |
|         - | 2396 | ``					 * resolution — the parens are exactly what distinguishes `($o->p)()` from`` |
|         - | 2397 | ``					 * the method call `$o->p()`. */`` |
|     28813 | 2398 | `					pInstr->iP2 = 1;` |
|         - | 2399 | ``					/* …and a `new`'s operand is the third shape that is NOT a method`` |
|         - | 2400 | ``					 * call: after `new`, php's class expression is a `new_variable`,`` |
|         - | 2401 | `					 * which has no call in it, so the parentheses that follow are` |
|         - | 2402 | ``					 * always the CONSTRUCTOR's. `new $config->defType()` -- how`` |
|         - | 2403 | `					 * nette/di spells every service it builds -- read the property as` |
|         - | 2404 | ``					 * a METHOD and died on `Call to undefined method`` |
|         - | 2405 | ``					 * stdClass::defType()`. Same exception, same reason, as the`` |
|         - | 2406 | `					 * parenthesised member above: the OP_MEMBER stays a property READ` |
|         - | 2407 | `					 * and leaves the class NAME for OP_NEW. */` |
|         - | 2408 | `					/* This OP_MEMBER is where php SCREENS a method call -- an undefined` |
|         - | 2409 | `					 * or inaccessible method, a class that is not there -- and php` |
|         - | 2410 | `					 * reports that refusal at the line the CALL BEGINS on. The emitter` |
|         - | 2411 | `					 * stamped it with the token the generator was standing on, which` |
|         - | 2412 | `					 * for a call spanning several lines is its closing ')'. Same rule,` |
|         - | 2413 | `					 * same source, as the OP_CALL/OP_NEW stamp further down. */` |
|     28813 | 2414 | `					if( pInstr->iOp == PH7_OP_MEMBER && pNode->pStart ){` |
|     28803 | 2415 | `						pInstr->nLine = pNode->pStart->nLine;` |
|     14380 | 2416 | `					}` |
|         - | 2417 | ``					/* A static call with a DYNAMIC method name (`C::$m(...)`): the`` |
|         - | 2418 | ``					 * static-`::` codegen folded the variable NAME into OP_MEMBER->p3`` |
|         - | 2419 | ``					 * as if it were a static-PROPERTY read (`C::$m`), but in a CALL the`` |
|         - | 2420 | `					 * method name is the variable's VALUE. Rebuild the sequence` |
|         - | 2421 | `					 * [class, OP_LOAD $m -> value, OP_MEMBER(static, method)] so the` |
|         - | 2422 | `					 * dynamic name is read off the stack, matching the instance` |
|         - | 2423 | ``					 * (`$o->$m()`) path. iP1==1 marks a static member. */`` |
|     28813 | 2424 | `					if( pInstr->iOp == PH7_OP_MEMBER && pInstr->iP1 == 1 && pInstr->p3 == 0 ){` |
|         - | 2425 | ``						/* A LITERAL `X::__construct()`, any case: php's compiler drops the`` |
|         - | 2426 | `						 * name and the call asks for the class's constructor itself, which` |
|         - | 2427 | `						 * refuses differently from a method lookup. The name's LOADC is the` |
|         - | 2428 | ``						 * instruction just below; a dynamic `X::$m()` never reaches here`` |
|         - | 2429 | `						 * with one (see VmInstr::bRefSrc). */` |
|      2520 | 2430 | `						VmInstr *pNameLit = PH7_VmPeekNextInstr(pGen->pVm);` |
|      2520 | 2431 | `						if( pNameLit && pNameLit->iOp == PH7_OP_LOADC ){` |
|      2520 | 2432 | `							ph7_value *pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pNameLit->iP2);` |
|      2515 | 2433 | `							if( pLit && (pLit->iFlags & MEMOBJ_STRING)` |
|      2515 | 2434 | `							 && SyBlobLength(&pLit->sBlob) == sizeof("__construct")-1` |
|      1373 | 2435 | `							 && SyStrnicmp((const char *)SyBlobData(&pLit->sBlob),"__construct",` |
|       115 | 2436 | `								sizeof("__construct")-1) == 0 ){` |
|        89 | 2437 | `								pInstr->bRefSrc = 1;` |
|        43 | 2438 | `							}` |
|      1253 | 2439 | `						}` |
|      1253 | 2440 | `					}` |
|     28813 | 2441 | `					if( pInstr->iOp == PH7_OP_MEMBER && pInstr->iP1 == 1 && pInstr->p3 ){` |
|        31 | 2442 | `						void *pDynName = pInstr->p3;` |
|        31 | 2443 | `						sxu8 bKwClass = pInstr->bDiscard;` |
|        31 | 2444 | `						(void)PH7_VmPopInstr(pGen->pVm);` |
|        31 | 2445 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,pDynName,0);` |
|        31 | 2446 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_MEMBER,1,PH7_MEMBER_METHOD,0,0);` |
|        31 | 2447 | `						PH7_VmPeekInstr(pGen->pVm)->bDiscard = bKwClass;` |
|        15 | 2448 | `					}` |
|     14385 | 2449 | `				}` |
|    601089 | 2450 | `			}` |
|         - | 2451 | `			/* The callee is resolved; NOW emit the arguments. php's order — the callee` |
|         - | 2452 | `			 * first, at its INIT_FCALL / INIT_METHOD_CALL, and the arguments only after` |
|         - | 2453 | ``			 * it — is what makes `(new C)->priv(boom())` report php's `Call to private`` |
|         - | 2454 | `` 			 * method` instead of whatever the argument threw, and what keeps `$o?->m(f())` `` |
|         - | 2455 | ``			 * from running `f()` on a null receiver. It also puts the callee in reach of`` |
|         - | 2456 | `			 * the argument ops one opcode EARLIER than OP_CALL.` |
|         - | 2457 | `			 *` |
|         - | 2458 | `			 * The stack that leaves here is therefore [callee][args…] — the mirror of the` |
|         - | 2459 | `			 * layout OP_CALL's whole dispatch is written against (the method-name pair` |
|         - | 2460 | `			 * below the arguments, the spread runs counted down from the top, the` |
|         - | 2461 | `			 * deferred-argument re-walk). OP_ROT_CALLEE turns the region back over just` |
|         - | 2462 | `			 * before the call, so nothing downstream of it changes. */` |
|   1204508 | 2463 | `			if( !bArgsEmitted ){` |
|         - | 2464 | `				int bTwoSlot;` |
|   1204508 | 2465 | `				sArgs.p3 = p3; /* the namespace map built just above, if any */` |
|         - | 2466 | `				/* A METHOD callee leaves TWO slots — [receiver][method name] — which` |
|         - | 2467 | `				 * OP_CALL reads as one callee (the receiver answers $this and the` |
|         - | 2468 | `				 * late-static-binding class); anything else leaves one. The instruction` |
|         - | 2469 | `				 * just emitted is what decides it. */` |
|   1204508 | 2470 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   1218915 | 2471 | `				bTwoSlot = pInstr && pInstr->iOp == PH7_OP_MEMBER` |
|    615534 | 2472 | `					&& pInstr->iP2 == PH7_MEMBER_METHOD` |
|         - | 2473 | `					/* …unless the member NAME was folded into p3 rather than pushed:` |
|         - | 2474 | `					 * that shape pushes the target alone, so the op leaves one slot,` |
|         - | 2475 | `					 * which is the same distinction vm_ops_oo.c makes before popping. */` |
|   1807917 | 2476 | `					&& pInstr->p3 == 0;` |
|         - | 2477 | `				/* Screen the callee HERE, where php screens it: an undefined function, a` |
|         - | 2478 | `				 * callable string/array naming nothing, a value that is not callable at` |
|         - | 2479 | `				 * all. A METHOD callee needs none — its OP_MEMBER just did it, against` |
|         - | 2480 | `				 * the entry it chose. An FCC needs none either: OP_LOAD_FCC runs the same` |
|         - | 2481 | `				 * screen, and nothing runs before it. And with NO arguments the call` |
|         - | 2482 | `				 * itself is already the first thing to happen, so there is nothing to` |
|         - | 2483 | `				 * order and no reason to pay for a second resolution. */` |
|         - | 2484 | `				{` |
|   1204508 | 2485 | `					ph7_expr_node **apCallArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   1204508 | 2486 | `					sxi32 nCallArg = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|   1299058 | 2487 | `					int bNodeFcc = nCallArg == 1 && apCallArg[0]` |
|   1554030 | 2488 | `						&& (apCallArg[0]->iFlags & EXPR_NODE_FCC);` |
|   1204508 | 2489 | `					if( bNewCallee ){` |
|         - | 2490 | ``						/* A `new`'s operand: the screen is OP_NEW itself, run with no`` |
|         - | 2491 | `						 * arguments on the stack (iP1 = -1). It asks every refusal the` |
|         - | 2492 | `						 * real pass asks and leaves the class name standing, so the two` |
|         - | 2493 | `						 * cannot disagree. Record where that push is — the NEW codegen` |
|         - | 2494 | `						 * used to find it one instruction behind the trailing OP_CALL,` |
|         - | 2495 | `						 * and the argument list now sits in between. */` |
|    120079 | 2496 | `						nNewClassInstr = PH7_VmInstrLength(pGen->pVm);` |
|    120079 | 2497 | `						if( nCallArg > 0 ){` |
|    117145 | 2498 | `							PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,-1,0,0,0);` |
|     58496 | 2499 | `						}` |
|   1144392 | 2500 | `					}else if( nCallArg > 0 && !bTwoSlot && !bNodeFcc ){` |
|         - | 2501 | ``						/* This screen is where a `Call to undefined function` is`` |
|         - | 2502 | `						 * raised for a call that HAS arguments, and php reports such a` |
|         - | 2503 | `						 * call at the line the CALLEE is written on -- not at the` |
|         - | 2504 | `						 * closing parenthesis, which is where the emitter's token` |
|         - | 2505 | `						 * cursor has reached by now. The callee's own load is the` |
|         - | 2506 | ``						 * instruction immediately behind (`pInstr`, peeked just above`` |
|         - | 2507 | `						 * for bTwoSlot), so its line is the one to carry. A call with` |
|         - | 2508 | `						 * no arguments needs nothing: OP_CALL itself is then the first` |
|         - | 2509 | `						 * thing to run and already reports the callee's line. */` |
|   1028349 | 2510 | `						sxu32 nInitIdx = PH7_VmInstrLength(pGen->pVm);` |
|   1543520 | 2511 | `						sxu32 nCalleeLine = pNode->pStart` |
|   1028344 | 2512 | `							? pNode->pStart->nLine : (pInstr ? pInstr->nLine : 0);` |
|   1028691 | 2513 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL_INIT,0,` |
|    513515 | 2514 | `							((p3 && ((VmCallArgMap *)p3)->bIsNamespaced)` |
|    515171 | 2515 | `								? PH7_CALLINIT_NAMESPACED : 0)` |
|   1543515 | 2516 | `							\| (GenStateCalleeIsConstruct(pNode->pLeft)` |
|    515171 | 2517 | `								? PH7_CALLINIT_CONSTRUCT : 0),0,0);` |
|   1028349 | 2518 | `						if( nCalleeLine ){` |
|   1028349 | 2519 | `							VmInstr *pInitInstr = PH7_VmGetInstr(pGen->pVm,nInitIdx);` |
|   1028349 | 2520 | `							if( pInitInstr ){` |
|   1028349 | 2521 | `								pInitInstr->nLine = nCalleeLine;` |
|    513173 | 2522 | `							}` |
|    513173 | 2523 | `						}` |
|    513173 | 2524 | `					}` |
|         - | 2525 | `				}` |
|   1204508 | 2526 | `				sArgs.bNewCallee = bNewCallee;` |
|         - | 2527 | `				/* php 8.4 compiles an unqualified call inside a namespace that could be a` |
|         - | 2528 | `				 * FRAMELESS builtin (PH7_VmFramelessArity) as two branches, and to keep` |
|         - | 2529 | `				 * them from nesting, a namespaced call in its arguments is never made` |
|         - | 2530 | `				 * frameless -- in either branch, however deep. The mark rides the inner` |
|         - | 2531 | `				 * call's map; the outer one is decided at run time, as php decides it. */` |
|         - | 2532 | `				{` |
|   1204508 | 2533 | `					VmCallArgMap *pNsMap = (VmCallArgMap *)p3;` |
|   1204508 | 2534 | `					int bOuterFrameless = 0;` |
|   1204508 | 2535 | `					if( pNsMap && pNsMap->bIsNamespaced ){` |
|       909 | 2536 | `						if( pGen->bInFramelessNsArgs ){` |
|        24 | 2537 | `							pNsMap->bNotFrameless = 1;` |
|       898 | 2538 | `						}else if( !bNewCallee && !bTwoSlot && pNsMap->nOrigNameLit > 0 ){` |
|       759 | 2539 | `							ph7_expr_node **apA = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       759 | 2540 | `							sxi32 nA = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       759 | 2541 | `							ph7_value *pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,pNsMap->nOrigNameLit - 1);` |
|         - | 2542 | `							sxi32 k;` |
|       759 | 2543 | `							bOuterFrameless = pLit && (pLit->iFlags & MEMOBJ_STRING);` |
|      1847 | 2544 | `							for( k = 0 ; k < nA && bOuterFrameless ; ++k ){` |
|      1093 | 2545 | `								if( apA[k] == 0 \|\| (apA[k]->iFlags & (EXPR_NODE_SPREAD\|EXPR_NODE_NAMED_ARG\|EXPR_NODE_FCC)) ){` |
|        48 | 2546 | `									bOuterFrameless = 0;` |
|        22 | 2547 | `								}` |
|       549 | 2548 | `							}` |
|       759 | 2549 | `							if( bOuterFrameless ){` |
|         - | 2550 | `								SyString sLitName;` |
|       715 | 2551 | `								SyStringInitFromBuf(&sLitName,SyBlobData(&pLit->sBlob),SyBlobLength(&pLit->sBlob));` |
|       715 | 2552 | `								bOuterFrameless = PH7_VmFramelessArity(&sLitName,(int)nA);` |
|       355 | 2553 | `							}` |
|       377 | 2554 | `						}` |
|       452 | 2555 | `					}` |
|   1204508 | 2556 | `					if( bOuterFrameless ){` |
|        96 | 2557 | `						pGen->bInFramelessNsArgs = 1;` |
|        46 | 2558 | `					}` |
|   1204508 | 2559 | `					rc = GenStateEmitCallArgs(&(*pGen),pNode,iFlags,&sArgs);` |
|   1204508 | 2560 | `					if( bOuterFrameless ){` |
|        96 | 2561 | `						pGen->bInFramelessNsArgs = 0;` |
|        46 | 2562 | `					}` |
|         - | 2563 | `				}` |
|   1204508 | 2564 | `				if( rc != SXRET_OK ){` |
|        12 | 2565 | `					return rc;` |
|         - | 2566 | `				}` |
|   1204498 | 2567 | `				iP1 = sArgs.iP1;` |
|   1204498 | 2568 | `				iP2 = sArgs.iP2;` |
|   1204498 | 2569 | `				p3  = sArgs.p3;` |
|   1204498 | 2570 | `				bFcc = sArgs.bFcc;` |
|   1204498 | 2571 | `				if( iP1 > 0 \|\| (iP2 & PH7_CALL_SPREAD) ){` |
|   1736358 | 2572 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_ROT_CALLEE,iP1,` |
|   1158291 | 2573 | `						((iP2 & PH7_CALL_SPREAD) ? PH7_ROT_SPREAD : 0)` |
|   1158291 | 2574 | `						\| (bTwoSlot ? PH7_ROT_TWOSLOT : 0),0,0);` |
|    578062 | 2575 | `				}` |
|   1204498 | 2576 | `				if( bNewCallee && nNewClassInstr > 0 ){` |
|    120079 | 2577 | `					if( p3 == 0 ){` |
|      2901 | 2578 | `						VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      2896 | 2579 | `							&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      2901 | 2580 | `						if( pMap ){` |
|      2901 | 2581 | `							SyZero(pMap,sizeof(VmCallArgMap));` |
|      2901 | 2582 | `							p3 = (void *)pMap;` |
|      1448 | 2583 | `						}` |
|      1448 | 2584 | `					}` |
|    120079 | 2585 | `					if( p3 ){` |
|    120079 | 2586 | `						((VmCallArgMap *)p3)->nNewClassInstr = nNewClassInstr;` |
|     59958 | 2587 | `					}` |
|     59958 | 2588 | `				}` |
|    601089 | 2589 | `			}` |
|   4064746 | 2590 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 2591 | `			ph7_expr_node **apNode;` |
|         - | 2592 | `			sxi32 n;` |
|    383650 | 2593 | `			sxi32 iChildMask = GEN_ACCESS_NAME_MASK;` |
|         - | 2594 | `			/* Recurse and generate bytecodes for array index */` |
|    383650 | 2595 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|    672637 | 2596 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|    288992 | 2597 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    288992 | 2598 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|    288992 | 2599 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 2600 | `					return rc;` |
|         - | 2601 | `				}` |
|         - | 2602 | `				/* Each subscript index is an independent nullsafe scope. */` |
|    288992 | 2603 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|    144308 | 2604 | `			}` |
|    383650 | 2605 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|    288992 | 2606 | `				iP1 = 1; /* Node have an index associated with it */` |
|    144308 | 2607 | `			}else{` |
|         - | 2608 | ``				/* `[]` names the element a WRITE is about to create, so php allows it`` |
|         - | 2609 | `				 * only where a write lands: an assignment target (plain, compound,` |
|         - | 2610 | ``				 * `=&`, a list()/foreach target) and a by-reference argument. Every`` |
|         - | 2611 | `				 * other placement is a COMPILE error there — PHL accepted them all and` |
|         - | 2612 | ``				 * answered NULL after a PH7-worded notice, so `$x = $a[];`,`` |
|         - | 2613 | ``				 * `isset($a[])` and `unset($a[][0])` were silent no-ops on source php`` |
|         - | 2614 | `				 * refuses to run. A call ARGUMENT is the one shape php also leaves to` |
|         - | 2615 | `				 * runtime (it cannot know the parameter's by-ref-ness at compile time),` |
|         - | 2616 | `				 * which is what DEFER_ARG marks. */` |
|     94663 | 2617 | `				if( iFlags & (EXPR_FLAG_LOAD_IDX_UNSET\|EXPR_FLAG_LOAD_IDX_UNSET_BASE) ){` |
|       ! 0 | 2618 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 2619 | `						pNode->pStart ? pNode->pStart->nLine : 0,` |
|         - | 2620 | `						"Cannot use [] for unsetting");` |
|       ! 0 | 2621 | `					return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 2622 | `				}` |
|     94663 | 2623 | `				if( (iFlags & (EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_DEFER_ARG)) == 0 ){` |
|       ! 0 | 2624 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 2625 | `						pNode->pStart ? pNode->pStart->nLine : 0,` |
|         - | 2626 | `						"Cannot use [] for reading");` |
|       ! 0 | 2627 | `					return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 2628 | `				}` |
|         - | 2629 | `			}` |
|    383650 | 2630 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 2631 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     17719 | 2632 | `				iP2 = 4;` |
|    374782 | 2633 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 2634 | `				/* offsetUnset for ArrayAccess; for an array, remove the ELEMENT. */` |
|       263 | 2635 | `				iP2 = 5;` |
|    365807 | 2636 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET_BASE ){` |
|         - | 2637 | `				/* An unset chain's intermediate container: read it, but with the` |
|         - | 2638 | `				 * unset context's COW-separate and no-vivify rules. */` |
|        26 | 2639 | `				iP2 = 10;` |
|    365666 | 2640 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 2641 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 2642 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 2643 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        61 | 2644 | `				iP2 = 6;` |
|    365626 | 2645 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 2646 | `				/* Create an empty entry when the desired index is not found.` |
|         - | 2647 | ``				 * A read-modify-write target (`$a[k] += v`, `$a[k]++`) creates it`` |
|         - | 2648 | `				 * the same way but READS it first, so php warns about the missing` |
|         - | 2649 | `				 * key before seeding it — the RMW context says which of the two` |
|         - | 2650 | `				 * this is (VM_IDX_CTX_RMW). The flag rides the whole LHS chain, so` |
|         - | 2651 | `				 * an intermediate level gets it too, as php's BP_VAR_RW fetch does. */` |
|    206441 | 2652 | `				iP2 = (iFlags & EXPR_FLAG_RMW_LOAD) ? VM_IDX_CTX_RMW : 1;` |
|    262243 | 2653 | `			}else if( iFlags & EXPR_FLAG_DEFER_ARG ){` |
|         - | 2654 | `				/* D1 commit 2: deferred by-ref/by-value element arg. Behaves as a read but,` |
|         - | 2655 | `				 * on a lookup miss, records the lvalue path instead of warning; OP_CALL` |
|         - | 2656 | `				 * re-walks it in vivify (by-ref) or read+warn (by-value) mode. */` |
|     27921 | 2657 | `				iP2 = 9;` |
|     13944 | 2658 | `			}` |
|   3271585 | 2659 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 2660 | `			/* POP the left node — its answer is dropped exactly as a statement's is,` |
|         - | 2661 | `			 * so a #[\NoDiscard] callee warns for it too (php warns for every` |
|         - | 2662 | ``			 * element of a `for` clause list, not just the last). */`` |
|        16 | 2663 | `			GenStateMarkDiscardedCall(&(*pGen));` |
|        16 | 2664 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         7 | 2665 | `		}` |
|   2330592 | 2666 | `	}` |
|   4668155 | 2667 | `	rc = SXRET_OK;` |
|   4668155 | 2668 | `	nJmpIdx = 0;` |
|         - | 2669 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 2670 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 2671 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|   4668155 | 2672 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|      6196 | 2673 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      6196 | 2674 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      6090 | 2675 | `			int isSpecial = GenStateLitIsScopeKeyword(&(*pGen),(sxu32)pInstr->iP2);` |
|      6090 | 2676 | `			bScopeKw = isSpecial;` |
|      7554 | 2677 | `			if( isSpecial && GenStateScreenScopeKeyword(&(*pGen),(sxu32)pInstr->iP2,` |
|       976 | 2678 | `					pNode->pLeft && pNode->pLeft->pStart ? pNode->pLeft->pStart->nLine` |
|       488 | 2679 | `					: pNode->pStart->nLine) == SXERR_ABORT ){` |
|       ! 0 | 2680 | `				return SXERR_ABORT;` |
|         - | 2681 | `			}` |
|      6090 | 2682 | `			pInstr->iP1 = 0;` |
|         - | 2683 | ``			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the`` |
|         - | 2684 | `			 * global class, so it must NOT be re-qualified with the current namespace.` |
|         - | 2685 | ``			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.`` |
|         - | 2686 | ``			 * (Multi-component `\A\B::m` already resolves absolutely because its`` |
|         - | 2687 | ``			 * literal keeps a backslash; the single-component `\Closure` lost it.) */`` |
|         - | 2688 | `			{` |
|      9128 | 2689 | `				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart` |
|      9132 | 2690 | `					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));` |
|      6090 | 2691 | `				if( !isSpecial && !bAbsolute ){` |
|      4986 | 2692 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      2486 | 2693 | `				}` |
|         - | 2694 | `			}` |
|         - | 2695 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 2696 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|      6090 | 2697 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|      5114 | 2698 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|      5114 | 2699 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|       316 | 2700 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|       225 | 2701 | `					return SXRET_OK;` |
|         - | 2702 | `				}` |
|      2440 | 2703 | `			}` |
|      2928 | 2704 | `		}` |
|      2981 | 2705 | `	}` |
|   4667930 | 2706 | `	if( iVmOp == PH7_OP_IS_A && nLhsFirst < PH7_VmInstrLength(pGen->pVm)` |
|     17597 | 2707 | `	 && GenStateInstanceofFoldsLhs(&(*pGen),nLhsFirst) ){` |
|         - | 2708 | `` 		/* php never even compiles the class operand when the SUBJECT of `instanceof` `` |
|         - | 2709 | `		 * is a compile-time constant: zend_compile_instanceof folds the whole` |
|         - | 2710 | `		 * expression to FALSE the moment its left operand comes back IS_CONST, so` |
|         - | 2711 | `` 		 * `5 instanceof $x` is false whatever $x holds -- while `$v instanceof $x` `` |
|         - | 2712 | `		 * with the same 5 in $v reaches the runtime opcode and is refused when $x is` |
|         - | 2713 | `		 * neither an object nor a string. The two spellings really do answer` |
|         - | 2714 | `		 * differently, so the screen added to OP_IS_A has to be told which one this` |
|         - | 2715 | `		 * is, and GenStateInstanceofFoldsLhs reads it off the subject's own` |
|         - | 2716 | `		 * instructions. */` |
|         - | 2717 | `		ph7_value *pFalse;` |
|         - | 2718 | `		sxu32 nFalseIdx;` |
|         - | 2719 | `		/* The subject's own instructions go with it: php frees the folded operand` |
|         - | 2720 | `		 * (zend_do_free) rather than leaving it to be computed and dropped. */` |
|        45 | 2721 | `		while( PH7_VmInstrLength(pGen->pVm) > nLhsFirst ){` |
|        29 | 2722 | `			(void)PH7_VmPopInstr(pGen->pVm);` |
|         1 | 2723 | `		}` |
|        17 | 2724 | `		pFalse = PH7_ReserveConstObj(pGen->pVm,&nFalseIdx);` |
|        17 | 2725 | `		if( pFalse == 0 ){` |
|       ! 0 | 2726 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 | 2727 | `			return SXERR_ABORT;` |
|         - | 2728 | `		}` |
|        17 | 2729 | `		PH7_MemObjInitFromBool(pGen->pVm,pFalse,0);` |
|        17 | 2730 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,(sxi32)nFalseIdx,0,0);` |
|        17 | 2731 | `		return SXRET_OK;` |
|         - | 2732 | `	}` |
|         - | 2733 | `	/* An operand waiting on the stack BORROWS its source's string bytes: PH7_MemObjLoad` |
|         - | 2734 | `	 * hands back a read-only view -- a pointer plus the length the source had at the push` |
|         - | 2735 | ``	 * -- so a right operand that can RUN (an assignment, a call, `++`, a fetch that may`` |
|         - | 2736 | ``	 * reach __get) writes through a left operand that is already there. `$a[0] . ($a[0] =`` |
|         - | 2737 | ``	 * 'new')` answered `newnew` for php's `oldnew`, and when the write REALLOCATES the`` |
|         - | 2738 | `	 * buffer instead of overwriting it the view is a use-after-free.` |
|         - | 2739 | `	 *` |
|         - | 2740 | `	 * php has no such window, and its rule has two halves that point opposite ways: it` |
|         - | 2741 | ``	 * materializes every operand where it is written EXCEPT a plain `$var`, which it never`` |
|         - | 2742 | ``	 * pushes at all -- the operator reads the compiled variable itself, so `$x . ($x =`` |
|         - | 2743 | ``	 * 'new')` is `newnew` there and `$n = 1; $n - ($n = 5)` is 0. A copy is the answer for`` |
|         - | 2744 | `	 * one half and the wrong answer for the other.` |
|         - | 2745 | `	 *` |
|         - | 2746 | `	 * So: copy the shapes php copies, and for the plain variable MOVE its load past the` |
|         - | 2747 | `	 * right operand (OP_SWAP puts the two back in the operator's order), which is what` |
|         - | 2748 | `	 * "read it at the operator" means in a stack machine. Both arms are gated on the right` |
|         - | 2749 | ``	 * operand being able to run something, so an ordinary `$a . $b` emits neither. */`` |
|   4667914 | 2750 | `	if( GenStateBinOpReadsBothOperands(iVmOp) && pNode->pLeft && pNode->pRight` |
|   1410573 | 2751 | `	 && GenStateArgRunsCode(pNode->pRight) ){` |
|    183329 | 2752 | `		if( GenStateNodeIsSimpleVar(pNode->pLeft) ){` |
|     60400 | 2753 | `			VmInstr *pLhsLoad = PH7_VmPeekInstr(pGen->pVm);` |
|     60395 | 2754 | `			if( pLhsLoad && pLhsLoad->iOp == PH7_OP_LOAD` |
|     60400 | 2755 | `			 && PH7_VmInstrLength(pGen->pVm) == nLhsFirst + 1 ){` |
|     60400 | 2756 | `				sMovedLhs = *pLhsLoad;` |
|     60400 | 2757 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|     60400 | 2758 | `				bMoveLhs = 1;` |
|     30160 | 2759 | `			}` |
|    153089 | 2760 | `		}else if( GenStateNodeMayAliasStorage(pNode->pLeft) ){` |
|     17984 | 2761 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_SNAPSHOT,0,0,0,0);` |
|      8977 | 2762 | `		}` |
|     91535 | 2763 | `	}` |
|         - | 2764 | `	/* Generate code for the right tree */` |
|   4667919 | 2765 | `	if( pNode->pRight ){` |
|   2649818 | 2766 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 2767 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    144418 | 2768 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|   2577515 | 2769 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 2770 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    101926 | 2771 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|   2454378 | 2772 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 2773 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|       657 | 2774 | `			iVmOp = 0; /* No binary operator to emit */` |
|       657 | 2775 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|   2403266 | 2776 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 2777 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 2778 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 2779 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 2780 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 2781 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 2782 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       221 | 2783 | `			sxu32 nNsJmp = 0;` |
|       221 | 2784 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       221 | 2785 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|   2402724 | 2786 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */` |
|   1937697 | 2787 | ``			\|\| pNode->pOp->iOp == EXPR_OP_REF /* `=&` reference bind */ ){`` |
|         - | 2788 | `` 			/* The lvalue is the RIGHT operand (prec-18 ops are right-associative; `=&` `` |
|         - | 2789 | `			 * swaps its operands in parse.c so its target is pRight too). Mark it a write` |
|         - | 2790 | `			 * target so a missing base (the container of a subscript-write, or a bare` |
|         - | 2791 | `` 			 * `$o->p`) is auto-created — PHP auto-vivifies on a plain write AND on a `=&` `` |
|         - | 2792 | ``			 * bind (`$a[0] =& $x` creates $a as [0 => &$x], it does not warn). */`` |
|         - | 2793 | `			/* php's compile-time write-target rules first ($this, a temporary base,` |
|         - | 2794 | `			 * the call that is the target itself). A COMPOUND assignment is a` |
|         - | 2795 | `			 * read-modify-write: php's $this rule does not reach it. */` |
|    960763 | 2796 | `			rc = GenStateWriteTargetCheck(&(*pGen),pNode->pRight,` |
|    479771 | 2797 | `				(iVmOp == PH7_OP_STORE \|\| pNode->pOp->iOp == EXPR_OP_REF)` |
|         - | 2798 | `					? 0 : PH7_WTC_RMW);` |
|    929046 | 2799 | `			if( rc != SXRET_OK ){` |
|        26 | 2800 | `				return rc;` |
|         - | 2801 | `			}` |
|    929024 | 2802 | `			if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|         - | 2803 | `				/* php compiles a reference SOURCE in write context too` |
|         - | 2804 | `` 				 * (`zend_compile_var(source, BP_VAR_W, 1)`), so `$r =& (new A)->p` `` |
|         - | 2805 | ``				 * and `$r =& (clone $o)->p` are the same two compile refusals a`` |
|         - | 2806 | `				 * write to them would be. Only the call-as-target question is not` |
|         - | 2807 | ``				 * asked here: `$r =& f()` is legal. The operands were swapped in`` |
|         - | 2808 | `				 * parse.c, so the source is pLeft. */` |
|       459 | 2809 | `				rc = GenStateWriteTargetCheck(&(*pGen),pNode->pLeft,PH7_WTC_REFSRC);` |
|       459 | 2810 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 2811 | `					return rc;` |
|         - | 2812 | `				}` |
|       227 | 2813 | `			}` |
|    929024 | 2814 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|    929024 | 2815 | `			if( iVmOp != PH7_OP_STORE && pNode->pOp->iOp != EXPR_OP_REF ){` |
|         - | 2816 | ``				/* COMPOUND assignment (`.=`, `+=`, ...) READS the target first, so`` |
|         - | 2817 | ``				 * php warns when it is undefined and then seeds it; a plain `=` and a`` |
|         - | 2818 | ``				 * `=&` rebind write without reading the target and stay silent. */`` |
|     31280 | 2819 | `				iFlags \|= EXPR_FLAG_RMW_LOAD;` |
|     15618 | 2820 | `			}` |
|    463882 | 2821 | `		}` |
|   2649796 | 2822 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|         - | 2823 | `		/* The RIGHT operand is never the reference source: for an assignment it is the` |
|         - | 2824 | `		 * TARGET (the operands were swapped), and for a member access it is the property` |
|         - | 2825 | ``		 * NAME -- and a name written as an expression (`$r =& $o->{$a->b}`) would`` |
|         - | 2826 | `		 * otherwise be compiled as a write-context fetch of its own. */` |
|   2649796 | 2827 | `		iRhsFlags = iFlags & ~EXPR_FLAG_MEMBER_REFSRC;` |
|   2649796 | 2828 | `		if( nParked > 0 ){` |
|         - | 2829 | `			/* The target's own emission is the only place a PICK may stand in for a` |
|         - | 2830 | `			 * name; a nested assignment inside the value has already been compiled` |
|         - | 2831 | `			 * with nothing parked, and one inside the target's container answers for` |
|         - | 2832 | `			 * itself through this save and the restore below. */` |
|       469 | 2833 | `			apOuterParked = pGen->apStoreKey;` |
|       469 | 2834 | `			nOuterParked = pGen->nStoreKey;` |
|       469 | 2835 | `			pGen->apStoreKey = apParked;` |
|       469 | 2836 | `			pGen->nStoreKey = nParked;` |
|       232 | 2837 | `		}` |
|   2649791 | 2838 | `		if( iVmOp == PH7_OP_STORE && pNode->pRight` |
|    897311 | 2839 | `		 && (pNode->pRight->xCode == PH7_CompileList` |
|    897277 | 2840 | `		  \|\| pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 2841 | ``			/* A destructuring target list may bind BY REFERENCE (`[&$t] = $src`),`` |
|         - | 2842 | `			 * and php asks at COMPILE time whether the source can hold one --` |
|         - | 2843 | ``			 * `zend_is_variable_or_call`, which takes a variable, a property, a`` |
|         - | 2844 | `			 * static property, a subscript and a CALL, and refuses everything else` |
|         - | 2845 | ``			 * with `Cannot assign reference to non referenceable value`. The list`` |
|         - | 2846 | `			 * body cannot ask: by the time it emits a bind the source is an` |
|         - | 2847 | `			 * anonymous value on the stack. Carry the answer to it. */` |
|       411 | 2848 | `			sxi8 bSavedSrcRef = pGen->bListSrcNotRef;` |
|       411 | 2849 | `			pGen->bListSrcNotRef = (sxi8)!GenStateNodeIsRefSource(pNode->pLeft);` |
|       411 | 2850 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iRhsFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|       411 | 2851 | `			pGen->bListSrcNotRef = bSavedSrcRef;` |
|       208 | 2852 | `		}else{` |
|   2649390 | 2853 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iRhsFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|         - | 2854 | `		}` |
|   2649796 | 2855 | `		if( nParked > 0 ){` |
|       469 | 2856 | `			pGen->apStoreKey = apOuterParked;` |
|       469 | 2857 | `			pGen->nStoreKey = nOuterParked;` |
|       232 | 2858 | `		}` |
|   2649796 | 2859 | `		if( !bIsChainOp ){` |
|         - | 2860 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 2861 | `			 * operator instruction is emitted. */` |
|   2604168 | 2862 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   1300329 | 2863 | `		}` |
|   2649796 | 2864 | `		if( iVmOp == PH7_OP_STORE ){` |
|    897311 | 2865 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|    897248 | 2866 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 2867 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 2868 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 2869 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 2870 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 2871 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 2872 | `				 */` |
|       411 | 2873 | `				iVmOp = 0;` |
|    897108 | 2874 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|    896905 | 2875 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 2876 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|      2245 | 2877 | `					iP2 = 1;` |
|      1125 | 2878 | `				}else{` |
|    894665 | 2879 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 2880 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    205675 | 2881 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    205675 | 2882 | `						iP1 = pInstr->iP1;` |
|    102703 | 2883 | `					}else{` |
|    688995 | 2884 | `						p3 = pInstr->p3;` |
|         - | 2885 | `					}` |
|         - | 2886 | `					/* POP the last dynamic load instruction */` |
|    894665 | 2887 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 2888 | `				}` |
|    447847 | 2889 | `			}` |
|   2200535 | 2890 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|         - | 2891 | `			/* php records at COMPILE time whether the reference SOURCE was written` |
|         - | 2892 | ``			 * as a CALL (ZEND_RETURNS_FUNCTION), so the bind can raise `Only`` |
|         - | 2893 | ``			 * variables should be assigned by reference` when the callee turns out`` |
|         - | 2894 | `` 			 * not to return by reference. It is the direct call only: `$r =& f()` `` |
|         - | 2895 | ``			 * warns where `$r =& f()[0]` and `$r =& f()->p` are silent. The operands`` |
|         - | 2896 | `			 * were swapped in parse.c, so the source is pLeft. */` |
|       438 | 2897 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|       354 | 2898 | `			 && pNode->pLeft->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        50 | 2899 | `				iP1 \|= PH7_STOREREF_CALLSRC;` |
|        23 | 2900 | `			}` |
|         - | 2901 | `			/* Peek first: a member LHS ($o->p =& $x, C::$s =& $x) keeps its` |
|         - | 2902 | `			 * OP_MEMBER in place (it resolves + stashes the target slot at` |
|         - | 2903 | `			 * runtime), unlike the variable/array shapes which fold their load` |
|         - | 2904 | `			 * away. Mirrors the normal member-store fold above (iP2 kept-MEMBER). */` |
|       443 | 2905 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|       443 | 2906 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 2907 | `				/* Tag the member as a reference target and flag STORE_REF (iP2=1)` |
|         - | 2908 | `				 * to take the member-rebind path in the VM. */` |
|        72 | 2909 | `				pInstr->iP2 = PH7_MEMBER_REF_TARGET;` |
|        72 | 2910 | `				iP2 = 1;` |
|        37 | 2911 | `			}else{` |
|       373 | 2912 | `				pInstr = PH7_VmPopInstr(pGen->pVm);` |
|       373 | 2913 | `				if( pInstr ){` |
|       373 | 2914 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 2915 | `						/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 2916 | `						 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 2917 | `						 */` |
|        98 | 2918 | `						iVmOp = PH7_OP_STORE_IDX_REF;` |
|        98 | 2919 | `						iP1 = pInstr->iP1 \| (iP1 & PH7_STOREREF_CALLSRC);` |
|        98 | 2920 | `						iP2 = pInstr->iP2;` |
|        98 | 2921 | `						p3  = pInstr->p3;` |
|        51 | 2922 | `					}else{` |
|       279 | 2923 | `						p3 = pInstr->p3;` |
|         - | 2924 | `					}` |
|       184 | 2925 | `				}` |
|         - | 2926 | `			}` |
|       219 | 2927 | `		}` |
|   1323124 | 2928 | `	}` |
|   4667892 | 2929 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|     63893 | 2930 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 2931 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 2932 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|       289 | 2933 | `		iVmOp = 0;` |
|       142 | 2934 | `	}` |
|   4667897 | 2935 | `	if( bMoveLhs ){` |
|         - | 2936 | `		/* The left operand's load, lifted to here so it reads the variable AFTER the` |
|         - | 2937 | `		 * right operand ran -- php's compiled-variable read. Unconditional, and ahead` |
|         - | 2938 | `		 * of every branch below: the instruction was taken OUT of the stream, so there` |
|         - | 2939 | `		 * is no path this may be skipped on. */` |
|     60400 | 2940 | `		GenStateReEmitInstr(&(*pGen),&sMovedLhs);` |
|     60400 | 2941 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWAP,0,0,0,0);` |
|     30155 | 2942 | `	}` |
|   4667897 | 2943 | `	if( iVmOp > 0 ){` |
|   4666541 | 2944 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|     77317 | 2945 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 2946 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|      8536 | 2947 | `				iP1 = 1;` |
|      4265 | 2948 | `			}` |
|   4627833 | 2949 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 2950 | `			/* Namespace-qualify the class name for NEW */ {` |
|    123727 | 2951 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    123727 | 2952 | `				VmInstr *pCallInstr = 0;` |
|    123727 | 2953 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    120079 | 2954 | `					VmCallArgMap *pNewMap = (VmCallArgMap *)pPeek->p3;` |
|    120079 | 2955 | `					pCallInstr = pPeek;` |
|         - | 2956 | `` 					/* The class-name push sits one instruction back only when this `new` `` |
|         - | 2957 | `					 * takes no arguments; with an argument list the reorder puts the whole` |
|         - | 2958 | `					 * list (and its screen and rotation) in between, so the call node` |
|         - | 2959 | `					 * recorded where the push is. */` |
|    180195 | 2960 | `					pPeek = (pNewMap && pNewMap->nNewClassInstr > 0)` |
|    120074 | 2961 | `						? PH7_VmGetInstr(pGen->pVm,pNewMap->nNewClassInstr - 1)` |
|     60116 | 2962 | `						: PH7_VmPeekNextInstr(pGen->pVm);` |
|     59958 | 2963 | `				}` |
|    123727 | 2964 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    123581 | 2965 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 2966 | `					sxu32 nLitForClass;` |
|    123581 | 2967 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 2968 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 2969 | `					 * imports, recover the original literal (recorded in the` |
|         - | 2970 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 2971 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 2972 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 2973 | `					 * with class imports. */` |
|    123581 | 2974 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|       135 | 2975 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        70 | 2976 | `					}else{` |
|    123451 | 2977 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 2978 | `					}` |
|    123581 | 2979 | `					pPeek->iP1 = 0;` |
|    123581 | 2980 | `					if( !bAbsolute ){` |
|         - | 2981 | `						/* self/static/parent are resolved at runtime against the` |
|         - | 2982 | `						 * current class — never namespace-qualify them (else` |
|         - | 2983 | ``						 * `new self` in namespace N becomes "N\self"). Mirrors the`` |
|         - | 2984 | `						 * instanceof (IS_A) guard below. */` |
|    123453 | 2985 | `						int isSpecialNew = GenStateLitIsScopeKeyword(&(*pGen),nLitForClass);` |
|    123453 | 2986 | `						bScopeKw = isSpecialNew;` |
|    123500 | 2987 | `						if( isSpecialNew && GenStateScreenScopeKeyword(&(*pGen),nLitForClass,` |
|       141 | 2988 | `								pNode->pStart->nLine) == SXERR_ABORT ){` |
|       ! 0 | 2989 | `							return SXERR_ABORT;` |
|         - | 2990 | `						}` |
|    123453 | 2991 | `						if( isSpecialNew && pCallNsMap && pCallNsMap->nNewClassInstr > 0 ){` |
|         - | 2992 | `							/* The screen pass (iP1 -1) resolves the class too. */` |
|        73 | 2993 | `							VmInstr *pScreen = PH7_VmGetInstr(pGen->pVm,pCallNsMap->nNewClassInstr);` |
|        73 | 2994 | `							if( pScreen && pScreen->iOp == PH7_OP_NEW && pScreen->iP1 == -1 ){` |
|        30 | 2995 | `								pScreen->bDiscard = 1;` |
|        13 | 2996 | `							}` |
|        34 | 2997 | `						}` |
|    123453 | 2998 | `						if( isSpecialNew ){` |
|        99 | 2999 | `							pPeek->iP2 = (sxi32)nLitForClass;` |
|        52 | 3000 | `						}else{` |
|    123359 | 3001 | `							pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|         - | 3002 | `						}` |
|     61648 | 3003 | `					}else{` |
|       133 | 3004 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 3005 | `					}` |
|     61707 | 3006 | `				}` |
|         - | 3007 | `			}` |
|    123727 | 3008 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    123727 | 3009 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 3010 | `				VmInstr *pPrev;` |
|         - | 3011 | `				int bPrevMember;` |
|    120079 | 3012 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|         - | 3013 | `				/* "Was the callee a MEMBER access?" — which, once the reorder puts a` |
|         - | 3014 | `				 * rotation between the callee and its call, is the question the rotation` |
|         - | 3015 | `				 * already answers (a method callee is the two-slot one). */` |
|    181662 | 3016 | `				bPrevMember = pPrev && (pPrev->iOp == PH7_OP_ROT_CALLEE` |
|    117140 | 3017 | `					? (pPrev->iP2 & PH7_ROT_TWOSLOT) != 0` |
|         - | 3018 | ``					/* …and only a METHOD member is one. A `new`'s class expression may`` |
|         - | 3019 | ``					 * BE a property read (`new $config->defType()`), which leaves an`` |
|         - | 3020 | `					 * OP_MEMBER in PH7_MEMBER_READ mode with the class NAME on the` |
|         - | 3021 | `					 * stack -- the trailing OP_CALL is the constructor's and must be` |
|         - | 3022 | `					 * folded away like any other. Reading the opcode alone kept it, so` |
|         - | 3023 | `					 * the property's VALUE was then called as a function. */` |
|      2934 | 3024 | `					: (pPrev->iOp == PH7_OP_MEMBER && pPrev->iP2 == PH7_MEMBER_METHOD));` |
|    120079 | 3025 | `				if( !bPrevMember ){` |
|         - | 3026 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 3027 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 3028 | `					 * accumulator exactly like OP_CALL would have). */` |
|    120079 | 3029 | `					iP1 = pInstr->iP1;` |
|    120079 | 3030 | `					iP2 = pInstr->iP2 & PH7_CALL_SPREAD;` |
|    120079 | 3031 | `					if( pInstr->p3 ){` |
|    120079 | 3032 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|     59958 | 3033 | `					}` |
|    120079 | 3034 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|     59958 | 3035 | `				}` |
|     59963 | 3036 | `			}` |
|   4527287 | 3037 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 3038 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 3039 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     17581 | 3040 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     17581 | 3041 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     17569 | 3042 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     17569 | 3043 | `				int isSpecialIs = GenStateLitIsScopeKeyword(&(*pGen),(sxu32)pInstr->iP2);` |
|     17626 | 3044 | `				if( isSpecialIs && GenStateScreenScopeKeyword(&(*pGen),(sxu32)pInstr->iP2,` |
|        38 | 3045 | `						pNode->pRight && pNode->pRight->pStart ? pNode->pRight->pStart->nLine` |
|        19 | 3046 | `						: pNode->pStart->nLine) == SXERR_ABORT ){` |
|       ! 0 | 3047 | `					return SXERR_ABORT;` |
|         - | 3048 | `				}` |
|         - | 3049 | `				/* OP_IS_A's iP1: the class operand is the written keyword, which is` |
|         - | 3050 | `				 * the only shape its handler resolves as one. */` |
|     17569 | 3051 | `				iP1 = isSpecialIs;` |
|     17569 | 3052 | `				pInstr->iP1 = 0;` |
|     17569 | 3053 | `				if( !isSpecialIs && !bAbsolute ){` |
|     17501 | 3054 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      8737 | 3055 | `				}` |
|      8776 | 3056 | `			}` |
|   4456708 | 3057 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 3058 | `			/* Prevent constant expansion for member/property names.` |
|         - | 3059 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 3060 | `			 * should not trigger constant lookup. */` |
|     45633 | 3061 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     45633 | 3062 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     44615 | 3063 | `				pInstr->iP1 = 0;` |
|     22286 | 3064 | `			}` |
|     45633 | 3065 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 3066 | `				/* Static member access,remember that */` |
|      5976 | 3067 | `				iP1 = 1;` |
|      5976 | 3068 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      5976 | 3069 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|       791 | 3070 | `					p3 = pInstr->p3;` |
|         - | 3071 | ``					/* A `$`-form (`C::$s`, `C::$$x`, `C::${$e}`) is a STATIC PROPERTY`` |
|         - | 3072 | `					 * access, never a constant. A LITERAL name folds into p3 (non-zero)` |
|         - | 3073 | ``					 * and the exec side reads it there; a DYNAMIC name (`$$x`/`${$e}`)`` |
|         - | 3074 | `					 * leaves p3==0 with the computed name on the stack — the SAME shape` |
|         - | 3075 | ``					 * as a bareword constant `C::C`. Mark iP1=2 so exec still routes it`` |
|         - | 3076 | `					 * to the property table (hAttr), not the constant table (hConst). */` |
|       791 | 3077 | `					if( p3 == 0 ){` |
|        26 | 3078 | `						iP1 = 2;` |
|        11 | 3079 | `					}` |
|       791 | 3080 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       393 | 3081 | `				}` |
|      2981 | 3082 | `			}` |
|         - | 3083 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 3084 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 3085 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 3086 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|     45633 | 3087 | `			if( iP2 == PH7_MEMBER_READ ){` |
|     45633 | 3088 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       221 | 3089 | `					iP2 = PH7_MEMBER_UNSET;` |
|     45525 | 3090 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       337 | 3091 | `					iP2 = PH7_MEMBER_ISSET;` |
|     45251 | 3092 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        63 | 3093 | `					iP2 = PH7_MEMBER_EMPTY;` |
|     45055 | 3094 | `				}else if( iFlags & EXPR_FLAG_MEMBER_COALESCE ){` |
|       299 | 3095 | `					iP2 = PH7_MEMBER_COALESCE;` |
|     44878 | 3096 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 3097 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|      2973 | 3098 | `					iP2 = PH7_MEMBER_WRITE;` |
|     43247 | 3099 | `				}else if( iFlags & EXPR_FLAG_DEFER_ARG ){` |
|         - | 3100 | `					/* D1 commit 2: deferred by-ref/by-value property arg ($o->p). */` |
|      3725 | 3101 | `					iP2 = PH7_MEMBER_DEFPATH;` |
|      1860 | 3102 | `				}` |
|     22795 | 3103 | `			}` |
|     22795 | 3104 | `		}` |
|         - | 3105 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 3106 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 3107 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 3108 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 3109 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|   4666541 | 3110 | `		if( bFcc ){` |
|       465 | 3111 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|         - | 3112 | `			/* php's global fallback applies to a first-class callable exactly as it does` |
|         - | 3113 | ``			 * to the call it stands for: inside a namespace, `strlen(...)` is the global`` |
|         - | 3114 | `			 * function when the current namespace has none. The callee's literal was` |
|         - | 3115 | `			 * namespace-qualified above and the arg map that records it is dropped here` |
|         - | 3116 | `			 * (an FCC has no arguments), so carry the one bit the resolution needs in the` |
|         - | 3117 | ``			 * instruction itself — without it `strlen(...)` in a namespaced file was`` |
|         - | 3118 | ``			 * `Call to undefined function A\B\strlen()`, a fatal on ordinary php. */`` |
|       465 | 3119 | `			iP2 = (p3 && ((VmCallArgMap *)p3)->bIsNamespaced) ? 1 : 0;` |
|       465 | 3120 | `			p3 = 0;` |
|       465 | 3121 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|       465 | 3122 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER && pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|         - | 3123 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 3124 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 3125 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 3126 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|       293 | 3127 | `				void *pMemberName = pInstr->p3;` |
|       293 | 3128 | ``				bScopeKw = pInstr->bDiscard; /* `self::m(...)`: the member's mark moves here */`` |
|       293 | 3129 | `				if( pInstr->iP1 == 1 && pMemberName == 0 && pInstr->bRefSrc ){` |
|         - | 3130 | ``					/* A literal `X::__construct(...)`, marked above: the class's constructor`` |
|         - | 3131 | `					 * (iP2==2), not a method lookup. The namespace bit is a plain callee's. */` |
|        25 | 3132 | `					iP2 = 2;` |
|        12 | 3133 | `				}` |
|       293 | 3134 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|       293 | 3135 | `				if( pMemberName ){` |
|       ! 0 | 3136 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       ! 0 | 3137 | `				}` |
|       293 | 3138 | `				iP1 = 2;` |
|       149 | 3139 | `			}else{` |
|         - | 3140 | `				/* Only a METHOD member is the callee's NAME. A parenthesised PROPERTY read` |
|         - | 3141 | ``				 * (`($o->cb)(...)`, `(C::$cb)(...)`) is php's variable-invocation: the member`` |
|         - | 3142 | `				 * op stays, its VALUE is the callable, and this is the iP1=1 wrap. Dropping it` |
|         - | 3143 | `				 * here read the property NAME as a method name and answered` |
|         - | 3144 | ``				 * `Call to undefined method H::cb()` for a closure the object was holding —`` |
|         - | 3145 | `				 * the CALL codegen above already made the distinction (it leaves the member a` |
|         - | 3146 | `				 * plain read for a parenthesised callee) and this branch undid it. */` |
|       177 | 3147 | `				iP1 = 1;` |
|         - | 3148 | `			}` |
|       230 | 3149 | `		}` |
|         - | 3150 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 3151 | `		 * This is the primary emit path for user-visible calls. */` |
|   4666541 | 3152 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   1327760 | 3153 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|    662634 | 3154 | `		}` |
|         - | 3155 | `		/* Finally,emit the VM instruction associated with this operator */` |
|   4666541 | 3156 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|   4666541 | 3157 | `		if( bScopeKw && iVmOp != PH7_OP_IS_A ){` |
|      1155 | 3158 | `			PH7_VmPeekInstr(pGen->pVm)->bDiscard = 1;` |
|       575 | 3159 | `		}` |
|   4666536 | 3160 | `		if( iVmOp == PH7_OP_MEMBER && iP2 == PH7_MEMBER_READ` |
|     41838 | 3161 | `		 && (iFlags & EXPR_FLAG_MEMBER_REFSRC) ){` |
|         - | 3162 | `			/* The reference SOURCE keeps its READ mode -- php hands back a copy for a` |
|         - | 3163 | `			 * handler-backed property, and dispatches __get for an overloaded one -- and` |
|         - | 3164 | `			 * carries the write-context marker beside it (see VmInstr::bRefSrc). */` |
|       146 | 3165 | `			VmInstr *pRefSrc = PH7_VmPeekInstr(pGen->pVm);` |
|       146 | 3166 | `			if( pRefSrc ){` |
|       146 | 3167 | `				pRefSrc->bRefSrc = 1;` |
|        71 | 3168 | `			}` |
|        71 | 3169 | `		}` |
|   4666541 | 3170 | `		if( (iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW) && pNode->pStart ){` |
|         - | 3171 | `			/* A call's own line is where it BEGINS, not where its argument list` |
|         - | 3172 | `			 * closes. The emitter stamps every instruction with the token the` |
|         - | 3173 | `			 * generator is standing on, which for a call is the ')' -- so a call` |
|         - | 3174 | `			 * written across several lines went into the backtrace at its LAST one` |
|         - | 3175 | `			 * and php records its first. */` |
|   1327760 | 3176 | `			VmInstr *pCallInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   1327760 | 3177 | `			if( pCallInstr ){` |
|   1327760 | 3178 | `				pCallInstr->nLine = pNode->pStart->nLine;` |
|    662634 | 3179 | `			}` |
|    662634 | 3180 | `		}` |
|   2329785 | 3181 | `	}` |
|   4667897 | 3182 | `	if( nParked > 0 ){` |
|         - | 3183 | `		/* The store consumed the value and the target and left the assignment's own` |
|         - | 3184 | `		 * result on top; the parked names are still under it. Lift the result over each` |
|         - | 3185 | `		 * one and drop it -- php's temporaries are freed at the same point. */` |
|         - | 3186 | `		int nAt;` |
|       941 | 3187 | `		for( nAt = 0 ; nAt < nParked ; ++nAt ){` |
|       477 | 3188 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWAP,0,0,0,0);` |
|       477 | 3189 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       241 | 3190 | `		}` |
|       232 | 3191 | `	}` |
|   4667897 | 3192 | `	if( nJmpIdx > 0 ){` |
|         - | 3193 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    246991 | 3194 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    246991 | 3195 | `		if( pInstr ){` |
|    246991 | 3196 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    123330 | 3197 | `		}` |
|    123330 | 3198 | `	}` |
|   4667897 | 3199 | `	return rc;` |
|   6172903 | 3200 | `}` |
|         - | 3201 | `/*` |
|         - | 3202 | ` * Emit a call's ARGUMENT LIST, and decide everything about it the OP_CALL then carries:` |
|         - | 3203 | ` * the count, the unpack flag, the named-argument / assert-source / argument-shape map.` |
|         - | 3204 | ` *` |
|         - | 3205 | ` * Split out of GenStateEmitExprCode because php resolves a callee BEFORE it evaluates` |
|         - | 3206 | ` * the arguments, so this now runs AFTER the callee sub-tree has been emitted (the one` |
|         - | 3207 | `` * exception is a `new`'s constructor list — see EXPR_FLAG_NEW_CALLEE). It is otherwise`` |
|         - | 3208 | ` * the same code, and reads only the node: nothing here inspects the instructions the` |
|         - | 3209 | ` * callee left behind.` |
|         - | 3210 | ` *` |
|         - | 3211 | ` * pArgs->p3 may arrive non-NULL — the callee's own namespace qualification builds the` |
|         - | 3212 | ` * VmCallArgMap first now — and every allocation site below reuses it.` |
|         - | 3213 | ` */` |
|   1205157 | 3214 | `static sxi32 GenStateEmitCallArgs(` |
|         - | 3215 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 3216 | `	ph7_expr_node *pNode, /* The call node */` |
|         - | 3217 | `	sxi32 iFlags,         /* Control flags of the call site */` |
|         - | 3218 | `	GenCallArgs *pArgs    /* OUT: what the OP_CALL needs */` |
|         - | 3219 | `	)` |
|         5 | 3220 | `{` |
|   1205162 | 3221 | `	void *p3 = pArgs->p3;` |
|   1205162 | 3222 | `	sxi32 iP1 = 0;` |
|   1205162 | 3223 | `	sxu32 iP2 = 0;` |
|   1205162 | 3224 | `	int bFcc = 0;` |
|         - | 3225 | `	sxi32 rc;` |
|         - | 3226 | `	ph7_expr_node **apNode;` |
|   1205162 | 3227 | `	int hasSpread = 0;` |
|   1205162 | 3228 | `	int hasNamed = 0;` |
|   1205162 | 3229 | `	sxu32 byRefMask = 0;` |
|         - | 3230 | `	sxi32 nArgs;` |
|         - | 3231 | `	sxi32 n;` |
|   1205162 | 3232 | `	int bAnySpread = 0;` |
|   1205162 | 3233 | `	sxi32 nLastRunner = 0;` |
|   1205162 | 3234 | `	int bConstruct = 0; /* the callee is a language construct's keyword -- PH7_CALL_CONSTRUCT */` |
|         - | 3235 | `	sxu32 aNamedSend[16]; /* the PH7_OP_NAMED_SEND screens emitted, patched with the map below */` |
|   1205162 | 3236 | `	sxu32 nNamedSend = 0;` |
|         - | 3237 | `	sxu32 aReadSend[16];  /* the positional reads emitted, patched with whichever map the call ends with */` |
|   1205162 | 3238 | `	sxu32 nReadSend = 0;` |
|   1205162 | 3239 | `	int bFramelessSite = 0; /* php would compile this call as a frameless instruction */` |
|         - | 3240 | `	/* Recurse and generate bytecodes for function arguments */` |
|   1205162 | 3241 | `	apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   1205162 | 3242 | `	nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - | 3243 | ``	/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - | 3244 | `	 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - | 3245 | `	 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   1205162 | 3246 | `	if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|       465 | 3247 | `		bFcc = 1;` |
|       465 | 3248 | `		nArgs = 0;` |
|       230 | 3249 | `	}` |
|         - | 3250 | `	/* Validate argument order like php: no positional argument after a` |
|         - | 3251 | ``	 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - | 3252 | `	{` |
|   1205162 | 3253 | `		int seenNamed = 0;` |
|   1205162 | 3254 | `		int seenSpread = 0;` |
|   2912963 | 3255 | `		for( n = 0; n < nArgs; ++n ){` |
|   1707810 | 3256 | `			if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       568 | 3257 | `				bAnySpread = 1;` |
|       568 | 3258 | `				seenSpread = 1;` |
|       568 | 3259 | `				if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 | 3260 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[n]->pStart->nLine,` |
|         - | 3261 | `						"syntax error, unexpected token \"...\"");` |
|       ! 0 | 3262 | `					return SXERR_SYNTAX;` |
|         - | 3263 | `				}` |
|       568 | 3264 | `				if( seenNamed ){` |
|         - | 3265 | `					/* The mirror of the positional-after-named rule: php refuses the` |
|         - | 3266 | ``					 * UNPACK too, and at compile time. Without it `f(x: 1, ...$a)` ran`` |
|         - | 3267 | `					 * and reported whatever the runtime binder made of the flattened` |
|         - | 3268 | `					 * list -- a different sentence, raised too late, on a program php` |
|         - | 3269 | `					 * never starts. */` |
|         3 | 3270 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 3271 | `						"Cannot use argument unpacking after named arguments");` |
|         3 | 3272 | `					return SXERR_SYNTAX;` |
|         5 | 3273 | `				}` |
|   1707527 | 3274 | `			}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      1545 | 3275 | `				seenNamed = 1;` |
|      1545 | 3276 | `				hasNamed = 1;` |
|   1706477 | 3277 | `			}else if( seenNamed ){` |
|         3 | 3278 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 3279 | `					"Cannot use positional argument after named argument");` |
|         3 | 3280 | `				return SXERR_SYNTAX;` |
|   1705705 | 3281 | `			}else if( seenSpread ){` |
|       ! 0 | 3282 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 3283 | `					"Cannot use positional argument after argument unpacking");` |
|       ! 0 | 3284 | `				return SXERR_SYNTAX;` |
|         - | 3285 | `			}` |
|    852185 | 3286 | `		}` |
|         - | 3287 | `	}` |
|         - | 3288 | `	/* Read-only load */` |
|   1205158 | 3289 | `	iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - | 3290 | `	/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - | 3291 | ``	 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - | 3292 | `	 * objects dispatch to the right method (offsetExists for both;` |
|         - | 3293 | `	 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   1205158 | 3294 | `	if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   1204504 | 3295 | `		SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|   1854558 | 3296 | `		int bIsset = pCallName->nByte == 5` |
|   1204499 | 3297 | `			&& SyStrnicmp(pCallName->zString,"isset",5) == 0;` |
|   1854558 | 3298 | `		int bEmpty = pCallName->nByte == 5` |
|   1204499 | 3299 | `			&& SyStrnicmp(pCallName->zString,"empty",5) == 0;` |
|         - | 3300 | ``		/* `isset`, `empty` and `eval` reach the VM as a call to a host function of`` |
|         - | 3301 | `		 * their own name, and php has no such function -- so the SITE has to say that` |
|         - | 3302 | `		 * the engine, not the program, spelled it (PH7_CALL_CONSTRUCT). */` |
|   1204504 | 3303 | `		bConstruct = GenStateCalleeIsConstruct(pNode->pLeft);` |
|         - | 3304 | `		/* isset()/empty() are language CONSTRUCTS, not functions: php parses` |
|         - | 3305 | `		 * their argument list in the grammar and a missing operand is a parse` |
|         - | 3306 | `		 * error on the ')'. They compile through this ordinary call loop, which` |
|         - | 3307 | ``		 * never checked arity, so `empty()` quietly evaluated to true and`` |
|         - | 3308 | ``		 * `isset()` to false. (empty() also takes exactly one operand in php,`` |
|         - | 3309 | `		 * unlike isset(), which is variadic.) */` |
|   1204504 | 3310 | `		if( (bIsset \|\| bEmpty) && nArgs < 1 ){` |
|         - | 3311 | `			/* php names the ')' itself as the unexpected token, so point at the` |
|         - | 3312 | `			 * node's last token rather than pGen->pIn (which has already moved` |
|         - | 3313 | `			 * past the call to the statement's ';'). */` |
|         6 | 3314 | `			SyToken *pTok = pNode->pEnd;` |
|         6 | 3315 | `			if( pTok && pTok > pNode->pStart && (pTok->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 3316 | `				pTok--;` |
|       ! 0 | 3317 | `			}` |
|         6 | 3318 | `			PH7_GenSyntaxError(&(*pGen),pTok,0);` |
|         6 | 3319 | `			return SXERR_ABORT;` |
|         - | 3320 | `		}` |
|   1204500 | 3321 | `		if( bIsset ){` |
|     18213 | 3322 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   1195385 | 3323 | `		}else if( bEmpty ){` |
|       217 | 3324 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|       106 | 3325 | `		}` |
|         - | 3326 | `		/* Auto-vivify by-reference out-params of known builtins so an` |
|         - | 3327 | `		 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - | 3328 | `		 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - | 3329 | `		 * write back through. Skipped when spread/named args are present:` |
|         - | 3330 | `		 * the compile-time positional index no longer maps to the` |
|         - | 3331 | `		 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   1204500 | 3332 | `		if( !bAnySpread && !hasNamed ){` |
|         - | 3333 | `			SyString sBuiltin;` |
|   1203031 | 3334 | `			GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   1203031 | 3335 | `			byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|    600351 | 3336 | `		}` |
|    601085 | 3337 | `	}` |
|         - | 3338 | `	/* The last argument position AFTER which nothing in this list can run code.` |
|         - | 3339 | `	 * Every by-value argument before it is pushed as a view of somebody's bytes and` |
|         - | 3340 | `	 * has to be given its own copy (PH7_OP_SNAPSHOT); at or after it, nothing between` |
|         - | 3341 | `	 * the push and the call can write, so the view is exactly php's answer and costs` |
|         - | 3342 | `	 * nothing. A list whose arguments are all plain variables and literals -- which is` |
|         - | 3343 | `	 * most of them -- leaves this at 0 and emits nothing. */` |
|   1205154 | 3344 | `	nLastRunner = 0;` |
|   2392944 | 3345 | `	for( n = nArgs - 1 ; n >= 0 ; --n ){` |
|   1523335 | 3346 | `		if( GenStateArgRunsCode(apNode[n]) ){` |
|    335545 | 3347 | `			nLastRunner = n;` |
|    335545 | 3348 | `			break;` |
|         - | 3349 | `		}` |
|    592878 | 3350 | `	}` |
|         - | 3351 | `	/* php compiles a direct call to one of its FRAMELESS builtins, at a listed arity, as` |
|         - | 3352 | ``	 * one instruction that reads a plain `$var` operand itself -- at the call, after every`` |
|         - | 3353 | ``	 * other argument has run: `max($u, s())` runs s() first. So does a call it SPECIALIZES`` |
|         - | 3354 | ``	 * into an opcode (`array_key_exists`, a `sprintf` it rewrites), though only where the`` |
|         - | 3355 | `	 * name cannot be a namespace's own. The positional read below leaves such an operand` |
|         - | 3356 | `	 * to the call, and is told so in case the name turns out to be a function of the` |
|         - | 3357 | `	 * namespace's own, which php calls the ordinary way. */` |
|   1205154 | 3358 | `	if( !hasNamed && !bAnySpread && !pArgs->bNewCallee ){` |
|         - | 3359 | `		SyString sFrameless;` |
|   1083739 | 3360 | `		GenStateCallBuiltinName(pNode->pLeft,&sFrameless);` |
|   1083739 | 3361 | `		bFramelessSite = sFrameless.nByte > 0 && PH7_VmFramelessArity(&sFrameless,(int)nArgs);` |
|   1083734 | 3362 | `		if( !bFramelessSite && !bConstruct && (p3 == 0 \|\| !((VmCallArgMap *)p3)->bIsNamespaced)` |
|    906713 | 3363 | `		 && GenStateCallIsSpecialized(&(*pGen),pNode,&sFrameless) ){` |
|    202266 | 3364 | `			bFramelessSite = 1;` |
|    100989 | 3365 | `		}` |
|    540457 | 3366 | `	}` |
|   2912295 | 3367 | `	for( n = 0 ; n < nArgs ; ++n ){` |
|   1707802 | 3368 | `		sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   1707802 | 3369 | `		sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE` |
|         - | 3370 | `			\|EXPR_FLAG_MEMBER_REFSRC);` |
|         - | 3371 | `		/* For a by-ref argument position, drop the read-only flag so the` |
|         - | 3372 | `		 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - | 3373 | `		 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - | 3374 | `		 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - | 3375 | `		 * builtin to write back through. A plain $var target is unaffected` |
|         - | 3376 | `		 * (iP1=0 either way).` |
|         - | 3377 | `		 *` |
|         - | 3378 | `		 * A PROPERTY target is the one shape this eager path cannot express, so it` |
|         - | 3379 | ``		 * is left to the deferred one below: what php's `FETCH_OBJ_W` does to`` |
|         - | 3380 | ``		 * `$o->p` is not what an ASSIGNMENT does to it — a missing property is`` |
|         - | 3381 | ``		 * CREATED, an overloaded one takes `Indirect modification of overloaded`` |
|         - | 3382 | ``		 * property` and is passed by VALUE (rather than reaching `__set`), and a`` |
|         - | 3383 | ``		 * non-object base is the catchable `Attempt to modify property`. The`` |
|         - | 3384 | `		 * deferred resolver already encodes all of that (VmBindPropByRef) and` |
|         - | 3385 | `		 * already reads a host function's by-ref mask, so routing the property` |
|         - | 3386 | ``		 * shapes through it is what makes `preg_match($p, $s, $this->matches)` —`` |
|         - | 3387 | `		 * the ordinary spelling — write anything at all. */` |
|   1707797 | 3388 | `		if( n < 31 && (byRefMask & (1u<<n))` |
|    857371 | 3389 | `		 && !GenStateArgHasPropertyStep(apNode[n]) ){` |
|     10278 | 3390 | `			iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     10278 | 3391 | `			iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|      5124 | 3392 | `		}` |
|         - | 3393 | ``		/* D1: a plain `$var` argument may bind to a by-ref parameter whose signature`` |
|         - | 3394 | `		 * is unknown at compile time (forward reference, dynamic call, or method` |
|         - | 3395 | ``		 * dispatch — e.g. PHPUnit's `willReturnReference($undef)`). We used to clear`` |
|         - | 3396 | `		 * the read-only flag here so an undefined variable vivified a real slot the` |
|         - | 3397 | `		 * by-ref write-back could reach — but that also invented the variable as NULL` |
|         - | 3398 | `		 * in the caller when the parameter turned out by-VALUE, and suppressed php's` |
|         - | 3399 | ``		 * `Undefined variable $x` warning. Instead mark it DEFERRED: OP_LOAD leaves an`` |
|         - | 3400 | `		 * undefined variable uncreated and carries a lazy-lvalue marker, and OP_CALL` |
|         - | 3401 | `		 * materializes it ONLY for a by-ref parameter once the callee is resolved` |
|         - | 3402 | `		 * (VmResolveDeferredArgs). Excludes isset()/empty()/unset(), which compile` |
|         - | 3403 | `		 * through this same call loop but must NEVER create their operand, and` |
|         - | 3404 | `		 * spread args (whose elements have no positional index of their own). A NAMED` |
|         - | 3405 | `		 * arg defers too: it binds to the formal its NAME picks, which the resolver` |
|         - | 3406 | `		 * looks up through the call's own argument map — excluding it left` |
|         - | 3407 | ``		 * `r(x: $a['new'])` warning `Undefined array key` and passing NULL where php`` |
|         - | 3408 | ``		 * creates the element for the by-ref parameter `$x`.`` |
|         - | 3409 | `		 *` |
|         - | 3410 | `		 * D1 commit 2: the same reasoning extends to an array-element ($a["k"]) or` |
|         - | 3411 | `		 * property ($o->p) argument — a by-ref user-function parameter must vivify the` |
|         - | 3412 | `		 * element/property, a by-value one must warn and NOT vivify. Those nodes carry a` |
|         - | 3413 | `		 * subscript/arrow operator (pOp != 0). The DEFER flag rides down to the base LOAD` |
|         - | 3414 | `		 * (undefined base auto-defers via commit 1) and to the LOAD_IDX/MEMBER, which` |
|         - | 3415 | ``		 * record the lvalue path on a lookup miss. Static `::` and nullsafe `?->` stay`` |
|         - | 3416 | `		 * eager. */` |
|   1707797 | 3417 | `		if( (iFlags & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_LOAD_IDX_UNSET` |
|    852178 | 3418 | `		               \|EXPR_FLAG_MEMBER_COALESCE)) == 0` |
|   1698565 | 3419 | `		 && (iArgFlags & EXPR_FLAG_RDONLY_LOAD) /* not a known builtin by-ref slot (kept eager above) */` |
|   1684206 | 3420 | `		 && (apNode[n]->iFlags & EXPR_NODE_SPREAD) == 0` |
|   1816557 | 3421 | `		 && ( (apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable)` |
|   1260971 | 3422 | `		   \|\| (apNode[n]->pOp != 0 && (apNode[n]->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|    289813 | 3423 | `		                            \|\| apNode[n]->pOp->iOp == EXPR_OP_ARROW)) ) ){` |
|    864889 | 3424 | `			iArgFlags \|= EXPR_FLAG_DEFER_ARG;` |
|    431819 | 3425 | `		}` |
|   1707802 | 3426 | `		rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   1707802 | 3427 | `		if( rc != SXRET_OK ){` |
|         3 | 3428 | `			return rc;` |
|         - | 3429 | `		}` |
|         - | 3430 | `		/* Each argument is an independent nullsafe scope. */` |
|   1707800 | 3431 | `		GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   1707795 | 3432 | `		if( bFramelessSite && n < nLastRunner && (iArgFlags & EXPR_FLAG_DEFER_ARG)` |
|    115698 | 3433 | `		 && apNode[n]->pOp == 0 ){` |
|         - | 3434 | ``			/* The frameless instruction reads a DEFINED `$var` at the call too: in`` |
|         - | 3435 | ``			 * `max($x, h())` a write h() makes to $x is what max() sees. So the load`` |
|         - | 3436 | `			 * leaves the slot unread whether or not the variable exists (OP_LOAD` |
|         - | 3437 | `			 * iP2 = 5), and the call reads it. */` |
|    101702 | 3438 | `			VmInstr *pLoad = PH7_VmPeekInstr(pGen->pVm);` |
|    101697 | 3439 | `			if( pLoad && pLoad->iOp == PH7_OP_LOAD && pLoad->iP2 == 3 && pLoad->p3` |
|    101702 | 3440 | `			 && SyStrncmp((const char *)pLoad->p3,"this",sizeof("this")) != 0 ){` |
|    101702 | 3441 | `				pLoad->iP2 = 5;` |
|     50781 | 3442 | `			}` |
|     50781 | 3443 | `		}` |
|   1707795 | 3444 | `		if( n < nLastRunner && (iArgFlags & EXPR_FLAG_DEFER_ARG) && !bConstruct` |
|    116073 | 3445 | `		 && (apNode[n]->iFlags & EXPR_NODE_NAMED_ARG) == 0` |
|    116023 | 3446 | `		 && nReadSend < sizeof(aReadSend)/sizeof(aReadSend[0]) ){` |
|         - | 3447 | `			/* php reads a deferred operand at its own SEND, against the formal it binds` |
|         - | 3448 | ``			 * to, so `f($u, s())` warns `Undefined variable $u` before s() runs -- and a`` |
|         - | 3449 | `			 * by-reference formal creates the variable before s() can see it. A NAMED` |
|         - | 3450 | `			 * one is read by its own screen below, once its name has passed. The map is` |
|         - | 3451 | `			 * attached once the list is complete: a named argument REPLACES the one the` |
|         - | 3452 | `			 * callee's qualification built, and frees it. Past the patch table's room the` |
|         - | 3453 | `			 * operand is simply left to the call, as it was. */` |
|    115968 | 3454 | `			aReadSend[nReadSend++] = PH7_VmInstrLength(pGen->pVm);` |
|    166768 | 3455 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NAMED_SEND,n,PH7_ROT_READ\|PH7_ROT_POSITIONAL` |
|    115963 | 3456 | `				\| (pArgs->bNewCallee ? PH7_ROT_NEW : 0)` |
|    115963 | 3457 | `				\| ((bFramelessSite && apNode[n]->pOp == 0) ? PH7_ROT_FRAMELESS : 0),0,0);` |
|     57896 | 3458 | `		}` |
|   1707800 | 3459 | `		if( n < nLastRunner && (apNode[n]->iFlags & EXPR_NODE_SPREAD) == 0 ){` |
|         - | 3460 | `			/* Something later in this list can write to whatever this argument was` |
|         - | 3461 | `			 * loaded from, so take the bytes now. A spread argument is an array,` |
|         - | 3462 | `			 * which is reference-counted rather than aliased, and is skipped. */` |
|    184376 | 3463 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_SNAPSHOT,0,0,0,0);` |
|     91898 | 3464 | `		}` |
|   1707795 | 3465 | `		if( (apNode[n]->iFlags & EXPR_NODE_NAMED_ARG) && !bConstruct` |
|      1541 | 3466 | `		 && nNamedSend < sizeof(aNamedSend)/sizeof(aNamedSend[0]) ){` |
|         - | 3467 | ``			/* php resolves a NAME at the send of its argument: `f(zz: $u, b: g())` is`` |
|         - | 3468 | ``			 * `Unknown named parameter $zz` without reading `$u` or running `g()`. The`` |
|         - | 3469 | `			 * screen runs after this argument's own expression (a subscript or a call in` |
|         - | 3470 | `			 * it has already run in php too) and is handed the call's map once the list` |
|         - | 3471 | ``			 * is complete. A plain `$var` operand is a deferred load here, so its`` |
|         - | 3472 | ``			 * `Undefined variable` is still unspoken when the screen throws. A `new`'s`` |
|         - | 3473 | `			 * list asks the constructor of the class its screen pass left below. */` |
|      1541 | 3474 | `			aNamedSend[nNamedSend++] = PH7_VmInstrLength(pGen->pVm);` |
|      2416 | 3475 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NAMED_SEND,n,(bAnySpread ? PH7_ROT_SPREAD : 0)` |
|      1536 | 3476 | `				\| (pArgs->bNewCallee ? PH7_ROT_NEW : 0)` |
|      1536 | 3477 | `				\| ((n < nLastRunner && (iArgFlags & EXPR_FLAG_DEFER_ARG)) ? PH7_ROT_READ : 0),0,0);` |
|       768 | 3478 | `		}` |
|   1707800 | 3479 | `		if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - | 3480 | `			/* Emit spread opcode to unpack this array argument. iP1 marks a` |
|         - | 3481 | ``			 * source php will unpack BY REFERENCE: only a plain `$var` (php`` |
|         - | 3482 | ``			 * fetches every other shape — `$a[0]`, `$o->p`, `C::$s`, a cast, a`` |
|         - | 3483 | `			 * call — as an R-value, so a by-ref parameter binds its elements in` |
|         - | 3484 | `			 * a temporary and the write-back is invisible). The expander needs` |
|         - | 3485 | `			 * the distinction because it carries each element's slot for the` |
|         - | 3486 | ``			 * by-ref binder; without it `r(...$a[0])` wrote through to the real`` |
|         - | 3487 | `			 * element, which php leaves alone. */` |
|       795 | 3488 | `			PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD,` |
|       561 | 3489 | `				(apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable) ? 1 : 0,` |
|         - | 3490 | `				0, 0, 0);` |
|       566 | 3491 | `			hasSpread = 1;` |
|       280 | 3492 | `		}` |
|    852182 | 3493 | `	}` |
|         - | 3494 | `	/* Total number of given arguments */` |
|   1204498 | 3495 | `	iP1 = nArgs;` |
|   1204498 | 3496 | `	iP2 = (hasSpread ? PH7_CALL_SPREAD : 0) \| (bConstruct ? PH7_CALL_CONSTRUCT : 0);` |
|         - | 3497 | `	/* Build VmCallArgMap if named arguments are present.` |
|         - | 3498 | `	 * Deep-copy name strings so they survive token stream cleanup. */` |
|   1204498 | 3499 | `	if( hasNamed ){` |
|      1023 | 3500 | `		sxu32 nStrBytes = 0;` |
|         - | 3501 | `		char *zBuf;` |
|      2967 | 3502 | `		for( n = 0; n < nArgs; ++n ){` |
|      1949 | 3503 | `			if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      1541 | 3504 | `				nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|       768 | 3505 | `			}` |
|       977 | 3506 | `		}` |
|         - | 3507 | `		{` |
|      1023 | 3508 | `		sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|      1023 | 3509 | `		VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      1018 | 3510 | `			&pGen->pVm->sAllocator, mapSize);` |
|      1023 | 3511 | `		if( pMap ){` |
|      1023 | 3512 | `			SyZero(pMap, mapSize);` |
|         - | 3513 | `			/* The names need their own contiguous allocation, so this map REPLACES` |
|         - | 3514 | `			 * whatever the callee's namespace qualification built -- and it has to` |
|         - | 3515 | `			 * carry that map's findings across. Dropping them lost the ORIGINAL` |
|         - | 3516 | ``			 * name literal, which is the only thing the `new` codegen can`` |
|         - | 3517 | ``			 * re-qualify with CLASS imports: `new Imported(x: 1)` then resolved`` |
|         - | 3518 | `			 * against the current namespace and the class was not found, while` |
|         - | 3519 | ``			 * the same `new` with positional arguments worked. */`` |
|      1023 | 3520 | `			if( p3 ){` |
|        71 | 3521 | `				VmCallArgMap *pPrior = (VmCallArgMap *)p3;` |
|        71 | 3522 | `				pMap->nOrigNameLit = pPrior->nOrigNameLit;` |
|        71 | 3523 | `				pMap->bIsNamespaced = pPrior->bIsNamespaced;` |
|        71 | 3524 | `				pMap->bNotFrameless = pPrior->bNotFrameless;` |
|        71 | 3525 | `				pMap->nNewClassInstr = pPrior->nNewClassInstr;` |
|        71 | 3526 | `				pMap->bStrict = pPrior->bStrict;` |
|         - | 3527 | `				/* Nothing else holds it: it is attached to no instruction yet. */` |
|        71 | 3528 | `				SyMemBackendFree(&pGen->pVm->sAllocator,pPrior);` |
|        34 | 3529 | `			}` |
|      1023 | 3530 | `			pMap->bHasNamed = 1;` |
|      1023 | 3531 | `			pMap->nTotal = (sxu32)nArgs;` |
|      1023 | 3532 | `			pMap->aNames = (SyString *)&pMap[1];` |
|      1023 | 3533 | `			zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|      2967 | 3534 | `			for( n = 0; n < nArgs; ++n ){` |
|      1949 | 3535 | `				if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      1541 | 3536 | `					sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|      1541 | 3537 | `					SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|      1541 | 3538 | `					SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|      1541 | 3539 | `					zBuf += nb;` |
|       768 | 3540 | `				}` |
|         - | 3541 | `				/* else: aNames[n] remains {NULL, 0} for positional */` |
|       977 | 3542 | `			}` |
|      1023 | 3543 | `			p3 = (void *)pMap;` |
|      2559 | 3544 | `			for( n = 0 ; n < (sxi32)nNamedSend ; ++n ){` |
|      1541 | 3545 | `				VmInstr *pSend = PH7_VmGetInstr(pGen->pVm,aNamedSend[n]);` |
|      1541 | 3546 | `				if( pSend ){` |
|      1541 | 3547 | `					pSend->p3 = (void *)pMap;` |
|       768 | 3548 | `				}` |
|       773 | 3549 | `			}` |
|       509 | 3550 | `		}` |
|         - | 3551 | `		}` |
|       509 | 3552 | `	}` |
|         - | 3553 | `	/* assert(): php's compiler keeps a copy of the assertion's AST and a` |
|         - | 3554 | ``	 * failing assert reports its rendered SOURCE (`assert(1 == 2)`), not the`` |
|         - | 3555 | `	 * evaluated value. Render the first argument's token span` |
|         - | 3556 | `	 * into the call map so vm_builtin_assert can echo it. Only a DIRECT` |
|         - | 3557 | `	 * unqualified/absolute call qualifies — matching php, an indirect call` |
|         - | 3558 | `	 * (call_user_func, a callable variable) has no source text and its` |
|         - | 3559 | `	 * AssertionError carries an empty message. A spread first argument is` |
|         - | 3560 | `	 * skipped (its span is the unpacked array, not the assertion). */` |
|   1204493 | 3561 | `	if( nArgs >= 1 && !bFcc` |
|   1158296 | 3562 | `	 && (apNode[0]->iFlags & EXPR_NODE_SPREAD) == 0 ){` |
|         - | 3563 | `		SyString sCallee;` |
|   1157886 | 3564 | `		GenStateCallBuiltinName(pNode->pLeft,&sCallee);` |
|   1157881 | 3565 | `		if( sCallee.nByte == sizeof("assert")-1` |
|    761276 | 3566 | `		 && SyStrnicmp(sCallee.zString,"assert",sizeof("assert")-1) == 0 ){` |
|         - | 3567 | `			/* An operator root's pStart/pEnd name only the operator token` |
|         - | 3568 | ``			 * (`1 == 2` roots at `==`); the subtree walk recovers the whole`` |
|         - | 3569 | `			 * raw extent, re-adding parens the grouping pass consumed. */` |
|        67 | 3570 | `			SyToken *pSpanIn = 0;` |
|        67 | 3571 | `			SyToken *pSpanEnd = 0;` |
|         - | 3572 | `			SyBlob sSrc;` |
|        67 | 3573 | `			PH7_ExprSubtreeSpan(apNode[0],&pSpanIn,&pSpanEnd);` |
|        67 | 3574 | `			SyBlobInit(&sSrc,&pGen->pVm->sAllocator);` |
|        67 | 3575 | `			if( pSpanIn && pSpanEnd && pSpanIn < pSpanEnd ){` |
|        67 | 3576 | `				if( apNode[0]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|         - | 3577 | ``					/* php renders the name too: `assert(assertion: 1 == 2)`. */`` |
|         3 | 3578 | `					SyBlobAppend(&sSrc,apNode[0]->sArgName.zString,apNode[0]->sArgName.nByte);` |
|         3 | 3579 | `					SyBlobAppend(&sSrc,": ",2);` |
|         1 | 3580 | `				}` |
|        67 | 3581 | `				PH7_GenRenderAssertSpan(pGen,pSpanIn,pSpanEnd,&sSrc);` |
|        31 | 3582 | `			}` |
|        67 | 3583 | `			if( SyBlobLength(&sSrc) > 0 ){` |
|        98 | 3584 | `				char *zDup = (char *)SyMemBackendDup(&pGen->pVm->sAllocator,` |
|        62 | 3585 | `					SyBlobData(&sSrc),SyBlobLength(&sSrc));` |
|        67 | 3586 | `				if( zDup ){` |
|        67 | 3587 | `					if( p3 == 0 ){` |
|        65 | 3588 | `						VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        60 | 3589 | `							&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        65 | 3590 | `						if( pMap ){` |
|        65 | 3591 | `							SyZero(pMap,sizeof(VmCallArgMap));` |
|        65 | 3592 | `							p3 = (void *)pMap;` |
|        30 | 3593 | `						}` |
|        30 | 3594 | `					}` |
|        67 | 3595 | `					if( p3 ){` |
|        67 | 3596 | `						SyStringInitFromBuf(&((VmCallArgMap *)p3)->sAssertSrc,` |
|         - | 3597 | `							zDup,SyBlobLength(&sSrc));` |
|        31 | 3598 | `					}` |
|        31 | 3599 | `				}` |
|        31 | 3600 | `			}` |
|        67 | 3601 | `			SyBlobRelease(&sSrc);` |
|        31 | 3602 | `		}` |
|    577857 | 3603 | `	}` |
|         - | 3604 | `	/* Record each argument's compile-time SHAPE so the by-ref binders can` |
|         - | 3605 | `	 * refuse a non-variable where php refuses it — at the CALL, before the` |
|         - | 3606 | `	 * callee's ZPP runs. Skipped when the call SPREADS (one compile-time` |
|         - | 3607 | `	 * argument becomes N runtime slots, so the positions no longer line up)` |
|         - | 3608 | `	 * or when it carries more arguments than the masks can hold; a call` |
|         - | 3609 | `	 * without the flag keeps the old runtime nIdx test. Named arguments are` |
|         - | 3610 | `	 * fine: they change which FORMAL a slot binds to, not the slot's index. */` |
|   1204498 | 3611 | `	if( !bAnySpread && nArgs > 0 && nArgs <= 31 && !bFcc ){` |
|   1157809 | 3612 | `		sxu32 nNonLval = 0;` |
|   1157809 | 3613 | `		sxu32 nTempCall = 0;` |
|   1157809 | 3614 | `		sxu32 nConstStr = 0;` |
|   2864914 | 3615 | `		for( n = 0 ; n < nArgs ; ++n ){` |
|   1707110 | 3616 | `			int iShape = GenStateArgShape(apNode[n]);` |
|   1707110 | 3617 | `			if( GenStateArgIsConstString(apNode[n]) ){` |
|    262662 | 3618 | `				nConstStr \|= (1u << n);` |
|    130908 | 3619 | `			}` |
|   1707110 | 3620 | `			if( iShape == GEN_ARG_NONE ){` |
|    719427 | 3621 | `				nNonLval \|= (1u << n);` |
|   1346432 | 3622 | `			}else if( iShape == GEN_ARG_TEMPCALL ){` |
|     93991 | 3623 | `				nTempCall \|= (1u << n);` |
|     46887 | 3624 | `			}` |
|    851838 | 3625 | `		}` |
|   1157809 | 3626 | `		if( p3 == 0 ){` |
|   1156095 | 3627 | `			VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|   1156090 | 3628 | `				&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|   1156095 | 3629 | `			if( pMap ){` |
|   1156095 | 3630 | `				SyZero(pMap,sizeof(VmCallArgMap));` |
|   1156095 | 3631 | `				p3 = (void *)pMap;` |
|    576962 | 3632 | `			}` |
|    576962 | 3633 | `		}` |
|   1157809 | 3634 | `		if( p3 ){` |
|   1157809 | 3635 | `			((VmCallArgMap *)p3)->bArgShapes = 1;` |
|   1157809 | 3636 | `			((VmCallArgMap *)p3)->nNonLvalMask = nNonLval;` |
|   1157809 | 3637 | `			((VmCallArgMap *)p3)->nTempCallMask = nTempCall;` |
|   1157809 | 3638 | `			((VmCallArgMap *)p3)->nConstStrMask = nConstStr;` |
|    577819 | 3639 | `		}` |
|    577819 | 3640 | `	}` |
|   1320461 | 3641 | `	for( n = 0 ; n < (sxi32)nReadSend ; ++n ){` |
|         - | 3642 | `		/* The map the call ended with: the global fallback of a name written in a` |
|         - | 3643 | `		 * namespace is read off it. */` |
|    115968 | 3644 | `		VmInstr *pRead = PH7_VmGetInstr(pGen->pVm,aReadSend[n]);` |
|    115968 | 3645 | `		if( pRead ){` |
|    115968 | 3646 | `			pRead->p3 = p3;` |
|     57896 | 3647 | `		}` |
|     57901 | 3648 | `	}` |
|   1204498 | 3649 | `	pArgs->iP1 = iP1;` |
|   1204498 | 3650 | `	pArgs->iP2 = iP2;` |
|   1204498 | 3651 | `	pArgs->p3  = p3;` |
|   1204498 | 3652 | `	pArgs->bFcc = bFcc;` |
|   1204498 | 3653 | `	pArgs->bAnySpread = bAnySpread;` |
|   1204498 | 3654 | `	return SXRET_OK;` |
|    601094 | 3655 | `}` |
|         - | 3656 | `/*` |
|         - | 3657 | ` * Compile a PHP expression.` |
|         - | 3658 | ` * According to the PHP language reference manual:` |
|         - | 3659 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 3660 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 3661 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 3662 | ` *  is "anything that has a value".` |
|         - | 3663 | ` * If something goes wrong while compiling the expression,this` |
|         - | 3664 | ` * function takes care of generating the appropriate error` |
|         - | 3665 | ` * message.` |
|         - | 3666 | ` */` |
|         - | 3667 | `/*` |
|         - | 3668 | ` * Does this expression tree contain a comma OPERATOR node?` |
|         - | 3669 | ` *` |
|         - | 3670 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|         - | 3671 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|         - | 3672 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|         - | 3673 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|         - | 3674 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|         - | 3675 | ` * meaning of source php rejects, which the scope policy classes as a bug — so every context` |
|         - | 3676 | ` * except for() now reports php's parse error.` |
|         - | 3677 | ` */` |
|  40307140 | 3678 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 3679 | `{` |
|         - | 3680 | `	ph7_expr_node **apArg;` |
|         - | 3681 | `	sxu32 n;` |
|  40307145 | 3682 | `	if( pNode == 0 ){` |
|  28446541 | 3683 | `		return 0;` |
|         - | 3684 | `	}` |
|  11860609 | 3685 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         9 | 3686 | `		return 1;` |
|         - | 3687 | `	}` |
|  11860598 | 3688 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  11860599 | 3689 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 3690 | `		return 1;` |
|         - | 3691 | `	}` |
|  11860599 | 3692 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  13832410 | 3693 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|   1971816 | 3694 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 3695 | `			return 1;` |
|         - | 3696 | `		}` |
|    984016 | 3697 | `	}` |
|  11860599 | 3698 | `	return 0;` |
|  20121795 | 3699 | `}` |
|   2908230 | 3700 | `PH7_PRIVATE sxi32 PH7_CompileExpr(` |
|         - | 3701 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 3702 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 3703 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 3704 | `	)` |
|         5 | 3705 | `{` |
|         - | 3706 | `	ph7_expr_node *pRoot;` |
|         - | 3707 | `	SySet sExprNode;` |
|         - | 3708 | `	SyToken *pEnd;` |
|         - | 3709 | `	sxi32 nExpr;` |
|         - | 3710 | `	sxi32 iNest;` |
|         - | 3711 | `	sxi32 rc;` |
|         - | 3712 | `	sxu32 nNullsafeBase;` |
|         - | 3713 | `	/* Initialize worker variables */` |
|   2908235 | 3714 | `	nExpr = 0;` |
|   2908235 | 3715 | `	pRoot = 0;` |
|         - | 3716 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 3717 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|   2908235 | 3718 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2908235 | 3719 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|   2908235 | 3720 | `	SySetAlloc(&sExprNode,0x10);` |
|   2908235 | 3721 | `	rc = SXRET_OK;` |
|         - | 3722 | `	/* Delimit the expression */` |
|   2908235 | 3723 | `	pEnd = pGen->pIn;` |
|   2908235 | 3724 | `	iNest = 0;` |
|  22741577 | 3725 | `	while( pEnd < pGen->pEnd ){` |
|  21474781 | 3726 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 3727 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     11042 | 3728 | `			iNest++;` |
|  21469244 | 3729 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     11092 | 3730 | `			iNest--;` |
|  21458182 | 3731 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|   1660493 | 3732 | `			if( iNest <= 0 ){` |
|   1641439 | 3733 | `				break;` |
|         - | 3734 | `			}` |
|      9486 | 3735 | `		}` |
|  19833347 | 3736 | `		pEnd++;` |
|         5 | 3737 | `	}` |
|   2908235 | 3738 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|      3837 | 3739 | `		SyToken *pEnd2 = pGen->pIn;` |
|      3837 | 3740 | `		iNest = 0;` |
|         - | 3741 | `		/* Stop at the first comma */` |
|     22949 | 3742 | `		while( pEnd2 < pEnd ){` |
|     19151 | 3743 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|       545 | 3744 | `				iNest++;` |
|     18881 | 3745 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|       545 | 3746 | `				iNest--;` |
|     18341 | 3747 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      6293 | 3748 | `				if( iNest <= 0 ){` |
|        38 | 3749 | `					break;` |
|         - | 3750 | `				}` |
|      3127 | 3751 | `			}` |
|     19117 | 3752 | `			pEnd2++;` |
|         5 | 3753 | `		}` |
|      3837 | 3754 | `		if( pEnd2 <pEnd ){` |
|        38 | 3755 | `			pEnd = pEnd2;` |
|        17 | 3756 | `		}` |
|      1916 | 3757 | `	}` |
|   2908235 | 3758 | `	if( pEnd > pGen->pIn ){` |
|   2908117 | 3759 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 3760 | `		/* Swap delimiter */` |
|   2908117 | 3761 | `		pGen->pEnd = pEnd;` |
|         - | 3762 | `		/* Try to get an expression tree */` |
|   2908117 | 3763 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|   2908112 | 3764 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|   2830481 | 3765 | `		 && GenStateTreeHasComma(pRoot) ){` |
|         - | 3766 | `			/* php has no comma operator outside a for() clause */` |
|         9 | 3767 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|         - | 3768 | `				"syntax error, unexpected token \",\"");` |
|         9 | 3769 | `			pGen->pEnd = pTmp;` |
|         - | 3770 | `			/* This refusal leaves by its own door, so it owes the release the` |
|         - | 3771 | `			 * ordinary path makes below -- the set owns every node the` |
|         - | 3772 | `			 * expression produced, and a bare SySetRelease drops the pointers` |
|         - | 3773 | `			 * without freeing what they point at. */` |
|         9 | 3774 | `			PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         9 | 3775 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 3776 | `				SySetRelease(&sExprNode);` |
|       ! 0 | 3777 | `				return SXERR_ABORT;` |
|         - | 3778 | `			}` |
|         9 | 3779 | `			pGen->pIn = pEnd;` |
|         9 | 3780 | `			SySetRelease(&sExprNode);` |
|         9 | 3781 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|         9 | 3782 | `			return SXRET_OK;` |
|         - | 3783 | `		}` |
|   2908111 | 3784 | `		if( rc == SXRET_OK && pRoot ){` |
|   2907615 | 3785 | `			rc = SXRET_OK;` |
|   2907615 | 3786 | `			if( xTreeValidator ){` |
|         - | 3787 | `				/* Call the upper layer validator callback */` |
|    228705 | 3788 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    114191 | 3789 | `			}` |
|   2907615 | 3790 | `			if( rc != SXERR_ABORT ){` |
|         - | 3791 | `				/* Generate code for the given tree */` |
|   2907615 | 3792 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 3793 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 3794 | `				 * expression so they short-circuit to its end. */` |
|   2907615 | 3795 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   1451497 | 3796 | `			}` |
|   2907615 | 3797 | `			nExpr = 1;` |
|   1451497 | 3798 | `		}` |
|         - | 3799 | `		/* Release the whole tree */` |
|   2908111 | 3800 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 3801 | `		/* Synchronize token stream */` |
|   2908111 | 3802 | `		pGen->pEnd = pTmp;` |
|   2908111 | 3803 | `		pGen->pIn  = pEnd;` |
|   2908111 | 3804 | `		if( rc == SXERR_ABORT ){` |
|        61 | 3805 | `			SySetRelease(&sExprNode);` |
|        61 | 3806 | `			return SXERR_ABORT;` |
|         - | 3807 | `		}` |
|   1451717 | 3808 | `	}` |
|   2908173 | 3809 | `	SySetRelease(&sExprNode);` |
|   2908173 | 3810 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   1451812 | 3811 | `}` |
|         - | 3812 | `/*` |
|         - | 3813 | ` * Return a pointer to the node construct handler associated` |
|         - | 3814 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 3815 | ` */` |
|   1880036 | 3816 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 3817 | `{` |
|   1880041 | 3818 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 3819 | `		/* Numeric literal: Either real or integer */` |
|   1005266 | 3820 | `		return PH7_CompileNumLiteral;` |
|    874780 | 3821 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 3822 | `		/* Double quoted string */` |
|     88315 | 3823 | `		return PH7_CompileString;` |
|    786470 | 3824 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 3825 | `		/* Single quoted string */` |
|    786286 | 3826 | `		return PH7_CompileSimpleString;` |
|       189 | 3827 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 3828 | `		/* Heredoc */` |
|        91 | 3829 | `		return PH7_CompileHereDoc;` |
|       103 | 3830 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 3831 | `		/* Nowdoc */` |
|        69 | 3832 | `		return PH7_CompileNowDoc;` |
|        37 | 3833 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 3834 | `		/* Backtick quoted string */` |
|         3 | 3835 | `		return PH7_CompileBacktic;` |
|         - | 3836 | `	}` |
|        34 | 3837 | `	return 0;` |
|    938446 | 3838 | `}` |
|         - | 3839 | `/*` |
|         - | 3840 | ` * Tree validator for unset() arguments — php's write-target rules, then its` |
|         - | 3841 | ``  * "Can't use nullsafe operator in write context", then the grammar: `unset()` `` |
|         - | 3842 | `` * takes a `variable`, so `unset(GK)`, `unset("s")` and `unset(A::K)` are php`` |
|         - | 3843 | ` * PARSE errors where PHL let them reach the VM and answer with a PH7-ism.` |
|         - | 3844 | ` */` |
|       478 | 3845 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 3846 | `{` |
|         - | 3847 | `	sxi32 rc;` |
|       483 | 3848 | `	rc = GenStateWriteTargetCheck(&(*pGen),pNode,PH7_WTC_UNSET);` |
|       483 | 3849 | `	if( rc != SXRET_OK ){` |
|         5 | 3850 | `		return rc;` |
|         - | 3851 | `	}` |
|       479 | 3852 | `	if( PH7_ExprContainsNullsafe(pNode) ){` |
|       ! 0 | 3853 | `		rc = PH7_GenCompileError(pGen,E_ERROR,` |
|       ! 0 | 3854 | `			pNode ? pNode->pStart->nLine : 1,` |
|         - | 3855 | `			"Can't use nullsafe operator in write context");` |
|       ! 0 | 3856 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 3857 | `	}` |
|       479 | 3858 | `	if( pNode && PH7_ExprIsModifiableValue(pNode) == FALSE ){` |
|         6 | 3859 | `		return PH7_ExprOperandNotAVariable(pGen,pNode);` |
|         - | 3860 | `	}` |
|       475 | 3861 | `	return SXRET_OK;` |
|       244 | 3862 | `}` |
|         - | 3863 | `/*` |
|         - | 3864 | ` * Compile an unset() statement.` |
|         - | 3865 | ` * unset($var, $arr[$key], ...);` |
|         - | 3866 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 3867 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 3868 | ` * parent array before extracting the element to unset.` |
|         - | 3869 | ` */` |
|      3710 | 3870 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 3871 | `{` |
|      3715 | 3872 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|      3715 | 3873 | `	sxu32 nIdx = 0;` |
|         - | 3874 | `	SyString sName;` |
|         - | 3875 | `	sxi32 rc;` |
|         - | 3876 | `	/* Jump the 'unset' keyword */` |
|      3715 | 3877 | `	pGen->pIn++;` |
|         - | 3878 | `	/* Save delimiter */` |
|      3715 | 3879 | `	pTmp = pGen->pEnd;` |
|         - | 3880 | `	/* Skip optional opening parenthesis and find the matching close */` |
|      3715 | 3881 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|      3715 | 3882 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 3883 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 3884 | `		SyToken *pClose;` |
|      3715 | 3885 | `		pGen->pIn++;   /* Skip '(' */` |
|      3715 | 3886 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|      3715 | 3887 | `		pEnd = pClose; /* Stop at ')' */` |
|      1853 | 3888 | `	}` |
|      3715 | 3889 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 3890 | `	/* Resolve the 'unset' builtin name once */` |
|      3715 | 3891 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|       636 | 3892 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       636 | 3893 | `		if( pObj == 0 ){` |
|       ! 0 | 3894 | `			return SXERR_ABORT;` |
|         - | 3895 | `		}` |
|       636 | 3896 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       636 | 3897 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       314 | 3898 | `	}` |
|         - | 3899 | `	/* Compile each comma-separated argument */` |
|     12836 | 3900 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|      9130 | 3901 | `		if( pGen->pIn < pNext ){` |
|         - | 3902 | `			/*` |
|         - | 3903 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|         - | 3904 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|         - | 3905 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|         - | 3906 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|         - | 3907 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|         - | 3908 | `			 * already removes just the element/property.` |
|         - | 3909 | `			 */` |
|      9125 | 3910 | `			if( &pGen->pIn[2] == pNext` |
|      8886 | 3911 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
|      8652 | 3912 | `				&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         - | 3913 | `				SyString *pVarName;` |
|         - | 3914 | ``				/* php refuses `unset($this)` where it is written. The tree validator`` |
|         - | 3915 | `				 * cannot see it — this fast path never builds a tree. */` |
|      8645 | 3916 | `				if( pGen->pIn[1].sData.nByte == sizeof("this")-1` |
|      4641 | 3917 | `				 && SyMemcmp((const void *)pGen->pIn[1].sData.zString,` |
|       320 | 3918 | `				             (const void *)"this",sizeof("this")-1) == 0 ){` |
|         6 | 3919 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - | 3920 | `						"Cannot unset $this");` |
|         6 | 3921 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 3922 | `						return SXERR_ABORT;` |
|         - | 3923 | `					}` |
|         - | 3924 | `					/* php stops compiling at its own fatal; this generator carries` |
|         - | 3925 | `					 * on to a budget of fifteen, so leave the cursor PAST the whole` |
|         - | 3926 | ``					 * `unset(...)` rather than on the operand it refused. Resuming`` |
|         - | 3927 | `					 * there re-read the closing ')' as a statement of its own and` |
|         - | 3928 | `					 * printed an "Unmatched ')'" under the fatal that php never` |
|         - | 3929 | `					 * reaches. */` |
|         6 | 3930 | `					pGen->pIn = pEnd;` |
|         6 | 3931 | `					if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|         6 | 3932 | `						pGen->pIn++;` |
|         2 | 3933 | `					}` |
|         6 | 3934 | `					pGen->pEnd = pTmp;` |
|         6 | 3935 | `					return SXERR_SYNTAX;` |
|         - | 3936 | `				}` |
|     12960 | 3937 | `				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      8641 | 3938 | `					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|      8646 | 3939 | `				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|      8646 | 3940 | `				if( zDup == 0 \|\| pVarName == 0 ){` |
|       ! 0 | 3941 | `					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - | 3942 | `						"Fatal, PH7 is running out of memory");` |
|       ! 0 | 3943 | `					return SXERR_ABORT;` |
|         - | 3944 | `				}` |
|      8646 | 3945 | `				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);` |
|      8646 | 3946 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);` |
|      8646 | 3947 | `				pGen->pIn = pNext;` |
|      8646 | 3948 | `				if( pGen->pIn < pEnd ){` |
|      5376 | 3949 | `					pGen->pIn++; /* Jump the trailing comma */` |
|      2681 | 3950 | `				}` |
|      8646 | 3951 | `				continue;` |
|         - | 3952 | `			}` |
|       485 | 3953 | `			pGen->pEnd = pNext;` |
|       485 | 3954 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 3955 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 3956 | `				GenStateUnsetValidator);` |
|       485 | 3957 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 3958 | `				return SXERR_ABORT;` |
|         - | 3959 | `			}` |
|       485 | 3960 | `			if( rc != SXERR_EMPTY ){` |
|         - | 3961 | ``				/* Emit call for this single argument. PH7_CALL_CONSTRUCT: `unset` is a`` |
|         - | 3962 | `				 * language construct, so the host function this dispatches to is hidden` |
|         - | 3963 | `				 * from every name a script can spell (PH7_VmGetHostFunction). */` |
|       483 | 3964 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       722 | 3965 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,PH7_CALL_CONSTRUCT,` |
|       239 | 3966 | `					GenStateAttachStrictFlag(pGen,0),0);` |
|       483 | 3967 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       239 | 3968 | `			}` |
|       240 | 3969 | `		}` |
|         - | 3970 | `		/* Jump trailing commas */` |
|       531 | 3971 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|        50 | 3972 | `			pNext++;` |
|         4 | 3973 | `		}` |
|       485 | 3974 | `		pGen->pIn = pNext;` |
|         5 | 3975 | `	}` |
|         - | 3976 | `	/* Skip past the closing ')' if present */` |
|      3711 | 3977 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|      3711 | 3978 | `		pGen->pIn++;` |
|      1851 | 3979 | `	}` |
|         - | 3980 | `	/* Restore token stream */` |
|      3711 | 3981 | `	pGen->pEnd = pTmp;` |
|      3711 | 3982 | `	return SXRET_OK;` |
|      1858 | 3983 | `}` |
|         - | 3984 | `/*` |
|         - | 3985 | ` * PHP Language construct table.` |
|         - | 3986 | ` */` |
|         - | 3987 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 3988 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 3989 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 3990 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 3991 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 3992 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 3993 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 3994 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 3995 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 3996 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 3997 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 3998 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 3999 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 4000 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 4001 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 4002 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 4003 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 4004 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 4005 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 4006 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 4007 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 4008 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 4009 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 4010 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 4011 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 4012 | `};` |
|         - | 4013 | `/*` |
|         - | 4014 | ` * Return a pointer to the statement handler routine associated` |
|         - | 4015 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 4016 | ` */` |
|   1514930 | 4017 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 4018 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 4019 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 4020 | `	)` |
|         5 | 4021 | `{` |
|   1514935 | 4022 | `	sxu32 n = 0;` |
|   4489309 | 4023 | `	for(;;){` |
|   8990180 | 4024 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|      8187 | 4025 | `			break;` |
|         - | 4026 | `		}` |
|   8981998 | 4027 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   1506753 | 4028 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|         5 | 4029 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|         5 | 4030 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 4031 | `					/* 'static' (class context),return null */` |
|         5 | 4032 | `					return 0;` |
|         - | 4033 | `				}` |
|       ! 0 | 4034 | `			}` |
|   1506744 | 4035 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|        86 | 4036 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|        52 | 4037 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 4038 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         5 | 4039 | `				return 0;` |
|         - | 4040 | `			}` |
|         - | 4041 | `			/* Return a pointer to the handler.` |
|         - | 4042 | `			*/` |
|   1506745 | 4043 | `			return aLangConstruct[n].xConstruct;` |
|         - | 4044 | `		}` |
|   7475250 | 4045 | `		n++;` |
|         5 | 4046 | `	}` |
|      8187 | 4047 | `	if( pLookahed ){` |
|      8187 | 4048 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && PH7_IsClassNameToken(pLookahed) ){` |
|       445 | 4049 | `			return PH7_CompileClassInterface;` |
|      7747 | 4050 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && PH7_IsClassNameToken(pLookahed) ){` |
|      5831 | 4051 | `			return PH7_CompileClass;` |
|      1921 | 4052 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && PH7_IsClassNameToken(pLookahed) ){` |
|       439 | 4053 | `			return PH7_CompileTrait;` |
|         - | 4054 | `		}` |
|         - | 4055 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 4056 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 4057 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 4058 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|       741 | 4059 | `	}` |
|         - | 4060 | `	/* Not a language construct */` |
|      1487 | 4061 | `	return 0;` |
|    756463 | 4062 | `}` |
|         - | 4063 | `/*` |
|         - | 4064 | ` * Which words may NAME a class, an interface, a trait or an enum, and which may` |
|         - | 4065 | ` * not — php has two separate answers and this file used to have one.` |
|         - | 4066 | ` *` |
|         - | 4067 | ` * php's SCANNER decides the first: a reserved keyword is not an identifier, so` |
|         - | 4068 | `` * `class list {}` and `class callable {}` are parse errors. Its COMPILER decides`` |
|         - | 4069 | ` * the second, for a handful of words the scanner does hand over as identifiers:` |
|         - | 4070 | ``  * `zend_is_reserved_class_name` refuses `int`, `bool`, `void`, `null`, `self` `` |
|         - | 4071 | `` * and their neighbours with `Cannot use "X" as a class name as it is reserved`.`` |
|         - | 4072 | ` *` |
|         - | 4073 | ` * PHL's keyword set is not php's, which is where the divergence came from in both` |
|         - | 4074 | `` * directions. `integer` and `boolean` are CAST words here and identifiers in php,`` |
|         - | 4075 | `` * so `class Integer extends Base {}` — phpseclib writes exactly that, three times`` |
|         - | 4076 | ``  * — did not compile at all. And `void`, `never`, `null`, `false`, `true`, `mixed` `` |
|         - | 4077 | `` * and `iterable` arrive as plain identifiers here, so declaring a class with one`` |
|         - | 4078 | ` * of those names SUCCEEDED where php refuses.` |
|         - | 4079 | ` */` |
|    133582 | 4080 | `static int GenStateNameIs(const SyString *pName,const char *zWord)` |
|         5 | 4081 | `{` |
|    133587 | 4082 | `	sxu32 n = (sxu32)SyStrlen(zWord);` |
|         - | 4083 | `	/* Length FIRST: the token's bytes point into the source and are not` |
|         - | 4084 | `	 * NUL-terminated, so a shorter name must never be compared over its end. */` |
|    133587 | 4085 | `	return pName->nByte == n && SyStrnicmp(pName->zString,zWord,n) == 0;` |
|         5 | 4086 | `}` |
|        24 | 4087 | `static int GenStateClassNameKeywordOk(const SyString *pName)` |
|         1 | 4088 | `{` |
|         - | 4089 | `	/* The only two words PHL lexes as keywords that php lets name a class. */` |
|        25 | 4090 | `	return GenStateNameIs(pName,"integer") \|\| GenStateNameIs(pName,"boolean");` |
|         1 | 4091 | `}` |
|     19976 | 4092 | `PH7_PRIVATE int PH7_IsClassNameToken(const SyToken *pTok)` |
|         5 | 4093 | `{` |
|     19981 | 4094 | `	if( pTok == 0 ){` |
|       ! 0 | 4095 | `		return 0;` |
|         - | 4096 | `	}` |
|     19981 | 4097 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|        25 | 4098 | `		return GenStateClassNameKeywordOk(&pTok->sData);` |
|         - | 4099 | `	}` |
|     19957 | 4100 | `	if( (pTok->nType & PH7_TK_ID) == 0 ){` |
|         5 | 4101 | `		return 0;` |
|         - | 4102 | `	}` |
|     19953 | 4103 | `	if( pTok->nType & PH7_TK_OP ){` |
|         - | 4104 | ``		/* php's alpha-stream operators — `and`, `or`, `xor`, `new`, `clone`,`` |
|         - | 4105 | ``		 * `instanceof` — are keywords in its scanner and cannot be identifiers.`` |
|         - | 4106 | `		 * They reach here carrying both flags, and were accepted as names. */` |
|       ! 0 | 4107 | `		return 0;` |
|         - | 4108 | `	}` |
|         - | 4109 | `	{` |
|         - | 4110 | `		/* Two more php keywords that PHL treats as context-sensitive identifiers. */` |
|         - | 4111 | `		static const char *const azNo[] = { "callable", "readonly" };` |
|         - | 4112 | `		sxu32 i;` |
|     59849 | 4113 | `		for( i = 0 ; i < SX_ARRAYSIZE(azNo) ; ++i ){` |
|     39901 | 4114 | `			if( GenStateNameIs(&pTok->sData,azNo[i]) ){` |
|       ! 0 | 4115 | `				return 0;` |
|         - | 4116 | `			}` |
|     19953 | 4117 | `		}` |
|         - | 4118 | `	}` |
|     19953 | 4119 | `	return 1;` |
|      9993 | 4120 | `}` |
|         - | 4121 | `/*` |
|         - | 4122 | ` * php's zend_is_reserved_class_name: a word its scanner DOES hand over as an` |
|         - | 4123 | ` * identifier but its compiler refuses to name a class with. The check is` |
|         - | 4124 | ` * case-insensitive and the refusal quotes the name as WRITTEN.` |
|         - | 4125 | ` */` |
|      6244 | 4126 | `PH7_PRIVATE int PH7_IsReservedClassName(const SyString *pName)` |
|         5 | 4127 | `{` |
|         - | 4128 | `	static const char *const azReserved[] = {` |
|         - | 4129 | `		"bool", "int", "float", "string", "null", "false", "true", "void",` |
|         - | 4130 | `		"never", "iterable", "object", "mixed", "self", "parent", "static"` |
|         - | 4131 | `	};` |
|         - | 4132 | `	sxu32 i;` |
|     99897 | 4133 | `	for( i = 0 ; i < SX_ARRAYSIZE(azReserved) ; ++i ){` |
|     93655 | 4134 | `		if( GenStateNameIs(pName,azReserved[i]) ){` |
|         3 | 4135 | `			return 1;` |
|         - | 4136 | `		}` |
|     46829 | 4137 | `	}` |
|      6247 | 4138 | `	return 0;` |
|      3127 | 4139 | `}` |
|         - | 4140 | `/*` |
|         - | 4141 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 4142 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 4143 | ` */` |
|      1490 | 4144 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 4145 | `{` |
|         - | 4146 | `	int rc;` |
|      1495 | 4147 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|      1495 | 4148 | `	if( rc == FALSE ){` |
|       903 | 4149 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|       752 | 4150 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 4151 | ``			/* `match` is an EXPRESSION, and php takes an expression statement made of`` |
|         - | 4152 | ``			 * one: `match (true) { ... };` is how a dispatch table is written when the`` |
|         - | 4153 | `			 * answer is not wanted. Without this the statement dispatcher refused the` |
|         - | 4154 | `			 * keyword outright, and Doctrine's DQL parser -- which dispatches its tree` |
|         - | 4155 | `			 * walkers exactly that way -- did not compile. */` |
|       425 | 4156 | `			\|\| nKeyword == PH7_TKWRD_MATCH` |
|         - | 4157 | ``			/* php reserves NONE of `int`/`integer`/`bool`/`boolean`/`float`/`` |
|         - | 4158 | ``			 * `string`/`object`: its scanner hands every one of them back as a`` |
|         - | 4159 | `			 * plain T_STRING, and only a TYPE position gives them a meaning. A` |
|         - | 4160 | `			 * statement that begins with one is therefore an ordinary expression` |
|         - | 4161 | ``			 * -- `Integer::setModulo($id, $m);`, which is how phpseclib's`` |
|         - | 4162 | `			 * BinaryField spells the class it imported under that name, and which` |
|         - | 4163 | ``			 * this dispatcher answered `Unexpected keyword 'Integer'` for. The`` |
|         - | 4164 | ``			 * same word after `$x = ` already compiled, so only the STATEMENT head`` |
|         - | 4165 | `			 * was refusing it. */` |
|        97 | 4166 | `			\|\| nKeyword == PH7_TKWRD_INT \|\| nKeyword == PH7_TKWRD_BOOL` |
|        87 | 4167 | `			\|\| nKeyword == PH7_TKWRD_FLOAT \|\| nKeyword == PH7_TKWRD_STRING` |
|        91 | 4168 | `			\|\| nKeyword == PH7_TKWRD_OBJECT` |
|         - | 4169 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 4170 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 4171 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 4172 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 4173 | `			*/` |
|         - | 4174 | `			){` |
|       865 | 4175 | `				rc = TRUE;` |
|       430 | 4176 | `		}` |
|       430 | 4177 | `	}` |
|      1495 | 4178 | `	return rc;` |
|         5 | 4179 | `}` |
|         - | 4180 | `/*` |
|         - | 4181 | ` * TRUE when the statement head is a keyword that opens no statement and starts` |
|         - | 4182 | `` * no expression -- `else`, `endwhile`, `case`, `public` -- which php's parser`` |
|         - | 4183 | ` * refuses before anything else is asked of the statement.` |
|         - | 4184 | ` */` |
|         6 | 4185 | `static int GenStateIsStrayKeyword(ph7_gen_state *pGen)` |
|         2 | 4186 | `{` |
|         - | 4187 | `	sxu32 nKeyword;` |
|         8 | 4188 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 4189 | `		return FALSE;` |
|         - | 4190 | `	}` |
|         8 | 4191 | `	nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         9 | 4192 | `	return GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0) == 0` |
|         6 | 4193 | `		&& GenStateisLangConstruct(nKeyword) == FALSE;` |
|         5 | 4194 | `}` |
|         - | 4195 | `/*` |
|         - | 4196 | ` * Compile a PHP chunk.` |
|         - | 4197 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 4198 | ` * takes care of generating the appropriate error message.` |
|         - | 4199 | ` */` |
|         - | 4200 | `/*` |
|         - | 4201 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 4202 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 4203 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 4204 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 4205 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 4206 | ` * intervening non-declaration statements.` |
|         - | 4207 | ` */` |
|   2591800 | 4208 | `PH7_PRIVATE void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 4209 | `{` |
|   2591805 | 4210 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   2591805 | 4211 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   2591805 | 4212 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 4213 | `	sxu32 nIdx, n;` |
|   2591800 | 4214 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|     15217 | 4215 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 4216 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 4217 | `		 * indexes do not map to the sidecar */` |
|   2576593 | 4218 | `		return;` |
|         - | 4219 | `	}` |
|     15217 | 4220 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 4221 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 4222 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|     15217 | 4223 | `	SySetReset(&pGen->aPendingAttrs);` |
|    117207 | 4224 | `	for( n = 0 ; n < nT ; n++ ){` |
|    101995 | 4225 | `		if( aT[n].nTokIdx != nIdx ){` |
|    100787 | 4226 | `			continue;` |
|         - | 4227 | `		}` |
|      1213 | 4228 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|       501 | 4229 | `			pGen->sPendingDoc = aT[n].sText;` |
|       965 | 4230 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|       717 | 4231 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|       356 | 4232 | `		}` |
|       609 | 4233 | `	}` |
|   1293989 | 4234 | `}` |
|         - | 4235 | `/*` |
|         - | 4236 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 4237 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 4238 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 4239 | ` */` |
|    207738 | 4240 | `PH7_PRIVATE void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 4241 | `{` |
|         - | 4242 | `	char *zDup;` |
|    207743 | 4243 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|    207293 | 4244 | `		return;` |
|         - | 4245 | `	}` |
|       680 | 4246 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       225 | 4247 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|       455 | 4248 | `	if( zDup ){` |
|       455 | 4249 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|       225 | 4250 | `	}` |
|       455 | 4251 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|    103736 | 4252 | `}` |
|         - | 4253 | `/*` |
|         - | 4254 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 4255 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 4256 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 4257 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 4258 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 4259 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 4260 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 4261 | ` */` |
|       776 | 4262 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 4263 | `{` |
|         - | 4264 | `	SySet *pToken;` |
|         - | 4265 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 4266 | `	char *zSpan;` |
|       781 | 4267 | `	sxi32 rc = SXRET_OK;` |
|       781 | 4268 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 4269 | `		return SXRET_OK;` |
|         - | 4270 | `	}` |
|      1169 | 4271 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       388 | 4272 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|       781 | 4273 | `	if( zSpan == 0 ){` |
|       ! 0 | 4274 | `		return SXRET_OK;` |
|         - | 4275 | `	}` |
|         - | 4276 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 4277 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 4278 | `	 * the number of attribute declarations in the program. */` |
|       781 | 4279 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|       781 | 4280 | `	if( pToken == 0 ){` |
|       ! 0 | 4281 | `		return SXRET_OK;` |
|         - | 4282 | `	}` |
|       781 | 4283 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       781 | 4284 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|       781 | 4285 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|       781 | 4286 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|       781 | 4287 | `	pSavedIn = pGen->pIn;` |
|       781 | 4288 | `	pSavedEnd = pGen->pEnd;` |
|       787 | 4289 | `	while( pIn < pEnd ){` |
|         - | 4290 | `		ph7_attribute sAttr;` |
|         - | 4291 | `		SyBlob sFQN;` |
|       787 | 4292 | `		int bAbsolute = 0;` |
|       787 | 4293 | `		SyZero(&sAttr,sizeof(sAttr));` |
|       787 | 4294 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|       787 | 4295 | `		sAttr.nLine = pIn->nLine;` |
|       787 | 4296 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|       575 | 4297 | `			bAbsolute = 1;` |
|       575 | 4298 | `			pIn++;` |
|       285 | 4299 | `		}` |
|       787 | 4300 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|         - | 4301 | ``		/* `#[namespace\Attr]` — the current namespace, absolute from there. */`` |
|       787 | 4302 | `		if( !bAbsolute && GenStateNsRelPrefix(pGen,&pIn,pEnd,&sFQN) ){` |
|       ! 0 | 4303 | `			bAbsolute = 1;` |
|       ! 0 | 4304 | `		}` |
|       807 | 4305 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       807 | 4306 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|       807 | 4307 | `			pIn++;` |
|       807 | 4308 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|        21 | 4309 | `				SyBlobAppend(&sFQN,"\\",1);` |
|        21 | 4310 | `				pIn++;` |
|        21 | 4311 | `				continue;` |
|         - | 4312 | `			}` |
|       787 | 4313 | `			break;` |
|       ! 0 | 4314 | `		}` |
|       787 | 4315 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 4316 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 4317 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 4318 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 4319 | `			break;` |
|         - | 4320 | `		}` |
|         - | 4321 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 4322 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 4323 | `		{` |
|       787 | 4324 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|       787 | 4325 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|       787 | 4326 | `			char *zDup = 0;` |
|       787 | 4327 | `			if( !bAbsolute ){` |
|         - | 4328 | `				/* An attribute name resolves exactly the way every other class name` |
|         - | 4329 | `				 * does -- the LEADING segment through the use imports, else the` |
|         - | 4330 | `				 * current-namespace prefix. This looked the WHOLE qualified string up` |
|         - | 4331 | `				 * in the import table, which can never match a single-segment alias,` |
|         - | 4332 | ``				 * and then prefixed the namespace anyway: `use Vv as Rule;` with`` |
|         - | 4333 | ``				 * `#[Rule\\A]` asked for `App\\Rule\\A` and got`` |
|         - | 4334 | ``				 * `Attribute class ... not found`. It is the same mistake`` |
|         - | 4335 | `				 * GenStateResolveName was written to fix for the other name positions,` |
|         - | 4336 | `				 * so it is that function's job here too. */` |
|         - | 4337 | `				SyBlob sTmp;` |
|         - | 4338 | `				SyString sRaw;` |
|       217 | 4339 | `				SyStringInitFromBuf(&sRaw,zName,nName);` |
|       217 | 4340 | `				SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       217 | 4341 | `				GenStateResolveName(&(*pGen),&sRaw,&sTmp);` |
|       217 | 4342 | `				if( SyBlobLength(&sTmp) > 0 ){` |
|       323 | 4343 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       212 | 4344 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       217 | 4345 | `					if( zDup ){` |
|       217 | 4346 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|       106 | 4347 | `					}` |
|       106 | 4348 | `				}` |
|       217 | 4349 | `				SyBlobRelease(&sTmp);` |
|       106 | 4350 | `			}` |
|       787 | 4351 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|       575 | 4352 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|       575 | 4353 | `				if( zDup ){` |
|       575 | 4354 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|       285 | 4355 | `				}` |
|       285 | 4356 | `			}` |
|         - | 4357 | `		}` |
|       787 | 4358 | `		SyBlobRelease(&sFQN);` |
|       787 | 4359 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 4360 | `			SyToken *pArgsEnd;` |
|       163 | 4361 | `			pIn++;` |
|       163 | 4362 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|       403 | 4363 | `			while( pIn < pArgsEnd ){` |
|       247 | 4364 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|       247 | 4365 | `				sxi32 iDepth = 0;` |
|         - | 4366 | `				ph7_attr_arg sArgRec;` |
|      1003 | 4367 | `				while( pArgStop < pArgsEnd ){` |
|       847 | 4368 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        54 | 4369 | `						iDepth++;` |
|       821 | 4370 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        54 | 4371 | `						iDepth--;` |
|       769 | 4372 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        88 | 4373 | `						break;` |
|         - | 4374 | `					}` |
|       761 | 4375 | `					pArgStop++;` |
|         5 | 4376 | `				}` |
|       247 | 4377 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|       247 | 4378 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       242 | 4379 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       168 | 4380 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        44 | 4381 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        14 | 4382 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        30 | 4383 | `					if( zN ){` |
|        30 | 4384 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|        14 | 4385 | `					}` |
|        30 | 4386 | `					pArgStart += 2;` |
|        14 | 4387 | `				}` |
|       247 | 4388 | `				if( pArgStart < pArgStop ){` |
|         - | 4389 | `					SySet *pInstrContainer;` |
|         - | 4390 | `					const char *zCErr;` |
|       247 | 4391 | `					pGen->pIn = pArgStart;` |
|       247 | 4392 | `					pGen->pEnd = pArgStop;` |
|         - | 4393 | `					/* An attribute argument is a constant expression -- php applies the` |
|         - | 4394 | ``					 * same rules it applies to a class constant, `new` excepted (an`` |
|         - | 4395 | `					 * attribute argument takes one). This is the argument's own rule,` |
|         - | 4396 | `					 * not the malformed-group case a few lines up, so it IS a fatal. */` |
|       247 | 4397 | `					zCErr = PH7_GenStateConstExprError(pGen,1);` |
|       247 | 4398 | `					if( zCErr ){` |
|         3 | 4399 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,"%s",zCErr);` |
|         3 | 4400 | `						pGen->pIn = pSavedIn;` |
|         3 | 4401 | `						pGen->pEnd = pSavedEnd;` |
|         3 | 4402 | `						return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - | 4403 | `					}` |
|       245 | 4404 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       245 | 4405 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|       245 | 4406 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|       245 | 4407 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|       245 | 4408 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       245 | 4409 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 4410 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 4411 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 4412 | `						return SXERR_ABORT;` |
|         - | 4413 | `					}` |
|       245 | 4414 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|       120 | 4415 | `				}` |
|       245 | 4416 | `				pIn = pArgStop;` |
|       245 | 4417 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        88 | 4418 | `					pIn++;` |
|        43 | 4419 | `				}` |
|         5 | 4420 | `			}` |
|       161 | 4421 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|        78 | 4422 | `		}` |
|       785 | 4423 | `		SySetPut(pOut,(const void *)&sAttr);` |
|       785 | 4424 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         7 | 4425 | `			pIn++;` |
|         7 | 4426 | `			continue;` |
|         - | 4427 | `		}` |
|       779 | 4428 | `		break;` |
|       ! 0 | 4429 | `	}` |
|       779 | 4430 | `	pGen->pIn = pSavedIn;` |
|       779 | 4431 | `	pGen->pEnd = pSavedEnd;` |
|       779 | 4432 | `	return SXRET_OK;` |
|       393 | 4433 | `}` |
|         - | 4434 | `/*` |
|         - | 4435 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 4436 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 4437 | ` */` |
|    207758 | 4438 | `PH7_PRIVATE sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 4439 | `{` |
|    207763 | 4440 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 4441 | `	sxu32 n;` |
|         - | 4442 | `	sxi32 rc;` |
|    208473 | 4443 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|       715 | 4444 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|       715 | 4445 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 4446 | `			return SXERR_ABORT;` |
|         - | 4447 | `		}` |
|       360 | 4448 | `	}` |
|    207763 | 4449 | `	SySetReset(&pGen->aPendingAttrs);` |
|    207763 | 4450 | `	return SXRET_OK;` |
|    103746 | 4451 | `}` |
|         - | 4452 | `/*` |
|         - | 4453 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 4454 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 4455 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 4456 | ` */` |
|    293870 | 4457 | `PH7_PRIVATE sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 4458 | `{` |
|    293875 | 4459 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|    293875 | 4460 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|    293875 | 4461 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 4462 | `	sxu32 nIdx, n;` |
|         - | 4463 | `	sxi32 rc;` |
|    293870 | 4464 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|      2283 | 4465 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|    291597 | 4466 | `		return SXRET_OK;` |
|         - | 4467 | `	}` |
|      2283 | 4468 | `	nIdx = (sxu32)(pTok - pBase);` |
|     16265 | 4469 | `	for( n = 0 ; n < nT ; n++ ){` |
|     13987 | 4470 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        70 | 4471 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        70 | 4472 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 4473 | `				return SXERR_ABORT;` |
|         - | 4474 | `			}` |
|        33 | 4475 | `		}` |
|      6996 | 4476 | `	}` |
|      2283 | 4477 | `	return SXRET_OK;` |
|    146637 | 4478 | `}` |
|         - | 4479 | `/*` |
|         - | 4480 | ` * ---------------------------------------------------------------------------` |
|         - | 4481 | ` * Where php's OWN attributes may be written.` |
|         - | 4482 | ` *` |
|         - | 4483 | ` * php's seven internal attribute classes each carry a target mask and a` |
|         - | 4484 | ` * validator, and the engine runs them where the declaration COMPILES: a` |
|         - | 4485 | `` * misplaced `#[\Attribute]`, `#[\Override]` or `#[\NoDiscard]` is a fatal at`` |
|         - | 4486 | ` * the line it sits on, before anything else in the file runs. A USERLAND` |
|         - | 4487 | ` * attribute is different — php checks its mask only when someone asks for it,` |
|         - | 4488 | `` * at `newInstance()` — so this table is closed on purpose and unknown names go`` |
|         - | 4489 | ` * unchecked, which is php's behaviour and not an omission.` |
|         - | 4490 | ` *` |
|         - | 4491 | ` * The masks are the same seven the classes declare (see VmInstallAttributes);` |
|         - | 4492 | ` * they are repeated here because the compiler runs before any class exists.` |
|         - | 4493 | ` * None of the seven is IS_REPEATABLE, so a second one is php's own refusal.` |
|         - | 4494 | ` * ---------------------------------------------------------------------------` |
|         - | 4495 | ` */` |
|         - | 4496 | `/* php's Attribute::TARGET_* names, in bit order (TARGET_CLASS is bit 0). */` |
|         - | 4497 | `static const char *const azGenAttrTarget[] = {` |
|         - | 4498 | `	"class","function","method","property","class constant","parameter","constant"` |
|         - | 4499 | `};` |
|         - | 4500 | `static const struct {` |
|         - | 4501 | `	const char *zName;` |
|         - | 4502 | `	int iMask;` |
|         - | 4503 | `} aGenInternalAttr[] = {` |
|         - | 4504 | `	{ "Attribute",              1  },` |
|         - | 4505 | `	{ "Deprecated",             87 },` |
|         - | 4506 | `	{ "AllowDynamicProperties", 1  },` |
|         - | 4507 | `	{ "SensitiveParameter",     32 },` |
|         - | 4508 | `	{ "ReturnTypeWillChange",   4  },` |
|         - | 4509 | `	{ "Override",               12 },` |
|         - | 4510 | `	{ "NoDiscard",              6  },` |
|         - | 4511 | `};` |
|         - | 4512 | `/*` |
|         - | 4513 | ` * The extra validator php gives three of them, asked only once the target is` |
|         - | 4514 | ` * known to be a CLASS: the mask says "a class" and these say WHICH kinds.` |
|         - | 4515 | ` * Answers php's noun for the refused kind, or 0 when the class is acceptable.` |
|         - | 4516 | ` */` |
|       112 | 4517 | `static const char * GenStateAttrClassRefusal(const char *zAttr,sxi32 iFlags)` |
|         5 | 4518 | `{` |
|         - | 4519 | `	/* zAttr is a row of aGenInternalAttr, so an exact compare is the whole test. */` |
|       117 | 4520 | `	int bAttr = SyStrncmp(zAttr,"Attribute",sizeof("Attribute")) == 0;` |
|       117 | 4521 | `	int bDyn  = SyStrncmp(zAttr,"AllowDynamicProperties",sizeof("AllowDynamicProperties")) == 0;` |
|       117 | 4522 | `	int bDep  = SyStrncmp(zAttr,"Deprecated",sizeof("Deprecated")) == 0;` |
|       117 | 4523 | `	if( !bAttr && !bDyn && !bDep ){` |
|       ! 0 | 4524 | `		return 0;` |
|         - | 4525 | `	}` |
|       117 | 4526 | `	if( iFlags & PH7_CLASS_INTERFACE ){ return "interface"; }` |
|       111 | 4527 | `	if( iFlags & PH7_CLASS_ENUM ){ return "enum"; }` |
|       105 | 4528 | `	if( iFlags & PH7_CLASS_TRAIT ){` |
|         - | 4529 | `		/* php 8.5 DOES mark a deprecated trait; the other two refuse one. */` |
|         7 | 4530 | `		return bDep ? 0 : "trait";` |
|         - | 4531 | `	}` |
|        99 | 4532 | `	if( bAttr ){` |
|         - | 4533 | `		/* An attribute class must be instantiable. */` |
|        72 | 4534 | `		return (iFlags & PH7_CLASS_ABSTRACT) ? "abstract class" : 0;` |
|         - | 4535 | `	}` |
|        30 | 4536 | `	if( bDyn ){` |
|         - | 4537 | `		/* A readonly class has no dynamic property to allow. */` |
|        21 | 4538 | `		return (iFlags & PH7_CLASS_READONLY) ? "readonly class" : 0;` |
|         - | 4539 | `	}` |
|        10 | 4540 | `	return "class";   /* #[\Deprecated] on any other class kind */` |
|        61 | 4541 | `}` |
|         - | 4542 | `/*` |
|         - | 4543 | ` * Validate one declaration's attribute set against php's placement rules.` |
|         - | 4544 | ` *` |
|         - | 4545 | ` * iTarget is the single Attribute::TARGET_* bit php NAMES for this declaration` |
|         - | 4546 | ` * and iAccept the mask it accepts, which differ in exactly one place: a PROMOTED` |
|         - | 4547 | ` * constructor parameter is a parameter and a property both, so it takes either` |
|         - | 4548 | ` * bit while still reporting "parameter". pClassName/iClassFlags describe the` |
|         - | 4549 | ` * subject when the target is a class (0 and 0 otherwise).` |
|         - | 4550 | ` *` |
|         - | 4551 | ` * nLine is the line php blames, which is the DECLARATION's, never the attribute's:` |
|         - | 4552 | ` * php validates the set while compiling the declaration's AST node, whose line is` |
|         - | 4553 | `` * the `function`/`fn`/`class`/`interface`/`trait`/`enum` keyword for those, the`` |
|         - | 4554 | ` * type (else the name) for a property or a parameter, and the first name for a` |
|         - | 4555 | ` * constant or an enum case -- modifiers in front of any of them move nothing.` |
|         - | 4556 | ` */` |
|    501490 | 4557 | `PH7_PRIVATE sxi32 GenStateCheckAttrPlacement(ph7_gen_state *pGen,SySet *pAttrs,sxu32 nLine,` |
|         - | 4558 | `	int iTarget,int iAccept,const SyString *pClassName,sxi32 iClassFlags)` |
|         5 | 4559 | `{` |
|    501495 | 4560 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|         - | 4561 | `	sxu32 n,k;` |
|    502123 | 4562 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; ++n ){` |
|       773 | 4563 | `		SyString *pName = &aAttr[n].sName;` |
|         - | 4564 | `		sxu32 iRow;` |
|      4161 | 4565 | `		for( iRow = 0 ; iRow < SX_ARRAYSIZE(aGenInternalAttr) ; ++iRow ){` |
|      3992 | 4566 | `			if( pName->nByte == (sxu32)SyStrlen(aGenInternalAttr[iRow].zName)` |
|      2424 | 4567 | `			 && SyStrnicmp(pName->zString,aGenInternalAttr[iRow].zName,pName->nByte) == 0 ){` |
|       609 | 4568 | `				break;` |
|         - | 4569 | `			}` |
|      1699 | 4570 | `		}` |
|       773 | 4571 | `		if( iRow >= SX_ARRAYSIZE(aGenInternalAttr) ){` |
|       169 | 4572 | `			continue;   /* a userland attribute: judged at newInstance(), not here */` |
|         - | 4573 | `		}` |
|       609 | 4574 | `		if( (aGenInternalAttr[iRow].iMask & iAccept) == 0 ){` |
|         - | 4575 | `			SyBlob sAllowed;` |
|         - | 4576 | `			int iBit;` |
|         - | 4577 | `			sxi32 rc;` |
|       105 | 4578 | `			SyBlobInit(&sAllowed,&pGen->pVm->sAllocator);` |
|       819 | 4579 | `			for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azGenAttrTarget) ; iBit++ ){` |
|       717 | 4580 | `				if( (aGenInternalAttr[iRow].iMask & (1 << iBit)) == 0 ){` |
|       547 | 4581 | `					continue;` |
|         - | 4582 | `				}` |
|       173 | 4583 | `				if( SyBlobLength(&sAllowed) > 0 ){` |
|        71 | 4584 | `					SyBlobAppend(&sAllowed,", ",sizeof(", ")-1);` |
|        34 | 4585 | `				}` |
|       258 | 4586 | `				SyBlobAppend(&sAllowed,azGenAttrTarget[iBit],` |
|       170 | 4587 | `					(sxu32)SyStrlen(azGenAttrTarget[iBit]));` |
|        88 | 4588 | `			}` |
|       105 | 4589 | `			SyBlobAppend(&sAllowed,"",sizeof(char));   /* NUL for the %s below */` |
|       371 | 4590 | `			for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azGenAttrTarget) ; iBit++ ){` |
|       371 | 4591 | `				if( iTarget == (1 << iBit) ){` |
|       105 | 4592 | `					break;` |
|         - | 4593 | `				}` |
|       136 | 4594 | `			}` |
|       156 | 4595 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        51 | 4596 | `				"Attribute \"%z\" cannot target %s (allowed targets: %s)",pName,` |
|        51 | 4597 | `				iBit < (int)SX_ARRAYSIZE(azGenAttrTarget) ? azGenAttrTarget[iBit] : "",` |
|        51 | 4598 | `				SyBlobData(&sAllowed));` |
|       105 | 4599 | `			SyBlobRelease(&sAllowed);` |
|       105 | 4600 | `			return rc;` |
|         - | 4601 | `		}` |
|         - | 4602 | `		/* ...then repetition, which is what php checks second: the FIRST of a` |
|         - | 4603 | `		 * misplaced pair reports its target instead. */` |
|       509 | 4604 | `		for( k = 0 ; k < n ; ++k ){` |
|        12 | 4605 | `			if( aAttr[k].sName.nByte == pName->nByte` |
|        15 | 4606 | `			 && SyStrnicmp(aAttr[k].sName.zString,pName->zString,pName->nByte) == 0 ){` |
|        18 | 4607 | `				return PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         5 | 4608 | `					"Attribute \"%z\" must not be repeated",pName);` |
|         - | 4609 | `			}` |
|         2 | 4610 | `		}` |
|       497 | 4611 | `		if( iTarget == 1 && pClassName ){` |
|       173 | 4612 | `			const char *zRefused = GenStateAttrClassRefusal(aGenInternalAttr[iRow].zName,` |
|        56 | 4613 | `				iClassFlags);` |
|       117 | 4614 | `			if( zRefused ){` |
|        44 | 4615 | `				return PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        28 | 4616 | `					"Cannot apply #[\\%s] to %s %z",aGenInternalAttr[iRow].zName,` |
|        14 | 4617 | `					zRefused,pClassName);` |
|         - | 4618 | `			}` |
|        42 | 4619 | `		}` |
|       237 | 4620 | `	}` |
|    501355 | 4621 | `	return SXRET_OK;` |
|    250309 | 4622 | `}` |
|         - | 4623 | `/*` |
|         - | 4624 | `` * php 8.5's `(void)` cast is a STATEMENT prefix, not an expression operator:`` |
|         - | 4625 | `` * `$x = (void) f();` and `return (void) f();` are parse errors there too, and`` |
|         - | 4626 | ` * the only thing it does is say that dropping the answer is DELIBERATE, which` |
|         - | 4627 | ` * silences a #[\NoDiscard] callee. The lexer already assembled the three tokens` |
|         - | 4628 | ` * into one (PH7_TK_VOID_CAST); this consumes it and answers 1.` |
|         - | 4629 | ` */` |
|   1066614 | 4630 | `PH7_PRIVATE int GenStateTakeVoidCast(ph7_gen_state *pGen)` |
|         5 | 4631 | `{` |
|   1066619 | 4632 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_VOID_CAST) ){` |
|        19 | 4633 | `		pGen->pIn++;` |
|        19 | 4634 | `		return 1;` |
|         - | 4635 | `	}` |
|   1066601 | 4636 | `	return 0;` |
|    532403 | 4637 | `}` |
|         - | 4638 | `/*` |
|         - | 4639 | `` * php's grammar takes a `(void)` cast at the head of an expression STATEMENT and`` |
|         - | 4640 | `` * at the head of each element of a `for` clause list — `for ((void) f(), $i = 0;`` |
|         - | 4641 | `` * $i < 1; $i++, (void) g())` is all valid, while `for ($i = (void) f();;)` is`` |
|         - | 4642 | ` * not. The statement head is consumed by GenStateTakeVoidCast; a clause is one` |
|         - | 4643 | ` * expression with comma operators in it, so its element heads are marked HERE,` |
|         - | 4644 | ` * before it compiles: the token becomes the no-op cast operator parse.c declares,` |
|         - | 4645 | `` * and every other `(void)` in the clause stays unrecognized, which is php's own`` |
|         - | 4646 | ` * refusal. Nothing is moved or removed — the token stream is shared with the` |
|         - | 4647 | ` * rest of the file.` |
|         - | 4648 | ` */` |
|    154135 | 4649 | `PH7_PRIVATE int GenStateEnableClauseVoidCasts(ph7_gen_state *pGen,int bLastToo)` |
|         5 | 4650 | `{` |
|    154140 | 4651 | `	SyToken *pTok = pGen->pIn,*pLastMark = 0;` |
|    154140 | 4652 | `	int iDepth = 0,bHead = 1,bCommaAfter = 0;` |
|    880612 | 4653 | `	for( ; pTok < pGen->pEnd ; pTok++ ){` |
|    829259 | 4654 | `		if( pTok->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     25512 | 4655 | `			iDepth++;` |
|    816489 | 4656 | `		}else if( pTok->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     25512 | 4657 | `			iDepth--;` |
|    790982 | 4658 | `		}else if( iDepth == 0 && (pTok->nType & PH7_TK_SEMI) ){` |
|         - | 4659 | `			/* The three clauses share one token range (only the post one is` |
|         - | 4660 | `			 * delimited), so this scan stops where its own clause does. */` |
|     51327 | 4661 | `			break;` |
|    675463 | 4662 | `		}else if( iDepth == 0 && (pTok->nType & PH7_TK_COMMA) ){` |
|        16 | 4663 | `			bHead = 1;` |
|        16 | 4664 | `			bCommaAfter = 1;` |
|        16 | 4665 | `			continue;` |
|    675449 | 4666 | `		}else if( iDepth == 0 && bHead && (pTok->nType & PH7_TK_VOID_CAST) ){` |
|         9 | 4667 | `			pTok->nType \|= PH7_TK_OP;` |
|         9 | 4668 | `			pTok->pUserData = (void *)PH7_ExprExtractOperator(&pTok->sData,0);` |
|         9 | 4669 | `			pLastMark = pTok;` |
|         9 | 4670 | `			bCommaAfter = 0;` |
|         4 | 4671 | `		}` |
|    726463 | 4672 | `		bHead = 0;` |
|    362750 | 4673 | `	}` |
|         - | 4674 | `	/* The CONDITION clause's last element is the condition VALUE, so php refuses a` |
|         - | 4675 | ``	 * `(void)` on that one and only that one: `for (;(void) f();)` is a parse error`` |
|         - | 4676 | ``	 * where `for (;(void) f(), $i < 1;)` is fine. */`` |
|    154140 | 4677 | `	if( !bLastToo && pLastMark && !bCommaAfter ){` |
|         3 | 4678 | `		pLastMark->nType &= ~(sxu32)PH7_TK_OP;` |
|         3 | 4679 | `		pLastMark->pUserData = 0;` |
|         3 | 4680 | `		return 1;` |
|         - | 4681 | `	}` |
|    154138 | 4682 | `	return 0;` |
|     76969 | 4683 | `}` |
|         - | 4684 | `/*` |
|         - | 4685 | ` * The statement is about to throw its expression's value away. When that value` |
|         - | 4686 | ` * came straight out of a CALL, mark the call: php's !RETURN_VALUE_USED, which is` |
|         - | 4687 | `` * what a #[\NoDiscard] callee reads. `f() + 1;` drops the ADD's result, not the`` |
|         - | 4688 | ` * call's, so only the last instruction is looked at.` |
|         - | 4689 | ` */` |
|   1168894 | 4690 | `PH7_PRIVATE void GenStateMarkDiscardedCall(ph7_gen_state *pGen)` |
|         5 | 4691 | `{` |
|   1168899 | 4692 | `	VmInstr *pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   1168899 | 4693 | `	if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|    169250 | 4694 | `		pInstr->bDiscard = 1;` |
|     84324 | 4695 | `	}` |
|   1168899 | 4696 | `}` |
|         - | 4697 | `/* TRUE when the cursor has run past the LAST token of a chunk that met the end of` |
|         - | 4698 | ``  * the file. A statement slice can end early (a single statement inside a `for` `` |
|         - | 4699 | `` * header), so `pIn >= pEnd` alone is not the question. */`` |
|       162 | 4700 | `static int GenStateAtChunkEof(ph7_gen_state *pGen)` |
|         5 | 4701 | `{` |
|         - | 4702 | `	SyToken *pBase;` |
|       167 | 4703 | `	if( !pGen->bChunkAtEof \|\| pGen->pTokenSet == 0 ){` |
|        62 | 4704 | `		return 0;` |
|         - | 4705 | `	}` |
|       108 | 4706 | `	pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|       108 | 4707 | `	return pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)];` |
|        86 | 4708 | `}` |
|         - | 4709 | `/*` |
|         - | 4710 | ` * php's grammar wants a TERMINATOR after a statement, and the end of the file is` |
|         - | 4711 | `` * not one: `<?php echo "a"` is `syntax error, unexpected end of file, expecting`` |
|         - | 4712 | `` * "," or ";"` there, while PHL ran it and exited 0. (A `?>` IS a terminator, which`` |
|         - | 4713 | `` * is why `<?php echo "a" ?>` is legal in both engines and why the check only`` |
|         - | 4714 | ` * applies to a chunk that met the end of the FILE.)` |
|         - | 4715 | ` *` |
|         - | 4716 | `` * A statement that ends in `}` -- a block, a declaration, a braced control`` |
|         - | 4717 | `` * structure -- needs nothing, and neither does one whose `;` the loop just`` |
|         - | 4718 | ` * stepped over; everything else was left unfinished.` |
|         - | 4719 | ` *` |
|         - | 4720 | ` * Answers the "expecting" clause php names for the statement that ran out, or 0` |
|         - | 4721 | ` * when php names none. php reports the set its parser was in, which for a` |
|         - | 4722 | ` * statement is decided by the KEYWORD it opened with -- the comma-list statements` |
|         - | 4723 | `` * may take another element, `return`/`break`/`continue`/`goto`/`unset` and a`` |
|         - | 4724 | `` * do-while may not, `namespace` still wants its block -- except when the last`` |
|         - | 4725 | `` * token consumed was an alternative-syntax `end*`, whose own `;` is what is`` |
|         - | 4726 | ` * missing.` |
|         - | 4727 | ` */` |
|        92 | 4728 | `static const char * GenStateEofExpecting(SyToken *pStmt,SyToken *pLast)` |
|         4 | 4729 | `{` |
|         - | 4730 | `	sxu32 nKw;` |
|        96 | 4731 | `	if( pLast && (pLast->nType & PH7_TK_KEYWORD) ){` |
|        24 | 4732 | `		nKw = (sxu32)SX_PTR_TO_INT(pLast->pUserData);` |
|        22 | 4733 | `		if( nKw == PH7_TKWRD_ENDIF \|\| nKw == PH7_TKWRD_ENDWHILE \|\| nKw == PH7_TKWRD_ENDFOR` |
|        12 | 4734 | `		 \|\| nKw == PH7_TKWRD_END4EACH \|\| nKw == PH7_TKWRD_ENDSWITCH \|\| nKw == PH7_TKWRD_ENDDEC ){` |
|        20 | 4735 | `			return "\";\"";` |
|         - | 4736 | `		}` |
|         2 | 4737 | `	}` |
|        77 | 4738 | `	if( pStmt == 0 \|\| (pStmt->nType & PH7_TK_KEYWORD) == 0 ){` |
|        13 | 4739 | `		return 0;` |
|         - | 4740 | `	}` |
|        64 | 4741 | `	nKw = (sxu32)SX_PTR_TO_INT(pStmt->pUserData);` |
|        62 | 4742 | `	if( nKw == PH7_TKWRD_ECHO \|\| nKw == PH7_TKWRD_GLOBAL \|\| nKw == PH7_TKWRD_STATIC` |
|        27 | 4743 | `	 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_USE ){` |
|        44 | 4744 | `		return "\",\" or \";\"";` |
|         - | 4745 | `	}` |
|        20 | 4746 | `	if( nKw == PH7_TKWRD_RETURN \|\| nKw == PH7_TKWRD_BREAK \|\| nKw == PH7_TKWRD_CONTINUE` |
|        15 | 4747 | `	 \|\| nKw == PH7_TKWRD_GOTO \|\| nKw == PH7_TKWRD_UNSET \|\| nKw == PH7_TKWRD_DO ){` |
|        13 | 4748 | `		return "\";\"";` |
|         - | 4749 | `	}` |
|         8 | 4750 | `	if( nKw == PH7_TKWRD_NAMESPACE ){` |
|         2 | 4751 | `		return "\"{\"";` |
|         - | 4752 | `	}` |
|         6 | 4753 | `	return 0;` |
|        50 | 4754 | `}` |
|         - | 4755 | ``/* Does this script spell `__halt_compiler` at all, in any case? A cheap scan`` |
|         - | 4756 | ` * that keeps the token pass below off every ordinary file. */` |
|     39752 | 4757 | `static int GenStateMentionsHalt(const char *zIn,sxu32 nIn)` |
|         5 | 4758 | `{` |
|         - | 4759 | `	static const char zWord[] = "__halt_compiler";` |
|     39757 | 4760 | `	sxu32 nWord = (sxu32)sizeof(zWord)-1;` |
|         - | 4761 | `	sxu32 i;` |
| 158868813 | 4762 | `	for( i = 0 ; i + nWord <= nIn ; ++i ){` |
| 158829106 | 4763 | `		if( zIn[i] != '_' ){` |
| 157427015 | 4764 | `			continue;` |
|         - | 4765 | `		}` |
|   1402096 | 4766 | `		if( SyStrnicmp(&zIn[i],zWord,nWord) == 0 ){` |
|        47 | 4767 | `			return 1;` |
|         - | 4768 | `		}` |
|    699833 | 4769 | `	}` |
|     39712 | 4770 | `	return 0;` |
|     19870 | 4771 | `}` |
|         - | 4772 | ``/* Is this token the `__halt_compiler` identifier? It is not a keyword in this`` |
|         - | 4773 | ` * lexer (the generated table takes nothing longer than twelve bytes), so it` |
|         - | 4774 | ` * arrives as an ordinary identifier and is recognised by NAME -- case` |
|         - | 4775 | ` * insensitively, as php's own scanner does. */` |
|   2588679 | 4776 | `static int GenStateIsHaltCompiler(SyToken *pTok,SyToken *pEnd)` |
|         5 | 4777 | `{` |
|   2583877 | 4778 | `	return pTok < pEnd` |
|   2588679 | 4779 | `	    && (pTok->nType & PH7_TK_ID)` |
|   1374084 | 4780 | `	    && pTok->sData.nByte == sizeof("__halt_compiler")-1` |
|   3885422 | 4781 | `	    && SyStrnicmp(pTok->sData.zString,"__halt_compiler",sizeof("__halt_compiler")-1) == 0;` |
|         5 | 4782 | `}` |
|         - | 4783 | `/*` |
|         - | 4784 | `` * The pre-scan behind `__COMPILER_HALT_OFFSET__`: find the halt statement in`` |
|         - | 4785 | `` * whichever PHP chunk holds it and remember the byte just past its `;`. The`` |
|         - | 4786 | ` * chunks are tokenized a second time here -- the compile below tokenizes each` |
|         - | 4787 | ` * one as it reaches it -- because the constant's value has to be known before` |
|         - | 4788 | ` * the first statement compiles. Only a file that spells the identifier gets` |
|         - | 4789 | ` * here at all.` |
|         - | 4790 | ` *` |
|         - | 4791 | `` * A `__halt_compiler` in a scope php refuses is still found: the statement`` |
|         - | 4792 | ` * compiler raises php's fatal when it reaches it, and the offset is never read.` |
|         - | 4793 | ` */` |
|        45 | 4794 | `static void GenStateScanHaltOffset(ph7_gen_state *pGen,SySet *pRawToken,const char *zFileBase)` |
|         2 | 4795 | `{` |
|        47 | 4796 | `	SyToken *pRaw = (SyToken *)SySetBasePtr(pRawToken);` |
|        47 | 4797 | `	SyToken *pRawEnd = &pRaw[SySetUsed(pRawToken)];` |
|         - | 4798 | `	SySet aTok,aTriv;` |
|        47 | 4799 | `	SySetInit(&aTok,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        47 | 4800 | `	SySetInit(&aTriv,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       141 | 4801 | `	for( ; pRaw < pRawEnd && pGen->bHaltSeen == 0 ; pRaw++ ){` |
|         - | 4802 | `		SyToken *pTok,*pEnd;` |
|        96 | 4803 | `		if( (pRaw->nType & PH7_TOKEN_PHP) == 0 ){` |
|        49 | 4804 | `			continue;` |
|         - | 4805 | `		}` |
|        49 | 4806 | `		SySetReset(&aTok);` |
|        49 | 4807 | `		SySetReset(&aTriv);` |
|        72 | 4808 | `		PH7_TokenizePHP(SyStringData(&pRaw->sData),SyStringLength(&pRaw->sData),` |
|        23 | 4809 | `			pRaw->nLine,&aTok,&aTriv);` |
|        49 | 4810 | `		pTok = (SyToken *)SySetBasePtr(&aTok);` |
|        49 | 4811 | `		pEnd = &pTok[SySetUsed(&aTok)];` |
|      7664 | 4812 | `		for( ; pTok < pEnd ; pTok++ ){` |
|      7651 | 4813 | `			if( !GenStateIsHaltCompiler(pTok,pEnd) ){` |
|      7617 | 4814 | `				continue;` |
|         - | 4815 | `			}` |
|        34 | 4816 | `			if( pTok + 3 < pEnd` |
|        32 | 4817 | `			 && (pTok[1].nType & PH7_TK_LPAREN)` |
|        30 | 4818 | `			 && (pTok[2].nType & PH7_TK_RPAREN)` |
|        32 | 4819 | `			 && (pTok[3].nType & PH7_TK_SEMI) ){` |
|        32 | 4820 | `				const char *zSemi = SyStringData(&pTok[3].sData);` |
|        32 | 4821 | `				if( zSemi > zFileBase ){` |
|        32 | 4822 | `					pGen->nHaltOffset = (sxu32)((zSemi - zFileBase) + 1);` |
|        32 | 4823 | `					pGen->bHaltSeen = 1;` |
|        15 | 4824 | `				}` |
|        15 | 4825 | `			}` |
|        36 | 4826 | `			break;` |
|       ! 0 | 4827 | `		}` |
|        25 | 4828 | `	}` |
|        47 | 4829 | `	SySetRelease(&aTok);` |
|        47 | 4830 | `	SySetRelease(&aTriv);` |
|        47 | 4831 | `}` |
|         - | 4832 | `/*` |
|         - | 4833 | ` * ---------------------------------------------------------------------------` |
|         - | 4834 | ``  * `__halt_compiler();` `` |
|         - | 4835 | ` *` |
|         - | 4836 | ` * php's scanner STOPS at it: the rest of the file is not code and is never` |
|         - | 4837 | ` * output either, which is what lets a .phar carry a binary archive in the bytes` |
|         - | 4838 | ` * behind its stub. Three rules come with it, all php's:` |
|         - | 4839 | ` *` |
|         - | 4840 | ` *   - it is only legal at the OUTERMOST scope -- inside a function, a class or` |
|         - | 4841 | `` *     even a plain `if` block it is a compile-time fatal, not a parse error;`` |
|         - | 4842 | ` *   - the parentheses and the semicolon are part of the construct, and php's` |
|         - | 4843 | ` *     parser names what it wanted when one is missing;` |
|         - | 4844 | `` *   - `__COMPILER_HALT_OFFSET__` expands to the byte just past that `;`.`` |
|         - | 4845 | ` *` |
|         - | 4846 | ` * It is NOT a keyword in this lexer (the generated table takes nothing longer` |
|         - | 4847 | ` * than twelve bytes), so it arrives as an ordinary identifier in statement` |
|         - | 4848 | ` * position and is recognised by name -- case-insensitively, as php does.` |
|         - | 4849 | ` * ---------------------------------------------------------------------------` |
|         - | 4850 | ` */` |
|        96 | 4851 | `static sxi32 GenStateCompileHaltCompiler(ph7_gen_state *pGen)` |
|         2 | 4852 | `{` |
|        98 | 4853 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        98 | 4854 | `	if( pGen->pCurrent != &pGen->sGlobal ){` |
|         - | 4855 | `		/* php's own sentence: a FATAL rather than a parse error, and one its` |
|         - | 4856 | `		 * PARSER makes -- so it prints no stack trace under it. */` |
|        67 | 4857 | `		pGen->iFatalTrace = PH7_FATAL_TRACE_NONE;` |
|        67 | 4858 | `		return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 4859 | `			"__HALT_COMPILER() can only be used from the outermost scope");` |
|         - | 4860 | `	}` |
|        32 | 4861 | `	pGen->pIn++;` |
|        32 | 4862 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 | 4863 | `		return PH7_GenSyntaxError(&(*pGen),` |
|         2 | 4864 | `			pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         - | 4865 | `	}` |
|         - | 4866 | ``	/* php's scanner ran out INSIDE the parentheses: it names the `(` it never`` |
|         - | 4867 | `	 * closed rather than the token it wanted, and reports it where the INPUT` |
|         - | 4868 | ``	 * ends rather than where the `(` is. */`` |
|         - | 4869 | `#define PHL_HALT_EOF_LINE (pGen->bChunkAtEof && pGen->nChunkEofLine > nLine \` |
|         - | 4870 | `	? pGen->nChunkEofLine : nLine)` |
|        30 | 4871 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 4872 | `		return PH7_GenCompileError(&(*pGen),E_PARSE,PHL_HALT_EOF_LINE,` |
|       ! 0 | 4873 | `			"Unclosed '(' on line %u",nLine);` |
|         - | 4874 | `	}` |
|        30 | 4875 | `	pGen->pIn++;` |
|        30 | 4876 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|         4 | 4877 | `		return pGen->pIn >= pGen->pEnd` |
|         2 | 4878 | `			? PH7_GenCompileError(&(*pGen),E_PARSE,PHL_HALT_EOF_LINE,` |
|         1 | 4879 | `				"Unclosed '(' on line %u",nLine)` |
|         2 | 4880 | `			: PH7_GenSyntaxError(&(*pGen),pGen->pIn,"\")\"");` |
|         - | 4881 | `	}` |
|        28 | 4882 | `	pGen->pIn++;` |
|        28 | 4883 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       ! 0 | 4884 | `		return PH7_GenSyntaxError(&(*pGen),` |
|       ! 0 | 4885 | `			pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|         - | 4886 | `	}` |
|        28 | 4887 | `	pGen->pIn++;` |
|         - | 4888 | `	/* Nothing after it is code, in this chunk or in any that follows. */` |
|        28 | 4889 | `	pGen->pIn = pGen->pEnd;` |
|        28 | 4890 | `	pGen->bHalted = 1;` |
|         - | 4891 | `#undef PHL_HALT_EOF_LINE` |
|        28 | 4892 | `	return SXRET_OK;` |
|        50 | 4893 | `}` |
|   2309300 | 4894 | `PH7_PRIVATE sxi32 GenStateCompileChunk(` |
|         - | 4895 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 4896 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 4897 | `	)` |
|         5 | 4898 | `{` |
|         - | 4899 | `	ProcLangConstruct xCons;` |
|         - | 4900 | `	sxi32 rc;` |
|   2309305 | 4901 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   1460367 | 4902 | `	for(;;){` |
|   2616569 | 4903 | `		int bStmtIsDeclare = 0;` |
|   2616569 | 4904 | `		int bStmtIsNamespace = 0;` |
|         - | 4905 | `		int bStmtIsNop;` |
|         - | 4906 | `		/* Whether php's grammar wants a TERMINATOR after this statement: a block, a` |
|         - | 4907 | `		 * declaration and a LABEL end themselves, everything else -- an expression` |
|         - | 4908 | ``		 * statement included, even one that ends in the `}` of a closure or a match`` |
|         - | 4909 | `		 * -- has to be closed. */` |
|   2616569 | 4910 | `		int bStmtWantsSemi = 1;` |
|         - | 4911 | `		SyToken *pStmtStart;` |
|   2616569 | 4912 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 4913 | `			/* No more input to process */` |
|     35547 | 4914 | `			break;` |
|         - | 4915 | `		}` |
|   2581027 | 4916 | `		pStmtStart = pGen->pIn; /* The keyword this statement opened with, for the` |
|         - | 4917 | `		                         * end-of-input check below */` |
|         - | 4918 | `		/* Bind a directly-preceding docblock to this statement */` |
|   2581027 | 4919 | `		GenStateSetPendingDoc(&(*pGen));` |
|   2581027 | 4920 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 4921 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 4922 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 4923 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 4924 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 4925 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|       421 | 4926 | `			int bAttrTarget = 0;` |
|       416 | 4927 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|       414 | 4928 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|        24 | 4929 | `				bAttrTarget = 1;` |
|       410 | 4930 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       399 | 4931 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       394 | 4932 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|       113 | 4933 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|        26 | 4934 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|        22 | 4935 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         5 | 4936 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|       399 | 4937 | `					bAttrTarget = 1;` |
|       197 | 4938 | `				}` |
|       197 | 4939 | `			}` |
|       421 | 4940 | `			if( !bAttrTarget ){` |
|       ! 0 | 4941 | `				rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|         - | 4942 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 4943 | `					&pGen->pIn->sData);` |
|       ! 0 | 4944 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 4945 | `					break;` |
|         - | 4946 | `				}` |
|       ! 0 | 4947 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 4948 | `			}` |
|       208 | 4949 | `		}` |
|         - | 4950 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 4951 | `		 * below doesn't fire before the directive has a chance to run. */` |
|   2581027 | 4952 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   1515185 | 4953 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1515185 | 4954 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|       119 | 4955 | `				bStmtIsDeclare = 1;` |
|   1515128 | 4956 | `			}else if( nPeek == PH7_TKWRD_NAMESPACE && !GenStateIsNsRelName(pGen->pIn,pGen->pEnd) ){` |
|         - | 4957 | `				/* A namespace DECLARATION asks the lock itself -- php's rule is that` |
|         - | 4958 | `				 * the first one must be the first statement, nops and declares` |
|         - | 4959 | `				 * aside -- and then sets it, since it is code for the declares` |
|         - | 4960 | `				 * after it. */` |
|       487 | 4961 | `				bStmtIsNamespace = 1;` |
|       241 | 4962 | `			}` |
|    756583 | 4963 | `		}` |
|         - | 4964 | `` 		/* php's zend_is_first_statement walks past a null statement: an empty `;` `` |
|         - | 4965 | ``		 * before `declare(strict_types=1)` or before `namespace` is not code. */`` |
|   2581027 | 4966 | `		bStmtIsNop = (pGen->pIn->nType & PH7_TK_SEMI) != 0;` |
|   2581027 | 4967 | `		if( !bStmtIsDeclare && !bStmtIsNamespace && !bStmtIsNop && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 4968 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 4969 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|    306749 | 4970 | `			pGen->bStrictTypesLocked = 1;` |
|    152992 | 4971 | `		}` |
|   2581022 | 4972 | `		if( pGen->pCurrent == &pGen->sGlobal && pGen->bNsBracketed && !pGen->bInNsBlock` |
|        60 | 4973 | `		 && !bStmtIsNamespace && !bStmtIsNop && !GenStateIsHaltCompiler(pGen->pIn,pGen->pEnd)` |
|        12 | 4974 | `		 && !GenStateIsStrayKeyword(&(*pGen)) ){` |
|         - | 4975 | `			/* php's zend_verify_namespace: once a file has used the bracketed form,` |
|         - | 4976 | ``			 * every statement outside a block but another `namespace` (or the halt)`` |
|         - | 4977 | `			 * is a compile fatal. A word that opens no statement is not one: php's` |
|         - | 4978 | `			 * parser refuses it first. */` |
|         5 | 4979 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 4980 | `				"No code may exist outside of namespace {}");` |
|         5 | 4981 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 4982 | `				break;` |
|         - | 4983 | `			}` |
|         2 | 4984 | `		}` |
|   2581027 | 4985 | `		if( GenStateIsHaltCompiler(pGen->pIn,pGen->pEnd) ){` |
|         - | 4986 | `			/* Everything from here on is DATA. php's own scanner stops in exactly` |
|         - | 4987 | `			 * the same place, which is what lets a .phar carry its archive in the` |
|         - | 4988 | `			 * bytes after its stub. */` |
|        98 | 4989 | `			rc = GenStateCompileHaltCompiler(&(*pGen));` |
|        98 | 4990 | `			break;` |
|         - | 4991 | `		}` |
|   2580931 | 4992 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 4993 | `			/* Compile block */` |
|       159 | 4994 | `			bStmtWantsSemi = 0;` |
|       159 | 4995 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       159 | 4996 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 4997 | `				break;` |
|         - | 4998 | `			}` |
|        82 | 4999 | `		}else{` |
|   2580777 | 5000 | `			xCons = 0;` |
|   2580777 | 5001 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 5002 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 5003 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 5004 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|       271 | 5005 | `				xCons = PH7_CompileClassModifiers;` |
|   2580644 | 5006 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 5007 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 5008 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|       157 | 5009 | `				xCons = PH7_CompileEnum;` |
|   2580435 | 5010 | `			}else if( GenStateStartsClosureExpr(pGen->pIn,pGen->pEnd) ){` |
|         - | 5011 | ``				/* `function () {…};` / `fn (…) => …;` at STATEMENT position is an`` |
|         - | 5012 | ``				 * expression statement in php, not a declaration — the `(` where a`` |
|         - | 5013 | `				 * named function has its name is what says so. */` |
|        16 | 5014 | `				xCons = 0;` |
|   2580352 | 5015 | `			}else if( GenStateIsNsRelName(pGen->pIn,pGen->pEnd) ){` |
|         - | 5016 | ``				/* A statement that STARTS with php's `namespace\X` name operator`` |
|         - | 5017 | ``				 * (`namespace\Cee::m();`) is an expression, not a namespace`` |
|         - | 5018 | ``				 * DECLARATION — the glued `\` is what tells the two apart. */`` |
|        13 | 5019 | `				xCons = 0;` |
|   2580340 | 5020 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   1514929 | 5021 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 5022 | `				/* Try to extract a language construct handler */` |
|   1514929 | 5023 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   1514929 | 5024 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|         - | 5025 | ``					/* A word that opens no statement (`else`, `endwhile`, `case`,`` |
|         - | 5026 | ``					 * `public`...): php's parser names it as a token. */`` |
|        86 | 5027 | `					rc = PH7_GenSyntaxError(pGen,pGen->pIn,PH7_GenStrayStatementTail(pGen));` |
|        86 | 5028 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 5029 | `						break;` |
|         - | 5030 | `					}` |
|         - | 5031 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 5032 | `					 * this erroneous statement.` |
|         - | 5033 | `					 */` |
|        86 | 5034 | `					xCons = PH7_ErrorRecover;` |
|        42 | 5035 | `				}` |
|   1821866 | 5036 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    161510 | 5037 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 5038 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       225 | 5039 | `				xCons = PH7_CompileLabel;` |
|       110 | 5040 | `			}` |
|   2580777 | 5041 | `			if( xCons == 0 ){` |
|         - | 5042 | `				/* Assume an expression an try to compile it. A leading php 8.5` |
|         - | 5043 | ``				 * `(void)` cast is consumed here — statement head is one of the two`` |
|         - | 5044 | `				 * places its grammar takes one — and says the answer is dropped` |
|         - | 5045 | `				 * DELIBERATELY, so the call below is not marked. */` |
|   1066619 | 5046 | `				int bVoid = GenStateTakeVoidCast(&(*pGen));` |
|   1066619 | 5047 | `				if( bVoid && pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|         - | 5048 | `` 					/* php's grammar wants an expression after the cast: `(void);` `` |
|         - | 5049 | ``					 * is `syntax error, unexpected token ";"` there. */`` |
|         3 | 5050 | `					rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|         - | 5051 | `						"syntax error, unexpected token \";\"");` |
|         2 | 5052 | `				}else{` |
|   1066617 | 5053 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   1066617 | 5054 | `					if( rc != SXERR_EMPTY ){` |
|   1066195 | 5055 | `						if( !bVoid ){` |
|   1066181 | 5056 | `							GenStateMarkDiscardedCall(&(*pGen));` |
|    532179 | 5057 | `						}` |
|         - | 5058 | `						/* Pop l-value */` |
|   1066195 | 5059 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    532186 | 5060 | `					}` |
|         - | 5061 | `				}` |
|    532403 | 5062 | `			}else{` |
|         - | 5063 | `				/* Go compile the sucker */` |
|   1514163 | 5064 | `				rc = xCons(&(*pGen));` |
|   1514158 | 5065 | `				if( xCons == PH7_CompileLabel` |
|   2270012 | 5066 | `				 \|\| ( pGen->pTokenSet` |
|   1513938 | 5067 | `				   && pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet)` |
|   1513936 | 5068 | `				   && (pGen->pIn[-1].nType & PH7_TK_CCB/*'}'*/) ) ){` |
|         - | 5069 | `					/* A label, and any construct that ends with its own block, close` |
|         - | 5070 | `					 * themselves. (An EXPRESSION statement never does, which is why` |
|         - | 5071 | ``					 * this asks the construct and not just the last token: the `}` of`` |
|         - | 5072 | ``					 * `$f = function () {}` is not a terminator.) */`` |
|    940958 | 5073 | `					bStmtWantsSemi = 0;` |
|    469857 | 5074 | `				}` |
|         - | 5075 | `			}` |
|   2580777 | 5076 | `			if( rc == SXERR_ABORT ){` |
|         - | 5077 | `				/* Request to abort compilation */` |
|       117 | 5078 | `				break;` |
|         - | 5079 | `			}` |
|         - | 5080 | `		}` |
|         - | 5081 | `		/* Ignore trailing semi-colons ';' */` |
|         - | 5082 | `		{` |
|         - | 5083 | ``			/* Terminated when a `;` is sitting there for the loop to step over, or`` |
|         - | 5084 | `			 * when the construct consumed its own (the alternative-syntax bodies` |
|         - | 5085 | ``			 * take the `;` after their `endif`/`endwhile`/… themselves). */`` |
|   2580819 | 5086 | `			int bTerminated = (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI)) != 0;` |
|   2580814 | 5087 | `			if( !bTerminated && pGen->pTokenSet` |
|    942621 | 5088 | `			 && pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet)` |
|    942626 | 5089 | `			 && (pGen->pIn[-1].nType & PH7_TK_SEMI) ){` |
|      1129 | 5090 | `				bTerminated = 1;` |
|       562 | 5091 | `			}` |
|   4219014 | 5092 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|   1638200 | 5093 | `				pGen->pIn++;` |
|         5 | 5094 | `			}` |
|   2580814 | 5095 | `			if( !bTerminated && bStmtWantsSemi && pGen->nErr < 1 && GenStateAtChunkEof(&(*pGen))` |
|       132 | 5096 | `			 && pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ){` |
|         - | 5097 | `				/* (GenStateAtChunkEof already answered no for a NULL token set.) */` |
|         - | 5098 | `				/* Ran out of input with the statement still open. */` |
|        96 | 5099 | `				rc = PH7_GenSyntaxError(&(*pGen),0,GenStateEofExpecting(pStmtStart,&pGen->pIn[-1]));` |
|        96 | 5100 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 5101 | `					break;` |
|         - | 5102 | `				}` |
|        46 | 5103 | `			}` |
|         - | 5104 | `		}` |
|   2580819 | 5105 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 5106 | `			/* Compile a single statement and return */` |
|   2273555 | 5107 | `			break;` |
|         - | 5108 | `		}` |
|         - | 5109 | `		/* LOOP ONE */` |
|         - | 5110 | `		/* LOOP TWO */` |
|         - | 5111 | `		/* LOOP THREE */` |
|         - | 5112 | `		/* LOOP FOUR */` |
|         5 | 5113 | `	}` |
|         - | 5114 | `	/* Return compilation status */` |
|   2309305 | 5115 | `	return rc;` |
|         5 | 5116 | `}` |
|         - | 5117 | `/*` |
|         - | 5118 | `` * TRUE when the SOURCE bytes of a double-quoted string interpolate -- `$name`,`` |
|         - | 5119 | `` * `${`, or `{$` -- which is what decides whether php's scanner produced ONE`` |
|         - | 5120 | ` * string token for it or an opening quote followed by parts. A backslash escapes` |
|         - | 5121 | `` * whatever follows it, so `"\\$b"` does not interpolate.`` |
|         - | 5122 | ` */` |
|        26 | 5123 | `static int GenStateDqInterpolates(SyString *pStr)` |
|         2 | 5124 | `{` |
|        28 | 5125 | `	const unsigned char *z = (const unsigned char *)pStr->zString;` |
|        28 | 5126 | `	const unsigned char *zEnd = &z[pStr->nByte];` |
|        60 | 5127 | `	while( z < zEnd ){` |
|        44 | 5128 | `		if( z[0] == '\\' ){` |
|         5 | 5129 | `			z += 2;` |
|         5 | 5130 | `			continue;` |
|         - | 5131 | `		}` |
|        38 | 5132 | `		if( z[0] == '$' && &z[1] < zEnd` |
|         9 | 5133 | `		 && (z[1] == '{' \|\| z[1] >= 0x80 \|\| SyisAlpha(z[1]) \|\| z[1] == '_') ){` |
|         7 | 5134 | `			return 1;` |
|         - | 5135 | `		}` |
|        34 | 5136 | `		if( z[0] == '{' && &z[1] < zEnd && z[1] == '$' ){` |
|         5 | 5137 | `			return 1;` |
|         - | 5138 | `		}` |
|        30 | 5139 | `		z++;` |
|         2 | 5140 | `	}` |
|        18 | 5141 | `	return 0;` |
|        15 | 5142 | `}` |
|         - | 5143 | `/*` |
|         - | 5144 | ` * Compile a Raw PHP chunk.` |
|         - | 5145 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 5146 | ` * takes care of generating the appropriate error message.` |
|         - | 5147 | ` */` |
|     35708 | 5148 | `static sxi32 PH7_CompilePHP(` |
|         - | 5149 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 5150 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 5151 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 5152 | `	)` |
|         5 | 5153 | `{` |
|     35713 | 5154 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 5155 | `	sxi32 rc;` |
|     35713 | 5156 | `	if( pGen->pTokenSet == pTokenSet ){` |
|         - | 5157 | `		/* An earlier chunk of this unit; a nested unit's first chunk finds its` |
|         - | 5158 | `		 * includer's set here instead. */` |
|     20002 | 5159 | `		PH7_GenCarryBraces(pGen);` |
|     10004 | 5160 | `	}` |
|         - | 5161 | `	/* Reset the token set (and its trivia sidecar) */` |
|     35713 | 5162 | `	SySetReset(&(*pTokenSet));` |
|     35713 | 5163 | `	SySetReset(&pGen->aTrivia);` |
|         - | 5164 | `	/* Mark as the default token set */` |
|     35713 | 5165 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 5166 | `	/* Advance the stream cursor */` |
|     35713 | 5167 | `	pGen->pRawIn++;` |
|         - | 5168 | `	/* Tokenize the PHP chunk first */` |
|     35713 | 5169 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 5170 | ``	/* The raw tokenizer marked whether this chunk was closed by a `?>`; only one`` |
|         - | 5171 | `	 * that met the end of the FILE can leave a statement unterminated. */` |
|     35713 | 5172 | `	pGen->bChunkAtEof = (sxi8)(SX_PTR_TO_INT(pScript->pUserData) == 0);` |
|     35713 | 5173 | `	pGen->bChunkLast = pGen->bChunkAtEof;` |
|         - | 5174 | `	/* Point to the head and tail of the token stream. */` |
|     35713 | 5175 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     35713 | 5176 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|         - | 5177 | ``	/* php REMOVED `(real)` in its SCANNER, so the refusal belongs to the chunk and`` |
|         - | 5178 | ``	 * not to the expression the cast sits in: `strlen(real)` -- where the token is`` |
|         - | 5179 | `	 * never an operator at all -- reports the same sentence, and reports it as a` |
|         - | 5180 | ``	 * PARSE error rather than the fatal `(unset)` gets from the compiler. */`` |
|         - | 5181 | `	{` |
|         - | 5182 | `		SyToken *pTok;` |
|     35713 | 5183 | `		sxi32 nBraceOpen = 0;` |
|  28696157 | 5184 | `		for( pTok = pGen->pIn ; pTok < pGen->pEnd ; pTok++ ){` |
|  28660449 | 5185 | `			if( pTok->nType & PH7_TK_OCB ){` |
|   1088878 | 5186 | `				nBraceOpen++;` |
|  28115290 | 5187 | `			}else if( pTok->nType & PH7_TK_CCB ){` |
|   1088824 | 5188 | `				nBraceOpen--;` |
|    543687 | 5189 | `			}` |
|  14308576 | 5190 | `		}` |
|     35713 | 5191 | `		pGen->nBraceNet += nBraceOpen;` |
|     35713 | 5192 | `		if( nBraceOpen > 0 ){` |
|         - | 5193 | ``			/* A `{` this chunk never closes: php's parser reports THAT at the end of`` |
|         - | 5194 | `			 * the file, ahead of any statement it left open and ahead of a string or` |
|         - | 5195 | `			 * heredoc the scanner was still inside. Stand the end-of-input questions` |
|         - | 5196 | ``			 * down and let the block compiler say its own `Unclosed '{'`. */`` |
|        67 | 5197 | `			pGen->bChunkAtEof = 0;` |
|        31 | 5198 | `		}` |
|  28696121 | 5199 | `		for( pTok = pGen->pIn ; pTok < pGen->pEnd ; pTok++ ){` |
|  28660441 | 5200 | `			if( pTok->nType & PH7_TK_ALIAS_CAST ){` |
|         - | 5201 | `				/* php 8.5 announces the four alias SPELLINGS -- (integer), (boolean),` |
|         - | 5202 | `				 * (double), (binary) -- from its SCANNER, not from the compiler: the` |
|         - | 5203 | ``				 * sentence comes out for `strlen(integer)`, where the token is never a`` |
|         - | 5204 | `				 * cast at all, and it comes out ahead of a parse error further down the` |
|         - | 5205 | `				 * file. It is one line per OCCURRENCE in the source, not per execution:` |
|         - | 5206 | `				 * a cast inside a function nobody calls still announces itself, and one` |
|         - | 5207 | `				 * inside a loop announces itself once. Nothing about the cast changes --` |
|         - | 5208 | `				 * the spelling is what is deprecated -- so this only reports, and the` |
|         - | 5209 | `				 * walk carries on to the rest of the chunk.` |
|         - | 5210 | `				 *` |
|         - | 5211 | `				 * Each of the four is the only alias of its target, so the canonical` |
|         - | 5212 | `				 * token text the lexer left behind names both halves of the sentence. */` |
|         - | 5213 | `				static const struct { const char *zCanon; int nCanon; const char *zAlias; } aAlias[] = {` |
|         - | 5214 | `					{ "(int)",    5, "integer" }, { "(bool)",   6, "boolean" },` |
|         - | 5215 | `					{ "(float)",  7, "double"  }, { "(string)", 8, "binary"  }` |
|         - | 5216 | `				};` |
|         - | 5217 | `				sxu32 i;` |
|       106 | 5218 | `				for( i = 0 ; i < SX_ARRAYSIZE(aAlias) ; ++i ){` |
|       104 | 5219 | `					if( pTok->sData.nByte == (sxu32)aAlias[i].nCanon` |
|        73 | 5220 | `					 && SyMemcmp((const void *)pTok->sData.zString,` |
|        57 | 5221 | `					             (const void *)aAlias[i].zCanon,pTok->sData.nByte) == 0 ){` |
|        59 | 5222 | `						PH7_GenCompileError(pGen,8192 /* E_DEPRECATED */,pTok->nLine,` |
|         - | 5223 | `							"Non-canonical cast (%s) is deprecated, use the %s cast instead",` |
|        38 | 5224 | `							aAlias[i].zAlias,aAlias[i].zCanon);` |
|        40 | 5225 | `						break;` |
|         - | 5226 | `					}` |
|        35 | 5227 | `				}` |
|        19 | 5228 | `			}` |
|  28660436 | 5229 | `			if( (pTok->nType & PH7_TK_OP) && pTok->sData.nByte == sizeof("(real)")-1` |
|   2226727 | 5230 | `			 && SyMemcmp((const void *)pTok->sData.zString,(const void *)"(real)",sizeof("(real)")-1) == 0 ){` |
|         5 | 5231 | `				return PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,` |
|         - | 5232 | `					"The (real) cast has been removed, use (float) instead");` |
|         - | 5233 | `			}` |
|  28660432 | 5234 | `			if( (pTok->nType & PH7_TK_UNTERM)` |
|  14308578 | 5235 | `			 && (pTok->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC))` |
|        27 | 5236 | `			 && pGen->bChunkAtEof == 0 ){` |
|         - | 5237 | `				/* php's parser reaches the end of file with the string still open and` |
|         - | 5238 | `				 * reports the UNCLOSED BRACE first -- the scanner is mid-interpolation` |
|         - | 5239 | `				 * there and has nothing of its own to say. (A single-quoted string and` |
|         - | 5240 | `				 * a block comment do: their sentences win over the brace, which is why` |
|         - | 5241 | `				 * only these three yield.) Leave it to the compile below. */` |
|         3 | 5242 | `				continue;` |
|         - | 5243 | `			}` |
|  28660435 | 5244 | `			if( pTok->nType & PH7_TK_UNTERM ){` |
|         - | 5245 | `				/* A quote, heredoc or block comment the input ran out under. php` |
|         - | 5246 | `				 * refuses the file for each; this used to take the rest of it as the` |
|         - | 5247 | ``				 * lexeme's body and RUN the program (`<?php echo 'a` printed `a`).`` |
|         - | 5248 | `				 * The wording is php's own per shape -- its scanner reports what it` |
|         - | 5249 | `				 * was still waiting for. */` |
|        24 | 5250 | `				if( pTok->nType & PH7_TK_SSTR ){` |
|         - | 5251 | `					/* php's single-quoted scanner hands the parser the CONTENT it` |
|         - | 5252 | `					 * had read, and the parser names that. */` |
|         6 | 5253 | `					return PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,` |
|         2 | 5254 | `						"syntax error, unexpected string content \"%z\"",&pTok->sData);` |
|         - | 5255 | `				}` |
|        20 | 5256 | `				if( pTok->nType & PH7_TK_DSTR ){` |
|        12 | 5257 | `					const char *zExp = pTok->sData.nByte < 1` |
|         - | 5258 | `						? "variable or string content or \"${\" or \"{$\""` |
|         7 | 5259 | `						: (GenStateDqInterpolates(&pTok->sData) ? 0` |
|         - | 5260 | `						                                       : "variable or \"${\" or \"{$\"");` |
|         4 | 5261 | `					return zExp` |
|         6 | 5262 | `						? PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,` |
|         2 | 5263 | `							"syntax error, unexpected end of file, expecting %s",zExp)` |
|         8 | 5264 | `						: PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,` |
|         - | 5265 | `							"syntax error, unexpected end of file");` |
|         - | 5266 | `				}` |
|        12 | 5267 | `				if( pTok->nType & (PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){` |
|         - | 5268 | `					/* An EMPTY body, or one that interpolates, leaves php's parser` |
|         - | 5269 | `					 * with nothing to expect but the end it just met. */` |
|        15 | 5270 | `					int bSet = pTok->sData.nByte > 0` |
|         8 | 5271 | `						&& !((pTok->nType & PH7_TK_HEREDOC) && GenStateDqInterpolates(&pTok->sData));` |
|         4 | 5272 | `					return bSet` |
|         4 | 5273 | `						? PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,` |
|         - | 5274 | `							"syntax error, unexpected end of file, "` |
|         - | 5275 | `							"expecting variable or heredoc end or \"${\" or \"{$\"")` |
|         8 | 5276 | `						: PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,` |
|         - | 5277 | `							"syntax error, unexpected end of file");` |
|         - | 5278 | `				}` |
|         - | 5279 | `				/* A block comment, whose sentence names where it began. */` |
|         6 | 5280 | `				return PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,` |
|         2 | 5281 | `					"Unterminated comment starting line %u",pTok->nLine);` |
|         - | 5282 | `			}` |
|  14308557 | 5283 | `		}` |
|         - | 5284 | `	}` |
|     35685 | 5285 | `	if( is_expr ){` |
|       ! 0 | 5286 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 5287 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 5288 | `			/* A simple expression,compile it */` |
|       ! 0 | 5289 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 5290 | `		}` |
|         - | 5291 | `		/* Emit the DONE instruction */` |
|       ! 0 | 5292 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 5293 | `		return SXRET_OK;` |
|         - | 5294 | `	}` |
|     35685 | 5295 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 5296 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 5297 | `		/*` |
|         - | 5298 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 5299 | `		 * According to the PHP reference manual:` |
|         - | 5300 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 5301 | `		 *  immediately follow` |
|         - | 5302 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 5303 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 5304 | `		 * Symisc extension:` |
|         - | 5305 | `		 *   This short syntax works with all PHP opening` |
|         - | 5306 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 5307 | `		 *   only short tag.` |
|         - | 5308 | `		 */` |
|         - | 5309 | ``		/* `<?=` opens the block with an `echo` STATEMENT, and the block goes on`` |
|         - | 5310 | ``		 * after it: `<?= 2; echo 3 ?>` prints both, and `<?= 2` at the end of the`` |
|         - | 5311 | ``		 * input wants its `;` as `<?php echo 2` does. Rename the token and let`` |
|         - | 5312 | `		 * the statement loop compile the whole block. */` |
|        68 | 5313 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|        68 | 5314 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|        68 | 5315 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|        33 | 5316 | `	}` |
|         - | 5317 | `	/* Compile the PHP chunk */` |
|     35685 | 5318 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 5319 | `	/* Fix exceptions jumps */` |
|     35685 | 5320 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 5321 | `	/* Fix gotos now, the jump destination is resolved */` |
|     35685 | 5322 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 5323 | `		rc = SXERR_ABORT;` |
|         1 | 5324 | `	}` |
|         - | 5325 | `	/* Reset container */` |
|     35685 | 5326 | `	SySetReset(&pGen->aGoto);` |
|     35685 | 5327 | `	SySetReset(&pGen->aLabel);` |
|     35685 | 5328 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 5329 | `	/* Compilation result */` |
|     35685 | 5330 | `	return rc;` |
|     17848 | 5331 | `}` |
|         - | 5332 | `/*` |
|         - | 5333 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 5334 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 5335 | ` * This is the only compile interface exported from this file.` |
|         - | 5336 | ` */` |
|     39762 | 5337 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 5338 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 5339 | `	SyString *pScript,  /* Script to compile */` |
|         - | 5340 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 5341 | `	)` |
|         5 | 5342 | `{` |
|         - | 5343 | `	SySet aPhpToken,aRawToken;` |
|         - | 5344 | `	ph7_gen_state *pCodeGen;` |
|         - | 5345 | `	ph7_value *pRawObj;` |
|         - | 5346 | `	sxu32 nObjIdx;` |
|         - | 5347 | `	sxi32 nRawObj;` |
|         - | 5348 | `	int is_expr;` |
|         - | 5349 | `	sxi8 bSavedStrict;` |
|         - | 5350 | `	sxi8 bSavedStrictLocked;` |
|         - | 5351 | `	sxi8 bSavedNsNamed,bSavedNsBracketed,bSavedInNsBlock;` |
|         - | 5352 | `	sxi8 bSavedHalted,bSavedHaltSeen;` |
|         - | 5353 | `	sxu32 aSavedBraceCarry[PHL_BRACE_CARRY];` |
|         - | 5354 | `	sxi32 nSavedBraceCarry;` |
|         - | 5355 | `	sxu32 nSavedHaltOffset;` |
|         - | 5356 | `	const char *zSavedScriptBase;` |
|         - | 5357 | `	const char *zFileBase;` |
|         - | 5358 | `	SyToken *pSavedIn,*pSavedEnd;` |
|         - | 5359 | `	sxi32 rc;` |
|     39767 | 5360 | `	sxu32 nBaseLine = 1;` |
|     39767 | 5361 | `	if( pScript->nByte < 1 ){` |
|         - | 5362 | `		/* Nothing to compile */` |
|        12 | 5363 | `		return PH7_OK;` |
|         - | 5364 | `	}` |
|         - | 5365 | `	/* Kept before the shebang skip below: php counts __COMPILER_HALT_OFFSET__` |
|         - | 5366 | `	 * from the first byte on DISK, shebang line included. */` |
|     39757 | 5367 | `	zFileBase = pScript->zString;` |
|         - | 5368 | `	/* php skips a "#!" shebang on the first line of a CLI script: consume it` |
|         - | 5369 | `	 * (including its newline) so it is not echoed as inline text, and bump the` |
|         - | 5370 | `	 * base line to 2 so the code below still reports php-matching line numbers. */` |
|     39757 | 5371 | `	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){` |
|         6 | 5372 | `		const char *z = pScript->zString;` |
|         6 | 5373 | `		const char *zEnd = &z[pScript->nByte];` |
|        78 | 5374 | `		while( z < zEnd && z[0] != '\n' ){ z++; }` |
|         6 | 5375 | `		if( z < zEnd ){ z++; } /* consume the newline too */` |
|         6 | 5376 | `		pScript->nByte -= (sxu32)(z - pScript->zString);` |
|         6 | 5377 | `		pScript->zString = z;` |
|         6 | 5378 | `		nBaseLine = 2;` |
|         6 | 5379 | `		if( pScript->nByte < 1 ){` |
|       ! 0 | 5380 | `			return PH7_OK;` |
|         - | 5381 | `		}` |
|         2 | 5382 | `	}` |
|         - | 5383 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 5384 | `	 * file's flags so include/require restore them on return. */` |
|     39757 | 5385 | `	pCodeGen = &pVm->sCodeGen;` |
|         - | 5386 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|         - | 5387 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|         - | 5388 | `	 * each instruction's source line, and instructions are still emitted after this` |
|         - | 5389 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|         - | 5390 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|         - | 5391 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|     39757 | 5392 | `	pSavedIn = pCodeGen->pIn;` |
|     39757 | 5393 | `	pSavedEnd = pCodeGen->pEnd;` |
|     39757 | 5394 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     39757 | 5395 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     39757 | 5396 | `	pCodeGen->bStrictTypes = 0;` |
|     39757 | 5397 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 5398 | `	/* The namespace placement state is per FILE as well (php's file_context). */` |
|     39757 | 5399 | `	bSavedNsNamed = pCodeGen->bNsNamed;` |
|     39757 | 5400 | `	bSavedNsBracketed = pCodeGen->bNsBracketed;` |
|     39757 | 5401 | `	bSavedInNsBlock = pCodeGen->bInNsBlock;` |
|     39757 | 5402 | `	pCodeGen->bNsNamed = 0;` |
|     39757 | 5403 | `	pCodeGen->bNsBracketed = 0;` |
|     39757 | 5404 | `	pCodeGen->bInNsBlock = 0;` |
|         - | 5405 | `	/* The halt is per-FILE too, and an include compiles inside its includer. */` |
|     39757 | 5406 | `	bSavedHalted = pCodeGen->bHalted;` |
|     39757 | 5407 | `	bSavedHaltSeen = pCodeGen->bHaltSeen;` |
|     39757 | 5408 | `	nSavedHaltOffset = pCodeGen->nHaltOffset;` |
|     39757 | 5409 | `	zSavedScriptBase = pCodeGen->zScriptBase;` |
|     39757 | 5410 | `	pCodeGen->bHalted = 0;` |
|     39757 | 5411 | `	pCodeGen->bHaltSeen = 0;` |
|     39757 | 5412 | `	pCodeGen->nHaltOffset = 0;` |
|     39757 | 5413 | `	pCodeGen->zScriptBase = zFileBase;` |
|         - | 5414 | `	/* So is the scanner's bracket stack. */` |
|     39757 | 5415 | `	SyMemcpy((const void *)pCodeGen->aBraceCarry,(void *)aSavedBraceCarry,sizeof(aSavedBraceCarry));` |
|     39757 | 5416 | `	nSavedBraceCarry = pCodeGen->nBraceCarry;` |
|         - | 5417 | `	/* Initialize the tokens containers */` |
|     39757 | 5418 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     39757 | 5419 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     39757 | 5420 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     39757 | 5421 | `	is_expr = 0;` |
|     39757 | 5422 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 5423 | `		SyToken sTmp;` |
|         - | 5424 | `		/* PHP only: -*/` |
|      9196 | 5425 | `		sTmp.nLine = 1;` |
|      9196 | 5426 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|      9196 | 5427 | `		sTmp.pUserData = 0;` |
|      9196 | 5428 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|      9196 | 5429 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|      9196 | 5430 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 5431 | `			/* A simple PHP expression */` |
|       ! 0 | 5432 | `			is_expr = 1;` |
|       ! 0 | 5433 | `		}` |
|      4595 | 5434 | `	}else{` |
|         - | 5435 | `		/* Tokenize raw text */` |
|     30566 | 5436 | `		SySetAlloc(&aRawToken,32);` |
|     30566 | 5437 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);` |
|         - | 5438 | `	}` |
|         - | 5439 | ``	/* Where the end of INPUT sits. php reports `unexpected end of file` at the line`` |
|         - | 5440 | `	 * the file ENDS on, which is past the last token whenever anything follows it --` |
|         - | 5441 | `	 * a trailing newline always does -- and the chunk a statement was cut off in` |
|         - | 5442 | `	 * does not carry that: the raw splitter hands it the source up to its last byte` |
|         - | 5443 | `	 * of code, newline excluded. Counted here, where the whole script is still in` |
|         - | 5444 | `	 * hand, and read back by PH7_GenSyntaxError's end-of-file branch. */` |
|         - | 5445 | `	{` |
|         - | 5446 | `		sxu32 i;` |
|     39757 | 5447 | `		pCodeGen->nChunkEofLine = nBaseLine;` |
|     39757 | 5448 | `		pCodeGen->nBraceNet = 0;` |
|     39757 | 5449 | `		pCodeGen->nBraceCarry = 0;` |
| 159417445 | 5450 | `		for( i = 0 ; i < pScript->nByte ; ++i ){` |
| 159377693 | 5451 | `			if( pScript->zString[i] == '\n' ){` |
|    259255 | 5452 | `				pCodeGen->nChunkEofLine++;` |
|    129201 | 5453 | `			}` |
|  79568795 | 5454 | `		}` |
|         - | 5455 | `	}` |
|         - | 5456 | `	/* Process high-level tokens */` |
|     39757 | 5457 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     39757 | 5458 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|         - | 5459 | `	/*` |
|         - | 5460 | ``	 * Where `__halt_compiler();` sits, decided BEFORE anything compiles: the`` |
|         - | 5461 | `	 * constant it defines may be read ahead of the statement that sets it (and` |
|         - | 5462 | `	 * in an earlier chunk than the one holding it), exactly as it may under php,` |
|         - | 5463 | `	 * whose compiler registers the constant for the whole file. The scan costs a` |
|         - | 5464 | `	 * second tokenization of every PHP chunk, so it only runs when the file` |
|         - | 5465 | `	 * contains the identifier at all -- which no ordinary program does.` |
|         - | 5466 | `	 */` |
|     39757 | 5467 | `	if( GenStateMentionsHalt(pScript->zString,pScript->nByte) ){` |
|        47 | 5468 | `		GenStateScanHaltOffset(pCodeGen,&aRawToken,zFileBase);` |
|        22 | 5469 | `	}` |
|     39757 | 5470 | `	rc = PH7_OK;` |
|     39757 | 5471 | `	if( is_expr ){` |
|         - | 5472 | `		/* Compile the expression */` |
|       ! 0 | 5473 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 5474 | `		goto cleanup;` |
|         - | 5475 | `	}` |
|     39757 | 5476 | `	nObjIdx = 0;` |
|         - | 5477 | `	/* Start the compilation process */` |
|     35178 | 5478 | `	for(;;){` |
|    105966 | 5479 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     39621 | 5480 | `			break; /* No more tokens to process */` |
|         - | 5481 | `		}` |
|     66350 | 5482 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 5483 | `			/* Compile the PHP chunk */` |
|     35713 | 5484 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     35713 | 5485 | `			if( rc == SXERR_ABORT ){` |
|       115 | 5486 | `				break;` |
|         - | 5487 | `			}` |
|     35603 | 5488 | `			if( pCodeGen->bHalted ){` |
|         - | 5489 | ``				/* `__halt_compiler();` -- the rest of the FILE is data, inline`` |
|         - | 5490 | `				 * text between later chunks included. */` |
|        28 | 5491 | `				break;` |
|         - | 5492 | `			}` |
|     35577 | 5493 | `			continue;` |
|         - | 5494 | `		}` |
|         - | 5495 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     30642 | 5496 | `		nRawObj = 0;` |
|     61331 | 5497 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 5498 | `			/* Consume the raw chunk without any processing */` |
|     30694 | 5499 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     30694 | 5500 | `			if( pRawObj == 0 ){` |
|       ! 0 | 5501 | `				rc = SXERR_MEM;` |
|       ! 0 | 5502 | `				break;` |
|         - | 5503 | `			}` |
|         - | 5504 | `			/* Mark as constant and emit the load constant instruction */` |
|     30694 | 5505 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     30694 | 5506 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     30694 | 5507 | `			++nRawObj;` |
|     30694 | 5508 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 5509 | `		}` |
|     30642 | 5510 | `		if( nRawObj > 0 ){` |
|         - | 5511 | `			/* Emit the consume instruction */` |
|     30642 | 5512 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     15313 | 5513 | `		}` |
|     19870 | 5514 | `	}` |
|     19887 | 5515 | `cleanup:` |
|         - | 5516 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|     39757 | 5517 | `	pCodeGen->pIn = pSavedIn;` |
|     39757 | 5518 | `	pCodeGen->pEnd = pSavedEnd;` |
|     39757 | 5519 | `	SySetRelease(&aRawToken);` |
|     39757 | 5520 | `	SySetRelease(&aPhpToken);` |
|         - | 5521 | `	/* Restore outer file's strict_types scope */` |
|     39757 | 5522 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     39757 | 5523 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|     39757 | 5524 | `	pCodeGen->bNsNamed = bSavedNsNamed;` |
|     39757 | 5525 | `	pCodeGen->bNsBracketed = bSavedNsBracketed;` |
|     39757 | 5526 | `	pCodeGen->bInNsBlock = bSavedInNsBlock;` |
|         - | 5527 | `	/* ...and its halt state. */` |
|     39757 | 5528 | `	pCodeGen->bHalted = bSavedHalted;` |
|     39757 | 5529 | `	pCodeGen->bHaltSeen = bSavedHaltSeen;` |
|     39757 | 5530 | `	pCodeGen->nHaltOffset = nSavedHaltOffset;` |
|     39757 | 5531 | `	pCodeGen->zScriptBase = zSavedScriptBase;` |
|     39757 | 5532 | `	SyMemcpy((const void *)aSavedBraceCarry,(void *)pCodeGen->aBraceCarry,sizeof(aSavedBraceCarry));` |
|     39757 | 5533 | `	pCodeGen->nBraceCarry = nSavedBraceCarry;` |
|     39757 | 5534 | `	return rc;` |
|     19875 | 5535 | `}` |
|         - | 5536 | `/*` |
|         - | 5537 | ` * Utility routines.Initialize the code generator.` |
|         - | 5538 | ` */` |
|      8445 | 5539 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 5540 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 5541 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 5542 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 5543 | `	)` |
|         5 | 5544 | `{` |
|      8450 | 5545 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 5546 | `	/* Zero the structure */` |
|      8450 | 5547 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 5548 | `	/* Initial state */` |
|      8450 | 5549 | `	pGen->pVm  = &(*pVm);` |
|      8450 | 5550 | `	pGen->xErr = xErr;` |
|      8450 | 5551 | `	pGen->pErrData = pErrData;` |
|      8450 | 5552 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      8450 | 5553 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      8450 | 5554 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|      8450 | 5555 | `	SySetInit(&pGen->aScope,&pVm->sAllocator,sizeof(GenScope));` |
|      8450 | 5556 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|      8450 | 5557 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      8450 | 5558 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      8450 | 5559 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      8450 | 5560 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      8450 | 5561 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 5562 | `	/* Error log buffer */` |
|      8450 | 5563 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|      8450 | 5564 | `	SyBlobInit(&pGen->sFirstErr,&pVm->sAllocator);` |
|         - | 5565 | `	/* General purpose working buffer */` |
|      8450 | 5566 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 5567 | `	/* Namespace state */` |
|      8450 | 5568 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      8450 | 5569 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|      8450 | 5570 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|         - | 5571 | `	/* Create the global scope */` |
|      8450 | 5572 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 5573 | `	/* Point to the global scope */` |
|      8450 | 5574 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      8450 | 5575 | `	return SXRET_OK;` |
|         5 | 5576 | `}` |
|         - | 5577 | `/*` |
|         - | 5578 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 5579 | ` */` |
|     46741 | 5580 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 5581 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 5582 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 5583 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 5584 | `	)` |
|         5 | 5585 | `{` |
|     46746 | 5586 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 5587 | `	GenBlock *pBlock,*pParent;` |
|         - | 5588 | `	/* Reset state */` |
|     46746 | 5589 | `	SySetReset(&pGen->aLabel);` |
|     46746 | 5590 | `	SySetReset(&pGen->aGoto);` |
|     46746 | 5591 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     46746 | 5592 | `	SySetReset(&pGen->aTrivia);` |
|     46746 | 5593 | `	SySetReset(&pGen->aPendingAttrs);` |
|     46746 | 5594 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     46746 | 5595 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     46746 | 5596 | `	SyBlobRelease(&pGen->sFirstErr);` |
|     46746 | 5597 | `	SyBlobRelease(&pGen->sWorker);` |
|     46746 | 5598 | `	SyBlobRelease(&pGen->sNamespace);` |
|     46746 | 5599 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     46746 | 5600 | `	GenStateResetUseImports(&(*pGen),&(*pVm));` |
|         - | 5601 | `	/* A fresh compile unit has declared nothing yet. */` |
|     46746 | 5602 | `	GenStateResetSeenSymbols(&(*pGen),&(*pVm));` |
|         - | 5603 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 5604 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 5605 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 5606 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 5607 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 5608 | `	 * number of unique names, which is acceptable. */` |
|         - | 5609 | `	/* Point to the global scope */` |
|     46746 | 5610 | `	pBlock = pGen->pCurrent;` |
|     46974 | 5611 | `	while( pBlock->pParent != 0 ){` |
|       230 | 5612 | `		pParent = pBlock->pParent;` |
|       230 | 5613 | `		GenStateFreeBlock(pBlock);` |
|       230 | 5614 | `		pBlock = pParent;` |
|         2 | 5615 | `	}` |
|     46746 | 5616 | `	pGen->xErr = xErr;` |
|     46746 | 5617 | `	pGen->pErrData = pErrData;` |
|     46746 | 5618 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     46746 | 5619 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     46746 | 5620 | `	pGen->pIn = pGen->pEnd = 0;` |
|     46746 | 5621 | `	pGen->nErr = 0;` |
|     46746 | 5622 | `	pGen->nFatal = 0;` |
|     46746 | 5623 | `	pGen->nFirstErrLine = 0;` |
|     46746 | 5624 | `	pGen->bParseThrows = 0;` |
|     46746 | 5625 | `	pGen->bDeclCheck = 0;` |
|     46746 | 5626 | `	pGen->pOblige = 0;     /* an outer class's unresolved pairs are not this unit's */` |
|     46746 | 5627 | `	pGen->bObligeRun = 0;` |
|     46746 | 5628 | `	pGen->bDeclQuiet = pVm->bDeclQuietNext;` |
|     46746 | 5629 | `	pVm->bDeclQuietNext = 0;` |
|     46746 | 5630 | `	pGen->iFatalTrace = PH7_FATAL_TRACE_COMPILE;` |
|         - | 5631 | `	/* Clear the class-body context (a prior compile aborted mid-class-body would` |
|         - | 5632 | `	 * otherwise leave these live for the next eval/include on this VM). */` |
|     46746 | 5633 | `	pGen->pCurClass = 0;` |
|     46746 | 5634 | `	pGen->pCurClassBlock = 0;` |
|     46746 | 5635 | `	pGen->iInMemberDefault = 0;` |
|     46746 | 5636 | `	return SXRET_OK;` |
|         5 | 5637 | `}` |
|         - | 5638 | `/*` |
|         - | 5639 | ` * Save the code generator's compile-position state and hand the live generator a` |
|         - | 5640 | ` * fresh, empty one for a NESTED compilation unit.` |
|         - | 5641 | ` *` |
|         - | 5642 | ` * A require/include normally runs at execution time, when no compile is in flight,` |
|         - | 5643 | ` * so VmEvalChunk can call PH7_ResetCodeGenerator to wipe the generator. But an` |
|         - | 5644 | `` * autoload can fire in the MIDDLE of compiling a class: resolving `class Child`` |
|         - | 5645 | `` * extends Base` calls PH7_VmExtractClass, which triggers the autoloader, whose`` |
|         - | 5646 | `` * body `require`s Base's file — a nested compile while the outer one is mid-token.`` |
|         - | 5647 | ` * PH7_ResetCodeGenerator would then blow away the outer compile's cursor` |
|         - | 5648 | ` * (pIn/pEnd), current block, use-imports, labels, etc.; the outer parse resumes` |
|         - | 5649 | ` * with pIn == NULL and reports a bogus "Expected '{' after class 'Child'". (Common` |
|         - | 5650 | ` * in every real framework: Composer autoloads parent classes on demand.)` |
|         - | 5651 | ` *` |
|         - | 5652 | ` * This snapshots the position/scope fields into *pSaved (a caller-stack` |
|         - | 5653 | ` * ph7_gen_state used purely as storage) and re-initializes the live generator's` |
|         - | 5654 | ` * position containers to FRESH, empty ones WITHOUT releasing the outer's (the` |
|         - | 5655 | ` * snapshot now owns those). hLiteral/hNumLiteral/hVar are intentionally left LIVE` |
|         - | 5656 | ` * and shared — compiled bytecode interns names/literals into them and they grow` |
|         - | 5657 | ` * monotonically (exactly why PH7_ResetCodeGenerator keeps them); restoring an old` |
|         - | 5658 | ` * copy of those headers after a nested grow would use-after-free the bucket array.` |
|         - | 5659 | ` */` |
|         6 | 5660 | `PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData)` |
|         1 | 5661 | `{` |
|         7 | 5662 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 5663 | `	/* Shallow-copy every field; the position containers below are then replaced` |
|         - | 5664 | `	 * with fresh ones on the live struct, so *pSaved keeps the outer's memory. */` |
|         7 | 5665 | `	*pSaved = *pGen;` |
|         7 | 5666 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|         7 | 5667 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|         7 | 5668 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|         7 | 5669 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|         7 | 5670 | `	SySetInit(&pGen->aScope,&pVm->sAllocator,sizeof(GenScope));` |
|         7 | 5671 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         7 | 5672 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         7 | 5673 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         7 | 5674 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         7 | 5675 | `	SyBlobInit(&pGen->sFirstErr,&pVm->sAllocator);` |
|         7 | 5676 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|         7 | 5677 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|         7 | 5678 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|         - | 5679 | `	/* Fresh global scope for the nested unit (address of the embedded sGlobal is` |
|         - | 5680 | `	 * stable, so any outer block still parented to it stays valid across restore). */` |
|         7 | 5681 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(pVm),0);` |
|         7 | 5682 | `	pGen->pCurrent = &pGen->sGlobal;` |
|         7 | 5683 | `	pGen->pIn = pGen->pEnd = 0;` |
|         7 | 5684 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|         7 | 5685 | `	pGen->pTokenSet = 0;` |
|         7 | 5686 | `	pGen->nErr = 0;` |
|         7 | 5687 | `	pGen->nFatal = 0;` |
|         7 | 5688 | `	pGen->nFirstErrLine = 0;` |
|         7 | 5689 | `	pGen->bParseThrows = 0;` |
|         7 | 5690 | `	pGen->bDeclCheck = 0;` |
|         7 | 5691 | `	pGen->pOblige = 0;     /* an outer class's unresolved pairs are not this unit's */` |
|         7 | 5692 | `	pGen->bObligeRun = 0;` |
|         7 | 5693 | `	pGen->bDeclQuiet = pVm->bDeclQuietNext;` |
|         7 | 5694 | `	pVm->bDeclQuietNext = 0;` |
|         7 | 5695 | `	pGen->iFatalTrace = PH7_FATAL_TRACE_COMPILE;` |
|         7 | 5696 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|         7 | 5697 | `	pGen->nCommaExprOk = 0;` |
|         7 | 5698 | `	pGen->bInFramelessNsArgs = 0;` |
|         7 | 5699 | `	pGen->zClauseCloser = 0;` |
|         7 | 5700 | `	pGen->bInGenerator = 0;` |
|         7 | 5701 | `	pGen->bStrictTypes = 0;` |
|         7 | 5702 | `	pGen->bStrictTypesLocked = 0;` |
|         7 | 5703 | `	pGen->bNsNamed = 0;` |
|         7 | 5704 | `	pGen->bNsBracketed = 0;` |
|         7 | 5705 | `	pGen->bInNsBlock = 0;` |
|         - | 5706 | `	/* The nested unit is a fresh top-level compile: it is not lexically inside the` |
|         - | 5707 | `	 * outer's class body nor its member default, so a __TRAIT__ in the nested file` |
|         - | 5708 | `	 * must not inherit the outer's trait. (Restore below carries the outer's values` |
|         - | 5709 | `	 * back, so only the nested unit sees these zeros.) */` |
|         7 | 5710 | `	pGen->pCurClass = 0;` |
|         7 | 5711 | `	pGen->pCurClassBlock = 0;` |
|         7 | 5712 | `	pGen->iInMemberDefault = 0;` |
|         7 | 5713 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|         7 | 5714 | `	pGen->xErr = xErr;` |
|         7 | 5715 | `	pGen->pErrData = pErrData;` |
|         7 | 5716 | `}` |
|         - | 5717 | `/*` |
|         - | 5718 | ` * Restore the outer compile-position state saved by PH7_CompilerSaveState,` |
|         - | 5719 | ` * releasing the nested unit's position containers first. The shared` |
|         - | 5720 | ` * hLiteral/hNumLiteral/hVar tables (grown by the nested compile) are carried` |
|         - | 5721 | ` * forward, NOT rolled back to the snapshot's stale headers.` |
|         - | 5722 | ` */` |
|         6 | 5723 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved)` |
|         1 | 5724 | `{` |
|         7 | 5725 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 5726 | `	GenBlock *pBlock,*pParent;` |
|         - | 5727 | `	SyHash hVar,hLiteral,hNumLiteral;` |
|         - | 5728 | `	/* Free any nested blocks left open (e.g. an aborted nested compile), then the` |
|         - | 5729 | `	 * nested global block's own fixup sets. */` |
|         7 | 5730 | `	pBlock = pGen->pCurrent;` |
|         7 | 5731 | `	while( pBlock && pBlock->pParent != 0 ){` |
|       ! 0 | 5732 | `		pParent = pBlock->pParent;` |
|       ! 0 | 5733 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 5734 | `		pBlock = pParent;` |
|       ! 0 | 5735 | `	}` |
|         7 | 5736 | `	GenStateReleaseBlock(&pGen->sGlobal);` |
|         - | 5737 | `	/* Release the nested unit's position containers. */` |
|         7 | 5738 | `	SySetRelease(&pGen->aLabel);` |
|         7 | 5739 | `	SySetRelease(&pGen->aGoto);` |
|         7 | 5740 | `	SySetRelease(&pGen->aNullsafeJmp);` |
|         7 | 5741 | `	SySetRelease(&pGen->aLoopParent);` |
|         7 | 5742 | `	SySetRelease(&pGen->aScope);` |
|         7 | 5743 | `	SySetRelease(&pGen->aTrivia);` |
|         7 | 5744 | `	SySetRelease(&pGen->aPendingAttrs);` |
|         7 | 5745 | `	SyBlobRelease(&pGen->sWorker);` |
|         7 | 5746 | `	SyBlobRelease(&pGen->sErrBuf);` |
|         7 | 5747 | `	SyBlobRelease(&pGen->sFirstErr);` |
|         7 | 5748 | `	SyBlobRelease(&pGen->sNamespace);` |
|         7 | 5749 | `	SyHashRelease(&pGen->hUseImports);` |
|         7 | 5750 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|         7 | 5751 | `	SyHashRelease(&pGen->hUseConstImports);` |
|         7 | 5752 | `	GenStateReleaseSeenSymbols(&(*pGen));` |
|         - | 5753 | `	/* Preserve the (possibly grown) shared intern tables across the restore. */` |
|         7 | 5754 | `	hVar = pGen->hVar;` |
|         7 | 5755 | `	hLiteral = pGen->hLiteral;` |
|         7 | 5756 | `	hNumLiteral = pGen->hNumLiteral;` |
|         7 | 5757 | `	*pGen = *pSaved;` |
|         7 | 5758 | `	pGen->hVar = hVar;` |
|         7 | 5759 | `	pGen->hLiteral = hLiteral;` |
|         7 | 5760 | `	pGen->hNumLiteral = hNumLiteral;` |
|         7 | 5761 | `}` |
|         - | 5762 | `/*` |
|         - | 5763 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|         - | 5764 | ` * php's parser prints, e.g.` |
|         - | 5765 | ` *` |
|         - | 5766 | ` *   syntax error, unexpected token ";", expecting "{"` |
|         - | 5767 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|         - | 5768 | ` *   syntax error, unexpected end of file` |
|         - | 5769 | ` *` |
|         - | 5770 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|         - | 5771 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|         - | 5772 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|         - | 5773 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|         - | 5774 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|         - | 5775 | ` *` |
|         - | 5776 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|         - | 5777 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|         - | 5778 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|         - | 5779 | ` */` |
|         - | 5780 | `/*` |
|         - | 5781 | `` * Rebuild the `<<<LABEL` marker of a heredoc/nowdoc token from the source the`` |
|         - | 5782 | ` * token's BODY points into: the header always sits immediately above it. Answers` |
|         - | 5783 | ` * FALSE when no marker is found within reach, in which case the caller falls back` |
|         - | 5784 | ` * to the generic noun rather than guessing.` |
|         - | 5785 | ` */` |
|         8 | 5786 | `static int GenStateHeredocMarker(SyString *pBody,SyString *pOut)` |
|         1 | 5787 | `{` |
|         9 | 5788 | `	const unsigned char *z = (const unsigned char *)pBody->zString;` |
|         - | 5789 | `	const unsigned char *zLabelEnd;` |
|         - | 5790 | `	/* Walk the header BACKWARDS from the body, which begins one byte past the` |
|         - | 5791 | `	 * terminator of the marker's own line: line terminator, trailing blanks, the` |
|         - | 5792 | ``	 * closing quote, the LABEL, the opening quote, leading blanks, `<<<`. Every`` |
|         - | 5793 | `	 * step stops on a byte the next step owns, so the walk cannot leave the` |
|         - | 5794 | ``	 * header -- `<` is not a label byte and a label is what sits above the body. */`` |
|         9 | 5795 | `	z--;` |
|         9 | 5796 | `	if( z[0] == '\n' ){` |
|         9 | 5797 | `		z--;` |
|         9 | 5798 | `		if( z[0] == '\r' ){` |
|       ! 0 | 5799 | `			z--;` |
|       ! 0 | 5800 | `		}` |
|         4 | 5801 | `	}` |
|         9 | 5802 | `	while( z[0] == ' ' \|\| z[0] == '\t' ){` |
|       ! 0 | 5803 | `		z--;` |
|       ! 0 | 5804 | `	}` |
|         9 | 5805 | `	if( z[0] == '"' \|\| z[0] == '\'' ){` |
|         5 | 5806 | `		z--;` |
|         2 | 5807 | `	}` |
|         9 | 5808 | `	zLabelEnd = &z[1];` |
|        33 | 5809 | `	while( z[0] >= 0x80 \|\| SyisAlphaNum(z[0]) \|\| z[0] == '_' ){` |
|        25 | 5810 | `		z--;` |
|         1 | 5811 | `	}` |
|         9 | 5812 | `	if( zLabelEnd == &z[1] ){` |
|       ! 0 | 5813 | `		return 0; /* No label: not a header this routine can read back */` |
|         - | 5814 | `	}` |
|         9 | 5815 | `	if( z[0] == '"' \|\| z[0] == '\'' ){` |
|         5 | 5816 | `		z--;` |
|         2 | 5817 | `	}` |
|        15 | 5818 | `	while( z[0] == ' ' \|\| z[0] == '\t' ){` |
|         7 | 5819 | `		z--;` |
|         1 | 5820 | `	}` |
|         9 | 5821 | `	if( !(z[0] == '<' && z[-1] == '<' && z[-2] == '<') ){` |
|       ! 0 | 5822 | `		return 0;` |
|         - | 5823 | `	}` |
|         - | 5824 | ``	/* php's token text runs from `<<<` to the end of the LABEL -- the opening`` |
|         - | 5825 | `	 * quote of a nowdoc is inside it, the closing one is not. */` |
|         9 | 5826 | `	SyStringInitFromBuf(pOut,&z[-2],(sxu32)(zLabelEnd - &z[-2]));` |
|         9 | 5827 | `	return 1;` |
|         5 | 5828 | `}` |
|         - | 5829 | `/*` |
|         - | 5830 | `` * The chunk the compiler is about to leave: whatever `{` it still holds open`` |
|         - | 5831 | ` * stays on php's scanner stack under the next one.` |
|         - | 5832 | ` */` |
|     20065 | 5833 | `PH7_PRIVATE void PH7_GenCarryBraces(ph7_gen_state *pGen)` |
|         5 | 5834 | `{` |
|         - | 5835 | `	SyToken *pTok,*pChunkEnd;` |
|     20070 | 5836 | `	if( pGen->pTokenSet == 0 ){` |
|       ! 0 | 5837 | `		return;` |
|         - | 5838 | `	}` |
|     20070 | 5839 | `	pTok = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|     20070 | 5840 | `	pChunkEnd = &pTok[SySetUsed(pGen->pTokenSet)];` |
|     21374 | 5841 | `	for( ; pTok < pChunkEnd ; pTok++ ){` |
|      1308 | 5842 | `		if( pTok->nType & PH7_TK_OCB ){` |
|        44 | 5843 | `			if( pGen->nBraceCarry < PHL_BRACE_CARRY ){` |
|        44 | 5844 | `				pGen->aBraceCarry[pGen->nBraceCarry] = pTok->nLine;` |
|        20 | 5845 | `			}` |
|        44 | 5846 | `			pGen->nBraceCarry++;` |
|      1288 | 5847 | `		}else if( (pTok->nType & PH7_TK_CCB) && pGen->nBraceCarry > 0 ){` |
|        26 | 5848 | `			pGen->nBraceCarry--;` |
|        12 | 5849 | `		}` |
|       656 | 5850 | `	}` |
|     10043 | 5851 | `}` |
|         - | 5852 | `/*` |
|         - | 5853 | `` * A `)`, `]` or `}` the expression compiler found nothing to match. php's`` |
|         - | 5854 | ` * SCANNER refuses it before any parser sees it, against ONE stack of the` |
|         - | 5855 | `` * brackets open in the file -- the `{` of an enclosing block or function body`` |
|         - | 5856 | `` * included, and one an earlier `<?php` block left open -- and names the`` |
|         - | 5857 | `` * innermost: `Unclosed '{' does not match ')'`, plus the line it was opened on`` |
|         - | 5858 | ` * when that is another line. Only a closer with nothing open at all is` |
|         - | 5859 | `` * `Unmatched`. Replay that stack up to the closer.`` |
|         - | 5860 | ` */` |
|       118 | 5861 | `PH7_PRIVATE sxi32 PH7_GenUnmatchedCloser(ph7_gen_state *pGen,SyToken *pTok)` |
|         5 | 5862 | `{` |
|         - | 5863 | `	struct { char c; sxu32 nLine; } aNest[PHL_BRACE_CARRY];` |
|         - | 5864 | `	SyToken *pBase,*pCur;` |
|       123 | 5865 | `	sxi32 nNest = 0;` |
|         - | 5866 | `	sxi32 n;` |
|       123 | 5867 | `	if( pGen->pTokenSet ){` |
|       123 | 5868 | `		pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|       123 | 5869 | `		if( pTok >= pBase && pTok < &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|       123 | 5870 | `			nNest = pGen->nBraceCarry;` |
|       127 | 5871 | `			for( n = 0 ; n < nNest && n < PHL_BRACE_CARRY ; n++ ){` |
|         5 | 5872 | `				aNest[n].c = '{';` |
|         5 | 5873 | `				aNest[n].nLine = pGen->aBraceCarry[n];` |
|         3 | 5874 | `			}` |
|         - | 5875 | `			/* The first closer the stack refuses is the one php names -- an earlier` |
|         - | 5876 | `			 * one than this when the compiler let a crossed pair through` |
|         - | 5877 | ``			 * (`[(2 ]]` delimits its array at the first `]`). */`` |
|       873 | 5878 | `			for( pCur = pBase ; pCur <= pTok ; pCur++ ){` |
|       847 | 5879 | `				char cOpen = 0,cClose = 0,cTop;` |
|       847 | 5880 | `				if( pCur->nType & PH7_TK_LPAREN ){` |
|        80 | 5881 | `					cOpen = '(';` |
|       809 | 5882 | `				}else if( pCur->nType & PH7_TK_OSB ){` |
|        19 | 5883 | `					cOpen = '[';` |
|       762 | 5884 | `				}else if( pCur->nType & PH7_TK_OCB ){` |
|        74 | 5885 | `					cOpen = '{';` |
|       718 | 5886 | `				}else if( pCur->nType & PH7_TK_RPAREN ){` |
|       119 | 5887 | `					cClose = ')';` |
|       626 | 5888 | `				}else if( pCur->nType & PH7_TK_CSB ){` |
|        26 | 5889 | `					cClose = ']';` |
|       557 | 5890 | `				}else if( pCur->nType & PH7_TK_CCB ){` |
|        67 | 5891 | `					cClose = '}';` |
|        31 | 5892 | `				}` |
|       847 | 5893 | `				if( cOpen ){` |
|       168 | 5894 | `					if( nNest < PHL_BRACE_CARRY ){` |
|       168 | 5895 | `						aNest[nNest].c = cOpen;` |
|       168 | 5896 | `						aNest[nNest].nLine = pCur->nLine;` |
|        82 | 5897 | `					}` |
|       168 | 5898 | `					nNest++;` |
|       168 | 5899 | `					continue;` |
|         - | 5900 | `				}` |
|       683 | 5901 | `				if( cClose == 0 ){` |
|       483 | 5902 | `					continue;` |
|         - | 5903 | `				}` |
|       205 | 5904 | `				if( nNest == 0 ){` |
|        49 | 5905 | `					return PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,"Unmatched '%c'",cClose);` |
|         - | 5906 | `				}` |
|       160 | 5907 | `				if( nNest > PHL_BRACE_CARRY ){` |
|         - | 5908 | `					/* Deeper than the stack keeps: trust the pair. */` |
|       ! 0 | 5909 | `					nNest--;` |
|       ! 0 | 5910 | `					continue;` |
|         - | 5911 | `				}` |
|       160 | 5912 | `				cTop = aNest[nNest-1].c;` |
|       160 | 5913 | `				if( (cTop == '(' && cClose == ')') \|\| (cTop == '[' && cClose == ']') \|\| (cTop == '{' && cClose == '}') ){` |
|       112 | 5914 | `					nNest--;` |
|       112 | 5915 | `					continue;` |
|         - | 5916 | `				}` |
|        49 | 5917 | `				if( aNest[nNest-1].nLine != pCur->nLine ){` |
|        13 | 5918 | `					return PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|         8 | 5919 | `						"Unclosed '%c' on line %u does not match '%c'",cTop,aNest[nNest-1].nLine,cClose);` |
|         - | 5920 | `				}` |
|        61 | 5921 | `				return PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|        20 | 5922 | `					"Unclosed '%c' does not match '%c'",cTop,cClose);` |
|       ! 0 | 5923 | `			}` |
|        13 | 5924 | `		}` |
|        13 | 5925 | `	}` |
|         - | 5926 | `	/* The scanner had nothing to say: the compiler's own sentence. */` |
|        42 | 5927 | `	return PH7_GenCompileError(&(*pGen),E_PARSE,pTok->nLine,"Unmatched '%c'",` |
|        26 | 5928 | `		(pTok->nType & PH7_TK_RPAREN) ? ')' : ((pTok->nType & PH7_TK_CSB) ? ']' : '}'));` |
|        64 | 5929 | `}` |
|         - | 5930 | `/*` |
|         - | 5931 | ` * The ", expecting" tail php's parser prints for a token that cannot open a` |
|         - | 5932 | ` * statement where one was wanted. A block whose grammar names what it still` |
|         - | 5933 | ` * takes (an alternative-syntax if/elseif body, a switch's case list) says so;` |
|         - | 5934 | ` * the file's own statement list -- outside every block, a braced namespace's` |
|         - | 5935 | ` * included -- is waiting for its end; every other block names nothing.` |
|         - | 5936 | ` */` |
|        92 | 5937 | `PH7_PRIVATE const char *PH7_GenStrayStatementTail(ph7_gen_state *pGen)` |
|         3 | 5938 | `{` |
|        95 | 5939 | `	if( pGen->pCurrent->zInnerTail ){` |
|         9 | 5940 | `		return pGen->pCurrent->zInnerTail;` |
|         - | 5941 | `	}` |
|        87 | 5942 | `	if( pGen->pCurrent == &pGen->sGlobal && !pGen->bInNsBlock ){` |
|        63 | 5943 | `		return "end of file";` |
|         - | 5944 | `	}` |
|        25 | 5945 | `	return 0;` |
|        49 | 5946 | `}` |
|      1078 | 5947 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|         - | 5948 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|         - | 5949 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|         - | 5950 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|         - | 5951 | `	)` |
|         5 | 5952 | `{` |
|         - | 5953 | ``	/* php's noun for the offending token. An ALPHA-stream operator (`and`, `or`,`` |
|         - | 5954 | ``	 * `xor`, `new`, `clone`, `instanceof`) is lexed ID\|OP here but php calls it a`` |
|         - | 5955 | `	 * TOKEN, like every other reserved word — only a real identifier gets the` |
|         - | 5956 | `	 * "identifier" noun. */` |
|      1083 | 5957 | `	const char *zNoun = "token";` |
|         - | 5958 | `	sxu32 nLine;` |
|      1083 | 5959 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|         - | 5960 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|         - | 5961 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|         - | 5962 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|         - | 5963 | `		 * it before concluding "end of file". */` |
|       339 | 5964 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|       339 | 5965 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|       339 | 5966 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|       155 | 5967 | `			pTok = pGen->pEnd;` |
|        75 | 5968 | `		}` |
|       167 | 5969 | `	}` |
|      1083 | 5970 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|      1083 | 5971 | `	if( pTok == 0 && pGen->bChunkAtEof && pGen->nChunkEofLine > nLine ){` |
|         - | 5972 | `		/* End of INPUT is reported where it sits, not where the last token ended. */` |
|        44 | 5973 | `		nLine = pGen->nChunkEofLine;` |
|        20 | 5974 | `	}` |
|      1083 | 5975 | `	if( pTok == 0 ){` |
|       281 | 5976 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        92 | 5977 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|         - | 5978 | `			           : "syntax error, unexpected end of file",` |
|        92 | 5979 | `			zExpecting);` |
|         - | 5980 | `	}` |
|       899 | 5981 | `	if( pTok->nType & PH7_TK_FQNAME ){` |
|         7 | 5982 | `		zNoun = "fully qualified name";` |
|       896 | 5983 | `	}else if( (pTok->nType & PH7_TK_ID) && (pTok->nType & PH7_TK_OP) == 0 ){` |
|        51 | 5984 | `		zNoun = "identifier";` |
|       870 | 5985 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|        18 | 5986 | `		zNoun = "variable";` |
|         - | 5987 | `		/* The '$' is its own token and carries only "$" as text; the NAME is the` |
|         - | 5988 | ``		 * token after it. php names the whole variable, so `$x` was being reported`` |
|         - | 5989 | ``		 * as the nameless `variable "$"`. Stitch the two back together. */`` |
|        18 | 5990 | `		if( pGen->pTokenSet ){` |
|        18 | 5991 | `			SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        18 | 5992 | `			SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        18 | 5993 | `			SyToken *pName = &pTok[1];` |
|        14 | 5994 | `			if( pTok >= pBase && pName < pStreamEnd` |
|        14 | 5995 | `				&& (pName->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        18 | 5996 | `				&& pName->sData.nByte > 0 ){` |
|        18 | 5997 | `				SyBlobReset(&pGen->sWorker);` |
|        18 | 5998 | `				SyBlobAppend(&pGen->sWorker,"$",sizeof(char));` |
|        18 | 5999 | `				SyBlobAppend(&pGen->sWorker,pName->sData.zString,pName->sData.nByte);` |
|         - | 6000 | `				{` |
|         - | 6001 | `					SyString sVar;` |
|        18 | 6002 | `					SyStringInitFromBuf(&sVar,SyBlobData(&pGen->sWorker),` |
|         - | 6003 | `						SyBlobLength(&pGen->sWorker));` |
|        18 | 6004 | `					if( zExpecting ){` |
|        22 | 6005 | `						return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|         - | 6006 | `							"syntax error, unexpected %s \"%z\", expecting %s",` |
|         6 | 6007 | `							zNoun,&sVar,zExpecting);` |
|         - | 6008 | `					}` |
|         4 | 6009 | `					return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|         1 | 6010 | `						"syntax error, unexpected %s \"%z\"",zNoun,&sVar);` |
|         - | 6011 | `				}` |
|         - | 6012 | `			}` |
|       ! 0 | 6013 | `		}` |
|       833 | 6014 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|        61 | 6015 | `		zNoun = "integer";` |
|       805 | 6016 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|         - | 6017 | ``		/* php's noun, which is not the type name: `float` is what the CAST is`` |
|         - | 6018 | ``		 * called, `floating-point number` what a stray literal is called. */`` |
|         7 | 6019 | `		zNoun = "floating-point number";` |
|       774 | 6020 | `	}else if( pTok->nType & (PH7_TK_SSTR\|PH7_TK_DSTR) ){` |
|         - | 6021 | `		/* php names a string literal by the QUOTE it was written with, and prints` |
|         - | 6022 | `		 * the SOURCE bytes between the quotes -- escapes unresolved, which is what` |
|         - | 6023 | `		 * the token already holds here. A double-quoted string that INTERPOLATES is` |
|         - | 6024 | `		 * not one token in php at all: its scanner emits the opening quote on its` |
|         - | 6025 | `		 * own, so the parser has nothing to quote and the noun stands alone. */` |
|        22 | 6026 | `		if( (pTok->nType & PH7_TK_DSTR) && GenStateDqInterpolates(&pTok->sData) ){` |
|         5 | 6027 | `			if( zExpecting ){` |
|         7 | 6028 | `				return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|         2 | 6029 | `					"syntax error, unexpected double-quote mark, expecting %s",zExpecting);` |
|         - | 6030 | `			}` |
|       ! 0 | 6031 | `			return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|         - | 6032 | `				"syntax error, unexpected double-quote mark");` |
|         - | 6033 | `		}` |
|        18 | 6034 | `		zNoun = (pTok->nType & PH7_TK_SSTR) ? "single-quoted string" : "double-quoted string";` |
|       759 | 6035 | `	}else if( pTok->nType & (PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){` |
|         - | 6036 | ``		/* php names the OPENING marker -- `<<<EOT`, or `<<<'EOT` for a nowdoc, the`` |
|         - | 6037 | `		 * closing quote dropped because the token text ends at the label -- and` |
|         - | 6038 | `		 * reports it on the line AFTER the marker's, its scanner having consumed` |
|         - | 6039 | `		 * that line's terminator before the token is handed over. The token here` |
|         - | 6040 | `		 * carries the BODY, so the marker is read back off the source it points` |
|         - | 6041 | `		 * into. */` |
|         - | 6042 | `		SyString sMark;` |
|         9 | 6043 | `		if( GenStateHeredocMarker(&pTok->sData,&sMark) ){` |
|         9 | 6044 | `			if( zExpecting ){` |
|        13 | 6045 | `				return PH7_GenCompileError(pGen,E_PARSE,nLine + 1,` |
|         4 | 6046 | `					"syntax error, unexpected heredoc start \"%z\", expecting %s",&sMark,zExpecting);` |
|         - | 6047 | `			}` |
|       ! 0 | 6048 | `			return PH7_GenCompileError(pGen,E_PARSE,nLine + 1,` |
|         - | 6049 | `				"syntax error, unexpected heredoc start \"%z\"",&sMark);` |
|         - | 6050 | `		}` |
|       ! 0 | 6051 | `	}` |
|       873 | 6052 | `	if( zExpecting ){` |
|       665 | 6053 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       220 | 6054 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|         - | 6055 | `	}` |
|       647 | 6056 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       214 | 6057 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|       544 | 6058 | `}` |
|         - | 6059 | `/*` |
|         - | 6060 | ` * Generate a compile-time error message.` |
|         - | 6061 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 6062 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 6063 | ` * abort compilation immediately.` |
|         - | 6064 | ` */` |
|         - | 6065 | `/*` |
|         - | 6066 | `` * php's `Stack trace:` block under a compile-time FATAL: the activations that are`` |
|         - | 6067 | ` * live at the refusal -- the enclosing functions, and the include/require/eval that` |
|         - | 6068 | `` * loaded the unit being compiled -- then the `#N {main}` marker. A parse error gets`` |
|         - | 6069 | ` * none: that one is the parser's own refusal and php reports it as an E_PARSE.` |
|         - | 6070 | ` *` |
|         - | 6071 | ` * A refusal raised while the VM is still INITIALIZING is the main script's own` |
|         - | 6072 | ` * compile: there is no runtime state to walk (and no object pool to build the array` |
|         - | 6073 | ` * in), and php's answer there is the bare bottom marker.` |
|         - | 6074 | ` */` |
|      1012 | 6075 | `PH7_PRIVATE void PH7_GenAppendFatalTrace(ph7_vm *pVm,SyBlob *pOut,int iTraceKind)` |
|         4 | 6076 | `{` |
|         - | 6077 | `	ph7_value *pTrace;` |
|      1016 | 6078 | `	if( pVm == 0 \|\| iTraceKind == PH7_FATAL_TRACE_NONE ){` |
|        40 | 6079 | `		return;` |
|         - | 6080 | `	}` |
|       980 | 6081 | `	SyBlobAppend(pOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|       980 | 6082 | `	if( pVm->nMagic == PH7_VM_INIT ){` |
|       934 | 6083 | `		SyBlobAppend(pOut,"#0 {main}",sizeof("#0 {main}")-1);` |
|       934 | 6084 | `		return;` |
|         - | 6085 | `	}` |
|        50 | 6086 | `	pTrace = ph7_new_array(&(*pVm));` |
|        50 | 6087 | `	if( pTrace == 0 ){` |
|       ! 0 | 6088 | `		SyBlobAppend(pOut,"#0 {main}",sizeof("#0 {main}")-1);` |
|       ! 0 | 6089 | `		return;` |
|         - | 6090 | `	}` |
|         - | 6091 | `	/* php's fatal trace carries an argument list only while zend.exception_ignore_args` |
|         - | 6092 | `	 * is Off. Nor, unless this is one of the refusals php makes at RUN time, does it` |
|         - | 6093 | `	 * carry the include/require/eval that loaded the unit being compiled: php raises` |
|         - | 6094 | `	 * a compile error before it pushes that activation. */` |
|        73 | 6095 | `	VmBuildBacktrace(&(*pVm),` |
|        46 | 6096 | `		(PH7_VmIniGetBool(&(*pVm),"zend.exception_ignore_args",1) ? 0x2 : 0)` |
|        46 | 6097 | `		\| (iTraceKind == PH7_FATAL_TRACE_RUNTIME ? 0 : 0x4),0,pTrace);` |
|        50 | 6098 | `	PH7_VmTraceToString(&(*pVm),pTrace,TRUE,pOut);` |
|        50 | 6099 | `	ph7_release_value(&(*pVm),pTrace);` |
|       510 | 6100 | `}` |
|      2986 | 6101 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 6102 | `{` |
|      2991 | 6103 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|         - | 6104 | `	SyBlob sLocal;` |
|      2991 | 6105 | `	int bLocal = 0;` |
|      2991 | 6106 | `	const char *zErr = "Error";` |
|         - | 6107 | `	SyString *pFile;` |
|         - | 6108 | `	va_list ap;` |
|      2991 | 6109 | `	sxu32 nBare = 0;` |
|         - | 6110 | `	sxi32 rc;` |
|      2991 | 6111 | `	if( pGen->bDeclQuiet && nErrType != E_ERROR && nErrType != E_PARSE ){` |
|         - | 6112 | `		/* A deferred declaration's re-compile: the file's own compile already said` |
|         - | 6113 | `		 * this, where php says it. */` |
|         3 | 6114 | `		return SXRET_OK;` |
|         - | 6115 | `	}` |
|      2989 | 6116 | `	if( pGen->xErr == 0 && nErrType != E_ERROR && nErrType != E_PARSE ){` |
|         - | 6117 | `		/* Nobody is logging -- an eval()'d chunk -- and this is NOT a refusal. The` |
|         - | 6118 | `		 * generator's buffer is that chunk's one-message store, holding the text a` |
|         - | 6119 | `		 * ParseError will carry, so a deprecation or a warning must neither displace` |
|         - | 6120 | `		 * it nor append to it. php still prints these from inside an eval, so build` |
|         - | 6121 | `		 * the sentence somewhere of our own and emit it. */` |
|         3 | 6122 | `		SyBlobInit(&sLocal,&pGen->pVm->sAllocator);` |
|         3 | 6123 | `		pWorker = &sLocal;` |
|         3 | 6124 | `		bLocal = 1;` |
|         1 | 6125 | `	}` |
|         - | 6126 | `	/* Reset the working buffer. NOT when nobody is logging: there the buffer is a` |
|         - | 6127 | `	 * one-message store eval() reads its ParseError text out of, and php stops at` |
|         - | 6128 | `	 * the FIRST error where this generator carries on to a budget of fifteen -- so` |
|         - | 6129 | ``	 * resetting handed eval the LAST message. `eval('echo 1 foo;')` reported`` |
|         - | 6130 | ``	 * `unexpected token ";"`, the synchronizer's own complaint, where php names the`` |
|         - | 6131 | ``	 * `identifier "foo"` it choked on. */`` |
|      2989 | 6132 | `	if( pGen->xErr ){` |
|      1973 | 6133 | `		SyBlobReset(pWorker);` |
|       984 | 6134 | `	}` |
|         - | 6135 | `	/* Peek the processed file path if available */` |
|      2989 | 6136 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|      2989 | 6137 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|         - | 6138 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|         - | 6139 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|         - | 6140 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|         - | 6141 | `		 * into execution with a 0 exit status. */` |
|      2863 | 6142 | `		pGen->nErr++;` |
|      2863 | 6143 | `		if( nErrType == E_ERROR ){` |
|         - | 6144 | `			/* php's E_COMPILE_ERROR: an uncatchable fatal, where a parse error is a` |
|         - | 6145 | `			 * catchable ParseError. The include path reads this to tell them apart. */` |
|      1142 | 6146 | `			pGen->nFatal++;` |
|       569 | 6147 | `		}` |
|      2863 | 6148 | `		if( pGen->nErr == 1 ){` |
|         - | 6149 | `			/* Keep the FIRST refusal's bare text and line: it is the one php reports,` |
|         - | 6150 | `			 * and it is the message an include's ParseError carries. */` |
|         - | 6151 | `			va_list apF;` |
|      2401 | 6152 | `			SyBlobReset(&pGen->sFirstErr);` |
|      2401 | 6153 | `			va_start(apF,zFormat);` |
|      2401 | 6154 | `			SyBlobFormatAp(&pGen->sFirstErr,zFormat,apF);` |
|      2401 | 6155 | `			va_end(apF);` |
|      2401 | 6156 | `			pGen->nFirstErrLine = nLine;` |
|      1203 | 6157 | `		}else{` |
|         - | 6158 | `			/* php stops at the first one. This generator recovers and carries on so` |
|         - | 6159 | `			 * that the rest of the unit is still walked (a later pass needs the` |
|         - | 6160 | `			 * symbols), but everything it says after the first refusal is its own` |
|         - | 6161 | `			 * recovery talking -- and printing it put diagnostics on the user's` |
|         - | 6162 | `			 * screen that php, having stopped, never reaches.` |
|         - | 6163 | `			 *` |
|         - | 6164 | `			 * The recovery is still bounded, and now SILENTLY: the old limit` |
|         - | 6165 | ``			 * announced itself with a `Error count limit reached` line of PH7's own`` |
|         - | 6166 | `			 * invention, which no php prints and which would land on top of the one` |
|         - | 6167 | `			 * diagnostic php does. The unit has already failed and its first message` |
|         - | 6168 | `			 * is already recorded, so there is nothing left to say. */` |
|       467 | 6169 | `			return (pGen->nErr > 15) ? SXERR_ABORT : SXRET_OK;` |
|         - | 6170 | `		}` |
|      1198 | 6171 | `	}` |
|      2527 | 6172 | `	if( nErrType == E_PARSE && pGen->bParseThrows ){` |
|         - | 6173 | `		/* An include/require unit: php's parser throws a ParseError rather than` |
|         - | 6174 | `		 * printing, and the text only reaches the screen if nobody catches it.` |
|         - | 6175 | `		 * The caller (VmEvalChunk) raises it from sFirstErr. */` |
|        39 | 6176 | `		return SXRET_OK;` |
|         - | 6177 | `	}` |
|      2491 | 6178 | `	if( pGen->xErr == 0 && !bLocal ){` |
|         - | 6179 | `		/* No consumer — but keep the BARE message in the error buffer so a caller` |
|         - | 6180 | `		 * that needs the text can read it back. eval() compiles with logging off` |
|         - | 6181 | `		 * (a parse error there is php's catchable ParseError, not a printed` |
|         - | 6182 | `		 * diagnostic) and needs exactly this string for the exception message. The` |
|         - | 6183 | `		 * first message stands: everything after it is this generator's recovery` |
|         - | 6184 | `		 * talking, and php never got that far. */` |
|       875 | 6185 | `		if( SyBlobLength(pWorker) < 1 ){` |
|       875 | 6186 | `			va_start(ap,zFormat);` |
|       875 | 6187 | `			SyBlobFormatAp(pWorker,zFormat,ap);` |
|       875 | 6188 | `			va_end(ap);` |
|       435 | 6189 | `		}` |
|       875 | 6190 | `		return SXRET_OK;` |
|         - | 6191 | `	}` |
|      1621 | 6192 | `	switch(nErrType){` |
|       998 | 6193 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|        62 | 6194 | `	case E_WARNING: zErr = "Warning";     break;` |
|        33 | 6195 | `	case 128 /* E_COMPILE_WARNING */: zErr = "Warning"; break;` |
|       500 | 6196 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       ! 0 | 6197 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 6198 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 6199 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 6200 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|        40 | 6201 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 6202 | `	default:` |
|       ! 0 | 6203 | `		break;` |
|         - | 6204 | `	}` |
|      1621 | 6205 | `	rc = SXRET_OK;` |
|         - | 6206 | ``	/* The BODY only -- `<message> in <file> on line <line>` plus a fatal's trace.`` |
|         - | 6207 | `	 * The label and the two copies php wraps it in are the emitter's` |
|         - | 6208 | `	 * (PH7_VmEmitCompileDiagnostic): a compile diagnostic owes the same log/display` |
|         - | 6209 | `	 * pair a runtime one does, and this used to write a single log-shaped copy to` |
|         - | 6210 | `	 * the engine's compile-error consumer whatever the ini said. */` |
|      1621 | 6211 | `	va_start(ap,zFormat);` |
|      1621 | 6212 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|      1621 | 6213 | `	va_end(ap);` |
|         - | 6214 | `	/* Where php's own sentence ENDS. Everything appended below is the location` |
|         - | 6215 | `	 * tail and a fatal's trace, which belong to the printed copies alone -- the` |
|         - | 6216 | `	 * user handler and error_get_last() get the sentence and the line as` |
|         - | 6217 | `	 * separate fields, the way a runtime diagnostic hands them over. */` |
|      1621 | 6218 | `	nBare = SyBlobLength(pWorker);` |
|      1621 | 6219 | `	if( pFile ){` |
|      1621 | 6220 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       808 | 6221 | `	}` |
|      1621 | 6222 | `	if( nErrType == E_ERROR ){` |
|       998 | 6223 | `		PH7_GenAppendFatalTrace(pGen->pVm,pWorker,pGen->iFatalTrace);` |
|       497 | 6224 | `	}` |
|         - | 6225 | `	/* iFatalTrace is a ONE-SHOT: a site that raises one of php's non-compiler` |
|         - | 6226 | `	 * refusals sets it just before the call and this consumes it, so no site has to` |
|         - | 6227 | `	 * remember to put it back and none of them can leak it onto a later refusal. */` |
|      1621 | 6228 | `	pGen->iFatalTrace = PH7_FATAL_TRACE_COMPILE;` |
|      1621 | 6229 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 6230 | `		/* php's error_reporting BIT for this diagnostic. The compiler's own` |
|         - | 6231 | `		 * refusals are E_COMPILE_ERROR and E_PARSE rather than E_ERROR, and a` |
|         - | 6232 | `		 * site that means php's E_COMPILE_WARNING says 128 outright; everything` |
|         - | 6233 | `		 * else (a compile-time E_WARNING, which is what php raises for the` |
|         - | 6234 | ``		 * `continue`-targeting-switch and magic-visibility rules) passes its own`` |
|         - | 6235 | `		 * level through. */` |
|         - | 6236 | `		sxi32 iPhpErr;` |
|      1621 | 6237 | `		switch( nErrType ){` |
|       998 | 6238 | `		case E_ERROR: iPhpErr = 64 /* E_COMPILE_ERROR */; break;` |
|       500 | 6239 | `		case E_PARSE: iPhpErr = 4  /* E_PARSE */;         break;` |
|       130 | 6240 | `		default:      iPhpErr = nErrType;                 break;` |
|         - | 6241 | `		}` |
|      2429 | 6242 | `		PH7_VmEmitCompileDiagnostic(pGen->pVm,iPhpErr,zErr,` |
|      1616 | 6243 | `			(const char *)SyBlobData(pWorker),SyBlobLength(pWorker),` |
|      1616 | 6244 | `			(const char *)SyBlobData(pWorker),nBare,nLine);` |
|       808 | 6245 | `	}` |
|      1621 | 6246 | `	if( bLocal ){` |
|         3 | 6247 | `		SyBlobRelease(&sLocal);` |
|         1 | 6248 | `	}` |
|      1621 | 6249 | `	return rc;` |
|      1498 | 6250 | `}` |
|         - | 6251 |  |
