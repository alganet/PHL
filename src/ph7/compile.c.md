# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4246/5465 lines (77.69%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `/*` |
|       - |    8 | ` * This file implement a thread-safe and full-reentrant compiler for the PH7 engine.` |
|       - |    9 | ` * That is, routines defined in this file takes a stream of tokens and output` |
|       - |   10 | ` * PH7 bytecode instructions.` |
|       - |   11 | ` */` |
|       - |   12 | `/* Forward declaration */` |
|       - |   13 | `typedef struct LangConstruct LangConstruct;` |
|       - |   14 | `typedef struct JumpFixup     JumpFixup;` |
|       - |   15 | `typedef struct Label         Label;` |
|       - |   16 | `/* Block [i.e: set of statements] control flags */` |
|       - |   17 | `#define GEN_BLOCK_LOOP        0x001    /* Loop block [i.e: for,while,...] */` |
|       - |   18 | `#define GEN_BLOCK_PROTECTED   0x002    /* Protected block */` |
|       - |   19 | `#define GEN_BLOCK_COND        0x004    /* Conditional block [i.e: if(condition){} ]*/` |
|       - |   20 | `#define GEN_BLOCK_FUNC        0x008    /* Function body */` |
|       - |   21 | `#define GEN_BLOCK_GLOBAL      0x010    /* Global block (always set)*/` |
|       - |   22 | `#define GEN_BLOC_NESTED_FUNC  0x020    /* Nested function body */` |
|       - |   23 | `#define GEN_BLOCK_EXPR        0x040    /* Expression */` |
|       - |   24 | `#define GEN_BLOCK_STD         0x080    /* Standard block */` |
|       - |   25 | `#define GEN_BLOCK_EXCEPTION   0x100    /* Exception block [i.e: try{ } }*/` |
|       - |   26 | `#define GEN_BLOCK_SWITCH      0x200    /* Switch statement */` |
|       - |   27 | `/*` |
|       - |   28 | ` * Each label seen in the input is recorded in an instance` |
|       - |   29 | ` * of the following structure.` |
|       - |   30 | ` * A label is a target point [i.e: a jump destination] that is specified` |
|       - |   31 | ` * by an identifier followed by a colon.` |
|       - |   32 | ` * Example` |
|       - |   33 | ` *  LABEL:` |
|       - |   34 | ` *		echo "hello\n";` |
|       - |   35 | ` */` |
|       - |   36 | `struct Label` |
|       - |   37 |  |
|       - |   38 | `	ph7_vm_func *pFunc;  /* Compiled function where the label was declared.NULL otherwise */` |
|       - |   39 | `	sxu32 nJumpDest;     /* Jump destination */` |
|       - |   40 | `	SyString sName;      /* Label name */` |
|       - |   41 | `	sxu32 nLine;         /* Line number this label occurs */` |
|       - |   42 | `	sxu8 bRef;           /* True if the label was referenced */` |
|       - |   43 | `};` |
|       - |   44 | `/*` |
|       - |   45 | ` * Compilation of some PHP constructs such as if, for, while, the logical or` |
|       - |   46 | ` * (\|\|) and logical and (&&) operators in expressions requires the` |
|       - |   47 | ` * generation of forward jumps.` |
|       - |   48 | ` * Since the destination PC target of these jumps isn't known when the jumps` |
|       - |   49 | ` * are emitted, we record each forward jump in an instance of the following` |
|       - |   50 | ` * structure. Those jumps are fixed later when the jump destination is resolved.` |
|       - |   51 | ` */` |
|       - |   52 | `struct JumpFixup` |
|       - |   53 |  |
|       - |   54 | `	sxi32 nJumpType;     /* Jump type. Either TRUE jump, FALSE jump or Unconditional jump */` |
|       - |   55 | `	sxu32 nInstrIdx;     /* Instruction index to fix later when the jump destination is resolved. */` |
|       - |   56 | `	/* The following fields are only used by the goto statement */` |
|       - |   57 | `	SyString sLabel;    /* Label name */` |
|       - |   58 | `	ph7_vm_func *pFunc; /* Compiled function inside which the goto was emitted. NULL otherwise */` |
|       - |   59 | `	sxu32 nLine;        /* Track line number */` |
|       - |   60 | `};` |
|       - |   61 | `/*` |
|       - |   62 | ` * Each language construct is represented by an instance` |
|       - |   63 | ` * of the following structure.` |
|       - |   64 | ` */` |
|       - |   65 | `struct LangConstruct` |
|       - |   66 |  |
|       - |   67 | `	sxu32 nID;                     /* Language construct ID [i.e: PH7_TKWRD_WHILE,PH7_TKWRD_FOR,PH7_TKWRD_IF...] */` |
|       - |   68 | `	ProcLangConstruct xConstruct;  /* C function implementing the language construct */` |
|       - |   69 | `};` |
|       - |   70 | `/* Compilation flags */` |
|       - |   71 | `#define PH7_COMPILE_SINGLE_STMT 0x001 /* Compile a single statement */` |
|       - |   72 | `/* Token stream synchronization macros */` |
|       - |   73 | `#define SWAP_TOKEN_STREAM(GEN,START,END)\` |
|       - |   74 | `	pTmp  = GEN->pEnd;\` |
|       - |   75 | `	pGen->pIn  = START;\` |
|       - |   76 | `	pGen->pEnd = END` |
|       - |   77 | `#define UPDATE_TOKEN_STREAM(GEN)\` |
|       - |   78 | `	if( GEN->pIn < pTmp ){\` |
|       - |   79 | `	    GEN->pIn++;\` |
|       - |   80 | `	}\` |
|       - |   81 | `	GEN->pEnd = pTmp` |
|       - |   82 | `#define SWAP_DELIMITER(GEN,START,END)\` |
|       - |   83 | `	pTmpIn  = GEN->pIn;\` |
|       - |   84 | `	pTmpEnd = GEN->pEnd;\` |
|       - |   85 | `	GEN->pIn = START;\` |
|       - |   86 | `	GEN->pEnd = END` |
|       - |   87 | `#define RE_SWAP_DELIMITER(GEN)\` |
|       - |   88 | `	GEN->pIn  = pTmpIn;\` |
|       - |   89 | `	GEN->pEnd = pTmpEnd` |
|       - |   90 | `/* Flags related to expression compilation */` |
|       - |   91 | `#define EXPR_FLAG_LOAD_IDX_STORE    0x001 /* Set the iP2 flag when dealing with the LOAD_IDX instruction */` |
|       - |   92 | `#define EXPR_FLAG_RDONLY_LOAD       0x002 /* Read-only load, refer to the 'PH7_OP_LOAD' VM instruction for more information */` |
|       - |   93 | `#define EXPR_FLAG_COMMA_STATEMENT   0x004 /* Treat comma expression as a single statement (used by class attributes) */` |
|       - |   94 | `/* Forward declaration */` |
|       - |   95 | `static sxi32 PH7_CompileExpr(ph7_gen_state *pGen,sxi32 iFlags,sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *));` |
|       - |   96 | `/*` |
|       - |   97 | ` * Local utility routines used in the code generation phase.` |
|       - |   98 | ` */` |
|       - |   99 | `/*` |
|       - |  100 | ` * Check if the given name refer to a valid label.` |
|       - |  101 | ` * Return SXRET_OK and write a pointer to that label on success.` |
|       - |  102 | ` * Any other return value indicates no such label.` |
|       - |  103 | ` */` |
|     148 |  104 | `static sxi32 GenStateGetLabel(ph7_gen_state *pGen,SyString *pName,Label **ppOut)` |
|       2 |  105 |  |
|       - |  106 | `	Label *aLabel;` |
|       - |  107 | `	sxu32 n;` |
|       - |  108 | `	/* Perform a linear scan on the label table */` |
|     150 |  109 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|     330 |  110 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     274 |  111 | `		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|       - |  112 | `			/* Jump destination found */` |
|      94 |  113 | `			aLabel[n].bRef = TRUE;` |
|      94 |  114 | `			if( ppOut ){` |
|      94 |  115 | `				*ppOut = &aLabel[n];` |
|      46 |  116 | `			}` |
|      94 |  117 | `			return SXRET_OK;` |
|       - |  118 | `		}` |
|      92 |  119 | `	}` |
|       - |  120 | `	/* No such destination */` |
|      57 |  121 | `	return SXERR_NOTFOUND;` |
|      76 |  122 |  |
|       - |  123 | `/*` |
|       - |  124 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|       - |  125 | ` * compiled blocks.` |
|       - |  126 | ` * Return a pointer to that block on success. NULL otherwise.` |
|       - |  127 | ` */` |
|    2962 |  128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |  129 |  |
|    2964 |  130 | `	GenBlock *pBlock = pCurrent;` |
|    8321 |  131 | `	for(;;){` |
|   16644 |  132 | `		if( pBlock->iFlags & iBlockType ){` |
|    2856 |  133 | `			iCount--; /* Decrement nesting level */` |
|    2856 |  134 | `			if( iCount < 1 ){` |
|       - |  135 | `				/* Block meet with the desired criteria */` |
|    2830 |  136 | `				return pBlock;` |
|       - |  137 | `			}` |
|      13 |  138 | `		}` |
|       - |  139 | `		/* Point to the upper block */` |
|   13816 |  140 | `		pBlock = pBlock->pParent;` |
|   13816 |  141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |  142 | `			/* Forbidden */` |
|      69 |  143 | `			break;` |
|       - |  144 | `		}` |
|       2 |  145 | `	}` |
|       - |  146 | `	/* No such block */` |
|     136 |  147 | `	return 0;` |
|    1483 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Initialize a freshly allocated block instance.` |
|       - |  151 | ` */` |
|  575614 |  152 | `static void GenStateInitBlock(` |
|       - |  153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |  155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |  157 | `	void *pUserData      /* Upper layer private data */` |
|       - |  158 | `	)` |
|       2 |  159 |  |
|       - |  160 | `	/* Initialize block fields */` |
|  575616 |  161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  575616 |  162 | `	pBlock->pUserData   = pUserData;` |
|  575616 |  163 | `	pBlock->pGen        = pGen;` |
|  575616 |  164 | `	pBlock->iFlags      = iType;` |
|  575616 |  165 | `	pBlock->pParent     = 0;` |
|  575616 |  166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  575616 |  167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  575616 |  168 |  |
|       - |  169 | `/*` |
|       - |  170 | ` * Allocate a new block instance.` |
|       - |  171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |  172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |  173 | ` * processing on failure.` |
|       - |  174 | ` */` |
|  572918 |  175 | `static sxi32 GenStateEnterBlock(` |
|       - |  176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |  179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |  180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |  181 | `	)` |
|       2 |  182 |  |
|       - |  183 | `	GenBlock *pBlock;` |
|       - |  184 | `	/* Allocate a new block instance */` |
|  572920 |  185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  572920 |  186 | `	if( pBlock == 0 ){` |
|       - |  187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  189 | `		 */` |
|     ! 0 |  190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |  191 | `		/* Abort processing immediately */` |
|     ! 0 |  192 | `		return SXERR_ABORT;` |
|       - |  193 | `	}` |
|       - |  194 | `	/* Zero the structure */` |
|  572920 |  195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  572920 |  196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |  197 | `	/* Link to the parent block */` |
|  572920 |  198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |  199 | `	/* Mark as the current block */` |
|  572920 |  200 | `	pGen->pCurrent = pBlock;` |
|  572920 |  201 | `	if( ppBlock ){` |
|       - |  202 | `		/* Write a pointer to the new instance */` |
|  277280 |  203 | `		*ppBlock = pBlock;` |
|  138639 |  204 | `	}` |
|  572920 |  205 | `	return SXRET_OK;` |
|  286461 |  206 |  |
|       - |  207 | `/*` |
|       - |  208 | ` * Release block fields without freeing the whole instance.` |
|       - |  209 | ` */` |
|  572910 |  210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |  211 |  |
|  572912 |  212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  572912 |  213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  572912 |  214 |  |
|       - |  215 | `/*` |
|       - |  216 | ` * Release a block.` |
|       - |  217 | ` */` |
|  572910 |  218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |  219 |  |
|  572912 |  220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  572912 |  221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |  222 | `	/* Free the instance */` |
|  572912 |  223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  572912 |  224 |  |
|       - |  225 | `/*` |
|       - |  226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |  227 | ` */` |
|  572910 |  228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |  229 |  |
|  572912 |  230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  572912 |  231 | `	if( pBlock == 0 ){` |
|       - |  232 | `		/* No more block to pop */` |
|     ! 0 |  233 | `		return SXERR_EMPTY;` |
|       - |  234 | `	}` |
|       - |  235 | `	/* Point to the upper block */` |
|  572912 |  236 | `	pGen->pCurrent = pBlock->pParent;` |
|  572912 |  237 | `	if( ppBlock ){` |
|       - |  238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |  239 | `		*ppBlock = pBlock;` |
|     ! 0 |  240 | `	}else{` |
|       - |  241 | `		/* Safely release the block */` |
|  572912 |  242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |  243 | `	}` |
|  572912 |  244 | `	return SXRET_OK;` |
|  286457 |  245 |  |
|       - |  246 | `/*` |
|       - |  247 | ` * Emit a forward jump.` |
|       - |  248 | ` * Notes on forward jumps` |
|       - |  249 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|       - |  250 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|       - |  251 | ` *  generation of forward jumps.` |
|       - |  252 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|       - |  253 | ` *  are emitted, we record each forward jump in an instance of the following` |
|       - |  254 | ` *  structure. Those jumps are fixed later when the jump destination is resolved.` |
|       - |  255 | ` */` |
|  174630 |  256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |  257 |  |
|       - |  258 | `	JumpFixup sJumpFix;` |
|       - |  259 | `	sxi32 rc;` |
|       - |  260 | `	/* Init the JumpFixup structure */` |
|  174632 |  261 | `	sJumpFix.nJumpType = nJumpType;` |
|  174632 |  262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |  263 | `	/* Insert in the jump fixup table */` |
|  174632 |  264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  174632 |  265 | `	return rc;` |
|       2 |  266 |  |
|       - |  267 | `/*` |
|       - |  268 | ` * Fix a forward jump now the jump destination is resolved.` |
|       - |  269 | ` * Return the total number of fixed jumps.` |
|       - |  270 | ` * Notes on forward jumps:` |
|       - |  271 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|       - |  272 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|       - |  273 | ` *  generation of forward jumps.` |
|       - |  274 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|       - |  275 | ` *  are emitted, we record each forward jump in an instance of the following` |
|       - |  276 | ` *  structure.Those jumps are fixed later when the jump destination is resolved.` |
|       - |  277 | ` */` |
|  407892 |  278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |  279 |  |
|       - |  280 | `	JumpFixup *aFix;` |
|       - |  281 | `	VmInstr *pInstr;` |
|       - |  282 | `	sxu32 nFixed;` |
|       - |  283 | `	sxu32 n;` |
|       - |  284 | `	/* Point to the jump fixup table */` |
|  407894 |  285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |  286 | `	/* Fix the desired jumps */` |
|  748172 |  287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  340280 |  288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |  289 | `			/* Already fixed */` |
|  132516 |  290 | `			continue;` |
|       - |  291 | `		}` |
|  207766 |  292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |  293 | `			/* Not of our interest */` |
|   33138 |  294 | `			continue;` |
|       - |  295 | `		}` |
|       - |  296 | `		/* Point to the instruction to fix */` |
|  174630 |  297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  174630 |  298 | `		if( pInstr ){` |
|  174630 |  299 | `			pInstr->iP2 = nJumpDest;` |
|  174630 |  300 | `			nFixed++;` |
|       - |  301 | `			/* Mark as fixed */` |
|  174630 |  302 | `			aFix[n].nJumpType = -1;` |
|   87314 |  303 | `		}` |
|   87316 |  304 | `	}` |
|       - |  305 | `	/* Total number of fixed jumps */` |
|  407894 |  306 | `	return nFixed;` |
|       2 |  307 |  |
|       - |  308 | `/*` |
|       - |  309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |  310 | ` * The goto statement can be used to jump to another section` |
|       - |  311 | ` * in the program.` |
|       - |  312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |  313 | ` * statement for more information.` |
|       - |  314 | ` */` |
|  155732 |  315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |  316 |  |
|       - |  317 | `	JumpFixup *pJump,*aJumps;` |
|       - |  318 | `	Label *pLabel,*aLabel;` |
|       - |  319 | `	VmInstr *pInstr;` |
|       - |  320 | `	sxi32 rc;` |
|       - |  321 | `	sxu32 n;` |
|       - |  322 | `	/* Point to the goto table */` |
|  155734 |  323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |  324 | `	/* Fix */` |
|  155880 |  325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|     150 |  326 | `		pJump = &aJumps[n];` |
|       - |  327 | `		/* Extract the target label */` |
|     150 |  328 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,&pLabel);` |
|     150 |  329 | `		if( rc != SXRET_OK ){` |
|       - |  330 | `			/* No such label */` |
|      57 |  331 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' was referenced but not defined",&pJump->sLabel);` |
|      57 |  332 | `			if( rc == SXERR_ABORT ){` |
|       3 |  333 | `				return SXERR_ABORT;` |
|       - |  334 | `			}` |
|      55 |  335 | `			continue;` |
|       - |  336 | `		}` |
|       - |  337 | `		/* Make sure the target label is reachable */` |
|      94 |  338 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|       9 |  339 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);` |
|       9 |  340 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  341 | `				return SXERR_ABORT;` |
|       - |  342 | `			}` |
|       4 |  343 | `		}` |
|       - |  344 | `		/* Fix the jump now the destination is resolved */` |
|      94 |  345 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|      94 |  346 | `		if( pInstr ){` |
|      94 |  347 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|      46 |  348 | `		}` |
|      48 |  349 | `	}` |
|  155732 |  350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  155864 |  351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |  352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |  353 | `			/* Emit a warning */` |
|      37 |  354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |  355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |  356 | `		}` |
|      68 |  357 | `	}` |
|  155732 |  358 | `	return SXRET_OK;` |
|   77868 |  359 |  |
|       - |  360 | `/*` |
|       - |  361 | ` * Check if a given token value is installed in the literal table.` |
|       - |  362 | ` */` |
|  506850 |  363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |  364 |  |
|       - |  365 | `	SyHashEntry *pEntry;` |
|  506852 |  366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  506852 |  367 | `	if( pEntry == 0 ){` |
|  249932 |  368 | `		return SXERR_NOTFOUND;` |
|       - |  369 | `	}` |
|  256922 |  370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  256922 |  371 | `	return SXRET_OK;` |
|  253427 |  372 |  |
|       - |  373 | `/*` |
|       - |  374 | ` * Install a given constant index in the literal table.` |
|       - |  375 | ` * In order to be installed, the ph7_value must be of type string.` |
|       - |  376 | ` *` |
|       - |  377 | ` * NOTE: empty strings are deliberately omitted here.  The VM reserves a` |
|       - |  378 | ` * single shared constant for "" during initialization (pVm->nEmptyStringIdx)` |
|       - |  379 | ` * and the compiler emits a LOADC referencing that slot whenever an empty` |
|       - |  380 | ` * literal is encountered.  This keeps the literal hash from growing when` |
|       - |  381 | ` * many "" literals appear in user code.` |
|       - |  382 | ` */` |
|  249930 |  383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |  384 |  |
|  249932 |  385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  249932 |  386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  124965 |  387 | `	}` |
|  249932 |  388 | `	return SXRET_OK;` |
|       2 |  389 |  |
|       - |  390 | `/*` |
|       - |  391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |  392 | ` * in the constant table.` |
|       - |  393 | ` */` |
|   88978 |  394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |  395 |  |
|       - |  396 | `	ph7_value *pObj;` |
|   88980 |  397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |  398 | `	/* Reserve a new constant */` |
|   88980 |  399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   88980 |  400 | `	if( pObj == 0 ){` |
|     ! 0 |  401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  402 | `		return 0;` |
|       - |  403 | `	}` |
|   88980 |  404 | `	*pIdx = nIdx;` |
|       - |  405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |  406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |  407 | `	 */` |
|   88980 |  408 | `	return pObj;` |
|   44491 |  409 |  |
|       - |  410 | `/*` |
|       - |  411 | ` * Implementation of the PHP language constructs.` |
|       - |  412 | ` */` |
|       - |  413 | `/* Forward declaration */` |
|       - |  414 | `static sxi32 GenStateCompileChunk(ph7_gen_state *pGen,sxi32 iFlags);` |
|       - |  415 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd);` |
|       - |  416 | `static void GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc);` |
|       - |  417 | `static const char * TokenTypeName(sxu32 nType);` |
|       - |  418 | `/*` |
|       - |  419 | ` * Stack-scratch size for stripping PHP 7.4 numeric separators. A typical` |
|       - |  420 | ` * literal (INT64_MAX decimal is 19 digits, binary 64-bit with per-nibble` |
|       - |  421 | ` * separators is ~80 chars) fits comfortably, so the fast path never touches` |
|       - |  422 | ` * the heap. The language itself imposes no upper bound on the length of a` |
|       - |  423 | ` * well-formed literal — the stripper falls back to a VM-allocator buffer` |
|       - |  424 | ` * for anything larger, so correctness is preserved even for pathological` |
|       - |  425 | ` * inputs like a thousand-digit number.` |
|       - |  426 | ` */` |
|       - |  427 | `#define GEN_NUM_SCRATCH 128` |
|       - |  428 | `/*` |
|       - |  429 | ` * Return TRUE if c is a valid digit for the given numeric base.` |
|       - |  430 | ` *   base 16 => SyisHex (0-9, a-f, A-F)` |
|       - |  431 | ` *   base  2 => 0 or 1` |
|       - |  432 | ` *   base 10 => SyisDigit (0-9, also used for octal literals which share the` |
|       - |  433 | ` *              decimal scan in the lexer)` |
|       - |  434 | ` */` |
|    1076 |  435 | `static int GenStateIsBaseDigit(int c, int base)` |
|       2 |  436 |  |
|    1078 |  437 | `	if( base == 16 ){ return SyisHex(c); }` |
|     980 |  438 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|     702 |  439 | `	return SyisDigit(c);` |
|     540 |  440 |  |
|       - |  441 | `/*` |
|       - |  442 | ` * Given the raw text of a numeric literal token, locate a misplaced PHP 7.4` |
|       - |  443 | ` * underscore separator so the caller can report the malformed portion with` |
|       - |  444 | ` * the exact wording PHP uses:` |
|       - |  445 | ` *` |
|       - |  446 | ` *   syntax error, unexpected identifier "X"` |
|       - |  447 | ` *` |
|       - |  448 | ` * The lexer guarantees that every underscore it consumed as a separator is` |
|       - |  449 | ` * surrounded by valid base digits; anything else sits in the trailing run` |
|       - |  450 | ` * absorbed by the lexer specifically to let this validator see and report` |
|       - |  451 | ` * it. That invariant means the malformed span is exactly [bad .. nByte) —` |
|       - |  452 | ` * no forward rescan needed.` |
|       - |  453 | ` *` |
|       - |  454 | ` * Returns 1 and fills pBadStart / pBadLen when the literal is malformed;` |
|       - |  455 | ` * returns 0 when it is well-formed.` |
|       - |  456 | ` */` |
|   89490 |  457 | `static int GenStateFindBadNumericSeparator(` |
|       - |  458 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       2 |  459 |  |
|   89492 |  460 | `	const char *z = pRaw->zString;` |
|   89492 |  461 | `	sxu32 n = pRaw->nByte;` |
|   89492 |  462 | `	int base = 10;` |
|       - |  463 | `	sxu32 i, start;` |
|   89492 |  464 | `	if( n < 2 ) return 0;` |
|    8076 |  465 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |  466 | `		base = 16;` |
|    8041 |  467 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |  468 | `		base = 2;` |
|     139 |  469 | `	}` |
|   30152 |  470 | `	for( i = 0; i < n; ++i ){` |
|   22092 |  471 | `		if( z[i] != '_' ) continue;` |
|     814 |  472 | `		if( i > 0 && i + 1 < n` |
|     543 |  473 | `			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)` |
|     540 |  474 | `			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){` |
|     533 |  475 | `			continue; /* well-placed separator */` |
|       - |  476 | `		}` |
|       - |  477 | `		/* First misplaced underscore — the lexer already absorbed the full` |
|       - |  478 | `		 * malformed tail, so it runs from here to the end of the token. */` |
|      15 |  479 | `		start = i;` |
|      20 |  480 | `		if( start > 0 && (z[start-1] == 'x' \|\| z[start-1] == 'X'` |
|      12 |  481 | `			\|\| z[start-1] == 'b' \|\| z[start-1] == 'B') ){` |
|       5 |  482 | `			start--; /* include the base letter for 0x_... / 0b_... */` |
|       2 |  483 | `		}` |
|      15 |  484 | `		*pBadStart = &z[start];` |
|      15 |  485 | `		*pBadLen = n - start;` |
|      15 |  486 | `		return 1;` |
|     ! 0 |  487 | `	}` |
|    8062 |  488 | `	return 0;` |
|   44747 |  489 |  |
|       - |  490 | `/*` |
|       - |  491 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |  492 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |  493 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |  494 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |  495 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |  496 | ` * so callers can bail from the current construct).` |
|       - |  497 | ` */` |
|   89490 |  498 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       2 |  499 |  |
|   89492 |  500 | `	const char *zBad = 0;` |
|   89492 |  501 | `	sxu32 nBad = 0;` |
|       - |  502 | `	SyString sBad;` |
|       - |  503 | `	sxi32 rc;` |
|   89492 |  504 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|   89478 |  505 | `		return SXRET_OK;` |
|       - |  506 | `	}` |
|      15 |  507 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      15 |  508 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |  509 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      15 |  510 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  511 | `		return SXERR_ABORT;` |
|       - |  512 | `	}` |
|      15 |  513 | `	return SXERR_SYNTAX;` |
|   44747 |  514 |  |
|       - |  515 | `/*` |
|       - |  516 | ` * Strip PHP 7.4 numeric literal separators (underscores between digits) from` |
|       - |  517 | ` * a numeric token's text and yield a SyString suitable for the low-level` |
|       - |  518 | ` * converters (SyStrToInt64 / SyStrToReal / etc.).` |
|       - |  519 | ` *` |
|       - |  520 | ` * Fast path: if the token contains no '_', *pOut aliases pToken with no copy` |
|       - |  521 | ` * and *pzAlloc is set to NULL.` |
|       - |  522 | ` * Stack path: if the cleaned bytes fit in zScratch, they are written there` |
|       - |  523 | ` * and *pzAlloc is set to NULL.` |
|       - |  524 | ` * Heap path: for literals larger than the scratch buffer, a fresh buffer is` |
|       - |  525 | ` * allocated from pAlloc, returned via *pzAlloc, and must be released by the` |
|       - |  526 | ` * caller with SyMemBackendFree once the converter is done.` |
|       - |  527 | ` *` |
|       - |  528 | ` * Returns SXRET_OK on success, SXERR_ABORT on allocator failure (in which` |
|       - |  529 | ` * case *pOut is left untouched and the caller must not read it).` |
|       - |  530 | ` */` |
|   89476 |  531 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |  532 | `	SyMemBackend *pAlloc,` |
|       - |  533 | `	const SyString *pToken,` |
|       - |  534 | `	char *zScratch, sxu32 nScratch,` |
|       - |  535 | `	SyString *pOut, char **pzAlloc)` |
|       2 |  536 |  |
|       - |  537 | `	sxu32 i, j;` |
|   89478 |  538 | `	int hasUnderscore = 0;` |
|       - |  539 | `	char *zBuf;` |
|   89478 |  540 | `	*pzAlloc = 0;` |
|  190904 |  541 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  101680 |  542 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   50715 |  543 | `	}` |
|   89478 |  544 | `	if( !hasUnderscore ){` |
|   89226 |  545 | `		SyStringDupPtr(pOut, pToken);` |
|   89226 |  546 | `		return SXRET_OK;` |
|       - |  547 | `	}` |
|     253 |  548 | `	if( pToken->nByte <= nScratch ){` |
|     251 |  549 | `		zBuf = zScratch;` |
|     126 |  550 | `	}else{` |
|       3 |  551 | `		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);` |
|       3 |  552 | `		if( zBuf == 0 ){` |
|     ! 0 |  553 | `			return SXERR_ABORT;` |
|       - |  554 | `		}` |
|       3 |  555 | `		*pzAlloc = zBuf;` |
|       - |  556 | `	}` |
|     253 |  557 | `	j = 0;` |
|    2895 |  558 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|    2643 |  559 | `		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }` |
|    1322 |  560 | `	}` |
|     253 |  561 | `	SyStringInitFromBuf(pOut, zBuf, j);` |
|     253 |  562 | `	return SXRET_OK;` |
|   44740 |  563 |  |
|       - |  564 | `/*` |
|       - |  565 | ` * Compile a numeric [i.e: integer or real] literal.` |
|       - |  566 | ` * Notes on the integer type.` |
|       - |  567 | ` *  According to the PHP language reference manual` |
|       - |  568 | ` *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)` |
|       - |  569 | ` *  or binary (base 2) notation, optionally preceded by a sign (- or +).` |
|       - |  570 | ` *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal` |
|       - |  571 | ` *  notation precede the number with 0x. To use binary notation precede the number with 0b.` |
|       - |  572 | ` * Symisc eXtension to the integer type.` |
|       - |  573 | ` *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine` |
|       - |  574 | ` *  where the size of an integer is platform-dependent.That is,the size of an integer` |
|       - |  575 | ` *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms` |
|       - |  576 | ` *  [i.e: either 32bit or 64bit].` |
|       - |  577 | ` *  For more information on this powerfull extension please refer to the official` |
|       - |  578 | ` *  documentation.` |
|       - |  579 | ` */` |
|   89462 |  580 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  581 |  |
|   89464 |  582 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   89464 |  583 | `	sxu32 nIdx = 0;` |
|       - |  584 | `	char zScratch[GEN_NUM_SCRATCH];` |
|   89464 |  585 | `	char *zAlloc = 0;` |
|       - |  586 | `	SyString sNum;` |
|       - |  587 | `	sxi32 rc;` |
|   44731 |  588 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|   89464 |  589 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|   89464 |  590 | `	if( rc != SXRET_OK ){` |
|      11 |  591 | `		return rc;` |
|       - |  592 | `	}` |
|  134180 |  593 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   44726 |  594 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|   89454 |  595 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  596 | `		return SXERR_ABORT;` |
|       - |  597 | `	}` |
|   89454 |  598 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |  599 | `		ph7_value *pObj;` |
|       - |  600 | `		sxi64 iValue;` |
|   88980 |  601 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|   88980 |  602 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   88980 |  603 | `		if( pObj == 0 ){` |
|     ! 0 |  604 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |  605 | `			return SXERR_ABORT;` |
|       - |  606 | `		}` |
|   88980 |  607 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   44491 |  608 | `	}else{` |
|       - |  609 | `		/* Real number */` |
|       - |  610 | `		ph7_value *pObj;` |
|       - |  611 | `		/* Reserve a new constant */` |
|     476 |  612 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     476 |  613 | `		if( pObj == 0 ){` |
|     ! 0 |  614 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  615 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |  616 | `			return SXERR_ABORT;` |
|       - |  617 | `		}` |
|     476 |  618 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     476 |  619 | `		PH7_MemObjToReal(pObj);` |
|       - |  620 | `	}` |
|   89454 |  621 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |  622 | `	/* Emit the load constant instruction */` |
|   89454 |  623 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  624 | `	/* Node successfully compiled */` |
|   89454 |  625 | `	return SXRET_OK;` |
|   44733 |  626 |  |
|       - |  627 | `/*` |
|       - |  628 | ` * Compile a single quoted string.` |
|       - |  629 | ` * According to the PHP language reference manual:` |
|       - |  630 | ` *` |
|       - |  631 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|       - |  632 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|       - |  633 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|       - |  634 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|       - |  635 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|       - |  636 | ` *` |
|       - |  637 | ` */` |
|   58140 |  638 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  639 |  |
|   58142 |  640 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  641 | `	const char *zIn,*zCur,*zEnd;` |
|       - |  642 | `	ph7_value *pObj;` |
|       - |  643 | `	sxu32 nIdx;` |
|   58142 |  644 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |  645 | `	/* Delimit the string */` |
|   58142 |  646 | `	zIn  = pStr->zString;` |
|   58142 |  647 | `	zEnd = &zIn[pStr->nByte];` |
|   58142 |  648 | `	if( zIn >= zEnd ){` |
|       - |  649 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |  650 | `		 * rather than reserving a new object each time. */` |
|     140 |  651 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     140 |  652 | `		return SXRET_OK;` |
|       - |  653 | `	}` |
|   58004 |  654 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |  655 | `		/* Already processed,emit the load constant instruction` |
|       - |  656 | `		 * and return.` |
|       - |  657 | `		 */` |
|   17182 |  658 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   17182 |  659 | `		return SXRET_OK;` |
|       - |  660 | `	}` |
|       - |  661 | `	/* Reserve a new constant */` |
|   40824 |  662 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   40824 |  663 | `	if( pObj == 0 ){` |
|     ! 0 |  664 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  665 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  666 | `		return SXERR_ABORT;` |
|       - |  667 | `	}` |
|   40824 |  668 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  669 | `	/* Compile the node */` |
|   40864 |  670 | `	for(;;){` |
|   81730 |  671 | `		if( zIn >= zEnd ){` |
|       - |  672 | `			/* End of input */` |
|   40824 |  673 | `			break;` |
|       - |  674 | `		}` |
|   40908 |  675 | `		zCur = zIn;` |
|  649568 |  676 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  608662 |  677 | `			zIn++;` |
|       2 |  678 | `		}` |
|   40908 |  679 | `		if( zIn > zCur ){` |
|       - |  680 | `			/* Append raw contents*/` |
|   40888 |  681 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   20443 |  682 | `		}` |
|   40908 |  683 | `		zIn++;` |
|   40908 |  684 | `		if( zIn < zEnd ){` |
|     105 |  685 | `			if( zIn[0] == '\\' ){` |
|       - |  686 | `				/* A literal backslash */` |
|      23 |  687 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      94 |  688 | `			}else if( zIn[0] == '\'' ){` |
|       - |  689 | `				/* A single quote */` |
|      11 |  690 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       6 |  691 | `			}else{` |
|       - |  692 | `				/* verbatim copy */` |
|      73 |  693 | `				zIn--;` |
|      73 |  694 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|      73 |  695 | `				zIn++;` |
|       - |  696 | `			}` |
|      52 |  697 | `		}` |
|       - |  698 | `		/* Advance the stream cursor */` |
|   40908 |  699 | `		zIn++;` |
|       2 |  700 | `	}` |
|       - |  701 | `	/* Emit the load constant instruction */` |
|   40824 |  702 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   40824 |  703 | `	if( pStr->nByte < 1024 ){` |
|       - |  704 | `		/* Install in the literal table */` |
|   40824 |  705 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   20411 |  706 | `	}` |
|       - |  707 | `	/* Node successfully compiled */` |
|   40824 |  708 | `	return SXRET_OK;` |
|   29072 |  709 |  |
|       - |  710 | `/*` |
|       - |  711 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|       - |  712 | ` *` |
|       - |  713 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|       - |  714 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|       - |  715 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|       - |  716 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|       - |  717 | ` * original source buffer — the buffer is stable through compilation.` |
|       - |  718 | ` *` |
|       - |  719 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|       - |  720 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|       - |  721 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|       - |  722 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|       - |  723 | ` *     at least N)" — line too short, or first differing byte is not` |
|       - |  724 | ` *     whitespace.` |
|       - |  725 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|       - |  726 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|       - |  727 | ` */` |
|     106 |  728 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|       2 |  729 |  |
|     108 |  730 | `	SyString *pIn = &pGen->pIn->sData;` |
|     108 |  731 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - |  732 | `	const char *zPrefix;` |
|       - |  733 | `	const char *z, *zEnd;` |
|       - |  734 | `	char *zBuf, *zDst;` |
|     108 |  735 | `	if( nIndent == 0 ){` |
|       - |  736 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|      64 |  737 | `		*pOut = *pIn;` |
|      64 |  738 | `		return SXRET_OK;` |
|       - |  739 | `	}` |
|       - |  740 | `	/* Recover the marker indent prefix from the original source buffer.` |
|       - |  741 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|       - |  742 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|       - |  743 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|       - |  744 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|       - |  745 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|      46 |  746 | `	zPrefix = pIn->zString + pIn->nByte;` |
|      46 |  747 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|     ! 0 |  748 | `		zPrefix += 2;` |
|     ! 0 |  749 | `	}else{` |
|      46 |  750 | `		zPrefix += 1;` |
|       - |  751 | `	}` |
|       - |  752 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|      46 |  753 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|      46 |  754 | `	if( zBuf == 0 ){` |
|     ! 0 |  755 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  756 | `		return SXERR_ABORT;` |
|       - |  757 | `	}` |
|      46 |  758 | `	zDst = zBuf;` |
|      46 |  759 | `	z = pIn->zString;` |
|      46 |  760 | `	zEnd = z + pIn->nByte;` |
|     128 |  761 | `	while( z < zEnd ){` |
|      70 |  762 | `		const char *zLine = z;` |
|       - |  763 | `		sxu32 nLine;` |
|       - |  764 | `		int bEmpty;` |
|     798 |  765 | `		while( z < zEnd && z[0] != '\n' ){` |
|     730 |  766 | `			z++;` |
|       2 |  767 | `		}` |
|      70 |  768 | `		nLine = (sxu32)(z - zLine);` |
|      70 |  769 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|      70 |  770 | `		if( !bEmpty ){` |
|       - |  771 | `			sxu32 i;` |
|      66 |  772 | `			if( nLine < nIndent ){` |
|     ! 0 |  773 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  774 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|     ! 0 |  775 | `					nIndent);` |
|     ! 0 |  776 | `				return SXERR_ABORT;` |
|       - |  777 | `			}` |
|     268 |  778 | `			for( i = 0; i < nIndent; i++ ){` |
|     212 |  779 | `				if( zLine[i] != zPrefix[i] ){` |
|       9 |  780 | `					unsigned char c = (unsigned char)zLine[i];` |
|       9 |  781 | `					if( c == ' ' \|\| c == '\t' ){` |
|       5 |  782 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  783 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|       3 |  784 | `					}else{` |
|       7 |  785 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  786 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       2 |  787 | `							nIndent);` |
|       - |  788 | `					}` |
|       9 |  789 | `					return SXERR_ABORT;` |
|       - |  790 | `				}` |
|     103 |  791 | `			}` |
|      57 |  792 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|      57 |  793 | `			zDst += nLine - nIndent;` |
|      33 |  794 | `		}else if( nLine == 1 ){` |
|       - |  795 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|     ! 0 |  796 | `			*zDst++ = '\r';` |
|     ! 0 |  797 | `		}` |
|      61 |  798 | `		if( z < zEnd ){` |
|      25 |  799 | `			*zDst++ = '\n';` |
|      25 |  800 | `			z++;` |
|      12 |  801 | `		}` |
|       1 |  802 | `	}` |
|      37 |  803 | `	pOut->zString = zBuf;` |
|      37 |  804 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|      37 |  805 | `	return SXRET_OK;` |
|      55 |  806 |  |
|       - |  807 | `/*` |
|       - |  808 | ` * Compile a nowdoc string.` |
|       - |  809 | ` * According to the PHP language reference manual:` |
|       - |  810 | ` *` |
|       - |  811 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|       - |  812 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|       - |  813 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|       - |  814 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|       - |  815 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|       - |  816 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|       - |  817 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|       - |  818 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|       - |  819 | ` *  of the closing identifier.` |
|       - |  820 | ` */` |
|      42 |  821 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  822 |  |
|       - |  823 | `	SyString sStripped;` |
|       - |  824 | `	SyString *pStr;` |
|       - |  825 | `	ph7_value *pObj;` |
|       - |  826 | `	sxu32 nIdx;` |
|       - |  827 | `	sxi32 rc;` |
|      44 |  828 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      44 |  829 | `	if( rc != SXRET_OK ){` |
|       5 |  830 | `		return rc;` |
|       - |  831 | `	}` |
|      40 |  832 | `	pStr = &sStripped;` |
|      40 |  833 | `	nIdx = 0; /* Prevent compiler warning */` |
|      40 |  834 | `	if( pStr->nByte <= 0 ){` |
|       - |  835 | `		/* Empty string,load NULL */` |
|       7 |  836 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |  837 | `		return SXRET_OK;` |
|       - |  838 | `	}` |
|       - |  839 | `	/* Reserve a new constant */` |
|      34 |  840 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      34 |  841 | `	if( pObj == 0 ){` |
|     ! 0 |  842 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  843 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  844 | `		return SXERR_ABORT;` |
|       - |  845 | `	}` |
|       - |  846 | `	/* No processing is done here, simply a memcpy() operation */` |
|      34 |  847 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|       - |  848 | `	/* Emit the load constant instruction */` |
|      34 |  849 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  850 | `	/* Node successfully compiled */` |
|      34 |  851 | `	return SXRET_OK;` |
|      23 |  852 |  |
|       - |  853 | `/*` |
|       - |  854 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|       - |  855 | ` * According to the PHP language reference manual` |
|       - |  856 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|       - |  857 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|       - |  858 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|       - |  859 | ` *  property in a string with a minimum of effort.` |
|       - |  860 | ` *  Simple syntax` |
|       - |  861 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|       - |  862 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|       - |  863 | ` *   the end of the name.` |
|       - |  864 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|       - |  865 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|       - |  866 | ` *   as to simple variables.` |
|       - |  867 | ` *  Complex (curly) syntax` |
|       - |  868 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|       - |  869 | ` *   of complex expressions.` |
|       - |  870 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|       - |  871 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|       - |  872 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|       - |  873 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|       - |  874 | ` */` |
|    1806 |  875 | `static sxi32 GenStateProcessStringExpression(` |
|       - |  876 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  877 | `	sxu32 nLine,         /* Line number */` |
|       - |  878 | `	const char *zIn,     /* Raw expression */` |
|       - |  879 | `	const char *zEnd     /* End of the expression */` |
|       - |  880 | `	)` |
|       2 |  881 |  |
|       - |  882 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  883 | `	SySet sToken;` |
|       - |  884 | `	sxi32 rc;` |
|       - |  885 | `	/* Initialize the token set */` |
|    1808 |  886 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |  887 | `	/* Preallocate some slots */` |
|    1808 |  888 | `	SySetAlloc(&sToken,0x08);` |
|       - |  889 | `	/* Tokenize the text */` |
|    1808 |  890 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |  891 | `	/* Swap delimiter */` |
|    1808 |  892 | `	pTmpIn  = pGen->pIn;` |
|    1808 |  893 | `	pTmpEnd = pGen->pEnd;` |
|    1808 |  894 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1808 |  895 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |  896 | `	/* Compile the expression */` |
|    1808 |  897 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  898 | `	/* Restore token stream */` |
|    1808 |  899 | `	pGen->pIn  = pTmpIn;` |
|    1808 |  900 | `	pGen->pEnd = pTmpEnd;` |
|       - |  901 | `	/* Release the token set */` |
|    1808 |  902 | `	SySetRelease(&sToken);` |
|       - |  903 | `	/* Compilation result */` |
|    1808 |  904 | `	return rc;` |
|       2 |  905 |  |
|       - |  906 | `/*` |
|       - |  907 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |  908 | ` */` |
|   17208 |  909 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |  910 |  |
|       - |  911 | `	ph7_value *pConstObj;` |
|   17210 |  912 | `	sxu32 nIdx = 0;` |
|       - |  913 | `	/* Reserve a new constant */` |
|   17210 |  914 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   17210 |  915 | `	if( pConstObj == 0 ){` |
|     ! 0 |  916 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  917 | `		return 0;` |
|       - |  918 | `	}` |
|   17210 |  919 | `	(*pCount)++;` |
|   17210 |  920 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  921 | `	/* Emit the load constant instruction */` |
|   17210 |  922 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   17210 |  923 | `	return pConstObj;` |
|    8606 |  924 |  |
|       - |  925 | `/*` |
|       - |  926 | ` * Compile a double quoted/heredoc string.` |
|       - |  927 | ` * According to the PHP language reference manual` |
|       - |  928 | ` * Heredoc` |
|       - |  929 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|       - |  930 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|       - |  931 | ` *  to close the quotation.` |
|       - |  932 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|       - |  933 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|       - |  934 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|       - |  935 | ` *  Warning` |
|       - |  936 | ` *  It is very important to note that the line with the closing identifier must contain` |
|       - |  937 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|       - |  938 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|       - |  939 | ` *  It's also important to realize that the first character before the closing identifier must` |
|       - |  940 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|       - |  941 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|       - |  942 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|       - |  943 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|       - |  944 | ` *  the end of the current file, a parse error will result at the last line.` |
|       - |  945 | ` *  Heredocs can not be used for initializing class properties.` |
|       - |  946 | ` * Double quoted` |
|       - |  947 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|       - |  948 | ` *  Escaped characters Sequence 	Meaning` |
|       - |  949 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|       - |  950 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|       - |  951 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|       - |  952 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|       - |  953 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|       - |  954 | ` *  \\ backslash` |
|       - |  955 | ` *  \$ dollar sign` |
|       - |  956 | ` *  \" double-quote` |
|       - |  957 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation` |
|       - |  958 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|       - |  959 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|       - |  960 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|       - |  961 | ` * See string parsing for details.` |
|       - |  962 | ` */` |
|   15922 |  963 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |  964 |  |
|   15924 |  965 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  966 | `	const char *zIn,*zCur,*zEnd;` |
|   15924 |  967 | `	ph7_value *pObj = 0;` |
|       - |  968 | `	sxi32 iCons;` |
|       - |  969 | `	sxi32 rc;` |
|       - |  970 | `	/* Delimit the string */` |
|   15924 |  971 | `	zIn  = pStr->zString;` |
|   15924 |  972 | `	zEnd = &zIn[pStr->nByte];` |
|   15924 |  973 | `	if( zIn >= zEnd ){` |
|       - |  974 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  975 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  976 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  977 | `		 */` |
|     226 |  978 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     226 |  979 | `		return SXRET_OK;` |
|       - |  980 | `	}` |
|   15700 |  981 | `	zCur = 0;` |
|       - |  982 | `	/* Compile the node */` |
|   15700 |  983 | `	iCons = 0;` |
|    8752 |  984 | `	for(;;){` |
|   26528 |  985 | `		zCur = zIn;` |
|  140328 |  986 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  115608 |  987 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      51 |  988 | `				break;` |
|  115510 |  989 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1710 |  990 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     855 |  991 | `					break;` |
|       - |  992 | `			}` |
|  113802 |  993 | `			zIn++;` |
|       2 |  994 | `		}` |
|   26528 |  995 | `		if( zIn > zCur ){` |
|   12128 |  996 | `			if( pObj == 0 ){` |
|   11852 |  997 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   11852 |  998 | `				if( pObj == 0 ){` |
|     ! 0 |  999 | `					return SXERR_ABORT;` |
|       - | 1000 | `				}` |
|    5925 | 1001 | `			}` |
|   12128 | 1002 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    6063 | 1003 | `		}` |
|   26528 | 1004 | `		if( zIn >= zEnd ){` |
|   15700 | 1005 | `			break;` |
|       - | 1006 | `		}` |
|   10830 | 1007 | `		if( zIn[0] == '\\' ){` |
|    9024 | 1008 | `			const char *zPtr = 0;` |
|       - | 1009 | `			sxu32 n;` |
|    9024 | 1010 | `			zIn++;` |
|    9024 | 1011 | `			if( zIn >= zEnd ){` |
|     ! 0 | 1012 | `				break;` |
|       - | 1013 | `			}` |
|    9024 | 1014 | `			if( pObj == 0 ){` |
|    5360 | 1015 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    5360 | 1016 | `				if( pObj == 0 ){` |
|     ! 0 | 1017 | `					return SXERR_ABORT;` |
|       - | 1018 | `				}` |
|    2679 | 1019 | `			}` |
|    9024 | 1020 | `			n = sizeof(char); /* size of conversion */` |
|    9024 | 1021 | `			switch( zIn[0] ){` |
|       3 | 1022 | `			case '$':` |
|       - | 1023 | `				/* Dollar sign */` |
|       7 | 1024 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|       7 | 1025 | `				break;` |
|      38 | 1026 | `			case '\\':` |
|       - | 1027 | `				/* A literal backslash */` |
|      78 | 1028 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      78 | 1029 | `				break;` |
|       2 | 1030 | `			case 'a':` |
|       - | 1031 | `				/* The "alert" character (BEL)[ctrl+g] ASCII code 7 */` |
|       5 | 1032 | `				PH7_MemObjStringAppend(pObj,"\a",sizeof(char));` |
|       5 | 1033 | `				break;` |
|       2 | 1034 | `			case 'b':` |
|       - | 1035 | `				/* Backspace (BS)[ctrl+h] ASCII code 8 */` |
|       5 | 1036 | `				PH7_MemObjStringAppend(pObj,"\b",sizeof(char));` |
|       5 | 1037 | `				break;` |
|       4 | 1038 | `			case 'f':` |
|       - | 1039 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|       9 | 1040 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|       9 | 1041 | `				break;` |
|    4146 | 1042 | `			case 'n':` |
|       - | 1043 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    8294 | 1044 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    8294 | 1045 | `				break;` |
|      19 | 1046 | `			case 'r':` |
|       - | 1047 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|      40 | 1048 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|      40 | 1049 | `				break;` |
|      24 | 1050 | `			case 't':` |
|       - | 1051 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      50 | 1052 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      50 | 1053 | `				break;` |
|       3 | 1054 | `			case 'v':` |
|       - | 1055 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|       7 | 1056 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|       7 | 1057 | `				break;` |
|       1 | 1058 | `			case '\'':` |
|       - | 1059 | `				/* Single quote */` |
|       3 | 1060 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       3 | 1061 | `				break;` |
|      50 | 1062 | `			case '"':` |
|       - | 1063 | `				/* Double quote */` |
|     102 | 1064 | `				PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|     102 | 1065 | `				break;` |
|       5 | 1066 | `			case '0':` |
|       - | 1067 | `				/* NUL byte */` |
|      11 | 1068 | `				PH7_MemObjStringAppend(pObj,"\0",sizeof(char));` |
|      11 | 1069 | `				break;` |
|     188 | 1070 | `			case 'x':` |
|     377 | 1071 | `				if((unsigned char)zIn[1] < 0xc0 && SyisHex(zIn[1]) ){` |
|       - | 1072 | `					int c;` |
|       - | 1073 | `					/* Hex digit */` |
|     363 | 1074 | `					c = SyHexToint(zIn[1]) << 4;` |
|     363 | 1075 | `					if( &zIn[2] < zEnd ){` |
|     363 | 1076 | `						c +=  SyHexToint(zIn[2]);` |
|     181 | 1077 | `					}` |
|       - | 1078 | `					/* Output char */` |
|     363 | 1079 | `					PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|     363 | 1080 | `					n += sizeof(char) * 2;` |
|     182 | 1081 | `				}else{` |
|       - | 1082 | `					/* Output literal character  */` |
|      15 | 1083 | `					PH7_MemObjStringAppend(pObj,"x",sizeof(char));` |
|       - | 1084 | `				}` |
|     377 | 1085 | `				break;` |
|      15 | 1086 | `			case 'o':` |
|      31 | 1087 | `				if( &zIn[1] < zEnd && (unsigned char)zIn[1] < 0xc0 && SyisDigit(zIn[1]) && (zIn[1] - '0') < 8 ){` |
|       - | 1088 | `					/* Octal digit stream */` |
|       - | 1089 | `					int c;` |
|      21 | 1090 | `					c = 0;` |
|      21 | 1091 | `					zIn++;` |
|      61 | 1092 | `					for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|      55 | 1093 | `						if( zPtr >= zEnd \|\| (unsigned char)zPtr[0] >= 0xc0 \|\| !SyisDigit(zPtr[0]) \|\| (zPtr[0] - '0') > 7 ){` |
|       8 | 1094 | `							break;` |
|       - | 1095 | `						}` |
|      41 | 1096 | `						c = c * 8 + (zPtr[0] - '0');` |
|      21 | 1097 | `					}` |
|      21 | 1098 | `					if ( c > 0 ){` |
|      15 | 1099 | `						PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|       7 | 1100 | `					}` |
|      21 | 1101 | `					n = (sxu32)(zPtr-zIn);` |
|      11 | 1102 | `				}else{` |
|       - | 1103 | `					/* Output literal character  */` |
|      11 | 1104 | `					PH7_MemObjStringAppend(pObj,"o",sizeof(char));` |
|       - | 1105 | `				}` |
|      31 | 1106 | `				break;` |
|      11 | 1107 | `			default:` |
|       - | 1108 | `				/* Output without a slash */` |
|      23 | 1109 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char));` |
|      22 | 1110 | `				break;` |
|       - | 1111 | `			}` |
|       - | 1112 | `			/* Advance the stream cursor */` |
|    9024 | 1113 | `			zIn += n;` |
|    9024 | 1114 | `			continue;` |
|       - | 1115 | `		}` |
|    1808 | 1116 | `		if( zIn[0] == '{' ){` |
|       - | 1117 | `			/* Curly syntax */` |
|       - | 1118 | `			const char *zExpr;` |
|     101 | 1119 | `			sxi32 iNest = 1;` |
|     101 | 1120 | `			zIn++;` |
|     101 | 1121 | `			zExpr = zIn;` |
|       - | 1122 | `			/* Synchronize with the next closing curly braces */` |
|    1135 | 1123 | `			while( zIn < zEnd ){` |
|    1135 | 1124 | `				if( zIn[0] == '{' ){` |
|       - | 1125 | `					/* Increment nesting level */` |
|       9 | 1126 | `					iNest++;` |
|    1131 | 1127 | `				}else if(zIn[0] == '}' ){` |
|       - | 1128 | `					/* Decrement nesting level */` |
|     109 | 1129 | `					iNest--;` |
|     109 | 1130 | `					if( iNest <= 0 ){` |
|     101 | 1131 | `						break;` |
|       - | 1132 | `					}` |
|       4 | 1133 | `				}` |
|    1035 | 1134 | `				zIn++;` |
|       1 | 1135 | `			}` |
|       - | 1136 | `			/* Process the expression */` |
|     101 | 1137 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     101 | 1138 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1139 | `				return SXERR_ABORT;` |
|       - | 1140 | `			}` |
|     101 | 1141 | `			if( rc != SXERR_EMPTY ){` |
|     101 | 1142 | `				++iCons;` |
|      50 | 1143 | `			}` |
|     101 | 1144 | `			if( zIn < zEnd ){` |
|       - | 1145 | `				/* Jump the trailing curly */` |
|     101 | 1146 | `				zIn++;` |
|      50 | 1147 | `			}` |
|      51 | 1148 | `		}else{` |
|       - | 1149 | `			/* Simple syntax */` |
|    1708 | 1150 | `			const char *zExpr = zIn;` |
|       - | 1151 | `			/* Assemble variable name */` |
|     853 | 1152 | `			for(;;){` |
|       - | 1153 | `				/* Jump leading dollars */` |
|    3414 | 1154 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1708 | 1155 | `					zIn++;` |
|       2 | 1156 | `				}` |
|     853 | 1157 | `				for(;;){` |
|   10347 | 1158 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    7788 | 1159 | `						zIn++;` |
|       2 | 1160 | `					}` |
|    1708 | 1161 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - | 1162 | `						/* UTF-8 stream */` |
|     ! 0 | 1163 | `						zIn++;` |
|     ! 0 | 1164 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 | 1165 | `							zIn++;` |
|     ! 0 | 1166 | `						}` |
|     ! 0 | 1167 | `						continue;` |
|       - | 1168 | `					}` |
|    1708 | 1169 | `					break;` |
|     ! 0 | 1170 | `				}` |
|    1708 | 1171 | `				if( zIn >= zEnd ){` |
|     102 | 1172 | `					break;` |
|       - | 1173 | `				}` |
|    1608 | 1174 | `				if( zIn[0] == '[' ){` |
|       9 | 1175 | `					sxi32 iSquare = 1;` |
|       9 | 1176 | `					zIn++;` |
|      17 | 1177 | `					while( zIn < zEnd ){` |
|      17 | 1178 | `						if( zIn[0] == '[' ){` |
|     ! 0 | 1179 | `							iSquare++;` |
|      17 | 1180 | `						}else if (zIn[0] == ']' ){` |
|       9 | 1181 | `							iSquare--;` |
|       9 | 1182 | `							if( iSquare <= 0 ){` |
|       9 | 1183 | `								break;` |
|       - | 1184 | `							}` |
|     ! 0 | 1185 | `						}` |
|       9 | 1186 | `						zIn++;` |
|       1 | 1187 | `					}` |
|       9 | 1188 | `					if( zIn < zEnd ){` |
|       9 | 1189 | `						zIn++;` |
|       4 | 1190 | `					}` |
|       9 | 1191 | `					break;` |
|    1600 | 1192 | `				}else if(zIn[0] == '{' ){` |
|       6 | 1193 | `					sxi32 iCurly = 1;` |
|       6 | 1194 | `					zIn++;` |
|      18 | 1195 | `					while( zIn < zEnd ){` |
|      16 | 1196 | `						if( zIn[0] == '{' ){` |
|     ! 0 | 1197 | `							iCurly++;` |
|      16 | 1198 | `						}else if (zIn[0] == '}' ){` |
|       3 | 1199 | `							iCurly--;` |
|       3 | 1200 | `							if( iCurly <= 0 ){` |
|       3 | 1201 | `								break;` |
|       - | 1202 | `							}` |
|     ! 0 | 1203 | `						}` |
|      14 | 1204 | `						zIn++;` |
|       2 | 1205 | `					}` |
|       6 | 1206 | `					if( zIn < zEnd ){` |
|       3 | 1207 | `						zIn++;` |
|       1 | 1208 | `					}` |
|       6 | 1209 | `					break;` |
|    1596 | 1210 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - | 1211 | `					/* Member access operator '->' */` |
|     ! 0 | 1212 | `					zIn += 2;` |
|    1596 | 1213 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - | 1214 | `					/* Static member access operator '::' */` |
|     ! 0 | 1215 | `					zIn += 2;` |
|     ! 0 | 1216 | `				}else{` |
|     799 | 1217 | `					break;` |
|       - | 1218 | `				}` |
|     ! 0 | 1219 | `			}` |
|       - | 1220 | `			/* Process the expression */` |
|    1708 | 1221 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1708 | 1222 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1223 | `				return SXERR_ABORT;` |
|       - | 1224 | `			}` |
|    1708 | 1225 | `			if( rc != SXERR_EMPTY ){` |
|    1706 | 1226 | `				++iCons;` |
|     852 | 1227 | `			}` |
|       - | 1228 | `		}` |
|       - | 1229 | `		/* Invalidate the previously used constant */` |
|    1808 | 1230 | `		pObj = 0;` |
|       2 | 1231 | `	}/*for(;;)*/` |
|   15700 | 1232 | `	if( iCons > 1 ){` |
|       - | 1233 | `		/* Concatenate all compiled constants */` |
|    1362 | 1234 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     680 | 1235 | `	}` |
|       - | 1236 | `	/* Node successfully compiled */` |
|   15700 | 1237 | `	return SXRET_OK;` |
|    7963 | 1238 |  |
|       - | 1239 | `/*` |
|       - | 1240 | ` * Compile a double quoted string.` |
|       - | 1241 | ` *  See the block-comment above for more information.` |
|       - | 1242 | ` */` |
|   15862 | 1243 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1244 |  |
|       - | 1245 | `	sxi32 rc;` |
|   15864 | 1246 | `	rc = GenStateCompileString(&(*pGen));` |
|    7931 | 1247 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 1248 | `	/* Compilation result */` |
|   15864 | 1249 | `	return rc;` |
|       2 | 1250 |  |
|       - | 1251 | `/*` |
|       - | 1252 | ` * Compile a Heredoc string.` |
|       - | 1253 | ` *  See the block-comment above for more information.` |
|       - | 1254 | ` */` |
|      64 | 1255 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1256 |  |
|       - | 1257 | `	SyString sOrig, sStripped;` |
|       - | 1258 | `	sxi32 rc;` |
|      66 | 1259 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      66 | 1260 | `	if( rc != SXRET_OK ){` |
|       5 | 1261 | `		return rc;` |
|       - | 1262 | `	}` |
|       - | 1263 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|       - | 1264 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|       - | 1265 | `	 * Restore before returning so downstream code that references pIn is` |
|       - | 1266 | `	 * unaffected, including on the error path. */` |
|      62 | 1267 | `	sOrig = pGen->pIn->sData;` |
|      62 | 1268 | `	pGen->pIn->sData = sStripped;` |
|      62 | 1269 | `	rc = GenStateCompileString(&(*pGen));` |
|      62 | 1270 | `	pGen->pIn->sData = sOrig;` |
|      30 | 1271 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|      62 | 1272 | `	return rc;` |
|      34 | 1273 |  |
|       - | 1274 | `/*` |
|       - | 1275 | ` * Compile an array entry whether it is a key or a value.` |
|       - | 1276 | ` *  Notes on array entries.` |
|       - | 1277 | ` *  According to the PHP language reference manual` |
|       - | 1278 | ` *  An array can be created by the array() language construct.` |
|       - | 1279 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|       - | 1280 | ` *  array(  key =>  value` |
|       - | 1281 | ` *    , ...` |
|       - | 1282 | ` *    )` |
|       - | 1283 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|       - | 1284 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|       - | 1285 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|       - | 1286 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|       - | 1287 | ` *  contain integer and string indices.` |
|       - | 1288 | ` *  A value can be any PHP type.` |
|       - | 1289 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|       - | 1290 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|       - | 1291 | ` *  is specified, that value will be overwritten.` |
|       - | 1292 | ` */` |
|   16114 | 1293 | `static sxi32 GenStateCompileArrayEntry(` |
|       - | 1294 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 1295 | `	SyToken *pIn,        /* Token stream */` |
|       - | 1296 | `	SyToken *pEnd,       /* End of the token stream */` |
|       - | 1297 | `	sxi32 iFlags,        /* Compilation flags */` |
|       - | 1298 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|       - | 1299 | `	)` |
|       2 | 1300 |  |
|       - | 1301 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 1302 | `	sxi32 rc;` |
|       - | 1303 | `	/* Swap token stream */` |
|   16116 | 1304 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - | 1305 | `	/* Compile the expression*/` |
|   16116 | 1306 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - | 1307 | `	/* Restore token stream */` |
|   16116 | 1308 | `	RE_SWAP_DELIMITER(pGen);` |
|   16116 | 1309 | `	return rc;` |
|       2 | 1310 |  |
|       - | 1311 | `/*` |
|       - | 1312 | ` * Expression tree validator callback for the 'array' language construct.` |
|       - | 1313 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1314 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1315 | ` * error message.` |
|       - | 1316 | ` * See the routine responible of compiling the array language construct` |
|       - | 1317 | ` * for more inforation.` |
|       - | 1318 | ` */` |
|      30 | 1319 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 1320 |  |
|      32 | 1321 | `	sxi32 rc = SXRET_OK;` |
|      32 | 1322 | `	if( pRoot->pOp ){` |
|      19 | 1323 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 | 1324 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      14 | 1325 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 1326 | `			/* Unexpected expression */` |
|      11 | 1327 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1328 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|      11 | 1329 | `			if( rc != SXERR_ABORT ){` |
|      11 | 1330 | `				rc = SXERR_INVALID;` |
|       5 | 1331 | `			}` |
|       7 | 1332 | `		}` |
|      25 | 1333 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1334 | `		/* Unexpected expression */` |
|       3 | 1335 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1336 | `			"array(): Expecting a variable after reference operator '&'");` |
|       3 | 1337 | `		if( rc != SXERR_ABORT ){` |
|       3 | 1338 | `			rc = SXERR_INVALID;` |
|       1 | 1339 | `		}` |
|       1 | 1340 | `	}` |
|      32 | 1341 | `	return rc;` |
|       2 | 1342 |  |
|       - | 1343 | `/*` |
|       - | 1344 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - | 1345 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - | 1346 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - | 1347 | ` */` |
|   23590 | 1348 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 | 1349 |  |
|       - | 1350 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - | 1351 | `	SyToken *pKey,*pCur;` |
|   23592 | 1352 | `	sxi32 iEmitRef = 0;` |
|   23592 | 1353 | `	sxi32 nPair = 0;` |
|       - | 1354 | `	sxi32 iNest;` |
|       - | 1355 | `	sxi32 rc;` |
|   23592 | 1356 | `	xValidator = 0;` |
|   19185 | 1357 | `	for(;;){` |
|       - | 1358 | `		/* Jump leading commas */` |
|   43386 | 1359 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    5016 | 1360 | `			pGen->pIn++;` |
|       2 | 1361 | `		}` |
|   38372 | 1362 | `		pCur = pGen->pIn;` |
|   38372 | 1363 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - | 1364 | `			/* No more entry to process */` |
|   23580 | 1365 | `			break;` |
|       - | 1366 | `		}` |
|   14794 | 1367 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 | 1368 | `			continue;` |
|       - | 1369 | `		}` |
|       - | 1370 | `		/* Compile the key if available */` |
|   14794 | 1371 | `		pKey = pCur;` |
|   14794 | 1372 | `		iNest = 0;` |
|   41046 | 1373 | `		while( pCur < pGen->pIn ){` |
|   27474 | 1374 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1218 | 1375 | `				break;` |
|       - | 1376 | `			}` |
|       - | 1377 | `			/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - | 1378 | `			 * The '=>' inside an arrow function is not an array key/value` |
|       - | 1379 | `			 * separator — it introduces the expression body. Skip past the` |
|       - | 1380 | `			 * signature so the body scan sees no false '=>'.` |
|       - | 1381 | `			 */` |
|   26258 | 1382 | `			if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|      74 | 1383 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|      74 | 1384 | `				SyToken *pFn = pCur;` |
|      72 | 1385 | `				if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pGen->pIn` |
|     ! 0 | 1386 | `					&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|       2 | 1387 | `					&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 | 1388 | `					pFn = &pCur[1];` |
|     ! 0 | 1389 | `					nKw = PH7_TKWRD_FN;` |
|     ! 0 | 1390 | `				}` |
|      74 | 1391 | `				if( nKw == PH7_TKWRD_FN ){` |
|       5 | 1392 | `					pCur = pFn + 1; /* past 'fn' */` |
|       5 | 1393 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_AMPER) ){` |
|     ! 0 | 1394 | `						pCur++;` |
|     ! 0 | 1395 | `					}` |
|       5 | 1396 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_LPAREN) ){` |
|       5 | 1397 | `						pCur++;` |
|       5 | 1398 | `						PH7_DelimitNestedTokens(pCur,pGen->pIn,` |
|       - | 1399 | `							PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       5 | 1400 | `						if( pCur < pGen->pIn ){` |
|       5 | 1401 | `							pCur++;` |
|       2 | 1402 | `						}` |
|       2 | 1403 | `					}` |
|       5 | 1404 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_COLON) ){` |
|     ! 0 | 1405 | `						pCur++;` |
|     ! 0 | 1406 | `						if( pCur < pGen->pIn && (pCur->nType & PH7_TK_OP)` |
|     ! 0 | 1407 | `							&& pCur->sData.nByte == 1` |
|     ! 0 | 1408 | `							&& pCur->sData.zString[0] == '?' ){` |
|     ! 0 | 1409 | `							pCur++;` |
|     ! 0 | 1410 | `						}` |
|     ! 0 | 1411 | `						if( pCur < pGen->pIn` |
|     ! 0 | 1412 | `							&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 | 1413 | `							pCur++;` |
|     ! 0 | 1414 | `						}` |
|     ! 0 | 1415 | `					}` |
|       - | 1416 | `					/* The rest of the entry is the arrow function body — no` |
|       - | 1417 | `					 * outer key to extract. Stop the scan here. */` |
|       5 | 1418 | `					pCur = pGen->pIn;` |
|       5 | 1419 | `					break;` |
|       - | 1420 | `				}` |
|      34 | 1421 | `			}` |
|   26254 | 1422 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      78 | 1423 | `				iNest++;` |
|   26216 | 1424 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - | 1425 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - | 1426 | `				 * parser will shortly detect any syntax error.` |
|       - | 1427 | `				 */` |
|      78 | 1428 | `				iNest--;` |
|      38 | 1429 | `			}` |
|   26254 | 1430 | `			pCur++;` |
|       2 | 1431 | `		}` |
|   14794 | 1432 | `		rc = SXERR_EMPTY;` |
|   14794 | 1433 | `		if( pCur < pGen->pIn ){` |
|    1218 | 1434 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - | 1435 | `				/* Missing value */` |
|      11 | 1436 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      11 | 1437 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1438 | `					return SXERR_ABORT;` |
|       - | 1439 | `				}` |
|      11 | 1440 | `				return SXRET_OK;` |
|       - | 1441 | `			}` |
|       - | 1442 | `			/* Compile the expression holding the key */` |
|    1208 | 1443 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - | 1444 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1208 | 1445 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1446 | `				return SXERR_ABORT;` |
|       - | 1447 | `			}` |
|    1208 | 1448 | `			pCur++; /* Jump the '=>' operator */` |
|   14181 | 1449 | `		}else if( pKey == pCur ){` |
|       - | 1450 | `			/* Key is omitted,emit a warning */` |
|     ! 0 | 1451 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 | 1452 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 | 1453 | `		}else{` |
|       - | 1454 | `			/* Reset back the cursor and point to the entry value */` |
|   13578 | 1455 | `			pCur = pKey;` |
|       - | 1456 | `		}` |
|   14784 | 1457 | `		if( rc == SXERR_EMPTY ){` |
|       - | 1458 | `			/* No available key,load NULL */` |
|   13580 | 1459 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    6789 | 1460 | `		}` |
|   14784 | 1461 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - | 1462 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      34 | 1463 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      34 | 1464 | `			iEmitRef = 1;` |
|      34 | 1465 | `			pCur++; /* Jump the '&' token */` |
|      34 | 1466 | `			if( pCur >= pGen->pIn ){` |
|       - | 1467 | `				/* Missing value */` |
|       3 | 1468 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|       3 | 1469 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1470 | `					return SXERR_ABORT;` |
|       - | 1471 | `				}` |
|       3 | 1472 | `				return SXRET_OK;` |
|       - | 1473 | `			}` |
|      15 | 1474 | `		}` |
|       - | 1475 | `		/* Compile indice value */` |
|   14782 | 1476 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   14782 | 1477 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1478 | `			return SXERR_ABORT;` |
|       - | 1479 | `		}` |
|   14782 | 1480 | `		if( iEmitRef ){` |
|       - | 1481 | `			/* Emit the load reference instruction */` |
|      32 | 1482 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 | 1483 | `		}` |
|   14782 | 1484 | `		xValidator = 0;` |
|   14782 | 1485 | `		iEmitRef = 0;` |
|   14782 | 1486 | `		nPair++;` |
|       2 | 1487 | `	}` |
|       - | 1488 | `	/* Emit the load map instruction */` |
|   23580 | 1489 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - | 1490 | `	/* Node successfully compiled */` |
|   23580 | 1491 | `	return SXRET_OK;` |
|   11797 | 1492 |  |
|       - | 1493 | `/*` |
|       - | 1494 | ` * Compile the 'array' language construct.` |
|       - | 1495 | ` *	 According to the PHP language reference manual` |
|       - | 1496 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - | 1497 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - | 1498 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - | 1499 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - | 1500 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - | 1501 | ` */` |
|   23324 | 1502 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1503 |  |
|       - | 1504 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   23326 | 1505 | `	pGen->pIn += 2;` |
|   23326 | 1506 | `	pGen->pEnd--;` |
|   11662 | 1507 | `	SXUNUSED(iCompileFlag);` |
|   23326 | 1508 | `	return GenStateCompileArrayBody(pGen);` |
|       2 | 1509 |  |
|       - | 1510 | `/*` |
|       - | 1511 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - | 1512 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - | 1513 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - | 1514 | ` */` |
|     266 | 1515 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1516 |  |
|       - | 1517 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     268 | 1518 | `	pGen->pIn++;` |
|     268 | 1519 | `	pGen->pEnd--;` |
|     133 | 1520 | `	SXUNUSED(iCompileFlag);` |
|     268 | 1521 | `	return GenStateCompileArrayBody(pGen);` |
|       2 | 1522 |  |
|       - | 1523 | `/*` |
|       - | 1524 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - | 1525 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1526 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1527 | ` * error message.` |
|       - | 1528 | ` * See the routine responible of compiling the list language construct` |
|       - | 1529 | ` * for more inforation.` |
|       - | 1530 | ` */` |
|     128 | 1531 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 1532 |  |
|     130 | 1533 | `	sxi32 rc = SXRET_OK;` |
|     130 | 1534 | `	if( pRoot->pOp ){` |
|     ! 0 | 1535 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|     ! 0 | 1536 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - | 1537 | `				/* Unexpected expression */` |
|     ! 0 | 1538 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1539 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 | 1540 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 | 1541 | `					rc = SXERR_INVALID;` |
|     ! 0 | 1542 | `				}` |
|     ! 0 | 1543 | `		}` |
|     130 | 1544 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1545 | `		/* Unexpected expression */` |
|       5 | 1546 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1547 | `			"list(): Expecting a variable not an expression");` |
|       5 | 1548 | `		if( rc != SXERR_ABORT ){` |
|       5 | 1549 | `			rc = SXERR_INVALID;` |
|       2 | 1550 | `		}` |
|       2 | 1551 | `	}` |
|     130 | 1552 | `	return rc;` |
|       2 | 1553 |  |
|       - | 1554 | `/*` |
|       - | 1555 | ` * Compile the 'list' language construct.` |
|       - | 1556 | ` *  According to the PHP language reference` |
|       - | 1557 | ` *  list(): Assign variables as if they were an array.` |
|       - | 1558 | ` *  list() is used to assign a list of variables in one operation.` |
|       - | 1559 | ` *  Description` |
|       - | 1560 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - | 1561 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - | 1562 | ` *   list() is used to assign a list of variables in one operation.` |
|       - | 1563 | ` *  Parameters` |
|       - | 1564 | ` *   $varname: A variable.` |
|       - | 1565 | ` *  Return Values` |
|       - | 1566 | ` *   The assigned array.` |
|       - | 1567 | ` */` |
|       - | 1568 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - | 1569 | `struct NestedListEntry {` |
|       - | 1570 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - | 1571 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - | 1572 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - | 1573 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - | 1574 | `};` |
|       - | 1575 | `/*` |
|       - | 1576 | ` * Shared body for list() and short list [...] compilation.` |
|       - | 1577 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - | 1578 | ` * the opening delimiter and before the closing delimiter.` |
|       - | 1579 | ` */` |
|      74 | 1580 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       2 | 1581 |  |
|       - | 1582 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - | 1583 | `	SyToken *pNext;` |
|       - | 1584 | `	sxi32 nExpr;` |
|       - | 1585 | `	sxi32 rc;` |
|      76 | 1586 | `	nExpr = 0;` |
|      76 | 1587 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     230 | 1588 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     156 | 1589 | `		if( pGen->pIn < pNext ){` |
|       - | 1590 | `			/* Check for nested list() */` |
|     144 | 1591 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 | 1592 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 1593 | `				/* Record this nested list for post-processing */` |
|       3 | 1594 | `				SyToken *pListEnd = 0;` |
|       3 | 1595 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 | 1596 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 | 1597 | `				}` |
|       3 | 1598 | `				if( pListEnd ){` |
|       - | 1599 | `					struct NestedListEntry sEntry;` |
|       3 | 1600 | `					sEntry.nIndex = nExpr;` |
|       3 | 1601 | `					sEntry.pStart = pGen->pIn;` |
|       3 | 1602 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 | 1603 | `					sEntry.isShort = 0;` |
|       3 | 1604 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 | 1605 | `				}` |
|       - | 1606 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 | 1607 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     143 | 1608 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - | 1609 | `				/* Nested short destructuring [...] */` |
|      13 | 1610 | `				SyToken *pBracketEnd = 0;` |
|      13 | 1611 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 | 1612 | `				if( pBracketEnd ){` |
|       - | 1613 | `					struct NestedListEntry sEntry;` |
|      13 | 1614 | `					sEntry.nIndex = nExpr;` |
|      13 | 1615 | `					sEntry.pStart = pGen->pIn;` |
|      13 | 1616 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 | 1617 | `					sEntry.isShort = 1;` |
|      13 | 1618 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 | 1619 | `				}` |
|       - | 1620 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 | 1621 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 | 1622 | `			}else{` |
|       - | 1623 | `				/* Compile the expression holding the variable */` |
|     130 | 1624 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     130 | 1625 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 1626 | `					SySetRelease(&sNested);` |
|     ! 0 | 1627 | `					return SXRET_OK;` |
|       - | 1628 | `				}` |
|       - | 1629 | `			}` |
|      73 | 1630 | `		}else{` |
|       - | 1631 | `			/* Empty entry,load NULL */` |
|      13 | 1632 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - | 1633 | `		}` |
|     156 | 1634 | `		nExpr++;` |
|       - | 1635 | `		/* Advance the stream cursor */` |
|     156 | 1636 | `		pGen->pIn = &pNext[1];` |
|       2 | 1637 | `	}` |
|       - | 1638 | `	/* Emit the LOAD_LIST instruction */` |
|      76 | 1639 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - | 1640 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - | 1641 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - | 1642 | `	 * at the corresponding index and recursively destructure it.` |
|       - | 1643 | `	 */` |
|      76 | 1644 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 | 1645 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - | 1646 | `		sxu32 i;` |
|      27 | 1647 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 | 1648 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 | 1649 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - | 1650 | `			ph7_value *pIdx;` |
|       - | 1651 | `			sxu32 nConstIdx;` |
|       - | 1652 | `			/* DUP the source array (it's on stack top) */` |
|      15 | 1653 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - | 1654 | `			/* Push the integer index for this nested entry */` |
|      15 | 1655 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 | 1656 | `			if( pIdx == 0 ){` |
|     ! 0 | 1657 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1658 | `				SySetRelease(&sNested);` |
|     ! 0 | 1659 | `				return SXERR_ABORT;` |
|       - | 1660 | `			}` |
|      15 | 1661 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 | 1662 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - | 1663 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - | 1664 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - | 1665 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - | 1666 | `			 */` |
|      15 | 1667 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - | 1668 | `			/* Recursively compile the inner list */` |
|      15 | 1669 | `			pGen->pIn = apNested[i].pStart;` |
|      15 | 1670 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 | 1671 | `			if( apNested[i].isShort ){` |
|      13 | 1672 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 | 1673 | `			}else{` |
|       3 | 1674 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - | 1675 | `			}` |
|      15 | 1676 | `			pGen->pIn = pSavedIn;` |
|      15 | 1677 | `			pGen->pEnd = pSavedEnd;` |
|      15 | 1678 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1679 | `				SySetRelease(&sNested);` |
|     ! 0 | 1680 | `				return SXERR_ABORT;` |
|       - | 1681 | `			}` |
|       - | 1682 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 | 1683 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 | 1684 | `		}` |
|       6 | 1685 | `	}` |
|      76 | 1686 | `	SySetRelease(&sNested);` |
|       - | 1687 | `	/* Node successfully compiled */` |
|      76 | 1688 | `	return SXRET_OK;` |
|      39 | 1689 |  |
|      32 | 1690 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1691 |  |
|       - | 1692 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      34 | 1693 | `	pGen->pIn += 2;` |
|      34 | 1694 | `	pGen->pEnd--;` |
|      16 | 1695 | `	SXUNUSED(iCompileFlag);` |
|      34 | 1696 | `	return GenStateCompileListBody(pGen);` |
|       2 | 1697 |  |
|      42 | 1698 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1699 |  |
|       - | 1700 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      44 | 1701 | `	pGen->pIn++;` |
|      44 | 1702 | `	pGen->pEnd--;` |
|      21 | 1703 | `	SXUNUSED(iCompileFlag);` |
|      44 | 1704 | `	return GenStateCompileListBody(pGen);` |
|       2 | 1705 |  |
|       - | 1706 | `/* Forward declarations */` |
|       - | 1707 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - | 1708 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - | 1709 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - | 1710 | `/*` |
|       - | 1711 | ` * Compile an annoynmous function or a closure.` |
|       - | 1712 | ` * According to the PHP language reference` |
|       - | 1713 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - | 1714 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - | 1715 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - | 1716 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - | 1717 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - | 1718 | ` *  Example Anonymous function variable assignment example` |
|       - | 1719 | ` * <?php` |
|       - | 1720 | ` * $greet = function($name)` |
|       - | 1721 | ` * {` |
|       - | 1722 | ` *    printf("Hello %s\r\n", $name);` |
|       - | 1723 | ` * };` |
|       - | 1724 | ` * $greet('World');` |
|       - | 1725 | ` * $greet('PHP');` |
|       - | 1726 | ` * ?>` |
|       - | 1727 | ` * Note that the implementation of annoynmous function and closure under` |
|       - | 1728 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - | 1729 | ` */` |
|     168 | 1730 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1731 |  |
|       - | 1732 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - | 1733 | `	char zName[512];         /* Unique lambda name */` |
|       - | 1734 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - | 1735 | `							  * one thread is allowed to compile the script.` |
|       - | 1736 | `						      */` |
|       - | 1737 | `	ph7_value *pObj;` |
|       - | 1738 | `	SyString sName;` |
|       - | 1739 | `	sxu32 nIdx;` |
|       - | 1740 | `	sxu32 nLen;` |
|       - | 1741 | `	sxi32 rc;` |
|       - | 1742 |  |
|     170 | 1743 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     170 | 1744 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 | 1745 | `		pGen->pIn++;` |
|     ! 0 | 1746 | `	}` |
|       - | 1747 | `	/* Reserve a constant for the lambda */` |
|     170 | 1748 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     170 | 1749 | `	if( pObj == 0 ){` |
|     ! 0 | 1750 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1751 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1752 | `		return SXERR_ABORT;` |
|       - | 1753 | `	}` |
|       - | 1754 | `	/* Generate a unique name */` |
|     170 | 1755 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - | 1756 | `	/* Make sure the generated name is unique */` |
|     170 | 1757 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 | 1758 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 | 1759 | `	}` |
|     170 | 1760 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     170 | 1761 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - | 1762 | `	/* Compile the lambda body */` |
|     170 | 1763 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     170 | 1764 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1765 | `		return SXERR_ABORT;` |
|       - | 1766 | `	}` |
|     170 | 1767 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 1768 | `		/* Emit the load closure instruction */` |
|      16 | 1769 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       9 | 1770 | `	}else{` |
|       - | 1771 | `		/* Emit the load constant instruction */` |
|     156 | 1772 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1773 | `	}` |
|       - | 1774 | `	/* Node successfully compiled */` |
|     170 | 1775 | `	return SXRET_OK;` |
|      86 | 1776 |  |
|       - | 1777 | `/*` |
|       - | 1778 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - | 1779 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - | 1780 | ` * enclosing arrow level, or has already been captured.` |
|       - | 1781 | ` */` |
|     120 | 1782 | `static sxi32 GenStateArrowAddCapture(` |
|       - | 1783 | `	ph7_gen_state *pGen,` |
|       - | 1784 | `	ph7_vm_func *pFunc,` |
|       - | 1785 | `	const char *zName,` |
|       - | 1786 | `	sxu32 nByte,` |
|       - | 1787 | `	SyString *aShadow,` |
|       - | 1788 | `	sxu32 nShadow)` |
|       1 | 1789 |  |
|       - | 1790 | `	ph7_vm_func_closure_env sEnv;` |
|       - | 1791 | `	ph7_vm_func_closure_env *aEnv;` |
|       - | 1792 | `	sxu32 n, nEnv;` |
|       - | 1793 | `	char *zDup;` |
|     121 | 1794 | `	if( nByte == 0 ){` |
|     ! 0 | 1795 | `		return SXRET_OK;` |
|       - | 1796 | `	}` |
|     120 | 1797 | `	if( nByte == sizeof("this")-1` |
|      65 | 1798 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 | 1799 | `		return SXRET_OK;` |
|       - | 1800 | `	}` |
|     145 | 1801 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|      92 | 1802 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|      88 | 1803 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      67 | 1804 | `			return SXRET_OK;` |
|       - | 1805 | `		}` |
|      14 | 1806 | `	}` |
|      53 | 1807 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      53 | 1808 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      81 | 1809 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 | 1810 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 | 1811 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 | 1812 | `			return SXRET_OK;` |
|       - | 1813 | `		}` |
|      15 | 1814 | `	}` |
|      53 | 1815 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      53 | 1816 | `	if( zDup == 0 ){` |
|     ! 0 | 1817 | `		return SXERR_ABORT;` |
|       - | 1818 | `	}` |
|      53 | 1819 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      53 | 1820 | `	sEnv.iFlags = 0;` |
|      53 | 1821 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      53 | 1822 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      53 | 1823 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      53 | 1824 | `	return SXRET_OK;` |
|      61 | 1825 |  |
|       - | 1826 | `/*` |
|       - | 1827 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - | 1828 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - | 1829 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - | 1830 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - | 1831 | ` */` |
|      14 | 1832 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - | 1833 | `	ph7_gen_state *pGen,` |
|       - | 1834 | `	ph7_vm_func *pFunc,` |
|       - | 1835 | `	const char *zIn,` |
|       - | 1836 | `	const char *zEnd,` |
|       - | 1837 | `	SyString *aShadow,` |
|       - | 1838 | `	sxu32 nShadow)` |
|       1 | 1839 |  |
|       - | 1840 | `	sxi32 rc;` |
|     159 | 1841 | `	while( zIn < zEnd ){` |
|     145 | 1842 | `		if( zIn[0] == '\\' ){` |
|     ! 0 | 1843 | `			zIn++;` |
|     ! 0 | 1844 | `			if( zIn < zEnd ){` |
|     ! 0 | 1845 | `				zIn++;` |
|     ! 0 | 1846 | `			}` |
|     ! 0 | 1847 | `			continue;` |
|       - | 1848 | `		}` |
|     144 | 1849 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      13 | 1850 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      12 | 1851 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - | 1852 | `			const char *zName;` |
|      13 | 1853 | `			zIn++; /* skip '$' */` |
|      13 | 1854 | `			zName = zIn;` |
|      39 | 1855 | `			while( zIn < zEnd ){` |
|      35 | 1856 | `				unsigned char c = (unsigned char)zIn[0];` |
|      35 | 1857 | `				if( c >= 0xc0 ){` |
|     ! 0 | 1858 | `					zIn++;` |
|     ! 0 | 1859 | `					while( zIn < zEnd` |
|     ! 0 | 1860 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 | 1861 | `						zIn++;` |
|     ! 0 | 1862 | `					}` |
|     ! 0 | 1863 | `					continue;` |
|       - | 1864 | `				}` |
|      35 | 1865 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       9 | 1866 | `					break;` |
|       - | 1867 | `				}` |
|      27 | 1868 | `				zIn++;` |
|       1 | 1869 | `			}` |
|      13 | 1870 | `			if( zIn > zName ){` |
|      19 | 1871 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      12 | 1872 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      13 | 1873 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1874 | `					return SXERR_ABORT;` |
|       - | 1875 | `				}` |
|       6 | 1876 | `			}` |
|      13 | 1877 | `			continue;` |
|       - | 1878 | `		}` |
|     133 | 1879 | `		zIn++;` |
|       1 | 1880 | `	}` |
|      15 | 1881 | `	return SXRET_OK;` |
|       8 | 1882 |  |
|       - | 1883 | `/*` |
|       - | 1884 | ` * Scan the body token range of an arrow function for free-variable` |
|       - | 1885 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - | 1886 | ` *   - plain $<id> pairs` |
|       - | 1887 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - | 1888 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - | 1889 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - | 1890 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - | 1891 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - | 1892 | ` *     are never mistakenly captured.` |
|       - | 1893 | ` */` |
|     102 | 1894 | `static sxi32 GenStateArrowCaptureScan(` |
|       - | 1895 | `	ph7_gen_state *pGen,` |
|       - | 1896 | `	ph7_vm_func *pFunc,` |
|       - | 1897 | `	SyToken *pStart,` |
|       - | 1898 | `	SyToken *pEnd,` |
|       - | 1899 | `	SyString *aShadow,` |
|       - | 1900 | `	sxu32 nShadow)` |
|       1 | 1901 |  |
|     103 | 1902 | `	SyToken *pScan = pStart;` |
|       - | 1903 | `	sxi32 rc;` |
|     371 | 1904 | `	while( pScan < pEnd ){` |
|     269 | 1905 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      22 | 1906 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|       7 | 1907 | `				pScan->sData.zString,` |
|      14 | 1908 | `				pScan->sData.zString + pScan->sData.nByte,` |
|       7 | 1909 | `				aShadow,nShadow);` |
|      15 | 1910 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1911 | `				return SXERR_ABORT;` |
|       - | 1912 | `			}` |
|      15 | 1913 | `			pScan++;` |
|      15 | 1914 | `			continue;` |
|       - | 1915 | `		}` |
|     255 | 1916 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      19 | 1917 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      19 | 1918 | `			SyToken *pFnKw = pScan;` |
|      18 | 1919 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 | 1920 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       1 | 1921 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 | 1922 | `				pFnKw = &pScan[1];` |
|     ! 0 | 1923 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 | 1924 | `			}` |
|      19 | 1925 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - | 1926 | `				SyToken *pInnerSigStart;` |
|       - | 1927 | `				SyToken *pInnerSigEnd;` |
|       - | 1928 | `				SyToken *pInnerBodyEnd;` |
|       - | 1929 | `				SyString *aInnerShadow;` |
|       - | 1930 | `				sxu32 nInnerShadow;` |
|       - | 1931 | `				sxu32 nInnerParamMax;` |
|       - | 1932 | `				SyToken *p;` |
|       - | 1933 | `				int iNestInner;` |
|      19 | 1934 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 | 1935 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 | 1936 | `					pScan++;` |
|     ! 0 | 1937 | `				}` |
|      19 | 1938 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 1939 | `					pScan++;` |
|     ! 0 | 1940 | `					continue;` |
|       - | 1941 | `				}` |
|      19 | 1942 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 | 1943 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - | 1944 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 | 1945 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 | 1946 | `					pScan = pEnd;` |
|     ! 0 | 1947 | `					continue;` |
|       - | 1948 | `				}` |
|       - | 1949 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 | 1950 | `				nInnerParamMax = 0;` |
|      57 | 1951 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 | 1952 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 | 1953 | `						nInnerParamMax++;` |
|       6 | 1954 | `					}` |
|      20 | 1955 | `				}` |
|      19 | 1956 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 | 1957 | `					&pGen->pVm->sAllocator,` |
|      18 | 1958 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 | 1959 | `				if( aInnerShadow == 0 ){` |
|     ! 0 | 1960 | `					return SXERR_ABORT;` |
|       - | 1961 | `				}` |
|      19 | 1962 | `				nInnerShadow = 0;` |
|      25 | 1963 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 | 1964 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 | 1965 | `				}` |
|      57 | 1966 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 | 1967 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 | 1968 | `						continue;` |
|       - | 1969 | `					}` |
|      13 | 1970 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 | 1971 | `						break;` |
|       - | 1972 | `					}` |
|      13 | 1973 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 1974 | `						continue;` |
|       - | 1975 | `					}` |
|      13 | 1976 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 | 1977 | `				}` |
|      19 | 1978 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 | 1979 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 | 1980 | `					pScan++;` |
|     ! 0 | 1981 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 | 1982 | `						&& pScan->sData.nByte == 1` |
|     ! 0 | 1983 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 | 1984 | `						pScan++;` |
|     ! 0 | 1985 | `					}` |
|     ! 0 | 1986 | `					if( pScan < pEnd` |
|     ! 0 | 1987 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 | 1988 | `						pScan++;` |
|     ! 0 | 1989 | `					}` |
|     ! 0 | 1990 | `				}` |
|      19 | 1991 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 | 1992 | `					pScan++; /* past '=>' */` |
|       9 | 1993 | `				}` |
|      19 | 1994 | `				pInnerBodyEnd = pScan;` |
|      19 | 1995 | `				iNestInner = 0;` |
|     131 | 1996 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 | 1997 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - | 1998 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - | 1999 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 | 2000 | `						break;` |
|       - | 2001 | `					}` |
|     113 | 2002 | `					if( pInnerBodyEnd->nType &` |
|       - | 2003 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 | 2004 | `						iNestInner++;` |
|     112 | 2005 | `					}else if( pInnerBodyEnd->nType &` |
|       - | 2006 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 | 2007 | `						iNestInner--;` |
|       1 | 2008 | `					}` |
|     113 | 2009 | `					pInnerBodyEnd++;` |
|       1 | 2010 | `				}` |
|       - | 2011 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - | 2012 | `				 * the outer's body: a default value is evaluated at call time` |
|       - | 2013 | `				 * in the outer frame, so any free variable it references is` |
|       - | 2014 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - | 2015 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - | 2016 | `				 * or those names leak into the outer's closure environment.` |
|       - | 2017 | `				 *` |
|       - | 2018 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - | 2019 | `				 * top-level commas, and for each argument scan only the token` |
|       - | 2020 | `				 * range after the '=' sign. */` |
|       - | 2021 | `				{` |
|      19 | 2022 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 | 2023 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 | 2024 | `						SyToken *pArgEnd = pArgStart;` |
|      13 | 2025 | `						SyToken *pEq = 0;` |
|      13 | 2026 | `						int iNestArg = 0;` |
|      49 | 2027 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 | 2028 | `							if( iNestArg == 0` |
|      39 | 2029 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 | 2030 | `								break;` |
|       - | 2031 | `							}` |
|      37 | 2032 | `							if( pArgEnd->nType &` |
|       - | 2033 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 | 2034 | `								iNestArg++;` |
|      37 | 2035 | `							}else if( pArgEnd->nType &` |
|       - | 2036 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 | 2037 | `								iNestArg--;` |
|     ! 0 | 2038 | `							}` |
|      36 | 2039 | `							if( pEq == 0 && iNestArg == 0` |
|      31 | 2040 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 | 2041 | `								pEq = pArgEnd;` |
|       3 | 2042 | `							}` |
|      37 | 2043 | `							pArgEnd++;` |
|       1 | 2044 | `						}` |
|      13 | 2045 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 | 2046 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 | 2047 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 | 2048 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 2049 | `								return SXERR_ABORT;` |
|       - | 2050 | `							}` |
|       3 | 2051 | `						}` |
|      13 | 2052 | `						pArgStart = pArgEnd;` |
|      12 | 2053 | `						if( pArgStart < pInnerSigEnd` |
|       8 | 2054 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 | 2055 | `							pArgStart++;` |
|       1 | 2056 | `						}` |
|       1 | 2057 | `					}` |
|       - | 2058 | `				}` |
|      28 | 2059 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 | 2060 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 | 2061 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2062 | `					return SXERR_ABORT;` |
|       - | 2063 | `				}` |
|      19 | 2064 | `				pScan = pInnerBodyEnd;` |
|      19 | 2065 | `				continue;` |
|       - | 2066 | `			}` |
|     ! 0 | 2067 | `		}` |
|     237 | 2068 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     129 | 2069 | `			pScan++;` |
|     129 | 2070 | `			continue;` |
|       - | 2071 | `		}` |
|       - | 2072 | `		{` |
|       - | 2073 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     109 | 2074 | `			SyToken *pDollar = pScan;` |
|     162 | 2075 | `			while( &pDollar[1] < pEnd` |
|     109 | 2076 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 | 2077 | `				pDollar++;` |
|     ! 0 | 2078 | `			}` |
|     109 | 2079 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 | 2080 | `				break;` |
|       - | 2081 | `			}` |
|     109 | 2082 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 2083 | `				pScan = pDollar + 1;` |
|     ! 0 | 2084 | `				continue;` |
|       - | 2085 | `			}` |
|     163 | 2086 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     108 | 2087 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      54 | 2088 | `				aShadow,nShadow);` |
|     109 | 2089 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2090 | `				return SXERR_ABORT;` |
|       - | 2091 | `			}` |
|     109 | 2092 | `			pScan = pDollar + 2;` |
|       - | 2093 | `		}` |
|       1 | 2094 | `	}` |
|     103 | 2095 | `	return SXRET_OK;` |
|      52 | 2096 |  |
|       - | 2097 | `/*` |
|       - | 2098 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - | 2099 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - | 2100 | ` * variables by value. The body is a single expression that acts as an` |
|       - | 2101 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - | 2102 | ` * $this is also made available.` |
|       - | 2103 | ` */` |
|      84 | 2104 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 2105 |  |
|       - | 2106 | `	ph7_vm_func *pFunc;` |
|       - | 2107 | `	ph7_vm_func_closure_env sEnv;` |
|       - | 2108 | `	GenBlock *pBlock;` |
|       - | 2109 | `	SySet *pInstrContainer;` |
|       - | 2110 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - | 2111 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - | 2112 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - | 2113 | `	SyToken *pSavedEnd;` |
|       - | 2114 | `	ph7_vm_func_arg *aArgs;` |
|       - | 2115 | `	char zName[512];` |
|       - | 2116 | `	static int iCnt = 1;` |
|       - | 2117 | `	char *zDup;` |
|       - | 2118 | `	sxu32 nLen;` |
|       - | 2119 | `	sxu32 nLine;` |
|      86 | 2120 | `	sxi32 iFlags = 0;` |
|      86 | 2121 | `	int bStatic = 0;` |
|       - | 2122 | `	sxi32 rc;` |
|       - | 2123 | `	sxu32 n;` |
|      42 | 2124 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 2125 |  |
|      86 | 2126 | `	nLine = pGen->pIn->nLine;` |
|       - | 2127 | `	/* Optional 'static' prefix */` |
|      84 | 2128 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      86 | 2129 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 | 2130 | `		bStatic = 1;` |
|       3 | 2131 | `		pGen->pIn++;` |
|       1 | 2132 | `	}` |
|       - | 2133 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|      84 | 2134 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      86 | 2135 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 | 2136 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 2137 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 | 2138 | `		return SXERR_SYNTAX;` |
|       - | 2139 | `	}` |
|      86 | 2140 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - | 2141 | `	/* Optional '&' — return by reference */` |
|      86 | 2142 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 | 2143 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 | 2144 | `		pGen->pIn++;` |
|     ! 0 | 2145 | `	}` |
|       - | 2146 | `	/* Expect '(' */` |
|      86 | 2147 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 | 2148 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 | 2149 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - | 2150 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 | 2151 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 | 2152 | `		}else{` |
|     ! 0 | 2153 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 2154 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - | 2155 | `		}` |
|       3 | 2156 | `		return SXERR_SYNTAX;` |
|       - | 2157 | `	}` |
|      84 | 2158 | `	pGen->pIn++; /* Jump '(' */` |
|       - | 2159 | `	/* Delimit the parameter list */` |
|      84 | 2160 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|      84 | 2161 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 | 2162 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 2163 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 | 2164 | `		return SXERR_SYNTAX;` |
|       - | 2165 | `	}` |
|       - | 2166 | `	/* Allocate the function state */` |
|      82 | 2167 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|      82 | 2168 | `	if( pFunc == 0 ){` |
|     ! 0 | 2169 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 2170 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2171 | `		return SXERR_ABORT;` |
|       - | 2172 | `	}` |
|       - | 2173 | `	/* Generate a unique lambda name */` |
|      82 | 2174 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     166 | 2175 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      85 | 2176 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       1 | 2177 | `	}` |
|      82 | 2178 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|      82 | 2179 | `	if( zDup == 0 ){` |
|     ! 0 | 2180 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 2181 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2182 | `		return SXERR_ABORT;` |
|       - | 2183 | `	}` |
|      82 | 2184 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - | 2185 | `	/* Collect function arguments */` |
|      82 | 2186 | `	if( pGen->pIn < pSigEnd ){` |
|      52 | 2187 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd);` |
|      52 | 2188 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2189 | `			return SXERR_ABORT;` |
|       - | 2190 | `		}` |
|      25 | 2191 | `	}` |
|       - | 2192 | `	/* Point past ')' and parse optional return type */` |
|      82 | 2193 | `	pGen->pIn = &pSigEnd[1];` |
|      82 | 2194 | `	GenStateParseReturnType(pGen,pFunc);` |
|       - | 2195 | `	/* Expect '=>' */` |
|      82 | 2196 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 | 2197 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 | 2198 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - | 2199 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 | 2200 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 | 2201 | `		}else{` |
|     ! 0 | 2202 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 2203 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - | 2204 | `		}` |
|       3 | 2205 | `		return SXERR_SYNTAX;` |
|       - | 2206 | `	}` |
|      79 | 2207 | `	pGen->pIn++; /* Jump '=>' */` |
|      79 | 2208 | `	pBodyStart = pGen->pIn;` |
|      79 | 2209 | `	pBodyEnd = pGen->pEnd;` |
|       - | 2210 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - | 2211 | `	 * recursively collect free-variable references from the body. The scan` |
|       - | 2212 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - | 2213 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|      79 | 2214 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - | 2215 | `	{` |
|      79 | 2216 | `		SyString *aShadow = 0;` |
|      79 | 2217 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|      79 | 2218 | `		if( nShadow > 0 ){` |
|      49 | 2219 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      48 | 2220 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      49 | 2221 | `			if( aShadow == 0 ){` |
|     ! 0 | 2222 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 2223 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2224 | `				return SXERR_ABORT;` |
|       - | 2225 | `			}` |
|     103 | 2226 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|      55 | 2227 | `				aShadow[n] = aArgs[n].sName;` |
|      28 | 2228 | `			}` |
|      24 | 2229 | `		}` |
|     118 | 2230 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      39 | 2231 | `			aShadow,nShadow);` |
|      79 | 2232 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2233 | `			return SXERR_ABORT;` |
|       - | 2234 | `		}` |
|       - | 2235 | `	}` |
|       - | 2236 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - | 2237 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - | 2238 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - | 2239 | `	 * $this. */` |
|      79 | 2240 | `	if( !bStatic ){` |
|       - | 2241 | `		char *zThisDup;` |
|      77 | 2242 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|      77 | 2243 | `		if( zThisDup == 0 ){` |
|     ! 0 | 2244 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 2245 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2246 | `			return SXERR_ABORT;` |
|       - | 2247 | `		}` |
|      77 | 2248 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      77 | 2249 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|      77 | 2250 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      77 | 2251 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|      77 | 2252 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      38 | 2253 | `	}` |
|       - | 2254 | `	/* Arrow functions are always closures */` |
|      79 | 2255 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - | 2256 | `	/* Compile the body expression as an implicit return */` |
|     118 | 2257 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      39 | 2258 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|      79 | 2259 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2260 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 2261 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 | 2262 | `		return SXERR_ABORT;` |
|       - | 2263 | `	}` |
|      79 | 2264 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      79 | 2265 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|      79 | 2266 | `	pSavedEnd = pGen->pEnd;` |
|      79 | 2267 | `	pGen->pIn = pBodyStart;` |
|      79 | 2268 | `	pGen->pEnd = pBodyEnd;` |
|      79 | 2269 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      79 | 2270 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2271 | `		return SXERR_ABORT;` |
|       - | 2272 | `	}` |
|       - | 2273 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack' */` |
|      79 | 2274 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      79 | 2275 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      79 | 2276 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 2277 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|      79 | 2278 | `	pGen->pIn = pBodyEnd;` |
|      79 | 2279 | `	pGen->pEnd = pSavedEnd;` |
|       - | 2280 | `	/* Emit the load-closure instruction */` |
|      79 | 2281 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|      79 | 2282 | `	return SXRET_OK;` |
|      44 | 2283 |  |
|       - | 2284 | `/*` |
|       - | 2285 | ` * Compile a backtick quoted string.` |
|       - | 2286 | ` */` |
|       4 | 2287 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 2288 |  |
|       - | 2289 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - | 2290 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - | 2291 | `	 */` |
|       7 | 2292 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - | 2293 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 | 2294 | `		ph7_lib_version()` |
|       - | 2295 | `		);` |
|       - | 2296 | `	/* Load NULL */` |
|       5 | 2297 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 2298 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 2299 | `	/* Node successfully compiled */` |
|       5 | 2300 | `	return SXRET_OK;` |
|       1 | 2301 |  |
|       - | 2302 | `/*` |
|       - | 2303 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - | 2304 | ` * construct.` |
|       - | 2305 | ` */` |
|      72 | 2306 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 2307 |  |
|       - | 2308 | `	SyString *pName;` |
|       - | 2309 | `	sxu32 nKeyID;` |
|       - | 2310 | `	sxi32 rc;` |
|       - | 2311 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      74 | 2312 | `	pName = &pGen->pIn->sData;` |
|      74 | 2313 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      74 | 2314 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      74 | 2315 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 | 2316 | `		SyToken *pTmp,*pNext = 0;` |
|       - | 2317 | `		/* Compile arguments one after one */` |
|       9 | 2318 | `		pTmp = pGen->pEnd;` |
|       - | 2319 | `		/* Symisc eXtension to the PHP programming language:` |
|       - | 2320 | `		 * 'echo' can be used in the context of a function which` |
|       - | 2321 | `		 *  mean that the following expression is valid:` |
|       - | 2322 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - | 2323 | `		 */` |
|       9 | 2324 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 | 2325 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 | 2326 | `			if( pGen->pIn < pNext ){` |
|       9 | 2327 | `				pGen->pEnd = pNext;` |
|       9 | 2328 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 | 2329 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2330 | `					return SXERR_ABORT;` |
|       - | 2331 | `				}` |
|       9 | 2332 | `				if( rc != SXERR_EMPTY ){` |
|       - | 2333 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - | 2334 | `					 * without the overhead of a function call.` |
|       - | 2335 | `					 * This is a very powerful optimization that improve` |
|       - | 2336 | `					 * performance greatly.` |
|       - | 2337 | `					 */` |
|       9 | 2338 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 | 2339 | `				}` |
|       4 | 2340 | `			}` |
|       - | 2341 | `			/* Jump trailing commas */` |
|       9 | 2342 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 | 2343 | `				pNext++;` |
|     ! 0 | 2344 | `			}` |
|       9 | 2345 | `			pGen->pIn = pNext;` |
|       1 | 2346 | `		}` |
|       - | 2347 | `		/* Restore token stream */` |
|       9 | 2348 | `		pGen->pEnd = pTmp;` |
|       5 | 2349 | `	}else{` |
|      66 | 2350 | `		sxi32 nArg = 0;` |
|      66 | 2351 | `		sxu32 nIdx = 0;` |
|      66 | 2352 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      66 | 2353 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2354 | `			return SXERR_ABORT;` |
|      66 | 2355 | `		}else if(rc != SXERR_EMPTY ){` |
|      66 | 2356 | `			nArg = 1;` |
|      32 | 2357 | `		}` |
|      66 | 2358 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - | 2359 | `			ph7_value *pObj;` |
|       - | 2360 | `			/* Emit the call instruction */` |
|      20 | 2361 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      20 | 2362 | `			if( pObj == 0 ){` |
|     ! 0 | 2363 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2364 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 2365 | `				return SXERR_ABORT;` |
|       - | 2366 | `			}` |
|      20 | 2367 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - | 2368 | `			/* Install in the literal table */` |
|      20 | 2369 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       9 | 2370 | `		}` |
|       - | 2371 | `		/* Emit the call instruction */` |
|      66 | 2372 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      66 | 2373 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,0,0);` |
|       - | 2374 | `	}` |
|       - | 2375 | `	/* Node successfully compiled */` |
|      74 | 2376 | `	return SXRET_OK;` |
|      38 | 2377 |  |
|       - | 2378 | `/*` |
|       - | 2379 | ` * Compile a node holding a variable declaration.` |
|       - | 2380 | ` * According to the PHP language reference` |
|       - | 2381 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - | 2382 | ` *  The variable name is case-sensitive.` |
|       - | 2383 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - | 2384 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 2385 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - | 2386 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - | 2387 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - | 2388 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - | 2389 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - | 2390 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - | 2391 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - | 2392 | ` *  the chapter on Expressions.` |
|       - | 2393 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - | 2394 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - | 2395 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - | 2396 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - | 2397 | ` *  is being assigned (the source variable).` |
|       - | 2398 | ` */` |
|  788270 | 2399 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 2400 |  |
|  788272 | 2401 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 2402 | `	sxi32 iVv;` |
|       - | 2403 | `	sxi32 iP1;` |
|       - | 2404 | `	void *p3;` |
|       - | 2405 | `	sxi32 rc;` |
|  788272 | 2406 | `	iVv = -1; /* Variable variable counter */` |
| 1576554 | 2407 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  788284 | 2408 | `		pGen->pIn++;` |
|  788284 | 2409 | `		iVv++;` |
|       2 | 2410 | `	}` |
|  788272 | 2411 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 2412 | `		/* Invalid variable name */` |
|     ! 0 | 2413 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 | 2414 | `		if( rc == SXERR_ABORT ){` |
|       - | 2415 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2416 | `			return SXERR_ABORT;` |
|       - | 2417 | `		}` |
|     ! 0 | 2418 | `		return SXRET_OK;` |
|       - | 2419 | `	}` |
|  788272 | 2420 | `	p3  = 0;` |
|  788272 | 2421 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - | 2422 | `		/* Dynamic variable creation */` |
|      18 | 2423 | `		pGen->pIn++;  /* Jump the open curly */` |
|      18 | 2424 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      18 | 2425 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 2426 | `			/* Empty expression */` |
|       3 | 2427 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 | 2428 | `			return SXRET_OK;` |
|       - | 2429 | `		}` |
|       - | 2430 | `		/* Compile the expression holding the variable name */` |
|      16 | 2431 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 | 2432 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2433 | `			return SXERR_ABORT;` |
|      16 | 2434 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 | 2435 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 | 2436 | `			return SXRET_OK;` |
|       - | 2437 | `		}` |
|       7 | 2438 | `	}else{` |
|       - | 2439 | `		SyHashEntry *pEntry;` |
|       - | 2440 | `		SyString *pName;` |
|  788256 | 2441 | `		char *zName = 0;` |
|       - | 2442 | `		/* Extract variable name */` |
|  788256 | 2443 | `		pName = &pGen->pIn->sData;` |
|       - | 2444 | `		/* Advance the stream cursor */` |
|  788256 | 2445 | `		pGen->pIn++;` |
|  788256 | 2446 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  788256 | 2447 | `		if( pEntry == 0 ){` |
|       - | 2448 | `			/* Duplicate name */` |
|  113268 | 2449 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  113268 | 2450 | `			if( zName == 0 ){` |
|     ! 0 | 2451 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2452 | `				return SXERR_ABORT;` |
|       - | 2453 | `			}` |
|       - | 2454 | `			/* Install in the hashtable */` |
|  113268 | 2455 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   56635 | 2456 | `		}else{` |
|       - | 2457 | `			/* Name already available */` |
|  674990 | 2458 | `			zName = (char *)pEntry->pUserData;` |
|       - | 2459 | `		}` |
|  788256 | 2460 | `		p3 = (void *)zName;` |
|       - | 2461 | `	}` |
|  788268 | 2462 | `	iP1 = 0;` |
|  788268 | 2463 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  302982 | 2464 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 2465 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  296768 | 2466 | `			iP1 = 1;` |
|  148383 | 2467 | `		}` |
|  151490 | 2468 | `	}` |
|       - | 2469 | `	/* Emit the load instruction */` |
|  788268 | 2470 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  788280 | 2471 | `	while( iVv > 0 ){` |
|      13 | 2472 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 | 2473 | `		iVv--;` |
|       1 | 2474 | `	}` |
|       - | 2475 | `	/* Node successfully compiled */` |
|  788268 | 2476 | `	return SXRET_OK;` |
|  394137 | 2477 |  |
|       - | 2478 | `/*` |
|       - | 2479 | ` * Load a literal.` |
|       - | 2480 | ` */` |
|  528330 | 2481 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 | 2482 |  |
|  528332 | 2483 | `	SyToken *pToken = pGen->pIn;` |
|       - | 2484 | `	ph7_value *pObj;` |
|       - | 2485 | `	SyString *pStr;` |
|       - | 2486 | `	sxu32 nIdx;` |
|       - | 2487 | `	/* Extract token value */` |
|  528332 | 2488 | `	pStr = &pToken->sData;` |
|       - | 2489 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  528332 | 2490 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   95984 | 2491 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 2492 | `			/* NULL constant are always indexed at 0 */` |
|   40824 | 2493 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   40824 | 2494 | `			return SXRET_OK;` |
|   55162 | 2495 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 2496 | `			/* TRUE constant are always indexed at 1 */` |
|     492 | 2497 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     492 | 2498 | `			return SXRET_OK;` |
|       2 | 2499 | `		}` |
|  501359 | 2500 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   83348 | 2501 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 2502 | `			/* FALSE constant are always indexed at 2 */` |
|   35614 | 2503 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   35614 | 2504 | `			return SXRET_OK;` |
|  433570 | 2505 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   73664 | 2506 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 2507 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    5396 | 2508 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    5396 | 2509 | `			if( pObj == 0 ){` |
|     ! 0 | 2510 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2511 | `				return SXERR_ABORT;` |
|       - | 2512 | `			}` |
|    5396 | 2513 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 2514 | `			/* Emit the load constant instruction */` |
|    5396 | 2515 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5396 | 2516 | `			return SXRET_OK;` |
|  404942 | 2517 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   27196 | 2518 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - | 2519 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 | 2520 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 | 2521 | `			if( pObj == 0 ){` |
|     ! 0 | 2522 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2523 | `				return SXERR_ABORT;` |
|       - | 2524 | `			}` |
|       7 | 2525 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - | 2526 | `				SyString sNs;` |
|       7 | 2527 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 | 2528 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 | 2529 | `			}else{` |
|     ! 0 | 2530 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - | 2531 | `			}` |
|       7 | 2532 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 | 2533 | `			return SXRET_OK;` |
|  404110 | 2534 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   11376 | 2535 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  398416 | 2536 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   14174 | 2537 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 | 2538 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - | 2539 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 | 2540 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - | 2541 | `				/* Point to the upper block */` |
|      11 | 2542 | `				pBlock = pBlock->pParent;` |
|       1 | 2543 | `			}` |
|      11 | 2544 | `			if( pBlock == 0 ){` |
|       - | 2545 | `				/* Called in the global scope,load NULL */` |
|       5 | 2546 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 | 2547 | `			}else{` |
|       - | 2548 | `				/* Extract the target function/method */` |
|       7 | 2549 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 | 2550 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - | 2551 | `					/* Not a class method,Load null */` |
|       3 | 2552 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 2553 | `				}else{` |
|       5 | 2554 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 | 2555 | `					if( pObj == 0 ){` |
|     ! 0 | 2556 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2557 | `						return SXERR_ABORT;` |
|       - | 2558 | `					}` |
|       5 | 2559 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - | 2560 | `					/* Emit the load constant instruction */` |
|       5 | 2561 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 2562 | `				}` |
|       - | 2563 | `			}` |
|      11 | 2564 | `			return SXRET_OK;` |
|       - | 2565 | `	}` |
|       - | 2566 | `	/* Query literal table */` |
|  445998 | 2567 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - | 2568 | `		ph7_value *pLitObj;` |
|       - | 2569 | `		/* Unknown literal,install it in the literal table */` |
|  208710 | 2570 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  208710 | 2571 | `		if( pLitObj == 0 ){` |
|     ! 0 | 2572 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 2573 | `			return SXERR_ABORT;` |
|       - | 2574 | `		}` |
|  208710 | 2575 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  208710 | 2576 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  104354 | 2577 | `	}` |
|       - | 2578 | `	/* Emit the load constant instruction */` |
|  445998 | 2579 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  445998 | 2580 | `	return SXRET_OK;` |
|  264167 | 2581 |  |
|       - | 2582 | `/*` |
|       - | 2583 | ` * Resolve a namespace path or simply load a literal.` |
|       - | 2584 | ` * If the token stream contains namespace separators (backslashes),` |
|       - | 2585 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - | 2586 | ` * Otherwise, load the simple literal directly.` |
|       - | 2587 | ` */` |
|  528354 | 2588 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 | 2589 |  |
|       - | 2590 | `	sxi32 rc;` |
|  528356 | 2591 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 2592 | `		return SXRET_OK;` |
|       - | 2593 | `	}` |
|       - | 2594 | `	/* Check if this is a multi-token namespace path */` |
|  528356 | 2595 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - | 2596 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      26 | 2597 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      26 | 2598 | `		int isAbsolute = 0;` |
|      26 | 2599 | `		SyBlobReset(pWorker);` |
|       - | 2600 | `		/* Check for leading backslash (absolute path) */` |
|      26 | 2601 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      24 | 2602 | `			isAbsolute = 1;` |
|      24 | 2603 | `			pGen->pIn++; /* Skip leading backslash */` |
|      11 | 2604 | `		}` |
|       - | 2605 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      26 | 2606 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 | 2607 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 | 2608 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 | 2609 | `		}` |
|       - | 2610 | `		/* Collect all path components */` |
|     102 | 2611 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     102 | 2612 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      40 | 2613 | `				SyBlobAppend(pWorker,"\\",1);` |
|      21 | 2614 | `			}else{` |
|      64 | 2615 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 2616 | `			}` |
|     102 | 2617 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      26 | 2618 | `				pGen->pIn++;` |
|      26 | 2619 | `				break;` |
|       - | 2620 | `			}` |
|      78 | 2621 | `			pGen->pIn++;` |
|       2 | 2622 | `		}` |
|      26 | 2623 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - | 2624 | `			ph7_value *pObj;` |
|       - | 2625 | `			SyString sPath;` |
|       - | 2626 | `			sxu32 nIdx;` |
|      26 | 2627 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - | 2628 | `			/* Install in the literal table */` |
|      26 | 2629 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      13 | 2630 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      13 | 2631 | `				if( pObj == 0 ){` |
|     ! 0 | 2632 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 2633 | `					return SXERR_ABORT;` |
|       - | 2634 | `				}` |
|      13 | 2635 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      13 | 2636 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       6 | 2637 | `			}` |
|       - | 2638 | `			/* Emit the load constant instruction.` |
|       - | 2639 | `			 * P1=1 means candidate for constant/function/class expansion. */` |
|      26 | 2640 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|      26 | 2641 | `			return SXRET_OK;` |
|       - | 2642 | `		}` |
|     ! 0 | 2643 | `	}` |
|       - | 2644 | `	/* Single-token literal: load directly */` |
|  528332 | 2645 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  528332 | 2646 | `	return rc;` |
|  264179 | 2647 |  |
|       - | 2648 | `/*` |
|       - | 2649 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 2650 | ` */` |
|  528354 | 2651 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 2652 |  |
|       - | 2653 | `	sxi32 rc;` |
|  528356 | 2654 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  528356 | 2655 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2656 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 2657 | `		return rc;` |
|       - | 2658 | `	}` |
|       - | 2659 | `	/* Node successfully compiled */` |
|  528356 | 2660 | `	return SXRET_OK;` |
|  264179 | 2661 |  |
|       - | 2662 | `/*` |
|       - | 2663 | ` * Recover from a compile-time error. In other words synchronize` |
|       - | 2664 | ` * the token stream cursor with the first semi-colon seen.` |
|       - | 2665 | ` */` |
|       8 | 2666 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 | 2667 |  |
|       - | 2668 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 | 2669 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 | 2670 | `		pGen->pIn++;` |
|       1 | 2671 | `	}` |
|       9 | 2672 | `	return SXRET_OK;` |
|       1 | 2673 |  |
|       - | 2674 | `/*` |
|       - | 2675 | ` * Check if the given identifier name is reserved or not.` |
|       - | 2676 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - | 2677 | ` */` |
|      56 | 2678 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       2 | 2679 |  |
|      58 | 2680 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      26 | 2681 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 | 2682 | `			return TRUE;` |
|      24 | 2683 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 | 2684 | `			return TRUE;` |
|       2 | 2685 | `		}` |
|      43 | 2686 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 | 2687 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 | 2688 | `			return TRUE;` |
|       - | 2689 | `		}` |
|     ! 0 | 2690 | `	}` |
|       - | 2691 | `	/* Not a reserved constant */` |
|      50 | 2692 | `	return FALSE;` |
|      30 | 2693 |  |
|       - | 2694 | `/*` |
|       - | 2695 | ` * Compile the 'const' statement.` |
|       - | 2696 | ` * According to the PHP language reference` |
|       - | 2697 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - | 2698 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - | 2699 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - | 2700 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - | 2701 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 2702 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - | 2703 | ` *  Syntax` |
|       - | 2704 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - | 2705 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - | 2706 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - | 2707 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - | 2708 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - | 2709 | ` *  to get a list of all defined constants.` |
|       - | 2710 | ` *` |
|       - | 2711 | ` * Symisc eXtension.` |
|       - | 2712 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - | 2713 | ` *  would allow only simple scalar value.` |
|       - | 2714 | ` *  Example` |
|       - | 2715 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 2716 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 2717 | ` */` |
|      32 | 2718 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       2 | 2719 |  |
|       - | 2720 | `	SySet *pConsCode,*pInstrContainer;` |
|      34 | 2721 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 2722 | `	SyString *pName;` |
|       - | 2723 | `	sxi32 rc;` |
|      34 | 2724 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      34 | 2725 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 2726 | `		/* Invalid constant name */` |
|       7 | 2727 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       7 | 2728 | `		if( rc == SXERR_ABORT ){` |
|       - | 2729 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2730 | `			return SXERR_ABORT;` |
|       - | 2731 | `		}` |
|       7 | 2732 | `		goto Synchronize;` |
|       - | 2733 | `	}` |
|       - | 2734 | `	/* Peek constant name */` |
|      28 | 2735 | `	pName = &pGen->pIn->sData;` |
|       - | 2736 | `	/* Make sure the constant name isn't reserved */` |
|      28 | 2737 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 2738 | `		/* Reserved constant */` |
|       9 | 2739 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 | 2740 | `		if( rc == SXERR_ABORT ){` |
|       - | 2741 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2742 | `			return SXERR_ABORT;` |
|       - | 2743 | `		}` |
|       9 | 2744 | `		goto Synchronize;` |
|       - | 2745 | `	}` |
|      20 | 2746 | `	pGen->pIn++;` |
|      20 | 2747 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 2748 | `		/* Invalid statement*/` |
|       5 | 2749 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       5 | 2750 | `		if( rc == SXERR_ABORT ){` |
|       - | 2751 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2752 | `			return SXERR_ABORT;` |
|       - | 2753 | `		}` |
|       5 | 2754 | `		goto Synchronize;` |
|       - | 2755 | `	}` |
|      15 | 2756 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - | 2757 | `	/* Allocate a new constant value container */` |
|      15 | 2758 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 | 2759 | `	if( pConsCode == 0 ){` |
|     ! 0 | 2760 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2761 | `		return SXERR_ABORT;` |
|       - | 2762 | `	}` |
|      15 | 2763 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 2764 | `	/* Swap bytecode container */` |
|      15 | 2765 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 | 2766 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - | 2767 | `	/* Compile constant value */` |
|      15 | 2768 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2769 | `	/* Emit the done instruction */` |
|      15 | 2770 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 | 2771 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 | 2772 | `	if( rc == SXERR_ABORT ){` |
|       - | 2773 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2774 | `		return SXERR_ABORT;` |
|       - | 2775 | `	}` |
|      15 | 2776 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - | 2777 | `	/* Register the constant with namespace-qualified name */` |
|       - | 2778 | `	{` |
|       - | 2779 | `		SyBlob sFQN;` |
|       - | 2780 | `		SyString sFQNStr;` |
|      15 | 2781 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 | 2782 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 | 2783 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 | 2784 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 | 2785 | `		SyBlobRelease(&sFQN);` |
|       - | 2786 | `	}` |
|      15 | 2787 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2788 | `		SySetRelease(pConsCode);` |
|     ! 0 | 2789 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 | 2790 | `	}` |
|      15 | 2791 | `	return SXRET_OK;` |
|       9 | 2792 | `Synchronize:` |
|       - | 2793 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      57 | 2794 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      39 | 2795 | `		pGen->pIn++;` |
|       1 | 2796 | `	}` |
|      19 | 2797 | `	return SXRET_OK;` |
|      18 | 2798 |  |
|       - | 2799 | `/*` |
|       - | 2800 | ` * Compile the 'continue' statement.` |
|       - | 2801 | ` * According to the PHP language reference` |
|       - | 2802 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - | 2803 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - | 2804 | ` *  iteration.` |
|       - | 2805 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - | 2806 | ` *  the purposes of continue.` |
|       - | 2807 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - | 2808 | ` *  of enclosing loops it should skip to the end of.` |
|       - | 2809 | ` *  Note:` |
|       - | 2810 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - | 2811 | ` */` |
|       - | 2812 | `/*` |
|       - | 2813 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - | 2814 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - | 2815 | ` * break/continue crosses a try boundary.` |
|       - | 2816 | ` *` |
|       - | 2817 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - | 2818 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - | 2819 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - | 2820 | ` */` |
|    2824 | 2821 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       2 | 2822 |  |
|    2826 | 2823 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   16490 | 2824 | `	while( pBlock && pBlock != pTarget ){` |
|   13666 | 2825 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 | 2826 | `			if( pBlock->pUserData ){` |
|       - | 2827 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 | 2828 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 | 2829 | `			}else{` |
|       - | 2830 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - | 2831 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - | 2832 | `				 * exception context from a sub-execution.` |
|       - | 2833 | `				 */` |
|     ! 0 | 2834 | `				break;` |
|       - | 2835 | `			}` |
|       1 | 2836 | `		}` |
|   13666 | 2837 | `		pBlock = pBlock->pParent;` |
|       2 | 2838 | `	}` |
|    2826 | 2839 |  |
|    2740 | 2840 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 | 2841 |  |
|       - | 2842 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 2843 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 2844 | `	sxu32 nLineLocal;` |
|       - | 2845 | `	sxi32 rc;` |
|    2742 | 2846 | `	nLineLocal = pGen->pIn->nLine;` |
|    2742 | 2847 | `	iLevel = 0;` |
|       - | 2848 | `	/* Jump the 'continue' keyword */` |
|    2742 | 2849 | `	pGen->pIn++;` |
|    2742 | 2850 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 2851 | `		/* optional numeric argument which tells us how many levels` |
|       - | 2852 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 2853 | `		 */` |
|       - | 2854 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      16 | 2855 | `		char *zAlloc = 0;` |
|       - | 2856 | `		SyString sNum;` |
|      16 | 2857 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      16 | 2858 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2859 | `			return SXERR_ABORT;` |
|       - | 2860 | `		}` |
|      16 | 2861 | `		if( rc == SXRET_OK ){` |
|      20 | 2862 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 | 2863 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 | 2864 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 2865 | `				return SXERR_ABORT;` |
|       - | 2866 | `			}` |
|      14 | 2867 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 | 2868 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 | 2869 | `		}` |
|      16 | 2870 | `		if( iLevel < 2 ){` |
|       3 | 2871 | `			iLevel = 0;` |
|       1 | 2872 | `		}` |
|      16 | 2873 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 | 2874 | `	}` |
|       - | 2875 | `	/* Point to the target loop */` |
|    2742 | 2876 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2742 | 2877 | `	if( pLoop == 0 ){` |
|       - | 2878 | `		/* Illegal continue */` |
|      11 | 2879 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 | 2880 | `		if( rc == SXERR_ABORT ){` |
|       - | 2881 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2882 | `			return SXERR_ABORT;` |
|       - | 2883 | `		}` |
|       6 | 2884 | `	}else{` |
|    2732 | 2885 | `		sxu32 nInstrIdx = 0;` |
|       - | 2886 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    2732 | 2887 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    2732 | 2888 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - | 2889 | `			/* According to the PHP language reference manual` |
|       - | 2890 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - | 2891 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - | 2892 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - | 2893 | `			 */` |
|       5 | 2894 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 | 2895 | `			if( rc == SXRET_OK ){` |
|       5 | 2896 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 | 2897 | `			}` |
|       3 | 2898 | `		}else{` |
|       - | 2899 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    2728 | 2900 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2728 | 2901 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - | 2902 | `				JumpFixup sJumpFix;` |
|       - | 2903 | `				/* Post-continue */` |
|      14 | 2904 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 | 2905 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 | 2906 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 | 2907 | `			}` |
|       - | 2908 | `		}` |
|       - | 2909 | `	}` |
|    2742 | 2910 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2911 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 2912 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 | 2913 | `	}` |
|       - | 2914 | `	/* Statement successfully compiled */` |
|    2742 | 2915 | `	return SXRET_OK;` |
|    1372 | 2916 |  |
|       - | 2917 | `/*` |
|       - | 2918 | ` * Compile the 'break' statement.` |
|       - | 2919 | ` * According to the PHP language reference` |
|       - | 2920 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - | 2921 | ` *  structure.` |
|       - | 2922 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - | 2923 | ` *  enclosing structures are to be broken out of.` |
|       - | 2924 | ` */` |
|     110 | 2925 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       2 | 2926 |  |
|       - | 2927 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 2928 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 2929 | `	sxi32 rc;` |
|     112 | 2930 | `	iLevel = 0;` |
|       - | 2931 | `	/* Jump the 'break' keyword */` |
|     112 | 2932 | `	pGen->pIn++;` |
|     112 | 2933 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 2934 | `		/* optional numeric argument which tells us how many levels` |
|       - | 2935 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 2936 | `		 */` |
|       - | 2937 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      16 | 2938 | `		char *zAlloc = 0;` |
|       - | 2939 | `		SyString sNum;` |
|      16 | 2940 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      16 | 2941 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2942 | `			return SXERR_ABORT;` |
|       - | 2943 | `		}` |
|      16 | 2944 | `		if( rc == SXRET_OK ){` |
|      20 | 2945 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 | 2946 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 | 2947 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 2948 | `				return SXERR_ABORT;` |
|       - | 2949 | `			}` |
|      14 | 2950 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 | 2951 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 | 2952 | `		}` |
|      16 | 2953 | `		if( iLevel < 2 ){` |
|       3 | 2954 | `			iLevel = 0;` |
|       1 | 2955 | `		}` |
|      16 | 2956 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 | 2957 | `	}` |
|       - | 2958 | `	/* Extract the target loop */` |
|     112 | 2959 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     112 | 2960 | `	if( pLoop == 0 ){` |
|       - | 2961 | `		/* Illegal break */` |
|      17 | 2962 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      17 | 2963 | `		if( rc == SXERR_ABORT ){` |
|       - | 2964 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2965 | `			return SXERR_ABORT;` |
|       - | 2966 | `		}` |
|       9 | 2967 | `	}else{` |
|       - | 2968 | `		sxu32 nInstrIdx;` |
|       - | 2969 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|      96 | 2970 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|      96 | 2971 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|      96 | 2972 | `		if( rc == SXRET_OK ){` |
|       - | 2973 | `			/* Fix the jump later when the jump destination is resolved */` |
|      96 | 2974 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      47 | 2975 | `		}` |
|       - | 2976 | `	}` |
|     112 | 2977 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2978 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 2979 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 | 2980 | `	}` |
|       - | 2981 | `	/* Statement successfully compiled */` |
|     112 | 2982 | `	return SXRET_OK;` |
|      57 | 2983 |  |
|       - | 2984 | `/*` |
|       - | 2985 | ` * Compile or record a label.` |
|       - | 2986 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - | 2987 | ` * Example` |
|       - | 2988 | ` *  goto LABEL;` |
|       - | 2989 | ` *   echo 'Foo';` |
|       - | 2990 | ` *  LABEL:` |
|       - | 2991 | ` *   echo 'Bar';` |
|       - | 2992 | ` */` |
|     112 | 2993 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       2 | 2994 |  |
|       - | 2995 | `	GenBlock *pBlock;` |
|       - | 2996 | `	Label sLabel;` |
|       - | 2997 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     114 | 2998 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     114 | 2999 | `	if( pBlock ){` |
|       - | 3000 | `		sxi32 rc;` |
|       7 | 3001 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 | 3002 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       5 | 3003 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3004 | `			return SXERR_ABORT;` |
|       - | 3005 | `		}` |
|       3 | 3006 | `	}else{` |
|     110 | 3007 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 3008 | `		char *zDup;` |
|       - | 3009 | `		/* Initialize label fields */` |
|     110 | 3010 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3011 | `		/* Duplicate label name */` |
|     110 | 3012 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     110 | 3013 | `		if( zDup == 0 ){` |
|     ! 0 | 3014 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3015 | `			return SXERR_ABORT;` |
|       - | 3016 | `		}` |
|     110 | 3017 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     110 | 3018 | `		sLabel.bRef  = FALSE;` |
|     110 | 3019 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     110 | 3020 | `		pBlock = pGen->pCurrent;` |
|     218 | 3021 | `		while( pBlock ){` |
|     130 | 3022 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      22 | 3023 | `				break;` |
|       - | 3024 | `			}` |
|       - | 3025 | `			/* Point to the upper block */` |
|     110 | 3026 | `			pBlock = pBlock->pParent;` |
|       2 | 3027 | `		}` |
|     110 | 3028 | `		if( pBlock ){` |
|      22 | 3029 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      12 | 3030 | `		}else{` |
|      90 | 3031 | `			sLabel.pFunc = 0;` |
|       - | 3032 | `		}` |
|       - | 3033 | `		/* Insert in label set */` |
|     110 | 3034 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - | 3035 | `	}` |
|     114 | 3036 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     114 | 3037 | `	return SXRET_OK;` |
|      58 | 3038 |  |
|       - | 3039 | `/*` |
|       - | 3040 | ` * Compile the so hated 'goto' statement.` |
|       - | 3041 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - | 3042 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - | 3043 | ` * a compiler it has to do this.` |
|       - | 3044 | ` * According to the PHP language reference manual` |
|       - | 3045 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - | 3046 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - | 3047 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - | 3048 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - | 3049 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - | 3050 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - | 3051 | ` *   of a multi-level break` |
|       - | 3052 | ` */` |
|     152 | 3053 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       2 | 3054 |  |
|       - | 3055 | `	JumpFixup sJump;` |
|       - | 3056 | `	sxi32 rc;` |
|     154 | 3057 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     154 | 3058 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3059 | `		/* Missing label */` |
|     ! 0 | 3060 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 | 3061 | `		if( rc == SXERR_ABORT ){` |
|       - | 3062 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3063 | `			return SXERR_ABORT;` |
|       - | 3064 | `		}` |
|     ! 0 | 3065 | `		return SXRET_OK;` |
|       - | 3066 | `	}` |
|     154 | 3067 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 | 3068 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 | 3069 | `		if( rc == SXERR_ABORT ){` |
|       - | 3070 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3071 | `			return SXERR_ABORT;` |
|       - | 3072 | `		}` |
|       3 | 3073 | `	}else{` |
|     150 | 3074 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 3075 | `		GenBlock *pBlock;` |
|       - | 3076 | `		char *zDup;` |
|       - | 3077 | `		/* Prepare the jump destination */` |
|     150 | 3078 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     150 | 3079 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - | 3080 | `		/* Duplicate label name */` |
|     150 | 3081 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     150 | 3082 | `		if( zDup == 0 ){` |
|     ! 0 | 3083 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3084 | `			return SXERR_ABORT;` |
|       - | 3085 | `		}` |
|     150 | 3086 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     150 | 3087 | `		pBlock = pGen->pCurrent;` |
|     312 | 3088 | `		while( pBlock ){` |
|     196 | 3089 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      34 | 3090 | `				break;` |
|       - | 3091 | `			}` |
|       - | 3092 | `			/* Point to the upper block */` |
|     164 | 3093 | `			pBlock = pBlock->pParent;` |
|       2 | 3094 | `		}` |
|     150 | 3095 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       7 | 3096 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       7 | 3097 | `			if( rc == SXERR_ABORT ){` |
|       - | 3098 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3099 | `				return SXERR_ABORT;` |
|       - | 3100 | `			}` |
|       3 | 3101 | `		}` |
|     150 | 3102 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      28 | 3103 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      15 | 3104 | `		}else{` |
|     124 | 3105 | `			sJump.pFunc = 0;` |
|       - | 3106 | `		}` |
|       - | 3107 | `		/* Emit the unconditional jump */` |
|     150 | 3108 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     150 | 3109 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 | 3110 | `		}` |
|       - | 3111 | `	}` |
|     154 | 3112 | `	pGen->pIn++; /* Jump the label name */` |
|     154 | 3113 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 | 3114 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 | 3115 | `	}` |
|       - | 3116 | `	/* Statement successfully compiled */` |
|     154 | 3117 | `	return SXRET_OK;` |
|      78 | 3118 |  |
|       - | 3119 | `/*` |
|       - | 3120 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - | 3121 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - | 3122 | ` * failure.` |
|       - | 3123 | ` */` |
|      20 | 3124 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 | 3125 |  |
|       - | 3126 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - | 3127 | `	sxu32 nRawObj;` |
|      10 | 3128 | `	sxu32 nObjIdx;` |
|       - | 3129 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - | 3130 | `	 * a PHP block.` |
|       - | 3131 | `	 */` |
|      10 | 3132 | `Consume:` |
|      21 | 3133 | `	nRawObj = nObjIdx = 0;` |
|      21 | 3134 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 | 3135 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 | 3136 | `		if( pRawObj == 0 ){` |
|     ! 0 | 3137 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3138 | `			return SXERR_ABORT;` |
|       - | 3139 | `		}` |
|       - | 3140 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 | 3141 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 | 3142 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 | 3143 | `		++nRawObj;` |
|     ! 0 | 3144 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 | 3145 | `	}` |
|      21 | 3146 | `	if( nRawObj > 0 ){` |
|       - | 3147 | `		/* Emit the consume instruction */` |
|     ! 0 | 3148 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 | 3149 | `	}` |
|      21 | 3150 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 | 3151 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - | 3152 | `		/* Reset the token set */` |
|     ! 0 | 3153 | `		SySetReset(pTokenSet);` |
|       - | 3154 | `		/* Tokenize input */` |
|     ! 0 | 3155 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 | 3156 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - | 3157 | `		/* Point to the fresh token stream */` |
|     ! 0 | 3158 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 | 3159 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - | 3160 | `		/* Advance the stream cursor */` |
|     ! 0 | 3161 | `		pGen->pRawIn++;` |
|       - | 3162 | `		/* TICKET 1433-011 */` |
|     ! 0 | 3163 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 3164 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 3165 | `			sxi32 rc;` |
|       - | 3166 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 | 3167 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 | 3168 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 | 3169 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 | 3170 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 3171 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3172 | `				return SXERR_ABORT;` |
|     ! 0 | 3173 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 | 3174 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 3175 | `			}` |
|     ! 0 | 3176 | `			goto Consume;` |
|       - | 3177 | `		}` |
|     ! 0 | 3178 | `	}else{` |
|       - | 3179 | `		/* No more chunks to process */` |
|      21 | 3180 | `		pGen->pIn = pGen->pEnd;` |
|      21 | 3181 | `		return SXERR_EOF;` |
|       - | 3182 | `	}` |
|     ! 0 | 3183 | `	return SXRET_OK;` |
|      11 | 3184 |  |
|       - | 3185 | `/*` |
|       - | 3186 | ` * Compile a PHP block.` |
|       - | 3187 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - | 3188 | ` * optionally delimited by braces {}.` |
|       - | 3189 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 3190 | ` * and this function takes care of generating the appropriate error` |
|       - | 3191 | ` * message.` |
|       - | 3192 | ` */` |
|  297050 | 3193 | `static sxi32 PH7_CompileBlock(` |
|       - | 3194 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 3195 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - | 3196 | `	)` |
|       2 | 3197 |  |
|       - | 3198 | `	sxi32 rc;` |
|       - | 3199 | `	sxu32 nLine;` |
|  297052 | 3200 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  295642 | 3201 | `		nLine = pGen->pIn->nLine;` |
|  295642 | 3202 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  295642 | 3203 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3204 | `			return SXERR_ABORT;` |
|       - | 3205 | `		}` |
|  295642 | 3206 | `		pGen->pIn++;` |
|       - | 3207 | `		/* Compile until we hit the closing braces '}' */` |
|  408141 | 3208 | `		for(;;){` |
|  816284 | 3209 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 | 3210 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 | 3211 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 3212 | `			 	   return SXERR_ABORT;` |
|       - | 3213 | `				}` |
|      21 | 3214 | `				if( rc == SXERR_EOF ){` |
|       - | 3215 | `					/* No more token to process. Missing closing braces */` |
|      21 | 3216 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 | 3217 | `					break;` |
|       - | 3218 | `				}` |
|     ! 0 | 3219 | `			}` |
|  816264 | 3220 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - | 3221 | `				/* Closing braces found,break immediately*/` |
|  295622 | 3222 | `				pGen->pIn++;` |
|  295622 | 3223 | `				break;` |
|       - | 3224 | `			}` |
|       - | 3225 | `			/* Compile a single statement */` |
|  520644 | 3226 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  520644 | 3227 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3228 | `				return SXERR_ABORT;` |
|       - | 3229 | `			}` |
|       2 | 3230 | `		}` |
|  295642 | 3231 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  149232 | 3232 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 | 3233 | `		pGen->pIn++;` |
|     ! 0 | 3234 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 | 3235 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3236 | `			return SXERR_ABORT;` |
|       - | 3237 | `		}` |
|       - | 3238 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 | 3239 | `		for(;;){` |
|     ! 0 | 3240 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3241 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 | 3242 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 3243 | `			 	   return SXERR_ABORT;` |
|       - | 3244 | `				}` |
|     ! 0 | 3245 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - | 3246 | `					/* No more token to process */` |
|     ! 0 | 3247 | `					if( rc == SXERR_EOF ){` |
|     ! 0 | 3248 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - | 3249 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 | 3250 | `					}` |
|     ! 0 | 3251 | `					break;` |
|       - | 3252 | `				}` |
|     ! 0 | 3253 | `			}` |
|     ! 0 | 3254 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 3255 | `				sxi32 nKwrd;` |
|       - | 3256 | `				/* Keyword found */` |
|     ! 0 | 3257 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 | 3258 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 | 3259 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - | 3260 | `						/* Delimiter keyword found,break */` |
|     ! 0 | 3261 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 | 3262 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 | 3263 | `						}` |
|     ! 0 | 3264 | `						break;` |
|       - | 3265 | `				}` |
|     ! 0 | 3266 | `			}` |
|       - | 3267 | `			/* Compile a single statement */` |
|     ! 0 | 3268 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 | 3269 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3270 | `				return SXERR_ABORT;` |
|       - | 3271 | `			}` |
|     ! 0 | 3272 | `		}` |
|     ! 0 | 3273 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 | 3274 | `	}else{` |
|       - | 3275 | `		/* Compile a single statement */` |
|    1412 | 3276 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1412 | 3277 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3278 | `			return SXERR_ABORT;` |
|       - | 3279 | `		}` |
|       - | 3280 | `	}` |
|       - | 3281 | `	/* Jump trailing semi-colons ';' */` |
|  297052 | 3282 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 3283 | `		pGen->pIn++;` |
|     ! 0 | 3284 | `	}` |
|  297052 | 3285 | `	return SXRET_OK;` |
|  148527 | 3286 |  |
|       - | 3287 | `/*` |
|       - | 3288 | ` * Compile the gentle 'while' statement.` |
|       - | 3289 | ` * According to the PHP language reference` |
|       - | 3290 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - | 3291 | ` *  The basic form of a while statement is:` |
|       - | 3292 | ` *  while (expr)` |
|       - | 3293 | ` *   statement` |
|       - | 3294 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - | 3295 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - | 3296 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - | 3297 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - | 3298 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - | 3299 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - | 3300 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - | 3301 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - | 3302 | ` *  while (expr):` |
|       - | 3303 | ` *    statement` |
|       - | 3304 | ` *   endwhile;` |
|       - | 3305 | ` */` |
|   10886 | 3306 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 | 3307 |  |
|   10888 | 3308 | `	GenBlock *pWhileBlock = 0;` |
|   10888 | 3309 | `	SyToken *pTmp,*pEnd = 0;` |
|       - | 3310 | `	sxu32 nFalseJump;` |
|       - | 3311 | `	sxu32 nLine;` |
|       - | 3312 | `	sxi32 rc;` |
|   10888 | 3313 | `	nLine = pGen->pIn->nLine;` |
|       - | 3314 | `	/* Jump the 'while' keyword */` |
|   10888 | 3315 | `	pGen->pIn++;` |
|   10888 | 3316 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 3317 | `		/* Syntax error */` |
|     ! 0 | 3318 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 3319 | `		if( rc == SXERR_ABORT ){` |
|       - | 3320 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3321 | `			return SXERR_ABORT;` |
|       - | 3322 | `		}` |
|     ! 0 | 3323 | `		goto Synchronize;` |
|       - | 3324 | `	}` |
|       - | 3325 | `	/* Jump the left parenthesis '(' */` |
|   10888 | 3326 | `	pGen->pIn++;` |
|       - | 3327 | `	/* Create the loop block */` |
|   10888 | 3328 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   10888 | 3329 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3330 | `		return SXERR_ABORT;` |
|       - | 3331 | `	}` |
|       - | 3332 | `	/* Delimit the condition */` |
|   10888 | 3333 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   10888 | 3334 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 3335 | `		/* Empty expression */` |
|       3 | 3336 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 | 3337 | `		if( rc == SXERR_ABORT ){` |
|       - | 3338 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3339 | `			return SXERR_ABORT;` |
|       - | 3340 | `		}` |
|       1 | 3341 | `	}` |
|       - | 3342 | `	/* Swap token streams */` |
|   10888 | 3343 | `	pTmp = pGen->pEnd;` |
|   10888 | 3344 | `	pGen->pEnd = pEnd;` |
|       - | 3345 | `	/* Compile the expression */` |
|   10888 | 3346 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10888 | 3347 | `	if( rc == SXERR_ABORT ){` |
|       - | 3348 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3349 | `		return SXERR_ABORT;` |
|       - | 3350 | `	}` |
|       - | 3351 | `	/* Update token stream */` |
|   10888 | 3352 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 3353 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3354 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3355 | `			return SXERR_ABORT;` |
|       - | 3356 | `		}` |
|     ! 0 | 3357 | `		pGen->pIn++;` |
|     ! 0 | 3358 | `	}` |
|       - | 3359 | `	/* Synchronize pointers */` |
|   10888 | 3360 | `	pGen->pIn  = &pEnd[1];` |
|   10888 | 3361 | `	pGen->pEnd = pTmp;` |
|       - | 3362 | `	/* Emit the false jump */` |
|   10888 | 3363 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 3364 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   10888 | 3365 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - | 3366 | `	/* Compile the loop body */` |
|   10888 | 3367 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   10888 | 3368 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3369 | `		return SXERR_ABORT;` |
|       - | 3370 | `	}` |
|       - | 3371 | `	/* Emit the unconditional jump to the start of the loop */` |
|   10888 | 3372 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - | 3373 | `	/* Fix all jumps now the destination is resolved */` |
|   10888 | 3374 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3375 | `	/* Release the loop block */` |
|   10888 | 3376 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3377 | `	/* Statement successfully compiled */` |
|   10888 | 3378 | `	return SXRET_OK;` |
|     ! 0 | 3379 | `Synchronize:` |
|       - | 3380 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 3381 | `	 * compiling this erroneous block.` |
|       - | 3382 | `	 */` |
|     ! 0 | 3383 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3384 | `		pGen->pIn++;` |
|     ! 0 | 3385 | `	}` |
|     ! 0 | 3386 | `	return SXRET_OK;` |
|    5445 | 3387 |  |
|       - | 3388 | `/*` |
|       - | 3389 | ` * Compile the ugly do..while() statement.` |
|       - | 3390 | ` * According to the PHP language reference` |
|       - | 3391 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - | 3392 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - | 3393 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - | 3394 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - | 3395 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - | 3396 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - | 3397 | ` *  would end immediately).` |
|       - | 3398 | ` *  There is just one syntax for do-while loops:` |
|       - | 3399 | ` *  <?php` |
|       - | 3400 | ` *  $i = 0;` |
|       - | 3401 | ` *  do {` |
|       - | 3402 | ` *   echo $i;` |
|       - | 3403 | ` *  } while ($i > 0);` |
|       - | 3404 | ` * ?>` |
|       - | 3405 | ` */` |
|       2 | 3406 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 | 3407 |  |
|       3 | 3408 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 | 3409 | `	GenBlock *pDoBlock = 0;` |
|       - | 3410 | `	sxu32 nLine;` |
|       - | 3411 | `	sxi32 rc;` |
|       3 | 3412 | `	nLine = pGen->pIn->nLine;` |
|       - | 3413 | `	/* Jump the 'do' keyword */` |
|       3 | 3414 | `	pGen->pIn++;` |
|       - | 3415 | `	/* Create the loop block */` |
|       3 | 3416 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 | 3417 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3418 | `		return SXERR_ABORT;` |
|       - | 3419 | `	}` |
|       - | 3420 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 | 3421 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 | 3422 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 | 3423 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3424 | `		return SXERR_ABORT;` |
|       - | 3425 | `	}` |
|       3 | 3426 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 3427 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 | 3428 | `	}` |
|       3 | 3429 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 | 3430 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - | 3431 | `			/* Missing 'while' statement */` |
|       3 | 3432 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 | 3433 | `			if( rc == SXERR_ABORT ){` |
|       - | 3434 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3435 | `				return SXERR_ABORT;` |
|       - | 3436 | `			}` |
|       3 | 3437 | `			goto Synchronize;` |
|       - | 3438 | `	}` |
|       - | 3439 | `	/* Jump the 'while' keyword */` |
|     ! 0 | 3440 | `	pGen->pIn++;` |
|     ! 0 | 3441 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 3442 | `		/* Syntax error */` |
|     ! 0 | 3443 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 3444 | `		if( rc == SXERR_ABORT ){` |
|       - | 3445 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3446 | `			return SXERR_ABORT;` |
|       - | 3447 | `		}` |
|     ! 0 | 3448 | `		goto Synchronize;` |
|       - | 3449 | `	}` |
|       - | 3450 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 | 3451 | `	pGen->pIn++;` |
|       - | 3452 | `	/* Delimit the condition */` |
|     ! 0 | 3453 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 | 3454 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 3455 | `		/* Empty expression */` |
|     ! 0 | 3456 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 | 3457 | `		if( rc == SXERR_ABORT ){` |
|       - | 3458 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3459 | `			return SXERR_ABORT;` |
|       - | 3460 | `		}` |
|     ! 0 | 3461 | `		goto Synchronize;` |
|       - | 3462 | `	}` |
|       - | 3463 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 | 3464 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - | 3465 | `		JumpFixup *aPost;` |
|       - | 3466 | `		VmInstr *pInstr;` |
|       - | 3467 | `		sxu32 nJumpDest;` |
|       - | 3468 | `		sxu32 n;` |
|     ! 0 | 3469 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 | 3470 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 | 3471 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 | 3472 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 | 3473 | `			if( pInstr ){` |
|       - | 3474 | `				/* Fix */` |
|     ! 0 | 3475 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 | 3476 | `			}` |
|     ! 0 | 3477 | `		}` |
|     ! 0 | 3478 | `	}` |
|       - | 3479 | `	/* Swap token streams */` |
|     ! 0 | 3480 | `	pTmp = pGen->pEnd;` |
|     ! 0 | 3481 | `	pGen->pEnd = pEnd;` |
|       - | 3482 | `	/* Compile the expression */` |
|     ! 0 | 3483 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 3484 | `	if( rc == SXERR_ABORT ){` |
|       - | 3485 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3486 | `		return SXERR_ABORT;` |
|       - | 3487 | `	}` |
|       - | 3488 | `	/* Update token stream */` |
|     ! 0 | 3489 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 3490 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3491 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3492 | `			return SXERR_ABORT;` |
|       - | 3493 | `		}` |
|     ! 0 | 3494 | `		pGen->pIn++;` |
|     ! 0 | 3495 | `	}` |
|     ! 0 | 3496 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 | 3497 | `	pGen->pEnd = pTmp;` |
|       - | 3498 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 | 3499 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - | 3500 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 | 3501 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3502 | `	/* Release the loop block */` |
|     ! 0 | 3503 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3504 | `	/* Statement successfully compiled */` |
|     ! 0 | 3505 | `	return SXRET_OK;` |
|       1 | 3506 | `Synchronize:` |
|       - | 3507 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 3508 | `	 * compiling this erroneous block.` |
|       - | 3509 | `	 */` |
|       3 | 3510 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3511 | `		pGen->pIn++;` |
|     ! 0 | 3512 | `	}` |
|       3 | 3513 | `	return SXRET_OK;` |
|       2 | 3514 |  |
|       - | 3515 | `/*` |
|       - | 3516 | ` * Compile the complex and powerful 'for' statement.` |
|       - | 3517 | ` * According to the PHP language reference` |
|       - | 3518 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - | 3519 | ` *  The syntax of a for loop is:` |
|       - | 3520 | ` *  for (expr1; expr2; expr3)` |
|       - | 3521 | ` *   statement` |
|       - | 3522 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - | 3523 | ` *  the beginning of the loop.` |
|       - | 3524 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - | 3525 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - | 3526 | ` *  to FALSE, the execution of the loop ends.` |
|       - | 3527 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - | 3528 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - | 3529 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - | 3530 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - | 3531 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - | 3532 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - | 3533 | ` *  of using the for truth expression.` |
|       - | 3534 | ` */` |
|   10886 | 3535 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 | 3536 |  |
|   10888 | 3537 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   10888 | 3538 | `	GenBlock *pForBlock = 0;` |
|       - | 3539 | `	sxu32 nFalseJump;` |
|       - | 3540 | `	sxu32 nLine;` |
|       - | 3541 | `	sxi32 rc;` |
|   10888 | 3542 | `	nLine = pGen->pIn->nLine;` |
|       - | 3543 | `	/* Jump the 'for' keyword */` |
|   10888 | 3544 | `	pGen->pIn++;` |
|   10888 | 3545 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 3546 | `		/* Syntax error */` |
|     ! 0 | 3547 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 3548 | `		if( rc == SXERR_ABORT ){` |
|       - | 3549 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3550 | `			return SXERR_ABORT;` |
|       - | 3551 | `		}` |
|     ! 0 | 3552 | `		return SXRET_OK;` |
|       - | 3553 | `	}` |
|       - | 3554 | `	/* Jump the left parenthesis '(' */` |
|   10888 | 3555 | `	pGen->pIn++;` |
|       - | 3556 | `	/* Delimit the init-expr;condition;post-expr */` |
|   10888 | 3557 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   10888 | 3558 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 3559 | `		/* Empty expression */` |
|     ! 0 | 3560 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 | 3561 | `		if( rc == SXERR_ABORT ){` |
|       - | 3562 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3563 | `			return SXERR_ABORT;` |
|       - | 3564 | `		}` |
|       - | 3565 | `		/* Synchronize */` |
|     ! 0 | 3566 | `		pGen->pIn = pEnd;` |
|     ! 0 | 3567 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 3568 | `			pGen->pIn++;` |
|     ! 0 | 3569 | `		}` |
|     ! 0 | 3570 | `		return SXRET_OK;` |
|       - | 3571 | `	}` |
|       - | 3572 | `	/* Swap token streams */` |
|   10888 | 3573 | `	pTmp = pGen->pEnd;` |
|   10888 | 3574 | `	pGen->pEnd = pEnd;` |
|       - | 3575 | `	/* Compile initialization expressions if available */` |
|   10888 | 3576 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3577 | `	/* Pop operand lvalues */` |
|   10888 | 3578 | `	if( rc == SXERR_ABORT ){` |
|       - | 3579 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3580 | `		return SXERR_ABORT;` |
|   10888 | 3581 | `	}else if( rc != SXERR_EMPTY ){` |
|   10886 | 3582 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5442 | 3583 | `	}` |
|   10888 | 3584 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3585 | `		/* Syntax error */` |
|     ! 0 | 3586 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 3587 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 | 3588 | `		if( rc == SXERR_ABORT ){` |
|       - | 3589 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3590 | `			return SXERR_ABORT;` |
|       - | 3591 | `		}` |
|     ! 0 | 3592 | `		return SXRET_OK;` |
|       - | 3593 | `	}` |
|       - | 3594 | `	/* Jump the trailing ';' */` |
|   10888 | 3595 | `	pGen->pIn++;` |
|       - | 3596 | `	/* Create the loop block */` |
|   10888 | 3597 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   10888 | 3598 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3599 | `		return SXERR_ABORT;` |
|       - | 3600 | `	}` |
|       - | 3601 | `	/* Deffer continue jumps */` |
|   10888 | 3602 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 3603 | `	/* Compile the condition */` |
|   10888 | 3604 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10888 | 3605 | `	if( rc == SXERR_ABORT ){` |
|       - | 3606 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3607 | `		return SXERR_ABORT;` |
|   10888 | 3608 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 3609 | `		/* Emit the false jump */` |
|   10886 | 3610 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 3611 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   10886 | 3612 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    5442 | 3613 | `	}` |
|   10888 | 3614 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3615 | `		/* Syntax error */` |
|       5 | 3616 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 3617 | `			"for: Expected ';' after conditionals expressions");` |
|       5 | 3618 | `		if( rc == SXERR_ABORT ){` |
|       - | 3619 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3620 | `			return SXERR_ABORT;` |
|       - | 3621 | `		}` |
|       5 | 3622 | `		return SXRET_OK;` |
|       - | 3623 | `	}` |
|       - | 3624 | `	/* Jump the trailing ';' */` |
|   10884 | 3625 | `	pGen->pIn++;` |
|       - | 3626 | `	/* Save the post condition stream */` |
|   10884 | 3627 | `	pPostStart = pGen->pIn;` |
|       - | 3628 | `	/* Compile the loop body */` |
|   10884 | 3629 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   10884 | 3630 | `	pGen->pEnd = pTmp;` |
|   10884 | 3631 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   10884 | 3632 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3633 | `		return SXERR_ABORT;` |
|       - | 3634 | `	}` |
|       - | 3635 | `	/* Fix post-continue jumps */` |
|   10884 | 3636 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - | 3637 | `		JumpFixup *aPost;` |
|       - | 3638 | `		VmInstr *pInstr;` |
|       - | 3639 | `		sxu32 nJumpDest;` |
|       - | 3640 | `		sxu32 n;` |
|      14 | 3641 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 | 3642 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 | 3643 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 | 3644 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 | 3645 | `			if( pInstr ){` |
|       - | 3646 | `				/* Fix jump */` |
|      14 | 3647 | `				pInstr->iP2 = nJumpDest;` |
|       6 | 3648 | `			}` |
|       8 | 3649 | `		}` |
|       6 | 3650 | `	}` |
|       - | 3651 | `	/* compile the post-expressions if available */` |
|   10884 | 3652 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 3653 | `		pPostStart++;` |
|     ! 0 | 3654 | `	}` |
|   10884 | 3655 | `	if( pPostStart < pEnd ){` |
|       - | 3656 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   10884 | 3657 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   10884 | 3658 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10884 | 3659 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 3660 | `			/* Syntax error */` |
|     ! 0 | 3661 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 | 3662 | `			if( rc == SXERR_ABORT ){` |
|       - | 3663 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3664 | `				return SXERR_ABORT;` |
|       - | 3665 | `			}` |
|     ! 0 | 3666 | `			return SXRET_OK;` |
|       - | 3667 | `		}` |
|   10884 | 3668 | `		RE_SWAP_DELIMITER(pGen);` |
|   10884 | 3669 | `		if( rc == SXERR_ABORT ){` |
|       - | 3670 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3671 | `			return SXERR_ABORT;` |
|   10884 | 3672 | `		}else if( rc != SXERR_EMPTY){` |
|       - | 3673 | `			/* Pop operand lvalue */` |
|   10884 | 3674 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5441 | 3675 | `		}` |
|    5441 | 3676 | `	}` |
|       - | 3677 | `	/* Emit the unconditional jump to the start of the loop */` |
|   10884 | 3678 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 3679 | `	/* Fix all jumps now the destination is resolved */` |
|   10884 | 3680 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3681 | `	/* Release the loop block */` |
|   10884 | 3682 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3683 | `	/* Statement successfully compiled */` |
|   10884 | 3684 | `	return SXRET_OK;` |
|    5445 | 3685 |  |
|       - | 3686 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 3687 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - | 3688 | ` * are allowed.` |
|       - | 3689 | ` */` |
|    5792 | 3690 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 3691 |  |
|    5794 | 3692 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    5794 | 3693 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 3694 | `		/* Unexpected expression */` |
|     ! 0 | 3695 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 3696 | `			"foreach: Expecting a variable name");` |
|     ! 0 | 3697 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 3698 | `			rc = SXERR_INVALID;` |
|     ! 0 | 3699 | `		}` |
|     ! 0 | 3700 | `	}` |
|    5794 | 3701 | `	return rc;` |
|       2 | 3702 |  |
|       - | 3703 | `/*` |
|       - | 3704 | ` * Compile the 'foreach' statement.` |
|       - | 3705 | ` * According to the PHP language reference` |
|       - | 3706 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - | 3707 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - | 3708 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - | 3709 | ` *  is a minor but useful extension of the first:` |
|       - | 3710 | ` *  foreach (array_expression as $value)` |
|       - | 3711 | ` *    statement` |
|       - | 3712 | ` *  foreach (array_expression as $key => $value)` |
|       - | 3713 | ` *   statement` |
|       - | 3714 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - | 3715 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - | 3716 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - | 3717 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - | 3718 | ` *  to the variable $key on each loop.` |
|       - | 3719 | ` *  Note:` |
|       - | 3720 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - | 3721 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - | 3722 | ` *  Note:` |
|       - | 3723 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - | 3724 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - | 3725 | ` *  or after the foreach without resetting it.` |
|       - | 3726 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - | 3727 | ` *  of copying the value.` |
|       - | 3728 | ` */` |
|    2948 | 3729 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 | 3730 |  |
|    2950 | 3731 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    2950 | 3732 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    2950 | 3733 | `	GenBlock *pForeachBlock = 0;` |
|       - | 3734 | `	ph7_foreach_info *pInfo;` |
|       - | 3735 | `	sxu32 nFalseJump;` |
|       - | 3736 | `	VmInstr *pInstr;` |
|       - | 3737 | `	sxu32 nLine;` |
|       - | 3738 | `	sxi32 rc;` |
|    2950 | 3739 | `	nLine = pGen->pIn->nLine;` |
|       - | 3740 | `	/* Jump the 'foreach' keyword */` |
|    2950 | 3741 | `	pGen->pIn++;` |
|    2950 | 3742 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 3743 | `		/* Syntax error */` |
|     ! 0 | 3744 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 3745 | `		if( rc == SXERR_ABORT ){` |
|       - | 3746 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3747 | `			return SXERR_ABORT;` |
|       - | 3748 | `		}` |
|     ! 0 | 3749 | `		goto Synchronize;` |
|       - | 3750 | `	}` |
|       - | 3751 | `	/* Jump the left parenthesis '(' */` |
|    2950 | 3752 | `	pGen->pIn++;` |
|       - | 3753 | `	/* Create the loop block */` |
|    2950 | 3754 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    2950 | 3755 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3756 | `		return SXERR_ABORT;` |
|       - | 3757 | `	}` |
|       - | 3758 | `	/* Delimit the expression */` |
|    2950 | 3759 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    2950 | 3760 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 3761 | `		/* Empty expression */` |
|     ! 0 | 3762 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 | 3763 | `		if( rc == SXERR_ABORT ){` |
|       - | 3764 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3765 | `			return SXERR_ABORT;` |
|       - | 3766 | `		}` |
|       - | 3767 | `		/* Synchronize */` |
|     ! 0 | 3768 | `		pGen->pIn = pEnd;` |
|     ! 0 | 3769 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 3770 | `			pGen->pIn++;` |
|     ! 0 | 3771 | `		}` |
|     ! 0 | 3772 | `		return SXRET_OK;` |
|       - | 3773 | `	}` |
|       - | 3774 | `	/* Compile the array expression */` |
|    2950 | 3775 | `	pCur = pGen->pIn;` |
|   19736 | 3776 | `	while( pCur < pEnd ){` |
|   19736 | 3777 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    2960 | 3778 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    2960 | 3779 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - | 3780 | `				/* Break with the first 'as' found */` |
|    2950 | 3781 | `				break;` |
|       - | 3782 | `			}` |
|       5 | 3783 | `		}` |
|       - | 3784 | `		/* Advance the stream cursor */` |
|   16788 | 3785 | `		pCur++;` |
|       2 | 3786 | `	}` |
|    2950 | 3787 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 3788 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 3789 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 3790 | `		if( rc == SXERR_ABORT ){` |
|       - | 3791 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3792 | `			return SXERR_ABORT;` |
|       - | 3793 | `		}` |
|     ! 0 | 3794 | `		goto Synchronize;` |
|       - | 3795 | `	}` |
|       - | 3796 | `	/* Swap token streams */` |
|    2950 | 3797 | `	pTmp = pGen->pEnd;` |
|    2950 | 3798 | `	pGen->pEnd = pCur;` |
|    2950 | 3799 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    2950 | 3800 | `	if( rc == SXERR_ABORT ){` |
|       - | 3801 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3802 | `		return SXERR_ABORT;` |
|       - | 3803 | `	}` |
|       - | 3804 | `	/* Update token stream */` |
|    2950 | 3805 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 3806 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3807 | `		if( rc == SXERR_ABORT ){` |
|       - | 3808 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3809 | `			return SXERR_ABORT;` |
|       - | 3810 | `		}` |
|     ! 0 | 3811 | `		pGen->pIn++;` |
|     ! 0 | 3812 | `	}` |
|    2950 | 3813 | `	pCur++; /* Jump the 'as' keyword */` |
|    2950 | 3814 | `	pGen->pIn = pCur;` |
|    2950 | 3815 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 3816 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 3817 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3818 | `			return SXERR_ABORT;` |
|       - | 3819 | `		}` |
|     ! 0 | 3820 | `	}` |
|       - | 3821 | `	/* Create the foreach context */` |
|    2950 | 3822 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    2950 | 3823 | `	if( pInfo == 0 ){` |
|     ! 0 | 3824 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 3825 | `		return SXERR_ABORT;` |
|       - | 3826 | `	}` |
|       - | 3827 | `	/* Zero the structure */` |
|    2950 | 3828 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 3829 | `	/* Initialize structure fields */` |
|    2950 | 3830 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 3831 | `	/* Check if we have a key field */` |
|    8896 | 3832 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    5948 | 3833 | `		pCur++;` |
|       2 | 3834 | `	}` |
|    2950 | 3835 | `	if( pCur < pEnd ){` |
|       - | 3836 | `		/* Compile the expression holding the key name */` |
|    2856 | 3837 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 3838 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 3839 | `			if( rc == SXERR_ABORT ){` |
|       - | 3840 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3841 | `				return SXERR_ABORT;` |
|       - | 3842 | `			}` |
|     ! 0 | 3843 | `		}else{` |
|    2856 | 3844 | `			pGen->pEnd = pCur;` |
|    2856 | 3845 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2856 | 3846 | `			if( rc == SXERR_ABORT ){` |
|       - | 3847 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3848 | `				return SXERR_ABORT;` |
|       - | 3849 | `			}` |
|    2856 | 3850 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2856 | 3851 | `			if( pInstr->p3 ){` |
|       - | 3852 | `				/* Record key name */` |
|    2856 | 3853 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1427 | 3854 | `			}` |
|    2856 | 3855 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 3856 | `		}` |
|    2856 | 3857 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1427 | 3858 | `	}` |
|    2950 | 3859 | `	pGen->pEnd = pEnd;` |
|    2950 | 3860 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 3861 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 3862 | `		if( rc == SXERR_ABORT ){` |
|       - | 3863 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3864 | `			return SXERR_ABORT;` |
|       - | 3865 | `		}` |
|     ! 0 | 3866 | `		goto Synchronize;` |
|       - | 3867 | `	}` |
|    2950 | 3868 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 | 3869 | `		pGen->pIn++;` |
|       - | 3870 | `		/* Pass by reference  */` |
|      11 | 3871 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 | 3872 | `	}` |
|       - | 3873 | `	/* Check if the value target is list() */` |
|    2950 | 3874 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 3875 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 3876 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - | 3877 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - | 3878 | `		 */` |
|       - | 3879 | `		static int iForeachListCnt = 0;` |
|       - | 3880 | `		char zTmp[128];` |
|       - | 3881 | `		sxu32 nLen;` |
|       - | 3882 | `		char *zDup;` |
|      10 | 3883 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 | 3884 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 | 3885 | `		if( zDup == 0 ){` |
|     ! 0 | 3886 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3887 | `			return SXERR_ABORT;` |
|       - | 3888 | `		}` |
|      10 | 3889 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 3890 | `		/* Save list() token boundaries */` |
|      10 | 3891 | `		pListStart = pGen->pIn;` |
|       - | 3892 | `		/* Advance past list(...) — validate parentheses */` |
|      10 | 3893 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 | 3894 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 | 3895 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - | 3896 | `				"foreach: Expected '(' after 'list'");` |
|       3 | 3897 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3898 | `				return SXERR_ABORT;` |
|       - | 3899 | `			}` |
|       3 | 3900 | `			goto Synchronize;` |
|       - | 3901 | `		}` |
|       7 | 3902 | `		pGen->pIn++; /* Jump '(' */` |
|       7 | 3903 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 | 3904 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 3905 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3906 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 | 3907 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3908 | `				return SXERR_ABORT;` |
|       - | 3909 | `			}` |
|     ! 0 | 3910 | `			goto Synchronize;` |
|       - | 3911 | `		}` |
|       7 | 3912 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 | 3913 | `		pListEnd = pGen->pIn;` |
|       7 | 3914 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    2945 | 3915 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - | 3916 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - | 3917 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - | 3918 | `		 */` |
|       - | 3919 | `		static int iForeachShortListCnt = 0;` |
|       - | 3920 | `		char zTmp[128];` |
|       - | 3921 | `		sxu32 nLen;` |
|       - | 3922 | `		char *zDup;` |
|       3 | 3923 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       3 | 3924 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       3 | 3925 | `		if( zDup == 0 ){` |
|     ! 0 | 3926 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3927 | `			return SXERR_ABORT;` |
|       - | 3928 | `		}` |
|       3 | 3929 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 3930 | `		/* Save [...] token boundaries */` |
|       3 | 3931 | `		pListStart = pGen->pIn;` |
|       - | 3932 | `		/* Advance past [...] */` |
|       3 | 3933 | `		pGen->pIn++; /* Jump '[' */` |
|       3 | 3934 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       3 | 3935 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 3936 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3937 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 | 3938 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3939 | `				return SXERR_ABORT;` |
|       - | 3940 | `			}` |
|     ! 0 | 3941 | `			goto Synchronize;` |
|       - | 3942 | `		}` |
|       3 | 3943 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       3 | 3944 | `		pListEnd = pGen->pIn;` |
|       3 | 3945 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       2 | 3946 | `	}else{` |
|       - | 3947 | `		/* Compile the expression holding the value name */` |
|    2940 | 3948 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2940 | 3949 | `		if( rc == SXERR_ABORT ){` |
|       - | 3950 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3951 | `			return SXERR_ABORT;` |
|       - | 3952 | `		}` |
|    2940 | 3953 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2940 | 3954 | `		if( pInstr->p3 ){` |
|       - | 3955 | `			/* Record value name */` |
|    2940 | 3956 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1469 | 3957 | `		}` |
|       - | 3958 | `	}` |
|       - | 3959 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    2948 | 3960 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 3961 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2948 | 3962 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 3963 | `	/* Record the first instruction to execute */` |
|    2948 | 3964 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3965 | `	/* Emit the FOREACH_STEP instruction */` |
|    2948 | 3966 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 3967 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2948 | 3968 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 3969 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    2948 | 3970 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - | 3971 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - | 3972 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - | 3973 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - | 3974 | `		 */` |
|       9 | 3975 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - | 3976 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - | 3977 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - | 3978 | `		 * picks up the delimiter and the variable names inside.` |
|       - | 3979 | `		 */` |
|       9 | 3980 | `		pSavedIn = pGen->pIn;` |
|       9 | 3981 | `		pSavedEnd = pGen->pEnd;` |
|       9 | 3982 | `		pGen->pIn = pListStart;` |
|       9 | 3983 | `		pGen->pEnd = pListEnd;` |
|       9 | 3984 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       3 | 3985 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       2 | 3986 | `		}else{` |
|       7 | 3987 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - | 3988 | `		}` |
|       9 | 3989 | `		pGen->pIn = pSavedIn;` |
|       9 | 3990 | `		pGen->pEnd = pSavedEnd;` |
|       9 | 3991 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3992 | `			return SXERR_ABORT;` |
|       - | 3993 | `		}` |
|       - | 3994 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       9 | 3995 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       4 | 3996 | `	}` |
|       - | 3997 | `	/* Compile the loop body */` |
|    2948 | 3998 | `	pGen->pIn = &pEnd[1];` |
|    2948 | 3999 | `	pGen->pEnd = pTmp;` |
|    2948 | 4000 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    2948 | 4001 | `	if( rc == SXERR_ABORT ){` |
|       - | 4002 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4003 | `		return SXERR_ABORT;` |
|       - | 4004 | `	}` |
|       - | 4005 | `	/* Emit the unconditional jump to the start of the loop */` |
|    2948 | 4006 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 4007 | `	/* Fix all jumps now the destination is resolved */` |
|    2948 | 4008 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4009 | `	/* Release the loop block */` |
|    2948 | 4010 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 4011 | `	/* Statement successfully compiled */` |
|    2948 | 4012 | `	return SXRET_OK;` |
|       1 | 4013 | `Synchronize:` |
|       - | 4014 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 4015 | `	 * compiling this erroneous block.` |
|       - | 4016 | `	 */` |
|       3 | 4017 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 4018 | `		pGen->pIn++;` |
|     ! 0 | 4019 | `	}` |
|       3 | 4020 | `	return SXRET_OK;` |
|    1476 | 4021 |  |
|       - | 4022 | `/*` |
|       - | 4023 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - | 4024 | ` * According to the PHP language reference` |
|       - | 4025 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - | 4026 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - | 4027 | ` *  that is similar to that of C:` |
|       - | 4028 | ` *  if (expr)` |
|       - | 4029 | ` *   statement` |
|       - | 4030 | ` *  else construct:` |
|       - | 4031 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - | 4032 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - | 4033 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - | 4034 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - | 4035 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - | 4036 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - | 4037 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - | 4038 | ` *  elseif` |
|       - | 4039 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - | 4040 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - | 4041 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - | 4042 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - | 4043 | ` *   than b, a equal to b or a is smaller than b:` |
|       - | 4044 | ` *   <?php` |
|       - | 4045 | ` *    if ($a > $b) {` |
|       - | 4046 | ` *     echo "a is bigger than b";` |
|       - | 4047 | ` *    } elseif ($a == $b) {` |
|       - | 4048 | ` *     echo "a is equal to b";` |
|       - | 4049 | ` *    } else {` |
|       - | 4050 | ` *     echo "a is smaller than b";` |
|       - | 4051 | ` *    }` |
|       - | 4052 | ` *    ?>` |
|       - | 4053 | ` */` |
|  108190 | 4054 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 | 4055 |  |
|  108192 | 4056 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  108192 | 4057 | `	GenBlock *pCondBlock = 0;` |
|       - | 4058 | `	sxu32 nJumpIdx;` |
|       - | 4059 | `	sxu32 nKeyID;` |
|       - | 4060 | `	sxi32 rc;` |
|       - | 4061 | `	/* Jump the 'if' keyword */` |
|  108192 | 4062 | `	pGen->pIn++;` |
|  108192 | 4063 | `	pToken = pGen->pIn;` |
|       - | 4064 | `	/* Create the conditional block */` |
|  108192 | 4065 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  108192 | 4066 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4067 | `		return SXERR_ABORT;` |
|       - | 4068 | `	}` |
|       - | 4069 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   59499 | 4070 | `	for(;;){` |
|  119000 | 4071 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4072 | `			/* Syntax error */` |
|     ! 0 | 4073 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 4074 | `				pToken--;` |
|     ! 0 | 4075 | `			}` |
|     ! 0 | 4076 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 | 4077 | `			if( rc == SXERR_ABORT ){` |
|       - | 4078 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4079 | `				return SXERR_ABORT;` |
|       - | 4080 | `			}` |
|     ! 0 | 4081 | `			goto Synchronize;` |
|       - | 4082 | `		}` |
|       - | 4083 | `		/* Jump the left parenthesis '(' */` |
|  119000 | 4084 | `		pToken++;` |
|       - | 4085 | `		/* Delimit the condition */` |
|  119000 | 4086 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  119000 | 4087 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - | 4088 | `			/* Syntax error */` |
|     ! 0 | 4089 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 4090 | `				pToken--;` |
|     ! 0 | 4091 | `			}` |
|     ! 0 | 4092 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 | 4093 | `			if( rc == SXERR_ABORT ){` |
|       - | 4094 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4095 | `				return SXERR_ABORT;` |
|       - | 4096 | `			}` |
|     ! 0 | 4097 | `			goto Synchronize;` |
|       - | 4098 | `		}` |
|       - | 4099 | `		/* Swap token streams */` |
|  119000 | 4100 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 4101 | `		/* Compile the condition */` |
|  119000 | 4102 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 4103 | `		/* Update token stream */` |
|  119000 | 4104 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 4105 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 4106 | `			pGen->pIn++;` |
|     ! 0 | 4107 | `		}` |
|  119000 | 4108 | `		pGen->pIn  = &pEnd[1];` |
|  119000 | 4109 | `		pGen->pEnd = pTmp;` |
|  119000 | 4110 | `		if( rc == SXERR_ABORT ){` |
|       - | 4111 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 4112 | `			return SXERR_ABORT;` |
|       - | 4113 | `		}` |
|       - | 4114 | `		/* Emit the false jump */` |
|  119000 | 4115 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 4116 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  119000 | 4117 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 4118 | `		/* Compile the body */` |
|  119000 | 4119 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  119000 | 4120 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4121 | `			return SXERR_ABORT;` |
|       - | 4122 | `		}` |
|  119000 | 4123 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   32026 | 4124 | `			break;` |
|       - | 4125 | `		}` |
|       - | 4126 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   54952 | 4127 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   54952 | 4128 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   35334 | 4129 | `			break;` |
|       - | 4130 | `		}` |
|       - | 4131 | `		/* Emit the unconditional jump */` |
|   19620 | 4132 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 4133 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   19620 | 4134 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   19620 | 4135 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   14204 | 4136 | `			pToken = &pGen->pIn[1];` |
|   14204 | 4137 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    5422 | 4138 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4407 | 4139 | `					break;` |
|       - | 4140 | `			}` |
|    5394 | 4141 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2696 | 4142 | `		}` |
|   10810 | 4143 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 4144 | `		/* Synchronize cursors */` |
|   10810 | 4145 | `		pToken = pGen->pIn;` |
|       - | 4146 | `		/* Fix the false jump */` |
|   10810 | 4147 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 | 4148 | `	} /* For(;;) */` |
|       - | 4149 | `	/* Fix the false jump */` |
|  108192 | 4150 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  108192 | 4151 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   44142 | 4152 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 4153 | `			/* Compile the else block */` |
|    8812 | 4154 | `			pGen->pIn++;` |
|    8812 | 4155 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    8812 | 4156 | `			if( rc == SXERR_ABORT ){` |
|       - | 4157 |  |
|     ! 0 | 4158 | `				return SXERR_ABORT;` |
|       - | 4159 | `			}` |
|    4405 | 4160 | `	}` |
|  108192 | 4161 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 4162 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  108192 | 4163 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 4164 | `	/* Release the conditional block */` |
|  108192 | 4165 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 4166 | `	/* Statement successfully compiled */` |
|  108192 | 4167 | `	return SXRET_OK;` |
|     ! 0 | 4168 | `Synchronize:` |
|       - | 4169 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 4170 | `	 */` |
|     ! 0 | 4171 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 4172 | `		pGen->pIn++;` |
|     ! 0 | 4173 | `	}` |
|     ! 0 | 4174 | `	return SXRET_OK;` |
|   54097 | 4175 |  |
|       - | 4176 | `/*` |
|       - | 4177 | ` * Compile the global construct.` |
|       - | 4178 | ` * According to the PHP language reference` |
|       - | 4179 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - | 4180 | ` *  to be used in that function.` |
|       - | 4181 | ` *  Example #1 Using global` |
|       - | 4182 | ` *  <?php` |
|       - | 4183 | ` *   $a = 1;` |
|       - | 4184 | ` *   $b = 2;` |
|       - | 4185 | ` *   function Sum()` |
|       - | 4186 | ` *   {` |
|       - | 4187 | ` *    global $a, $b;` |
|       - | 4188 | ` *    $b = $a + $b;` |
|       - | 4189 | ` *   }` |
|       - | 4190 | ` *   Sum();` |
|       - | 4191 | ` *   echo $b;` |
|       - | 4192 | ` *  ?>` |
|       - | 4193 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - | 4194 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - | 4195 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - | 4196 | ` */` |
|      26 | 4197 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       2 | 4198 |  |
|      28 | 4199 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 4200 | `	sxi32 nExpr;` |
|       - | 4201 | `	sxi32 rc;` |
|       - | 4202 | `	/* Jump the 'global' keyword */` |
|      28 | 4203 | `	pGen->pIn++;` |
|      28 | 4204 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - | 4205 | `		/* Nothing to process */` |
|     ! 0 | 4206 | `		return SXRET_OK;` |
|       - | 4207 | `	}` |
|      28 | 4208 | `	pTmp = pGen->pEnd;` |
|      28 | 4209 | `	nExpr = 0;` |
|      56 | 4210 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      30 | 4211 | `		if( pGen->pIn < pNext ){` |
|      30 | 4212 | `			pGen->pEnd = pNext;` |
|      30 | 4213 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 4214 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 | 4215 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4216 | `					return SXERR_ABORT;` |
|       - | 4217 | `				}` |
|     ! 0 | 4218 | `			}else{` |
|      30 | 4219 | `				pGen->pIn++;` |
|      30 | 4220 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4221 | `					/* Emit a warning */` |
|     ! 0 | 4222 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 | 4223 | `				}else{` |
|      30 | 4224 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 | 4225 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4226 | `						return SXERR_ABORT;` |
|      30 | 4227 | `					}else if(rc != SXERR_EMPTY ){` |
|      30 | 4228 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      30 | 4229 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - | 4230 | `							/* Variable name, not a constant */` |
|      30 | 4231 | `							pLast->iP1 = 0;` |
|      14 | 4232 | `						}` |
|      30 | 4233 | `						nExpr++;` |
|      14 | 4234 | `					}` |
|       - | 4235 | `				}` |
|       - | 4236 | `			}` |
|      14 | 4237 | `		}` |
|       - | 4238 | `		/* Next expression in the stream */` |
|      30 | 4239 | `		pGen->pIn = pNext;` |
|       - | 4240 | `		/* Jump trailing commas */` |
|      32 | 4241 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 4242 | `			pGen->pIn++;` |
|       1 | 4243 | `		}` |
|       2 | 4244 | `	}` |
|       - | 4245 | `	/* Restore token stream */` |
|      28 | 4246 | `	pGen->pEnd = pTmp;` |
|      28 | 4247 | `	if( nExpr > 0 ){` |
|       - | 4248 | `		/* Emit the uplink instruction */` |
|      28 | 4249 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      13 | 4250 | `	}` |
|      28 | 4251 | `	return SXRET_OK;` |
|      15 | 4252 |  |
|       - | 4253 | `/*` |
|       - | 4254 | ` * Compile the return statement.` |
|       - | 4255 | ` * According to the PHP language reference` |
|       - | 4256 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - | 4257 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - | 4258 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - | 4259 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - | 4260 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - | 4261 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - | 4262 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - | 4263 | ` *  from within the main script file, then script execution end.` |
|       - | 4264 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - | 4265 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - | 4266 | ` *  should do so as PHP has less work to do in this case.` |
|       - | 4267 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - | 4268 | ` */` |
|  157068 | 4269 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 | 4270 |  |
|  157070 | 4271 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 4272 | `	sxi32 rc;` |
|       - | 4273 | `	/* Jump the 'return' keyword */` |
|  157070 | 4274 | `	pGen->pIn++;` |
|  157070 | 4275 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 4276 | `		/* Compile the expression */` |
|  157048 | 4277 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  157048 | 4278 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4279 | `			return SXERR_ABORT;` |
|  157048 | 4280 | `		}else if(rc != SXERR_EMPTY ){` |
|  157048 | 4281 | `			nRet = 1;` |
|   78523 | 4282 | `		}` |
|   78523 | 4283 | `	}` |
|       - | 4284 | `	/* Emit the done instruction */` |
|  157070 | 4285 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  157070 | 4286 | `	return SXRET_OK;` |
|   78536 | 4287 |  |
|       - | 4288 | `/*` |
|       - | 4289 | ` * Compile a yield expression.` |
|       - | 4290 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - | 4291 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - | 4292 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - | 4293 | ` */` |
|      32 | 4294 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 | 4295 |  |
|       - | 4296 | `	SyToken *pTmp, *pSplit;` |
|      34 | 4297 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      34 | 4298 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - | 4299 | `	sxi32 rc;` |
|      16 | 4300 | `	(void)iCompileFlag;` |
|       - | 4301 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      34 | 4302 | `	pGen->pIn++;` |
|       - | 4303 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - | 4304 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|      34 | 4305 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4306 | `		/* Bare yield — no value */` |
|     ! 0 | 4307 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 | 4308 | `		return SXRET_OK;` |
|       - | 4309 | `	}` |
|       - | 4310 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      34 | 4311 | `	pSplit = 0;` |
|       - | 4312 | `	{` |
|      34 | 4313 | `		SyToken *pCur = pGen->pIn;` |
|      34 | 4314 | `		sxi32 nNest = 0;` |
|      78 | 4315 | `		while( pCur < pGen->pEnd ){` |
|      52 | 4316 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 | 4317 | `				nNest++;` |
|      52 | 4318 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 | 4319 | `				nNest--;` |
|      52 | 4320 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       7 | 4321 | `				pSplit = pCur;` |
|       7 | 4322 | `				break;` |
|       - | 4323 | `			}` |
|      46 | 4324 | `			pCur++;` |
|       2 | 4325 | `		}` |
|       - | 4326 | `	}` |
|      34 | 4327 | `	pTmp = pGen->pEnd;` |
|      34 | 4328 | `	if( pSplit ){` |
|       - | 4329 | `		/* yield $key => $value */` |
|       7 | 4330 | `		pGen->pEnd = pSplit;` |
|       7 | 4331 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 | 4332 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 | 4333 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       7 | 4334 | `		pGen->pEnd = pTmp;` |
|       7 | 4335 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 | 4336 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 | 4337 | `		iP1 = 1;` |
|       7 | 4338 | `		iP2 = 1;` |
|       4 | 4339 | `	}else{` |
|       - | 4340 | `		/* yield $value */` |
|      28 | 4341 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      28 | 4342 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      28 | 4343 | `		if( rc != SXERR_EMPTY ){` |
|      28 | 4344 | `			iP1 = 1;` |
|      13 | 4345 | `		}` |
|       - | 4346 | `	}` |
|      34 | 4347 | `	pGen->pEnd = pTmp;` |
|      34 | 4348 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      34 | 4349 | `	return SXRET_OK;` |
|      18 | 4350 |  |
|       - | 4351 | `/*` |
|       - | 4352 | ` * Compile the die/exit language construct.` |
|       - | 4353 | ` * The role of these constructs is to terminate execution of the script.` |
|       - | 4354 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - | 4355 | ` */` |
|      88 | 4356 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       2 | 4357 |  |
|      90 | 4358 | `	sxi32 nExpr = 0;` |
|       - | 4359 | `	sxi32 rc;` |
|       - | 4360 | `	/* Jump the die/exit keyword */` |
|      90 | 4361 | `	pGen->pIn++;` |
|      90 | 4362 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 4363 | `		/* Compile the expression */` |
|      90 | 4364 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      90 | 4365 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4366 | `			return SXERR_ABORT;` |
|      90 | 4367 | `		}else if(rc != SXERR_EMPTY ){` |
|      90 | 4368 | `			nExpr = 1;` |
|      44 | 4369 | `		}` |
|      44 | 4370 | `	}` |
|       - | 4371 | `	/* Emit the HALT instruction */` |
|      90 | 4372 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      90 | 4373 | `	return SXRET_OK;` |
|      46 | 4374 |  |
|       - | 4375 | `/*` |
|       - | 4376 | ` * Compile the 'echo' language construct.` |
|       - | 4377 | ` */` |
|   11218 | 4378 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 | 4379 |  |
|   11220 | 4380 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 4381 | `	sxi32 rc;` |
|       - | 4382 | `	/* Jump the 'echo' keyword */` |
|   11220 | 4383 | `	pGen->pIn++;` |
|       - | 4384 | `	/* Compile arguments one after one */` |
|   11220 | 4385 | `	pTmp = pGen->pEnd;` |
|   22910 | 4386 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   11692 | 4387 | `		if( pGen->pIn < pNext ){` |
|   11692 | 4388 | `			pGen->pEnd = pNext;` |
|   11692 | 4389 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   11692 | 4390 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4391 | `				return SXERR_ABORT;` |
|   11692 | 4392 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 4393 | `				/* Emit the consume instruction */` |
|   11668 | 4394 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    5833 | 4395 | `			}` |
|    5845 | 4396 | `		}` |
|       - | 4397 | `		/* Jump trailing commas */` |
|   12164 | 4398 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     474 | 4399 | `			pNext++;` |
|       2 | 4400 | `		}` |
|   11692 | 4401 | `		pGen->pIn = pNext;` |
|       2 | 4402 | `	}` |
|       - | 4403 | `	/* Restore token stream */` |
|   11220 | 4404 | `	pGen->pEnd = pTmp;` |
|   11220 | 4405 | `	return SXRET_OK;` |
|    5611 | 4406 |  |
|       - | 4407 | `/*` |
|       - | 4408 | ` * Compile the static statement.` |
|       - | 4409 | ` * According to the PHP language reference` |
|       - | 4410 | ` *  Another important feature of variable scoping is the static variable.` |
|       - | 4411 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - | 4412 | ` *  when program execution leaves this scope.` |
|       - | 4413 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - | 4414 | ` * Symisc eXtension.` |
|       - | 4415 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - | 4416 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4417 | ` *  Example` |
|       - | 4418 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 4419 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 4420 | ` */` |
|       2 | 4421 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       1 | 4422 |  |
|       - | 4423 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - | 4424 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - | 4425 | `	GenBlock *pBlock;` |
|       - | 4426 | `	SyString *pName;` |
|       - | 4427 | `	char *zDup;` |
|       - | 4428 | `	sxu32 nLine;` |
|       - | 4429 | `	sxi32 rc;` |
|       - | 4430 | `	/* Jump the static keyword */` |
|       3 | 4431 | `	nLine = pGen->pIn->nLine;` |
|       3 | 4432 | `	pGen->pIn++;` |
|       - | 4433 | `	/* Extract the enclosing function if any */` |
|       3 | 4434 | `	pBlock = pGen->pCurrent;` |
|       5 | 4435 | `	while( pBlock ){` |
|       5 | 4436 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       3 | 4437 | `			break;` |
|       - | 4438 | `		}` |
|       - | 4439 | `		/* Point to the upper block */` |
|       3 | 4440 | `		pBlock = pBlock->pParent;` |
|       1 | 4441 | `	}` |
|       3 | 4442 | `	if( pBlock == 0 ){` |
|       - | 4443 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 | 4444 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 4445 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 | 4446 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4447 | `				return SXERR_ABORT;` |
|       - | 4448 | `			}` |
|     ! 0 | 4449 | `			goto Synchronize;` |
|       - | 4450 | `		}` |
|       - | 4451 | `		/* Compile the expression holding the variable */` |
|     ! 0 | 4452 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 4453 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4454 | `			return SXERR_ABORT;` |
|     ! 0 | 4455 | `		}else if( rc != SXERR_EMPTY ){` |
|       - | 4456 | `			/* Emit the POP instruction */` |
|     ! 0 | 4457 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 4458 | `		}` |
|     ! 0 | 4459 | `		return SXRET_OK;` |
|       - | 4460 | `	}` |
|       3 | 4461 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - | 4462 | `	/* Make sure we are dealing with a valid statement */` |
|       3 | 4463 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     ! 0 | 4464 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 | 4465 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 | 4466 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4467 | `				return SXERR_ABORT;` |
|       - | 4468 | `			}` |
|       3 | 4469 | `			goto Synchronize;` |
|       - | 4470 | `	}` |
|     ! 0 | 4471 | `	pGen->pIn++;` |
|       - | 4472 | `	/* Extract variable name */` |
|     ! 0 | 4473 | `	pName = &pGen->pIn->sData;` |
|     ! 0 | 4474 | `	pGen->pIn++; /* Jump the var name */` |
|     ! 0 | 4475 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 | 4476 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 4477 | `		goto Synchronize;` |
|       - | 4478 | `	}` |
|       - | 4479 | `	/* Initialize the structure describing the static variable */` |
|     ! 0 | 4480 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     ! 0 | 4481 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - | 4482 | `	/* Duplicate variable name */` |
|     ! 0 | 4483 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     ! 0 | 4484 | `	if( zDup == 0 ){` |
|     ! 0 | 4485 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 4486 | `		return SXERR_ABORT;` |
|       - | 4487 | `	}` |
|     ! 0 | 4488 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - | 4489 | `	/* Check if we have an expression to compile */` |
|     ! 0 | 4490 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - | 4491 | `		SySet *pInstrContainer;` |
|       - | 4492 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - | 4493 | `		 * Static variable can take any complex expression including function` |
|       - | 4494 | `		 * call as their initialization value.` |
|       - | 4495 | `		 * Example:` |
|       - | 4496 | `		 *		static $var = foo(1,4+5,bar());` |
|       - | 4497 | `		 */` |
|     ! 0 | 4498 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - | 4499 | `		/* Swap bytecode container */` |
|     ! 0 | 4500 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     ! 0 | 4501 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - | 4502 | `		/* Compile the expression */` |
|     ! 0 | 4503 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 4504 | `		/* Emit the done instruction */` |
|     ! 0 | 4505 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - | 4506 | `		/* Restore default bytecode container */` |
|     ! 0 | 4507 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 4508 | `	}` |
|       - | 4509 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     ! 0 | 4510 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     ! 0 | 4511 | `	return SXRET_OK;` |
|       1 | 4512 | `Synchronize:` |
|       - | 4513 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - | 4514 | `	 * statement.` |
|       - | 4515 | `	 */` |
|       5 | 4516 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 | 4517 | `		pGen->pIn++;` |
|       1 | 4518 | `	}` |
|       3 | 4519 | `	return SXRET_OK;` |
|       2 | 4520 |  |
|       - | 4521 | `/*` |
|       - | 4522 | ` * Compile the var statement.` |
|       - | 4523 | ` * Symisc Extension:` |
|       - | 4524 | ` *      var statement can be used outside of a class definition.` |
|       - | 4525 | ` */` |
|       4 | 4526 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 | 4527 |  |
|       - | 4528 | `	sxu32 nLine;` |
|       - | 4529 | `	sxi32 rc;` |
|       5 | 4530 | `	nLine = pGen->pIn->nLine;` |
|       - | 4531 | `	/* Jump the 'var' keyword */` |
|       5 | 4532 | `	pGen->pIn++;` |
|       5 | 4533 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 4534 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - | 4535 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 | 4536 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 | 4537 | `			pGen->pIn++;` |
|     ! 0 | 4538 | `		}` |
|     ! 0 | 4539 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4540 | `			return SXERR_ABORT;` |
|       - | 4541 | `		}` |
|     ! 0 | 4542 | `	}else{` |
|       - | 4543 | `		/* Compile the expression */` |
|       5 | 4544 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 | 4545 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4546 | `			return SXERR_ABORT;` |
|       5 | 4547 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 | 4548 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 | 4549 | `		}` |
|       - | 4550 | `	}` |
|       5 | 4551 | `	return SXRET_OK;` |
|       3 | 4552 |  |
|       - | 4553 | `/*` |
|       - | 4554 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - | 4555 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - | 4556 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - | 4557 | ` */` |
|       - | 4558 | `/*` |
|       - | 4559 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - | 4560 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - | 4561 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - | 4562 | ` * qualified name and updates the instruction's operand index.` |
|       - | 4563 | ` *` |
|       - | 4564 | ` * Resolution order:` |
|       - | 4565 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - | 4566 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - | 4567 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - | 4568 | ` *` |
|       - | 4569 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - | 4570 | ` * came from an import (step 1) and 0 otherwise.` |
|       - | 4571 | ` * Returns the (possibly new) literal index.` |
|       - | 4572 | ` */` |
|  322354 | 4573 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       2 | 4574 |  |
|       - | 4575 | `	ph7_value *pLit;` |
|       - | 4576 | `	const char *zLit;` |
|       - | 4577 | `	SyString sQualified;` |
|       - | 4578 | `	sxu32 nLit;` |
|       - | 4579 | `	sxu32 k;` |
|       - | 4580 | `	sxu32 nNewIdx;` |
|       - | 4581 | `	int hasNsSep;` |
|       - | 4582 | `	SyHashEntry *pImport;` |
|       - | 4583 | `	ph7_value *pNew;` |
|  322356 | 4584 | `	if( pFromImport ){` |
|  308282 | 4585 | `		*pFromImport = 0;` |
|  154140 | 4586 | `	}` |
|  322356 | 4587 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  322356 | 4588 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 | 4589 | `		return nOrigIdx;` |
|       - | 4590 | `	}` |
|  322356 | 4591 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  322356 | 4592 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - | 4593 | `	/* Skip if already qualified (contains backslash) */` |
|  322356 | 4594 | `	hasNsSep = 0;` |
| 3467110 | 4595 | `	for( k = 0; k < nLit; k++ ){` |
| 3144788 | 4596 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 1572379 | 4597 | `	}` |
|  322356 | 4598 | `	if( hasNsSep ){` |
|      34 | 4599 | `		return nOrigIdx;` |
|       - | 4600 | `	}` |
|       - | 4601 | `	/* Check use imports first (works even outside namespaces) */` |
|  322324 | 4602 | `	SyBlobReset(&pGen->sWorker);` |
|  322324 | 4603 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  322324 | 4604 | `	if( pImport ){` |
|      38 | 4605 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      38 | 4606 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      38 | 4607 | `		if( pFromImport ){` |
|      18 | 4608 | `			*pFromImport = 1;` |
|       8 | 4609 | `		}` |
|      20 | 4610 | `	}else{` |
|  322288 | 4611 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  322206 | 4612 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - | 4613 | `		}` |
|       - | 4614 | `		/* Prepend current namespace */` |
|      84 | 4615 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      84 | 4616 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      84 | 4617 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - | 4618 | `	}` |
|       - | 4619 | `	/* Look up or create a new literal for the qualified name */` |
|     120 | 4620 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     120 | 4621 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      52 | 4622 | `		return nNewIdx; /* Already interned */` |
|       - | 4623 | `	}` |
|      70 | 4624 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      70 | 4625 | `	if( pNew == 0 ){` |
|     ! 0 | 4626 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - | 4627 | `	}` |
|      70 | 4628 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      70 | 4629 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      70 | 4630 | `	return nNewIdx;` |
|  161179 | 4631 |  |
|       - | 4632 | `/*` |
|       - | 4633 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - | 4634 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - | 4635 | ` */` |
|   27276 | 4636 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 4637 |  |
|       - | 4638 | `	SyHashEntry *pImport;` |
|       - | 4639 | `	/* Check use imports first */` |
|   27278 | 4640 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   27278 | 4641 | `	if( pImport ){` |
|      12 | 4642 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      12 | 4643 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      12 | 4644 | `		return;` |
|       - | 4645 | `	}` |
|       - | 4646 | `	/* Prepend current namespace if active */` |
|   27268 | 4647 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 | 4648 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 | 4649 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 | 4650 | `	}` |
|   27268 | 4651 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   13640 | 4652 |  |
|       - | 4653 | `/*` |
|       - | 4654 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - | 4655 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - | 4656 | ` * The caller must release pOut when done.` |
|       - | 4657 | ` */` |
|   46422 | 4658 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 4659 |  |
|   46424 | 4660 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      50 | 4661 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      50 | 4662 | `		SyBlobAppend(pOut,"\\",1);` |
|      24 | 4663 | `	}` |
|   46424 | 4664 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   46424 | 4665 |  |
|       - | 4666 | `/*` |
|       - | 4667 | ` * Compile a namespace statement` |
|       - | 4668 | ` * According to the PHP language reference manual` |
|       - | 4669 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - | 4670 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - | 4671 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - | 4672 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - | 4673 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - | 4674 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - | 4675 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - | 4676 | ` *  programming world.` |
|       - | 4677 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - | 4678 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - | 4679 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - | 4680 | ` *  classes/functions/constants.` |
|       - | 4681 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - | 4682 | ` *  readability of source code.` |
|       - | 4683 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - | 4684 | ` *  Here is an example of namespace syntax in PHP:` |
|       - | 4685 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - | 4686 | ` *       class MyClass {}` |
|       - | 4687 | ` *       function myfunction() {}` |
|       - | 4688 | ` *       const MYCONST = 1;` |
|       - | 4689 | ` *       $a = new MyClass;` |
|       - | 4690 | ` *       $c = new \my\name\MyClass;` |
|       - | 4691 | ` *       $a = strlen('hi');` |
|       - | 4692 | ` *       $d = namespace\MYCONST;` |
|       - | 4693 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - | 4694 | ` *       echo constant($d);` |
|       - | 4695 | ` * NOTE` |
|       - | 4696 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 4697 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 4698 | ` */` |
|       - | 4699 | `/*` |
|       - | 4700 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - | 4701 | ` */` |
|      14 | 4702 | `static const char * TokenTypeName(sxu32 nType)` |
|       1 | 4703 |  |
|      15 | 4704 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       9 | 4705 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       9 | 4706 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       9 | 4707 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       9 | 4708 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       9 | 4709 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 | 4710 | `	return "token";` |
|       8 | 4711 |  |
|      96 | 4712 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       2 | 4713 |  |
|       - | 4714 | `	sxu32 nLine;` |
|       - | 4715 | `	sxi32 rc;` |
|      98 | 4716 | `	nLine = pGen->pIn->nLine;` |
|      98 | 4717 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - | 4718 | `	/* Reset namespace and clear previous use imports */` |
|      98 | 4719 | `	SyBlobReset(&pGen->sNamespace);` |
|      98 | 4720 | `	SyHashRelease(&pGen->hUseImports);` |
|      98 | 4721 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      98 | 4722 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|      98 | 4723 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|      98 | 4724 | `	SyHashRelease(&pGen->hUseConstImports);` |
|      98 | 4725 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|      98 | 4726 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4727 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 | 4728 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 4729 | `		return SXRET_OK;` |
|       - | 4730 | `	}` |
|      98 | 4731 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - | 4732 | `		/* namespace; — switch to global namespace */` |
|     ! 0 | 4733 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 4734 | `		return SXRET_OK;` |
|       - | 4735 | `	}` |
|      98 | 4736 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - | 4737 | `		/* namespace { } — global namespace block */` |
|     ! 0 | 4738 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 4739 | `		return SXRET_OK;` |
|       - | 4740 | `	}` |
|       - | 4741 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     232 | 4742 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     136 | 4743 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - | 4744 | `			/* Append backslash separator */` |
|      21 | 4745 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      21 | 4746 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      10 | 4747 | `			}` |
|      11 | 4748 | `		}else{` |
|       - | 4749 | `			/* Append identifier */` |
|     116 | 4750 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 4751 | `		}` |
|     136 | 4752 | `		pGen->pIn++;` |
|       2 | 4753 | `	}` |
|       - | 4754 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - | 4755 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - | 4756 | `	{` |
|      98 | 4757 | `		char *zNsDup = 0;` |
|      98 | 4758 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     143 | 4759 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      94 | 4760 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      47 | 4761 | `		}` |
|      98 | 4762 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - | 4763 | `	}` |
|      98 | 4764 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 | 4765 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 4766 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 | 4767 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 | 4768 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4769 | `			return SXERR_ABORT;` |
|       - | 4770 | `		}` |
|       2 | 4771 | `	}` |
|      98 | 4772 | `	return SXRET_OK;` |
|      50 | 4773 |  |
|       - | 4774 | `/*` |
|       - | 4775 | ` * Compile the 'use' statement` |
|       - | 4776 | ` * According to the PHP language reference manual` |
|       - | 4777 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - | 4778 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - | 4779 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - | 4780 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - | 4781 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - | 4782 | ` *  a function or constant is not supported.` |
|       - | 4783 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - | 4784 | ` * NOTE` |
|       - | 4785 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 4786 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 4787 | ` */` |
|      66 | 4788 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       2 | 4789 |  |
|       - | 4790 | `	sxu32 nLine;` |
|       - | 4791 | `	sxi32 rc;` |
|       - | 4792 | `	SyBlob sPath;` |
|       - | 4793 | `	SyString sAlias;` |
|       - | 4794 | `	SyToken *pLast;` |
|       - | 4795 | `	char *zDup;` |
|       - | 4796 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - | 4797 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - | 4798 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      68 | 4799 | `	nLine = pGen->pIn->nLine;` |
|      68 | 4800 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - | 4801 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      68 | 4802 | `	iUseType = 0;` |
|      68 | 4803 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 | 4804 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 | 4805 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 | 4806 | `			iUseType = 1;` |
|      16 | 4807 | `			pGen->pIn++;` |
|      23 | 4808 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 | 4809 | `			iUseType = 2;` |
|      16 | 4810 | `			pGen->pIn++;` |
|       7 | 4811 | `		}` |
|      14 | 4812 | `	}` |
|       - | 4813 | `	/* Select target hash tables based on import type */` |
|      68 | 4814 | `	switch( iUseType ){` |
|       7 | 4815 | `		case 1:` |
|      16 | 4816 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 | 4817 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 | 4818 | `			break;` |
|       7 | 4819 | `		case 2:` |
|      16 | 4820 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 | 4821 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 | 4822 | `			break;` |
|      19 | 4823 | `		default:` |
|      40 | 4824 | `			pGenHash = &pGen->hUseImports;` |
|      40 | 4825 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      38 | 4826 | `			break;` |
|       - | 4827 | `	}` |
|      68 | 4828 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - | 4829 | `	/* Process one or more use declarations separated by commas */` |
|      34 | 4830 | `	for(;;){` |
|      70 | 4831 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 4832 | `			break;` |
|       - | 4833 | `		}` |
|      70 | 4834 | `		SyBlobReset(&sPath);` |
|      70 | 4835 | `		pLast = 0;` |
|       - | 4836 | `		/* Collect the full namespace path */` |
|     254 | 4837 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     186 | 4838 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     126 | 4839 | `				pLast = pGen->pIn;` |
|     126 | 4840 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      62 | 4841 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 | 4842 | `				}` |
|     126 | 4843 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      62 | 4844 | `			}` |
|     186 | 4845 | `			pGen->pIn++;` |
|       2 | 4846 | `		}` |
|      70 | 4847 | `		if( pLast == 0 ){` |
|       - | 4848 | `			/* Empty path */` |
|       5 | 4849 | `			break;` |
|       - | 4850 | `		}` |
|       - | 4851 | `		/* Default alias is the last component of the path */` |
|      66 | 4852 | `		sAlias = pLast->sData;` |
|       - | 4853 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      64 | 4854 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      42 | 4855 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      18 | 4856 | `			pGen->pIn++; /* Jump 'as' */` |
|      18 | 4857 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      18 | 4858 | `				sAlias = pGen->pIn->sData;` |
|      18 | 4859 | `				pGen->pIn++;` |
|       8 | 4860 | `			}` |
|       8 | 4861 | `		}` |
|       - | 4862 | `		/* Check for duplicate import alias (per-type) */` |
|      66 | 4863 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       7 | 4864 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 4865 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 | 4866 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       5 | 4867 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4868 | `				SyBlobRelease(&sPath);` |
|     ! 0 | 4869 | `				return SXERR_ABORT;` |
|       - | 4870 | `			}` |
|       2 | 4871 | `		}` |
|       - | 4872 | `		/* Register the import: alias -> FQN.` |
|       - | 4873 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - | 4874 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - | 4875 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      98 | 4876 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      64 | 4877 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      66 | 4878 | `		if( zDup ){` |
|      66 | 4879 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      66 | 4880 | `			if( pVmHash ){` |
|       - | 4881 | `				/* Class imports: populate VM table directly (class resolution` |
|       - | 4882 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      38 | 4883 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      38 | 4884 | `				if( zAliasDup ){` |
|      38 | 4885 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      18 | 4886 | `				}` |
|      18 | 4887 | `			}` |
|      66 | 4888 | `			if( iUseType == 2 ){` |
|       - | 4889 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - | 4890 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 | 4891 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 | 4892 | `				if( zAliasDup ){` |
|       - | 4893 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - | 4894 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - | 4895 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 | 4896 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 | 4897 | `					if( azPair ){` |
|      16 | 4898 | `						azPair[0] = zAliasDup;` |
|      16 | 4899 | `						azPair[1] = zDup;` |
|      16 | 4900 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 | 4901 | `					}` |
|       7 | 4902 | `				}` |
|       7 | 4903 | `			}` |
|      32 | 4904 | `		}` |
|       - | 4905 | `		/* Check for comma (multiple use declarations) */` |
|      66 | 4906 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 4907 | `			pGen->pIn++;` |
|       2 | 4908 | `		}else{` |
|      33 | 4909 | `			break;` |
|       - | 4910 | `		}` |
|       1 | 4911 | `	}` |
|      68 | 4912 | `	SyBlobRelease(&sPath);` |
|      68 | 4913 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 | 4914 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 | 4915 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 | 4916 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4917 | `			return SXERR_ABORT;` |
|       - | 4918 | `		}` |
|       1 | 4919 | `	}` |
|      68 | 4920 | `	return SXRET_OK;` |
|      35 | 4921 |  |
|       - | 4922 | `/*` |
|       - | 4923 | ` * Compile the stupid 'declare' language construct.` |
|       - | 4924 | ` *` |
|       - | 4925 | ` * According to the PHP language reference manual.` |
|       - | 4926 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - | 4927 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - | 4928 | ` *  declare (directive)` |
|       - | 4929 | ` *   statement` |
|       - | 4930 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - | 4931 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - | 4932 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - | 4933 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - | 4934 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - | 4935 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - | 4936 | ` * <?php` |
|       - | 4937 | ` * // these are the same:` |
|       - | 4938 | ` * // you can use this:` |
|       - | 4939 | ` * declare(ticks=1) {` |
|       - | 4940 | ` *   // entire script here` |
|       - | 4941 | ` * }` |
|       - | 4942 | ` * // or you can use this:` |
|       - | 4943 | ` * declare(ticks=1);` |
|       - | 4944 | ` * // entire script here` |
|       - | 4945 | ` * ?>` |
|       - | 4946 | ` *` |
|       - | 4947 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - | 4948 | ` */` |
|       8 | 4949 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       1 | 4950 |  |
|       9 | 4951 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       9 | 4952 | `	SyToken *pEnd = 0; /* cc warning */` |
|       - | 4953 | `	sxi32 rc;` |
|       9 | 4954 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       9 | 4955 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 | 4956 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 | 4957 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4958 | `			return SXERR_ABORT;` |
|       - | 4959 | `		}` |
|       5 | 4960 | `		goto Synchro;` |
|       - | 4961 | `	}` |
|       5 | 4962 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       - | 4963 | `	/* Delimit the directive */` |
|       5 | 4964 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);` |
|       5 | 4965 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 4966 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 | 4967 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4968 | `			return SXERR_ABORT;` |
|       - | 4969 | `		}` |
|     ! 0 | 4970 | `		return SXRET_OK;` |
|       - | 4971 | `	}` |
|       - | 4972 | `	/* Update the cursor */` |
|       5 | 4973 | `	pGen->pIn = &pEnd[1];` |
|       5 | 4974 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0  ){` |
|     ! 0 | 4975 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 | 4976 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4977 | `			return SXERR_ABORT;` |
|       - | 4978 | `		}` |
|     ! 0 | 4979 | `	}` |
|       - | 4980 | `	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */` |
|       7 | 4981 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */` |
|       - | 4982 | `		"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 | 4983 | `		ph7_lib_version()` |
|       - | 4984 | `		);` |
|       - | 4985 | `	/*All done */` |
|       5 | 4986 | `	return SXRET_OK;` |
|       2 | 4987 | `Synchro:` |
|       - | 4988 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 | 4989 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 | 4990 | `		pGen->pIn++;` |
|       1 | 4991 | `	}` |
|       5 | 4992 | `	return SXRET_OK;` |
|       5 | 4993 |  |
|       - | 4994 | `/*` |
|       - | 4995 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - | 4996 | ` * as follows:` |
|       - | 4997 | ` * function makecoffee($type = "cappuccino")` |
|       - | 4998 | ` * {` |
|       - | 4999 | ` *   return "Making a cup of $type.\n";` |
|       - | 5000 | ` * }` |
|       - | 5001 | ` * Symisc eXtension.` |
|       - | 5002 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - | 5003 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - | 5004 | ` *      Example: Work only with PH7,generate error under zend` |
|       - | 5005 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - | 5006 | ` *      {` |
|       - | 5007 | ` *       var_dump($a);` |
|       - | 5008 | ` *      }` |
|       - | 5009 | ` *     //call test without args` |
|       - | 5010 | ` *      test();` |
|       - | 5011 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - | 5012 | ` *      Example:` |
|       - | 5013 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - | 5014 | ` * 3 -) Function overloading!!` |
|       - | 5015 | ` *      Example:` |
|       - | 5016 | ` *      function foo($a) {` |
|       - | 5017 | ` *   	  return $a.PHP_EOL;` |
|       - | 5018 | ` *	    }` |
|       - | 5019 | ` *	    function foo($a, $b) {` |
|       - | 5020 | ` *   	  return $a + $b;` |
|       - | 5021 | ` *	    }` |
|       - | 5022 | ` *	    echo foo(5); // Prints "5"` |
|       - | 5023 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - | 5024 | ` *      // Same arg` |
|       - | 5025 | ` *	   function foo(string $a)` |
|       - | 5026 | ` *	   {` |
|       - | 5027 | ` *	     echo "a is a string\n";` |
|       - | 5028 | ` *	     var_dump($a);` |
|       - | 5029 | ` *	   }` |
|       - | 5030 | ` *	  function foo(int $a)` |
|       - | 5031 | ` *	  {` |
|       - | 5032 | ` *	    echo "a is integer\n";` |
|       - | 5033 | ` *	    var_dump($a);` |
|       - | 5034 | ` *	  }` |
|       - | 5035 | ` *	  function foo(array $a)` |
|       - | 5036 | ` *	  {` |
|       - | 5037 | ` * 	    echo "a is an array\n";` |
|       - | 5038 | ` * 	    var_dump($a);` |
|       - | 5039 | ` *	  }` |
|       - | 5040 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - | 5041 | ` *	  foo(52); // a is integer [second foo]` |
|       - | 5042 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - | 5043 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - | 5044 | ` * introduced by the PH7 engine.` |
|       - | 5045 | ` */` |
|   43156 | 5046 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 | 5047 |  |
|       - | 5048 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 5049 | `	SySet *pInstrContainer;` |
|       - | 5050 | `	sxi32 rc;` |
|       - | 5051 | `	/* Swap token stream */` |
|   43158 | 5052 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   43158 | 5053 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   43158 | 5054 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - | 5055 | `	/* Compile the expression holding the argument value */` |
|   43158 | 5056 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 5057 | `	/* Emit the done instruction */` |
|   43158 | 5058 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   43158 | 5059 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   43158 | 5060 | `	RE_SWAP_DELIMITER(pGen);` |
|   43158 | 5061 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5062 | `		return SXERR_ABORT;` |
|       - | 5063 | `	}` |
|   43158 | 5064 | `	return SXRET_OK;` |
|   21580 | 5065 |  |
|       - | 5066 | `/*` |
|       - | 5067 | ` * Collect function arguments one after one.` |
|       - | 5068 | ` * According to the PHP language reference manual.` |
|       - | 5069 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - | 5070 | ` * list of expressions.` |
|       - | 5071 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - | 5072 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - | 5073 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - | 5074 | ` * for more information.` |
|       - | 5075 | ` * Example #1 Passing arrays to functions` |
|       - | 5076 | ` * <?php` |
|       - | 5077 | ` * function takes_array($input)` |
|       - | 5078 | ` * {` |
|       - | 5079 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - | 5080 | ` * }` |
|       - | 5081 | ` * ?>` |
|       - | 5082 | ` * Making arguments be passed by reference` |
|       - | 5083 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - | 5084 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - | 5085 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - | 5086 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - | 5087 | ` * to the argument name in the function definition:` |
|       - | 5088 | ` * Example #2 Passing function parameters by reference` |
|       - | 5089 | ` * <?php` |
|       - | 5090 | ` * function add_some_extra(&$string)` |
|       - | 5091 | ` * {` |
|       - | 5092 | ` *   $string .= 'and something extra.';` |
|       - | 5093 | ` * }` |
|       - | 5094 | ` * $str = 'This is a string, ';` |
|       - | 5095 | ` * add_some_extra($str);` |
|       - | 5096 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - | 5097 | ` * ?>` |
|       - | 5098 | ` *` |
|       - | 5099 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - | 5100 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - | 5101 | ` * on these extension.` |
|       - | 5102 | ` */` |
|   51882 | 5103 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)` |
|       2 | 5104 |  |
|       - | 5105 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - | 5106 | `	SyToken *pIn;  /* Token stream */` |
|       - | 5107 | `	SyBlob sSig;         /* Function signature */` |
|       - | 5108 | `	char *zDup;          /* Copy of argument name */` |
|       - | 5109 | `	sxi32 rc;` |
|       - | 5110 |  |
|   51884 | 5111 | `	pIn = pGen->pIn;` |
|   51884 | 5112 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - | 5113 | `	/* Process arguments one after one */` |
|   65627 | 5114 | `	for(;;){` |
|  131256 | 5115 | `		if( pIn >= pEnd ){` |
|       - | 5116 | `			/* No more arguments to process */` |
|   51882 | 5117 | `			break;` |
|       - | 5118 | `		}` |
|   79376 | 5119 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   79376 | 5120 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 5121 | `		/* Detect nullable prefix '?' on type hints */` |
|   79376 | 5122 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|      16 | 5123 | `			sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|      16 | 5124 | `			pIn++;` |
|       7 | 5125 | `		}` |
|       - | 5126 | `		/* Skip leading namespace separator '\' on FQN type hints like \Throwable */` |
|   79376 | 5127 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       5 | 5128 | `			pIn++;` |
|       2 | 5129 | `		}` |
|   79376 | 5130 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|   53976 | 5131 | `			if( pIn->nType & PH7_TK_KEYWORD ){` |
|   48576 | 5132 | `				sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   48576 | 5133 | `				if( nKey & PH7_TKWRD_ARRAY ){` |
|     ! 0 | 5134 | `					sArg.nType = MEMOBJ_HASHMAP;` |
|   48576 | 5135 | `				}else if( nKey & PH7_TKWRD_BOOL ){` |
|     ! 0 | 5136 | `					sArg.nType = MEMOBJ_BOOL;` |
|   48576 | 5137 | `				}else if( nKey & PH7_TKWRD_INT ){` |
|   13494 | 5138 | `					sArg.nType = MEMOBJ_INT;` |
|   41830 | 5139 | `				}else if( nKey & PH7_TKWRD_STRING ){` |
|   35068 | 5140 | `					sArg.nType = MEMOBJ_STRING;` |
|   17551 | 5141 | `				}else if( nKey & PH7_TKWRD_FLOAT ){` |
|     ! 0 | 5142 | `					sArg.nType = MEMOBJ_REAL;` |
|      18 | 5143 | `				}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      16 | 5144 | `					sArg.nType = MEMOBJ_OBJ;` |
|       9 | 5145 | `				}else{` |
|       4 | 5146 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,` |
|       - | 5147 | `						"Invalid argument type '%z',Automatic cast will not be performed",` |
|       1 | 5148 | `						&pIn->sData);` |
|       - | 5149 | `				}` |
|   24289 | 5150 | `			}else{` |
|    5402 | 5151 | `				SyString *pName = &pIn->sData; /* Class name */` |
|       - | 5152 | `				char *zDupLocal;` |
|       - | 5153 | `				/* Argument must be a class instance,record that*/` |
|    5402 | 5154 | `				zDupLocal = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    5402 | 5155 | `				if( zDupLocal ){` |
|    5402 | 5156 | `					sArg.nType = SXU32_HIGH; /* 0xFFFFFFFF as sentinel */` |
|    5402 | 5157 | `					SyStringInitFromBuf(&sArg.sClass,zDupLocal,pName->nByte);` |
|    2700 | 5158 | `				}` |
|       - | 5159 | `			}` |
|   53976 | 5160 | `			pIn++;` |
|   26987 | 5161 | `		}` |
|   79376 | 5162 | `		if( pIn >= pEnd ){` |
|     ! 0 | 5163 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 | 5164 | `			return rc;` |
|       - | 5165 | `		}` |
|   79376 | 5166 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - | 5167 | `			/* Pass by reference,record that */` |
|    2722 | 5168 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    2722 | 5169 | `			pIn++;` |
|    1360 | 5170 | `		}` |
|   79376 | 5171 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - | 5172 | `			/* Variadic parameter: ...$args */` |
|      30 | 5173 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      30 | 5174 | `			pIn++;` |
|      14 | 5175 | `		}` |
|   79376 | 5176 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 5177 | `			/* Invalid argument */` |
|     ! 0 | 5178 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 | 5179 | `			return rc;` |
|       - | 5180 | `		}` |
|   79376 | 5181 | `		pIn++; /* Jump the dollar sign */` |
|       - | 5182 | `		/* Copy argument name */` |
|   79376 | 5183 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   79376 | 5184 | `		if( zDup == 0 ){` |
|     ! 0 | 5185 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 | 5186 | `			return SXERR_ABORT;` |
|       - | 5187 | `		}` |
|   79376 | 5188 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   79376 | 5189 | `		pIn++;` |
|   79376 | 5190 | `		if( pIn < pEnd ){` |
|   49086 | 5191 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - | 5192 | `				SyToken *pDefend;` |
|   43160 | 5193 | `				sxi32 iNest = 0;` |
|   43160 | 5194 | `				pIn++; /* Jump the equal sign */` |
|   43160 | 5195 | `				pDefend = pIn;` |
|       - | 5196 | `				/* Process the default value associated with this argument */` |
|   91708 | 5197 | `				while( pDefend < pEnd ){` |
|   70120 | 5198 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   21572 | 5199 | `						break;` |
|       - | 5200 | `					}` |
|   48550 | 5201 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - | 5202 | `						/* Increment nesting level */` |
|    2698 | 5203 | `						iNest++;` |
|   47202 | 5204 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - | 5205 | `						/* Decrement nesting level */` |
|    2698 | 5206 | `						iNest--;` |
|    1348 | 5207 | `					}` |
|   48550 | 5208 | `					pDefend++;` |
|       2 | 5209 | `				}` |
|   43160 | 5210 | `				if( pIn >= pDefend ){` |
|       3 | 5211 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 | 5212 | `					return rc;` |
|       - | 5213 | `				}` |
|       - | 5214 | `				/* Process default value */` |
|   43158 | 5215 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   43158 | 5216 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 5217 | `					return rc;` |
|       - | 5218 | `				}` |
|       - | 5219 | `				/* Point beyond the default value */` |
|   43158 | 5220 | `				pIn = pDefend;` |
|   21578 | 5221 | `			}` |
|   49084 | 5222 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 5223 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 | 5224 | `				return rc;` |
|       - | 5225 | `			}` |
|   49084 | 5226 | `			pIn++; /* Jump the trailing comma */` |
|   24541 | 5227 | `		}` |
|       - | 5228 | `		/* Append argument signature */` |
|   79374 | 5229 | `		if( sArg.nType > 0 ){` |
|   53974 | 5230 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - | 5231 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    5402 | 5232 | `				int marker = 'o';` |
|    5402 | 5233 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    5402 | 5234 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    2702 | 5235 | `			}else{` |
|       - | 5236 | `				int c;` |
|   48574 | 5237 | `				c = 'n'; /* cc warning */` |
|       - | 5238 | `				/* Type leading character */` |
|   48574 | 5239 | `				switch(sArg.nType){` |
|     ! 0 | 5240 | `				case MEMOBJ_HASHMAP:` |
|       - | 5241 | `					/* Hashmap aka 'array' */` |
|     ! 0 | 5242 | `					c = 'h';` |
|     ! 0 | 5243 | `					break;` |
|    6746 | 5244 | `				case MEMOBJ_INT:` |
|       - | 5245 | `					/* Integer */` |
|   13494 | 5246 | `					c = 'i';` |
|   13494 | 5247 | `					break;` |
|     ! 0 | 5248 | `				case MEMOBJ_BOOL:` |
|       - | 5249 | `					/* Bool */` |
|     ! 0 | 5250 | `					c = 'b';` |
|     ! 0 | 5251 | `					break;` |
|     ! 0 | 5252 | `				case MEMOBJ_REAL:` |
|       - | 5253 | `					/* Float */` |
|     ! 0 | 5254 | `					c = 'f';` |
|     ! 0 | 5255 | `					break;` |
|   17533 | 5256 | `				case MEMOBJ_STRING:` |
|       - | 5257 | `					/* String */` |
|   35068 | 5258 | `					c = 's';` |
|   35068 | 5259 | `					break;` |
|       7 | 5260 | `				case MEMOBJ_OBJ:` |
|       - | 5261 | `					/* Object */` |
|      16 | 5262 | `					c = 'o';` |
|      14 | 5263 | `					break;` |
|     ! 0 | 5264 | `				default:` |
|     ! 0 | 5265 | `					break;` |
|       - | 5266 | `				}` |
|   48574 | 5267 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - | 5268 | `			}` |
|   26988 | 5269 | `		}else{` |
|       - | 5270 | `			/* No type is associated with this parameter which mean` |
|       - | 5271 | `			 * that this function is not condidate for overloading.` |
|       - | 5272 | `			 */` |
|   25402 | 5273 | `			SyBlobRelease(&sSig);` |
|       - | 5274 | `		}` |
|       - | 5275 | `		/* Save in the argument set */` |
|   79374 | 5276 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 | 5277 | `	}` |
|   51882 | 5278 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - | 5279 | `		/* Save function signature */` |
|   32400 | 5280 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   16199 | 5281 | `	}` |
|   51882 | 5282 | `	return SXRET_OK;` |
|   25943 | 5283 |  |
|       - | 5284 | `/*` |
|       - | 5285 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - | 5286 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 5287 | ` * and this routine takes care of generating the appropriate error message.` |
|       - | 5288 | ` */` |
|  144030 | 5289 | `static sxi32 GenStateCompileFuncBody(` |
|       - | 5290 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 5291 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - | 5292 | `	)` |
|       2 | 5293 |  |
|       - | 5294 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - | 5295 | `	GenBlock *pBlock;` |
|       - | 5296 | `	sxu32 nGotoOfft;` |
|       - | 5297 | `	sxi32 rc;` |
|       - | 5298 | `	/* Attach the new function */` |
|  144032 | 5299 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  144032 | 5300 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5301 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - | 5302 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 5303 | `		return SXERR_ABORT;` |
|       - | 5304 | `	}` |
|  144032 | 5305 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - | 5306 | `	/* Swap bytecode containers */` |
|  144032 | 5307 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  144032 | 5308 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - | 5309 | `	/* Compile the body */` |
|  144032 | 5310 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 5311 | `	/* Fix exception jumps now the destination is resolved */` |
|  144032 | 5312 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 5313 | `	/* Emit the final return if not yet done */` |
|  144032 | 5314 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 5315 | `	/* Fix gotos jumps now the destination is resolved */` |
|  144032 | 5316 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 | 5317 | `		rc = SXERR_ABORT;` |
|     ! 0 | 5318 | `	}` |
|  144032 | 5319 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - | 5320 | `	/* Restore the default container */` |
|  144032 | 5321 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 5322 | `	/* Leave function block */` |
|  144032 | 5323 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  144032 | 5324 | `	if( rc == SXERR_ABORT ){` |
|       - | 5325 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 5326 | `		return SXERR_ABORT;` |
|       - | 5327 | `	}` |
|       - | 5328 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - | 5329 | `	{` |
|  144032 | 5330 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - | 5331 | `		sxu32 i;` |
| 2990286 | 5332 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 2846272 | 5333 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      18 | 5334 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      18 | 5335 | `				break;` |
|       - | 5336 | `			}` |
| 1423129 | 5337 | `		}` |
|       - | 5338 | `	}` |
|       - | 5339 | `	/* All done, function body compiled */` |
|  144032 | 5340 | `	return SXRET_OK;` |
|   72017 | 5341 |  |
|       - | 5342 | `/*` |
|       - | 5343 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - | 5344 | ` * According to the PHP language reference manual.` |
|       - | 5345 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - | 5346 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - | 5347 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - | 5348 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 5349 | ` *  Functions need not be defined before they are referenced.` |
|       - | 5350 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - | 5351 | ` *  a function even if they were defined inside and vice versa.` |
|       - | 5352 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - | 5353 | ` *  calls with over 32-64 recursion levels.` |
|       - | 5354 | ` *` |
|       - | 5355 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - | 5356 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - | 5357 | ` * on these extension.` |
|       - | 5358 | ` */` |
|       - | 5359 | `/*` |
|       - | 5360 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - | 5361 | ` */` |
|       6 | 5362 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       1 | 5363 |  |
|       - | 5364 | `	sxu32 i;` |
|      31 | 5365 | `	for( i = 0; i < n; i++ ){` |
|      25 | 5366 | `		int a = zA[i], b = zB[i];` |
|      25 | 5367 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      25 | 5368 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      25 | 5369 | `		if( a != b ) return a - b;` |
|      13 | 5370 | `	}` |
|       7 | 5371 | `	return 0;` |
|       4 | 5372 |  |
|       - | 5373 | `/*` |
|       - | 5374 | ` * Helper: set the return type to a class/self/parent/static sentinel.` |
|       - | 5375 | ` */` |
|       2 | 5376 | `static void GenStateSetReturnClass(ph7_gen_state *pGen, ph7_vm_func *pFunc, const char *zName, sxu32 nByte)` |
|       1 | 5377 |  |
|       3 | 5378 | `	char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator, zName, nByte);` |
|       3 | 5379 | `	if( zDup ){` |
|       3 | 5380 | `		pFunc->nReturnType = SXU32_HIGH;` |
|       3 | 5381 | `		SyStringInitFromBuf(&pFunc->sReturnClass, zDup, nByte);` |
|       1 | 5382 | `	}` |
|       3 | 5383 |  |
|       - | 5384 | `/*` |
|       - | 5385 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - | 5386 | `` * pGen->pIn should point to the token after `)`.`` |
|       - | 5387 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - | 5388 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - | 5389 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, and nullable `: ?type`.`` |
|       - | 5390 | ` */` |
|  165726 | 5391 | `static void GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       2 | 5392 |  |
|  165728 | 5393 | `	SyToken *pCur = pGen->pIn;` |
|  165728 | 5394 | `	pFunc->nReturnType = 0;` |
|  165728 | 5395 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  165728 | 5396 | `	if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_COLON) == 0 ){` |
|  165658 | 5397 | `		return; /* No return type */` |
|       - | 5398 | `	}` |
|      72 | 5399 | `	pCur++; /* Skip ':' */` |
|      72 | 5400 | `	if( pCur >= pGen->pEnd ){` |
|     ! 0 | 5401 | `		pGen->pIn = pCur;` |
|     ! 0 | 5402 | `		return;` |
|       - | 5403 | `	}` |
|       - | 5404 | `	/* Handle nullable prefix '?' (tokenized as PH7_TK_OP with '?' operator) */` |
|      72 | 5405 | `	if( (pCur->nType & PH7_TK_OP) && pCur->sData.nByte == 1 && pCur->sData.zString[0] == '?' ){` |
|      10 | 5406 | `		pCur++;` |
|      10 | 5407 | `		if( pCur >= pGen->pEnd ){` |
|     ! 0 | 5408 | `			pGen->pIn = pCur;` |
|     ! 0 | 5409 | `			return;` |
|       - | 5410 | `		}` |
|       4 | 5411 | `	}` |
|      72 | 5412 | `	if( pCur->nType & PH7_TK_KEYWORD ){` |
|      66 | 5413 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pCur->pUserData));` |
|      66 | 5414 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|       3 | 5415 | `			pFunc->nReturnType = MEMOBJ_HASHMAP;` |
|      65 | 5416 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       3 | 5417 | `			pFunc->nReturnType = MEMOBJ_BOOL;` |
|      63 | 5418 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|      24 | 5419 | `			pFunc->nReturnType = MEMOBJ_INT;` |
|      51 | 5420 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|      34 | 5421 | `			pFunc->nReturnType = MEMOBJ_STRING;` |
|      24 | 5422 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|       3 | 5423 | `			pFunc->nReturnType = MEMOBJ_REAL;` |
|       7 | 5424 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|       3 | 5425 | `			pFunc->nReturnType = MEMOBJ_OBJ;` |
|       4 | 5426 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT \|\| nKey == PH7_TKWRD_STATIC ){` |
|       - | 5427 | `			/* self/parent/static — store as class sentinel */` |
|       3 | 5428 | `			GenStateSetReturnClass(pGen, pFunc, pCur->sData.zString, pCur->sData.nByte);` |
|       1 | 5429 | `		}` |
|      66 | 5430 | `		pCur++;` |
|      39 | 5431 | `	}else if( pCur->nType & PH7_TK_ID ){` |
|       7 | 5432 | `		SyString *pType = &pCur->sData;` |
|       7 | 5433 | `		if( pType->nByte == 4 && SyMemcmpNoCase(pType->zString, "void", 4) == 0 ){` |
|       7 | 5434 | `			pFunc->nReturnType = MEMOBJ_VOID;` |
|       4 | 5435 | `		}else{` |
|       - | 5436 | `			/* Class/interface name */` |
|     ! 0 | 5437 | `			GenStateSetReturnClass(pGen, pFunc, pType->zString, pType->nByte);` |
|       - | 5438 | `		}` |
|       7 | 5439 | `		pCur++;` |
|       3 | 5440 | `	}` |
|      72 | 5441 | `	pGen->pIn = pCur;` |
|   82865 | 5442 |  |
|       - | 5443 |  |
|   35760 | 5444 | `static sxi32 GenStateCompileFunc(` |
|       - | 5445 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 5446 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - | 5447 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 5448 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - | 5449 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - | 5450 | `	)` |
|       2 | 5451 |  |
|       - | 5452 | `	ph7_vm_func *pFunc;` |
|       - | 5453 | `	SyToken *pEnd;` |
|       - | 5454 | `	sxu32 nLine;` |
|       - | 5455 | `	char *zName;` |
|       - | 5456 | `	sxi32 rc;` |
|       - | 5457 | `	/* Extract line number */` |
|   35762 | 5458 | `	nLine = pGen->pIn->nLine;` |
|       - | 5459 | `	/* Jump the left parenthesis '(' */` |
|   35762 | 5460 | `	pGen->pIn++;` |
|       - | 5461 | `	/* Delimit the function signature */` |
|   35762 | 5462 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   35762 | 5463 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5464 | `		/* Syntax error */` |
|       7 | 5465 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 | 5466 | `		if( rc == SXERR_ABORT ){` |
|       - | 5467 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5468 | `			return SXERR_ABORT;` |
|       - | 5469 | `		}` |
|       7 | 5470 | `		pGen->pIn = pGen->pEnd;` |
|       7 | 5471 | `		return SXRET_OK;` |
|       - | 5472 | `	}` |
|       - | 5473 | `	/* Create the function state */` |
|   35756 | 5474 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   35756 | 5475 | `	if( pFunc == 0 ){` |
|     ! 0 | 5476 | `		goto OutOfMem;` |
|       - | 5477 | `	}` |
|       - | 5478 | `	/* Build the function name, prepending namespace if active */` |
|   35763 | 5479 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - | 5480 | `		SyBlob sFQN;` |
|       - | 5481 | `		sxu32 nLen;` |
|      16 | 5482 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 | 5483 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 | 5484 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 | 5485 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 | 5486 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 | 5487 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 | 5488 | `		SyBlobRelease(&sFQN);` |
|      16 | 5489 | `		if( zName == 0 ){` |
|     ! 0 | 5490 | `			goto OutOfMem;` |
|       - | 5491 | `		}` |
|      16 | 5492 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 | 5493 | `	}else{` |
|   35742 | 5494 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   35742 | 5495 | `		if( zName == 0 ){` |
|     ! 0 | 5496 | `			goto OutOfMem;` |
|       - | 5497 | `		}` |
|   35742 | 5498 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - | 5499 | `	}` |
|   35756 | 5500 | `	if( pGen->pIn < pEnd ){` |
|       - | 5501 | `		/* Collect function arguments */` |
|   24786 | 5502 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);` |
|   24786 | 5503 | `		if( rc == SXERR_ABORT ){` |
|       - | 5504 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 5505 | `			return SXERR_ABORT;` |
|       - | 5506 | `		}` |
|   12392 | 5507 | `	}` |
|       - | 5508 | `	/* Point past ')' and parse optional return type ': type' */` |
|   35756 | 5509 | `	pGen->pIn = &pEnd[1];` |
|   35756 | 5510 | `	GenStateParseReturnType(pGen, pFunc);` |
|   35756 | 5511 | `	if( bHandleClosure ){` |
|       - | 5512 | `		ph7_vm_func_closure_env sEnv;` |
|     170 | 5513 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     168 | 5514 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      93 | 5515 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      16 | 5516 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 5517 | `				/* Closure,record environment variable */` |
|      16 | 5518 | `				pGen->pIn++;` |
|      16 | 5519 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 5520 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 | 5521 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5522 | `						return SXERR_ABORT;` |
|       - | 5523 | `					}` |
|     ! 0 | 5524 | `				}` |
|      16 | 5525 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - | 5526 | `				/* Compile until we hit the first closing parenthesis */` |
|      34 | 5527 | `				while( pGen->pIn < pGen->pEnd ){` |
|      34 | 5528 | `					int iFlagsLocal = 0;` |
|      34 | 5529 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      16 | 5530 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      16 | 5531 | `						break;` |
|       - | 5532 | `					}` |
|      20 | 5533 | `					nLineLocal = pGen->pIn->nLine;` |
|      20 | 5534 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - | 5535 | `						/* Pass by reference,record that */` |
|     ! 0 | 5536 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - | 5537 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - | 5538 | `							);` |
|     ! 0 | 5539 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 | 5540 | `						pGen->pIn++;` |
|     ! 0 | 5541 | `					}` |
|      18 | 5542 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      20 | 5543 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 5544 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - | 5545 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 | 5546 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 5547 | `								return SXERR_ABORT;` |
|       - | 5548 | `							}` |
|       - | 5549 | `							/* Find the closing parenthesis */` |
|     ! 0 | 5550 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 5551 | `								pGen->pIn++;` |
|     ! 0 | 5552 | `							}` |
|     ! 0 | 5553 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 | 5554 | `								pGen->pIn++;` |
|     ! 0 | 5555 | `							}` |
|     ! 0 | 5556 | `							break;` |
|       - | 5557 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 | 5558 | `					}else{` |
|       - | 5559 | `						SyString *pNameLocal;` |
|       - | 5560 | `						char *zDup;` |
|       - | 5561 | `						/* Duplicate variable name */` |
|      20 | 5562 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      20 | 5563 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      20 | 5564 | `						if( zDup ){` |
|       - | 5565 | `							/* Zero the structure */` |
|      20 | 5566 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      20 | 5567 | `							sEnv.iFlags = iFlagsLocal;` |
|      20 | 5568 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      20 | 5569 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      20 | 5570 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 | 5571 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 | 5572 | `									got_this = 1;` |
|     ! 0 | 5573 | `							}` |
|       - | 5574 | `							/* Save imported variable */` |
|      20 | 5575 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      11 | 5576 | `						}else{` |
|     ! 0 | 5577 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5578 | `							 return SXERR_ABORT;` |
|       - | 5579 | `						}` |
|       - | 5580 | `					}` |
|      20 | 5581 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      26 | 5582 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 5583 | `						/* Ignore trailing commas */` |
|       7 | 5584 | `						pGen->pIn++;` |
|       1 | 5585 | `					}` |
|       2 | 5586 | `				}` |
|      16 | 5587 | `				if( !got_this ){` |
|       - | 5588 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - | 5589 | `					 * available to the closure environment.` |
|       - | 5590 | `					 */` |
|      16 | 5591 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      16 | 5592 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      16 | 5593 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      16 | 5594 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      16 | 5595 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       7 | 5596 | `				}` |
|      16 | 5597 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - | 5598 | `					/* Mark as closure */` |
|      16 | 5599 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       7 | 5600 | `				}` |
|       7 | 5601 | `		}` |
|      84 | 5602 | `	}` |
|       - | 5603 | `	/* Compile the body */` |
|   35756 | 5604 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   35756 | 5605 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5606 | `		return SXERR_ABORT;` |
|       - | 5607 | `	}` |
|   35756 | 5608 | `	if( ppFunc ){` |
|     170 | 5609 | `		*ppFunc = pFunc;` |
|      84 | 5610 | `	}` |
|   35756 | 5611 | `	rc = SXRET_OK;` |
|   35756 | 5612 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - | 5613 | `		/* Finally register the function */` |
|   35742 | 5614 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   17870 | 5615 | `	}` |
|   35756 | 5616 | `	if( rc == SXRET_OK ){` |
|   35756 | 5617 | `		return SXRET_OK;` |
|       - | 5618 | `	}` |
|       - | 5619 | `	/* Fall through if something goes wrong */` |
|     ! 0 | 5620 | `OutOfMem:` |
|       - | 5621 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - | 5622 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - | 5623 | `	 */` |
|     ! 0 | 5624 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 5625 | `	return SXERR_ABORT;` |
|   17882 | 5626 |  |
|       - | 5627 | `/*` |
|       - | 5628 | ` * Compile a standard PHP function.` |
|       - | 5629 | ` *  Refer to the block-comment above for more information.` |
|       - | 5630 | ` */` |
|   35598 | 5631 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 | 5632 |  |
|       - | 5633 | `	SyString *pName;` |
|       - | 5634 | `	sxi32 iFlags;` |
|       - | 5635 | `	sxu32 nLine;` |
|       - | 5636 | `	sxi32 rc;` |
|       - | 5637 |  |
|   35600 | 5638 | `	nLine = pGen->pIn->nLine;` |
|   35600 | 5639 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   35600 | 5640 | `	iFlags = 0;` |
|   35600 | 5641 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 5642 | `		/* Return by reference,remember that */` |
|       7 | 5643 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 5644 | `		/* Jump the '&' token */` |
|       7 | 5645 | `		pGen->pIn++;` |
|       3 | 5646 | `	}` |
|   35600 | 5647 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 5648 | `		/* Invalid function name */` |
|       5 | 5649 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 | 5650 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5651 | `			return SXERR_ABORT;` |
|       - | 5652 | `		}` |
|       - | 5653 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 | 5654 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 | 5655 | `			pGen->pIn++;` |
|       1 | 5656 | `		}` |
|       5 | 5657 | `		return SXRET_OK;` |
|       - | 5658 | `	}` |
|   35596 | 5659 | `	pName = &pGen->pIn->sData;` |
|   35596 | 5660 | `	nLine = pGen->pIn->nLine;` |
|       - | 5661 | `	/* Jump the function name */` |
|   35596 | 5662 | `	pGen->pIn++;` |
|   35596 | 5663 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 5664 | `		/* Syntax error */` |
|       3 | 5665 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 | 5666 | `		if( rc == SXERR_ABORT ){` |
|       - | 5667 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5668 | `			return SXERR_ABORT;` |
|       - | 5669 | `		}` |
|       - | 5670 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 | 5671 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 5672 | `			pGen->pIn++;` |
|     ! 0 | 5673 | `		}` |
|       3 | 5674 | `		return SXRET_OK;` |
|       - | 5675 | `	}` |
|       - | 5676 | `	/* Compile function body */` |
|   35594 | 5677 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   35594 | 5678 | `	return rc;` |
|   17801 | 5679 |  |
|       - | 5680 | `/*` |
|       - | 5681 | ` * Extract the visibility level associated with a given keyword.` |
|       - | 5682 | ` * According to the PHP language reference manual` |
|       - | 5683 | ` *  Visibility:` |
|       - | 5684 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - | 5685 | ` *  the declaration with the keywords public, protected or private.` |
|       - | 5686 | ` *  Class members declared public can be accessed everywhere.` |
|       - | 5687 | ` *  Members declared protected can be accessed only within the class` |
|       - | 5688 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - | 5689 | ` *  may only be accessed by the class that defines the member.` |
|       - | 5690 | ` */` |
|  165200 | 5691 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 | 5692 |  |
|  165202 | 5693 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    8154 | 5694 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  157050 | 5695 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   18916 | 5696 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - | 5697 | `	}` |
|       - | 5698 | `	/* Assume public by default */` |
|  138136 | 5699 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   82602 | 5700 |  |
|       - | 5701 | `/*` |
|       - | 5702 | ` * Compile a class constant.` |
|       - | 5703 | ` * According to the PHP language reference manual` |
|       - | 5704 | ` *  Class Constants` |
|       - | 5705 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 5706 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 5707 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 5708 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 5709 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 5710 | ` *   It's also possible for interfaces to have constants.` |
|       - | 5711 | ` * Symisc eXtension.` |
|       - | 5712 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 5713 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 5714 | ` *  Example:` |
|       - | 5715 | ` *   class Test{` |
|       - | 5716 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 5717 | ` *   };` |
|       - | 5718 | ` *   var_dump(TEST::MyConst);` |
|       - | 5719 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 5720 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 5721 | ` */` |
|      30 | 5722 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 5723 |  |
|      32 | 5724 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5725 | `	SySet *pInstrContainer;` |
|       - | 5726 | `	ph7_class_attr *pCons;` |
|       - | 5727 | `	SyString *pName;` |
|       - | 5728 | `	sxi32 rc;` |
|       - | 5729 | `	/* Extract visibility level */` |
|      32 | 5730 | `	iProtection = GetProtectionLevel(iProtection);` |
|      32 | 5731 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      15 | 5732 | `loop:` |
|       - | 5733 | `	/* Mark as constant */` |
|      32 | 5734 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      32 | 5735 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5736 | `		/* Invalid constant name */` |
|     ! 0 | 5737 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 | 5738 | `		if( rc == SXERR_ABORT ){` |
|       - | 5739 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5740 | `			return SXERR_ABORT;` |
|       - | 5741 | `		}` |
|     ! 0 | 5742 | `		goto Synchronize;` |
|       - | 5743 | `	}` |
|       - | 5744 | `	/* Peek constant name */` |
|      32 | 5745 | `	pName = &pGen->pIn->sData;` |
|       - | 5746 | `	/* Make sure the constant name isn't reserved */` |
|      32 | 5747 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 5748 | `		/* Reserved constant name */` |
|     ! 0 | 5749 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 | 5750 | `		if( rc == SXERR_ABORT ){` |
|       - | 5751 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5752 | `			return SXERR_ABORT;` |
|       - | 5753 | `		}` |
|     ! 0 | 5754 | `		goto Synchronize;` |
|       - | 5755 | `	}` |
|       - | 5756 | `	/* Advance the stream cursor */` |
|      32 | 5757 | `	pGen->pIn++;` |
|      32 | 5758 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 5759 | `		/* Invalid declaration */` |
|     ! 0 | 5760 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 | 5761 | `		if( rc == SXERR_ABORT ){` |
|       - | 5762 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5763 | `			return SXERR_ABORT;` |
|       - | 5764 | `		}` |
|     ! 0 | 5765 | `		goto Synchronize;` |
|       - | 5766 | `	}` |
|      32 | 5767 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - | 5768 | `	/* Allocate a new class attribute */` |
|      32 | 5769 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      32 | 5770 | `	if( pCons == 0 ){` |
|     ! 0 | 5771 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5772 | `		return SXERR_ABORT;` |
|       - | 5773 | `	}` |
|       - | 5774 | `	/* Swap bytecode container */` |
|      32 | 5775 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 | 5776 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - | 5777 | `	/* Compile constant value.` |
|       - | 5778 | `	 */` |
|      32 | 5779 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      32 | 5780 | `	if( rc == SXERR_EMPTY ){` |
|       3 | 5781 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 | 5782 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5783 | `			return SXERR_ABORT;` |
|       - | 5784 | `		}` |
|       1 | 5785 | `	}` |
|       - | 5786 | `	/* Emit the done instruction */` |
|      32 | 5787 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      32 | 5788 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 | 5789 | `	if( rc == SXERR_ABORT ){` |
|       - | 5790 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 5791 | `		return SXERR_ABORT;` |
|       - | 5792 | `	}` |
|       - | 5793 | `	/* All done,install the constant */` |
|      32 | 5794 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      32 | 5795 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5796 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5797 | `		return SXERR_ABORT;` |
|       - | 5798 | `	}` |
|      32 | 5799 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 5800 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 | 5801 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 5802 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 5803 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 5804 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 5805 | `				pTok--;` |
|     ! 0 | 5806 | `			}` |
|     ! 0 | 5807 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5808 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 | 5809 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 5810 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5811 | `				return SXERR_ABORT;` |
|       - | 5812 | `			}` |
|     ! 0 | 5813 | `		}else{` |
|     ! 0 | 5814 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 | 5815 | `				goto loop;` |
|       - | 5816 | `			}` |
|       - | 5817 | `		}` |
|     ! 0 | 5818 | `	}` |
|      32 | 5819 | `	return SXRET_OK;` |
|     ! 0 | 5820 | `Synchronize:` |
|       - | 5821 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 5822 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 | 5823 | `		pGen->pIn++;` |
|     ! 0 | 5824 | `	}` |
|     ! 0 | 5825 | `	return SXERR_CORRUPT;` |
|      17 | 5826 |  |
|       - | 5827 | `/*` |
|       - | 5828 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - | 5829 | ` * According to the PHP language reference manual` |
|       - | 5830 | ` *  Properties` |
|       - | 5831 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - | 5832 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - | 5833 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - | 5834 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - | 5835 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - | 5836 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - | 5837 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - | 5838 | ` * Symisc eXtension.` |
|       - | 5839 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - | 5840 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 5841 | ` *  Example:` |
|       - | 5842 | ` *   class Test{` |
|       - | 5843 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 5844 | ` *   };` |
|       - | 5845 | ` *   var_dump(TEST::myVar);` |
|       - | 5846 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 5847 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 5848 | ` */` |
|   35276 | 5849 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 5850 |  |
|   35278 | 5851 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5852 | `	ph7_class_attr *pAttr;` |
|       - | 5853 | `	SyString *pName;` |
|       - | 5854 | `	sxi32 rc;` |
|       - | 5855 | `	/* Extract visibility level */` |
|   35278 | 5856 | `	iProtection = GetProtectionLevel(iProtection);` |
|   17638 | 5857 | `loop:` |
|   35278 | 5858 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   35278 | 5859 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - | 5860 | `		/* Invalid attribute name */` |
|     ! 0 | 5861 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 | 5862 | `		if( rc == SXERR_ABORT ){` |
|       - | 5863 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5864 | `			return SXERR_ABORT;` |
|       - | 5865 | `		}` |
|     ! 0 | 5866 | `		goto Synchronize;` |
|       - | 5867 | `	}` |
|       - | 5868 | `	/* Peek attribute name */` |
|   35278 | 5869 | `	pName = &pGen->pIn->sData;` |
|       - | 5870 | `	/* Advance the stream cursor */` |
|   35278 | 5871 | `	pGen->pIn++;` |
|   35278 | 5872 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - | 5873 | `		/* Invalid declaration */` |
|       3 | 5874 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 | 5875 | `		if( rc == SXERR_ABORT ){` |
|       - | 5876 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5877 | `			return SXERR_ABORT;` |
|       - | 5878 | `		}` |
|       3 | 5879 | `		goto Synchronize;` |
|       - | 5880 | `	}` |
|       - | 5881 | `	/* Allocate a new class attribute */` |
|   35276 | 5882 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|   35276 | 5883 | `	if( pAttr == 0 ){` |
|     ! 0 | 5884 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 5885 | `		return SXERR_ABORT;` |
|       - | 5886 | `	}` |
|   35276 | 5887 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - | 5888 | `		SySet *pInstrContainer;` |
|   10958 | 5889 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - | 5890 | `		/* Swap bytecode container */` |
|   10958 | 5891 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   10958 | 5892 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - | 5893 | `		/* Compile attribute value.` |
|       - | 5894 | `		 */` |
|   10958 | 5895 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   10958 | 5896 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 5897 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 | 5898 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5899 | `				return SXERR_ABORT;` |
|       - | 5900 | `			}` |
|     ! 0 | 5901 | `		}` |
|       - | 5902 | `		/* Emit the done instruction */` |
|   10958 | 5903 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   10958 | 5904 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    5478 | 5905 | `	}` |
|       - | 5906 | `	/* All done,install the attribute */` |
|   35276 | 5907 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   35276 | 5908 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5909 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5910 | `		return SXERR_ABORT;` |
|       - | 5911 | `	}` |
|   35276 | 5912 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 5913 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|     ! 0 | 5914 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 5915 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 5916 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 5917 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 5918 | `				pTok--;` |
|     ! 0 | 5919 | `			}` |
|     ! 0 | 5920 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5921 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5922 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 5923 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5924 | `				return SXERR_ABORT;` |
|       - | 5925 | `			}` |
|     ! 0 | 5926 | `		}else{` |
|     ! 0 | 5927 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 | 5928 | `				goto loop;` |
|       - | 5929 | `			}` |
|       - | 5930 | `		}` |
|     ! 0 | 5931 | `	}` |
|   35276 | 5932 | `	return SXRET_OK;` |
|       1 | 5933 | `Synchronize:` |
|       - | 5934 | `	/* Synchronize with the first semi-colon */` |
|       5 | 5935 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       3 | 5936 | `		pGen->pIn++;` |
|       1 | 5937 | `	}` |
|       3 | 5938 | `	return SXERR_CORRUPT;` |
|   17640 | 5939 |  |
|       - | 5940 | `/*` |
|       - | 5941 | ` * Compile a class method.` |
|       - | 5942 | ` *` |
|       - | 5943 | ` * Refer to the official documentation for more information` |
|       - | 5944 | ` * on the powerful extension introduced by the PH7 engine` |
|       - | 5945 | ` * to the OO subsystem such as full type hinting,method` |
|       - | 5946 | ` * overloading and many more.` |
|       - | 5947 | ` */` |
|  129894 | 5948 | `static sxi32 GenStateCompileClassMethod(` |
|       - | 5949 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 5950 | `	sxi32 iProtection,   /* Visibility level */` |
|       - | 5951 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - | 5952 | `	int doBody,          /* TRUE to process method body */` |
|       - | 5953 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - | 5954 | `	)` |
|       2 | 5955 |  |
|  129896 | 5956 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5957 | `	ph7_class_method *pMeth;` |
|       - | 5958 | `	sxi32 iFuncFlags;` |
|       - | 5959 | `	SyString *pName;` |
|       - | 5960 | `	SyToken *pEnd;` |
|       - | 5961 | `	sxi32 rc;` |
|       - | 5962 | `	/* Extract visibility level */` |
|  129896 | 5963 | `	iProtection = GetProtectionLevel(iProtection);` |
|  129896 | 5964 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  129896 | 5965 | `	iFuncFlags = 0;` |
|  129896 | 5966 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5967 | `		/* Invalid method name */` |
|     ! 0 | 5968 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 5969 | `		if( rc == SXERR_ABORT ){` |
|       - | 5970 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5971 | `			return SXERR_ABORT;` |
|       - | 5972 | `		}` |
|     ! 0 | 5973 | `		goto Synchronize;` |
|       - | 5974 | `	}` |
|  129896 | 5975 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 5976 | `		/* Return by reference,remember that */` |
|     ! 0 | 5977 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 5978 | `		/* Jump the '&' token */` |
|     ! 0 | 5979 | `		pGen->pIn++;` |
|     ! 0 | 5980 | `	}` |
|  129896 | 5981 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 5982 | `		/* Invalid method name */` |
|     ! 0 | 5983 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 5984 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5985 | `			return SXERR_ABORT;` |
|       - | 5986 | `		}` |
|     ! 0 | 5987 | `		goto Synchronize;` |
|       - | 5988 | `	}` |
|       - | 5989 | `	/* Peek method name */` |
|  129896 | 5990 | `	pName = &pGen->pIn->sData;` |
|  129896 | 5991 | `	nLine = pGen->pIn->nLine;` |
|       - | 5992 | `	/* Jump the method name */` |
|  129896 | 5993 | `	pGen->pIn++;` |
|  129896 | 5994 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 5995 | `		/* Abstract method */` |
|   21618 | 5996 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 5997 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5998 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 | 5999 | `				&pClass->sName,pName);` |
|     ! 0 | 6000 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6001 | `				return SXERR_ABORT;` |
|       - | 6002 | `			}` |
|     ! 0 | 6003 | `		}` |
|       - | 6004 | `		/* Assemble method signature only */` |
|   21618 | 6005 | `		doBody = FALSE;` |
|   10808 | 6006 | `	}` |
|  129896 | 6007 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 6008 | `		/* Syntax error */` |
|     ! 0 | 6009 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 | 6010 | `		if( rc == SXERR_ABORT ){` |
|       - | 6011 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6012 | `			return SXERR_ABORT;` |
|       - | 6013 | `		}` |
|     ! 0 | 6014 | `		goto Synchronize;` |
|       - | 6015 | `	}` |
|       - | 6016 | `	/* Allocate a new class_method instance */` |
|  129896 | 6017 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  129896 | 6018 | `	if( pMeth == 0 ){` |
|     ! 0 | 6019 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6020 | `		return SXERR_ABORT;` |
|       - | 6021 | `	}` |
|       - | 6022 | `	/* Jump the left parenthesis '(' */` |
|  129896 | 6023 | `	pGen->pIn++;` |
|  129896 | 6024 | `	pEnd = 0; /* cc warning */` |
|       - | 6025 | `	/* Delimit the method signature */` |
|  129896 | 6026 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  129896 | 6027 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 6028 | `		/* Syntax error */` |
|       3 | 6029 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 | 6030 | `		if( rc == SXERR_ABORT ){` |
|       - | 6031 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6032 | `			return SXERR_ABORT;` |
|       - | 6033 | `		}` |
|       3 | 6034 | `		goto Synchronize;` |
|       - | 6035 | `	}` |
|  129894 | 6036 | `	if( pGen->pIn < pEnd ){` |
|       - | 6037 | `		/* Collect method arguments */` |
|   27050 | 6038 | `		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);` |
|   27050 | 6039 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6040 | `			return SXERR_ABORT;` |
|       - | 6041 | `		}` |
|   13524 | 6042 | `	}` |
|       - | 6043 | `	/* Point past ')' and parse optional return type ': type' */` |
|  129894 | 6044 | `	pGen->pIn = &pEnd[1];` |
|  129894 | 6045 | `	GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  129894 | 6046 | `	if( doBody ){` |
|       - | 6047 | `		/* Compile method body */` |
|  108278 | 6048 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  108278 | 6049 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6050 | `			return SXERR_ABORT;` |
|       - | 6051 | `		}` |
|   54140 | 6052 | `	}else{` |
|       - | 6053 | `		/* Only method signature is allowed */` |
|   21618 | 6054 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 | 6055 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6056 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 | 6057 | `				if( rc == SXERR_ABORT ){` |
|       - | 6058 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 6059 | `					return SXERR_ABORT;` |
|       - | 6060 | `				}` |
|     ! 0 | 6061 | `				return SXERR_CORRUPT;` |
|       - | 6062 | `			}` |
|       - | 6063 | `	}` |
|       - | 6064 | `	/* All done,install the method */` |
|  129894 | 6065 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  129894 | 6066 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6067 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6068 | `		return SXERR_ABORT;` |
|       - | 6069 | `	}` |
|  129894 | 6070 | `	return SXRET_OK;` |
|       1 | 6071 | `Synchronize:` |
|       - | 6072 | `	/* Synchronize with the first semi-colon */` |
|       7 | 6073 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       5 | 6074 | `		pGen->pIn++;` |
|       1 | 6075 | `	}` |
|       3 | 6076 | `	return SXERR_CORRUPT;` |
|   64949 | 6077 |  |
|       - | 6078 | `/*` |
|       - | 6079 | ` * Compile an object interface.` |
|       - | 6080 | ` *  According to the PHP language reference manual` |
|       - | 6081 | ` *   Object Interfaces:` |
|       - | 6082 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - | 6083 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - | 6084 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - | 6085 | ` *   class, but without any of the methods having their contents defined.` |
|       - | 6086 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - | 6087 | ` */` |
|    8126 | 6088 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 | 6089 |  |
|    8128 | 6090 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6091 | `	ph7_class *pClass,*pBase;` |
|       - | 6092 | `	SyToken *pEnd,*pTmp;` |
|       - | 6093 | `	SyString *pName;` |
|       - | 6094 | `	sxi32 nKwrd;` |
|       - | 6095 | `	sxi32 rc;` |
|       - | 6096 | `	/* Jump the 'interface' keyword */` |
|    8128 | 6097 | `	pGen->pIn++;` |
|       - | 6098 | `	/* Extract interface name */` |
|    8128 | 6099 | `	pName = &pGen->pIn->sData;` |
|       - | 6100 | `	/* Advance the stream cursor */` |
|    8128 | 6101 | `	pGen->pIn++;` |
|       - | 6102 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 6103 | `		SyBlob sFQN;` |
|       - | 6104 | `		SyString sFQNStr;` |
|    8128 | 6105 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    8128 | 6106 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    8128 | 6107 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    8128 | 6108 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    8128 | 6109 | `		SyBlobRelease(&sFQN);` |
|       - | 6110 | `	}` |
|    8128 | 6111 | `	if( pClass == 0 ){` |
|     ! 0 | 6112 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6113 | `		return SXERR_ABORT;` |
|       - | 6114 | `	}` |
|       - | 6115 | `	/* Mark as an interface */` |
|    8128 | 6116 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - | 6117 | `	/* Assume no base class is given */` |
|    8128 | 6118 | `	pBase = 0;` |
|    8128 | 6119 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 6120 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 6121 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - | 6122 | `			SyString *pBaseName;` |
|       - | 6123 | `			/* Extract base interface */` |
|       3 | 6124 | `			pGen->pIn++;` |
|       3 | 6125 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 6126 | `				/* Syntax error */` |
|     ! 0 | 6127 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 6128 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 | 6129 | `					pName);` |
|     ! 0 | 6130 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6131 | `				if( rc == SXERR_ABORT ){` |
|       - | 6132 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 6133 | `					return SXERR_ABORT;` |
|       - | 6134 | `				}` |
|     ! 0 | 6135 | `				return SXRET_OK;` |
|       - | 6136 | `			}` |
|       3 | 6137 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 6138 | `			{` |
|       - | 6139 | `				SyBlob sResolved;` |
|       3 | 6140 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       3 | 6141 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|       4 | 6142 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|       2 | 6143 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       3 | 6144 | `				SyBlobRelease(&sResolved);` |
|       - | 6145 | `			}` |
|       - | 6146 | `			/* Only interfaces is allowed */` |
|       3 | 6147 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 6148 | `				pBase = pBase->pNextName;` |
|     ! 0 | 6149 | `			}` |
|       3 | 6150 | `			if( pBase == 0 ){` |
|       - | 6151 | `				/* Inexistant interface */` |
|     ! 0 | 6152 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 | 6153 | `				if( rc == SXERR_ABORT ){` |
|       - | 6154 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 6155 | `					return SXERR_ABORT;` |
|       - | 6156 | `				}` |
|     ! 0 | 6157 | `			}` |
|       - | 6158 | `			/* Advance the stream cursor */` |
|       3 | 6159 | `			pGen->pIn++;` |
|       1 | 6160 | `		}` |
|       1 | 6161 | `	}` |
|    8128 | 6162 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 6163 | `		/* Syntax error */` |
|     ! 0 | 6164 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 | 6165 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6166 | `		if( rc == SXERR_ABORT ){` |
|       - | 6167 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6168 | `			return SXERR_ABORT;` |
|       - | 6169 | `		}` |
|     ! 0 | 6170 | `		return SXRET_OK;` |
|       - | 6171 | `	}` |
|    8128 | 6172 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    8128 | 6173 | `	pEnd = 0; /* cc warning */` |
|       - | 6174 | `	/* Delimit the interface body */` |
|    8128 | 6175 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    8128 | 6176 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 6177 | `		/* Syntax error */` |
|     ! 0 | 6178 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 | 6179 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6180 | `		if( rc == SXERR_ABORT ){` |
|       - | 6181 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6182 | `			return SXERR_ABORT;` |
|       - | 6183 | `		}` |
|     ! 0 | 6184 | `		return SXRET_OK;` |
|       - | 6185 | `	}` |
|       - | 6186 | `	/* Swap token stream */` |
|    8128 | 6187 | `	pTmp = pGen->pEnd;` |
|    8128 | 6188 | `	pGen->pEnd = pEnd;` |
|       - | 6189 | `	/* Start the parse process` |
|       - | 6190 | `	 * Note (According to the PHP reference manual):` |
|       - | 6191 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - | 6192 | `	 *  Only 'public' visibility is allowed.` |
|       - | 6193 | `	 */` |
|   14866 | 6194 | `	for(;;){` |
|       - | 6195 | `		/* Jump leading/trailing semi-colons */` |
|   51340 | 6196 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   21608 | 6197 | `			pGen->pIn++;` |
|       2 | 6198 | `		}` |
|   29734 | 6199 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 6200 | `			/* End of interface body */` |
|    8126 | 6201 | `			break;` |
|       - | 6202 | `		}` |
|   21610 | 6203 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6204 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6205 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 | 6206 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 6207 | `			if( rc == SXERR_ABORT ){` |
|       - | 6208 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 6209 | `				return SXERR_ABORT;` |
|       - | 6210 | `			}` |
|     ! 0 | 6211 | `			goto done;` |
|       - | 6212 | `		}` |
|       - | 6213 | `		/* Extract the current keyword */` |
|   21610 | 6214 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   21610 | 6215 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 6216 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - | 6217 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 | 6218 | `			const char *zKind = "member";` |
|       3 | 6219 | `			SyString *pMemberName = 0;` |
|       3 | 6220 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 | 6221 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 | 6222 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 | 6223 | `					zKind = "constant";` |
|       3 | 6224 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 | 6225 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 | 6226 | `					}` |
|       1 | 6227 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6228 | `					zKind = "method";` |
|     ! 0 | 6229 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 | 6230 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 | 6231 | `					}` |
|     ! 0 | 6232 | `				}` |
|       1 | 6233 | `			}` |
|       3 | 6234 | `			if( pMemberName ){` |
|       4 | 6235 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 | 6236 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 | 6237 | `			}else{` |
|     ! 0 | 6238 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6239 | `					"Access type for interface %s must be public",zKind);` |
|       - | 6240 | `			}` |
|       3 | 6241 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6242 | `				return SXERR_ABORT;` |
|       - | 6243 | `			}` |
|       3 | 6244 | `			goto done;` |
|       - | 6245 | `		}` |
|   21608 | 6246 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 6247 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6248 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 6249 | `			if( rc == SXERR_ABORT ){` |
|       - | 6250 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 6251 | `				return SXERR_ABORT;` |
|       - | 6252 | `			}` |
|     ! 0 | 6253 | `			goto done;` |
|       - | 6254 | `		}` |
|   21608 | 6255 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - | 6256 | `			/* Advance the stream cursor */` |
|   21604 | 6257 | `			pGen->pIn++;` |
|   21604 | 6258 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6259 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6260 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 6261 | `				if( rc == SXERR_ABORT ){` |
|       - | 6262 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 6263 | `					return SXERR_ABORT;` |
|       - | 6264 | `				}` |
|     ! 0 | 6265 | `				goto done;` |
|       - | 6266 | `			}` |
|   21604 | 6267 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   21604 | 6268 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 6269 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6270 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 6271 | `				if( rc == SXERR_ABORT ){` |
|       - | 6272 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 6273 | `					return SXERR_ABORT;` |
|       - | 6274 | `				}` |
|     ! 0 | 6275 | `				goto done;` |
|       - | 6276 | `			}` |
|   10801 | 6277 | `		}` |
|   21608 | 6278 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 6279 | `			/* Parse constant */` |
|       3 | 6280 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 | 6281 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6282 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6283 | `					return SXERR_ABORT;` |
|       - | 6284 | `				}` |
|     ! 0 | 6285 | `				goto done;` |
|       - | 6286 | `			}` |
|       2 | 6287 | `		}else{` |
|   21606 | 6288 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   21606 | 6289 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 6290 | `				/* Static method,record that */` |
|     ! 0 | 6291 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - | 6292 | `				/* Advance the stream cursor */` |
|     ! 0 | 6293 | `				pGen->pIn++;` |
|     ! 0 | 6294 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 | 6295 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6296 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6297 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 6298 | `						if( rc == SXERR_ABORT ){` |
|       - | 6299 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 6300 | `							return SXERR_ABORT;` |
|       - | 6301 | `						}` |
|     ! 0 | 6302 | `						goto done;` |
|       - | 6303 | `				}` |
|     ! 0 | 6304 | `			}` |
|       - | 6305 | `			/* Process method signature (no body for interface methods) */` |
|   21606 | 6306 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   21606 | 6307 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6308 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6309 | `					return SXERR_ABORT;` |
|       - | 6310 | `				}` |
|     ! 0 | 6311 | `				goto done;` |
|       - | 6312 | `			}` |
|       - | 6313 | `		}` |
|       2 | 6314 | `	}` |
|       - | 6315 | `	/* Install the interface */` |
|    8126 | 6316 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    8126 | 6317 | `	if( rc == SXRET_OK && pBase ){` |
|       - | 6318 | `		/* Inherit from the base interface */` |
|       3 | 6319 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 | 6320 | `	}` |
|    8126 | 6321 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6322 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6323 | `		return SXERR_ABORT;` |
|       - | 6324 | `	}` |
|    4062 | 6325 | `done:` |
|       - | 6326 | `	/* Point beyond the interface body */` |
|    8128 | 6327 | `	pGen->pIn  = &pEnd[1];` |
|    8128 | 6328 | `	pGen->pEnd = pTmp;` |
|    8128 | 6329 | `	return PH7_OK;` |
|    4065 | 6330 |  |
|       - | 6331 | `/*` |
|       - | 6332 | ` * Compile a user-defined class.` |
|       - | 6333 | ` * According to the PHP language reference manual` |
|       - | 6334 | ` *  class` |
|       - | 6335 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - | 6336 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - | 6337 | ` *  of the properties and methods belonging to the class.` |
|       - | 6338 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - | 6339 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - | 6340 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - | 6341 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 6342 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - | 6343 | ` *  (called "methods").` |
|       - | 6344 | ` */` |
|       - | 6345 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - | 6346 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - | 6347 | `struct TraitUseEntry {` |
|       - | 6348 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - | 6349 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - | 6350 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - | 6351 | `};` |
|       - | 6352 | `/*` |
|       - | 6353 | ` * Validate that methods implementing interface contracts have compatible` |
|       - | 6354 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - | 6355 | ` */` |
|   38226 | 6356 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 6357 |  |
|       - | 6358 | `	ph7_class **apIface;` |
|       - | 6359 | `	sxu32 nIface,i;` |
|       - | 6360 | `	sxi32 rc;` |
|   38228 | 6361 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 | 6362 | `		return SXRET_OK;` |
|       - | 6363 | `	}` |
|   38228 | 6364 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   38228 | 6365 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   40962 | 6366 | `	for(i = 0; i < nIface; i++){` |
|    2736 | 6367 | `		ph7_class *pIface = apIface[i];` |
|       - | 6368 | `		SyHashEntry *pEntry;` |
|    2736 | 6369 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   16294 | 6370 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   13560 | 6371 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - | 6372 | `			ph7_class_method *pImplMeth;` |
|   13560 | 6373 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - | 6374 | `			/* Find the implementing method in the class */` |
|   13560 | 6375 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   13560 | 6376 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 | 6377 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - | 6378 | `			}` |
|       - | 6379 | `			/* Check visibility: interface methods must be implemented as public */` |
|   13546 | 6380 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 | 6381 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 6382 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 | 6383 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 | 6384 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6385 | `					return SXERR_ABORT;` |
|       - | 6386 | `				}` |
|       1 | 6387 | `			}` |
|       - | 6388 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - | 6389 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - | 6390 | `			 */` |
|       - | 6391 | `			{` |
|   13546 | 6392 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   13546 | 6393 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   13546 | 6394 | `				int sigError = 0;` |
|   13546 | 6395 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 | 6396 | `					sigError = 1;` |
|   13545 | 6397 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - | 6398 | `					/* Extra parameters must all have default values */` |
|       5 | 6399 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - | 6400 | `					sxu32 k;` |
|       7 | 6401 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       5 | 6402 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 | 6403 | `							sigError = 1;` |
|       3 | 6404 | `							break;` |
|       - | 6405 | `						}` |
|       2 | 6406 | `					}` |
|       2 | 6407 | `				}` |
|   13546 | 6408 | `				if( sigError ){` |
|       - | 6409 | `					SyBlob sImplSig, sIfaceSig;` |
|       - | 6410 | `					ph7_vm_func_arg *aArgs;` |
|       - | 6411 | `					sxu32 j;` |
|       5 | 6412 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       5 | 6413 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - | 6414 | `					/* Build implementing method signature */` |
|       5 | 6415 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      11 | 6416 | `					for(j = 0; j < nImplArgs; j++){` |
|       7 | 6417 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       7 | 6418 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       7 | 6419 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 6420 | `					}` |
|       - | 6421 | `					/* Build interface method signature */` |
|       5 | 6422 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      11 | 6423 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       7 | 6424 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       7 | 6425 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       7 | 6426 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 6427 | `					}` |
|       7 | 6428 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 6429 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 | 6430 | `						&pClass->sName,pMName,` |
|       4 | 6431 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 | 6432 | `						&pIface->sName,pMName,` |
|       4 | 6433 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       5 | 6434 | `					SyBlobRelease(&sImplSig);` |
|       5 | 6435 | `					SyBlobRelease(&sIfaceSig);` |
|       5 | 6436 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6437 | `						return SXERR_ABORT;` |
|       - | 6438 | `					}` |
|       2 | 6439 | `				}` |
|       - | 6440 | `			}` |
|       2 | 6441 | `		}` |
|    1369 | 6442 | `	}` |
|   38228 | 6443 | `	return SXRET_OK;` |
|   19115 | 6444 |  |
|       - | 6445 | `/*` |
|       - | 6446 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - | 6447 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - | 6448 | ` */` |
|   38226 | 6449 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 6450 |  |
|       - | 6451 | `	ph7_class_method *pMeth;` |
|       - | 6452 | `	SyHashEntry *pEntry;` |
|       - | 6453 | `	sxu32 nAbstract;` |
|       - | 6454 | `	SyBlob sMsg;` |
|       - | 6455 | `	sxi32 rc;` |
|       - | 6456 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   38228 | 6457 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      20 | 6458 | `		return SXRET_OK;` |
|       - | 6459 | `	}` |
|       - | 6460 | `	/* Count abstract methods */` |
|   38210 | 6461 | `	nAbstract = 0;` |
|   38210 | 6462 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  362594 | 6463 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  324386 | 6464 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  324386 | 6465 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      17 | 6466 | `			nAbstract++;` |
|       8 | 6467 | `		}` |
|       2 | 6468 | `	}` |
|   38210 | 6469 | `	if( nAbstract == 0 ){` |
|   38196 | 6470 | `		return SXRET_OK;` |
|       - | 6471 | `	}` |
|       - | 6472 | `	/* Build the error message listing all abstract methods with origins */` |
|      15 | 6473 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      15 | 6474 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - | 6475 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 | 6476 | `		&pClass->sName,nAbstract,` |
|       7 | 6477 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 | 6478 | `		(nAbstract > 1 ? "s" : ""));` |
|       - | 6479 | `	/* Second pass: list methods with origins */` |
|       - | 6480 | `	{` |
|      15 | 6481 | `		sxu32 nListed = 0;` |
|      15 | 6482 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      33 | 6483 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      19 | 6484 | `			ph7_class *pOrigin = 0;` |
|       - | 6485 | `			SyString *pMName;` |
|      19 | 6486 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      19 | 6487 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 | 6488 | `				continue;` |
|       - | 6489 | `			}` |
|      17 | 6490 | `			pMName = &pMeth->sFunc.sName;` |
|      17 | 6491 | `			if( nListed > 0 ){` |
|       3 | 6492 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 | 6493 | `			}` |
|       - | 6494 | `			/* Find the origin of this abstract method.` |
|       - | 6495 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - | 6496 | `			 * inheritance chains) take precedence for interface-declared` |
|       - | 6497 | `			 * methods. Abstract class methods only win when the class` |
|       - | 6498 | `			 * itself declared the abstract method (not inherited from` |
|       - | 6499 | `			 * an interface). Trait methods are adopted into the using` |
|       - | 6500 | `			 * class's namespace.` |
|       - | 6501 | `			 */` |
|       - | 6502 | `			{` |
|       - | 6503 | `				ph7_class **apIface;` |
|       - | 6504 | `				ph7_class **apTrait;` |
|       - | 6505 | `				ph7_class *pWalk;` |
|       - | 6506 | `				sxu32 i;` |
|       - | 6507 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - | 6508 | `				 * (one that was written in the class body, not inherited from an` |
|       - | 6509 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - | 6510 | `				 */` |
|      17 | 6511 | `				if( pClass->pBase ){` |
|       9 | 6512 | `					pWalk = pClass->pBase;` |
|      17 | 6513 | `					while( pWalk ){` |
|       - | 6514 | `						ph7_class_method *pParentMeth;` |
|      11 | 6515 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      11 | 6516 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - | 6517 | `							/* Exclude methods that came from an interface anywhere` |
|       - | 6518 | `							 * in this class's ancestor chain.` |
|       - | 6519 | `							 */` |
|      11 | 6520 | `							int fromIface = 0;` |
|      11 | 6521 | `							ph7_class *pAnc = pWalk;` |
|      15 | 6522 | `							while( pAnc ){` |
|       - | 6523 | `								ph7_class **apPI;` |
|       - | 6524 | `								sxu32 j;` |
|      13 | 6525 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      13 | 6526 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       9 | 6527 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       9 | 6528 | `										fromIface = 1;` |
|       9 | 6529 | `										break;` |
|       - | 6530 | `									}` |
|     ! 0 | 6531 | `								}` |
|      13 | 6532 | `								if( fromIface ) break;` |
|       5 | 6533 | `								pAnc = pAnc->pBase;` |
|       1 | 6534 | `							}` |
|      11 | 6535 | `							if( !fromIface ){` |
|       3 | 6536 | `								pOrigin = pWalk;` |
|       3 | 6537 | `								break;` |
|       - | 6538 | `							}` |
|       4 | 6539 | `						}` |
|       9 | 6540 | `						pWalk = pWalk->pBase;` |
|       1 | 6541 | `					}` |
|       4 | 6542 | `				}` |
|       - | 6543 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - | 6544 | `				 * each interface's own parent chain for the deepest origin.` |
|       - | 6545 | `				 */` |
|      17 | 6546 | `				if( !pOrigin ){` |
|      15 | 6547 | `					pWalk = pClass;` |
|      37 | 6548 | `					while( pWalk && !pOrigin ){` |
|      23 | 6549 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      23 | 6550 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      13 | 6551 | `							ph7_class *pIface = apIface[i];` |
|      13 | 6552 | `							ph7_class *pDeepest = 0;` |
|      25 | 6553 | `							while( pIface ){` |
|      13 | 6554 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      13 | 6555 | `									pDeepest = pIface;` |
|       6 | 6556 | `								}` |
|      13 | 6557 | `								pIface = pIface->pBase;` |
|       1 | 6558 | `							}` |
|      13 | 6559 | `							if( pDeepest ){` |
|      13 | 6560 | `								pOrigin = pDeepest;` |
|      13 | 6561 | `								break;` |
|       - | 6562 | `							}` |
|     ! 0 | 6563 | `						}` |
|      23 | 6564 | `						pWalk = pWalk->pBase;` |
|       1 | 6565 | `					}` |
|       7 | 6566 | `				}` |
|       - | 6567 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      17 | 6568 | `				if( !pOrigin ){` |
|       3 | 6569 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 | 6570 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 | 6571 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 | 6572 | `							pOrigin = pClass;` |
|       3 | 6573 | `							break;` |
|       - | 6574 | `						}` |
|     ! 0 | 6575 | `					}` |
|       1 | 6576 | `				}` |
|       - | 6577 | `			}` |
|      17 | 6578 | `			if( pOrigin ){` |
|      17 | 6579 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       9 | 6580 | `			}else{` |
|       - | 6581 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 | 6582 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - | 6583 | `			}` |
|      17 | 6584 | `			nListed++;` |
|       1 | 6585 | `		}` |
|       - | 6586 | `	}` |
|      15 | 6587 | `	SyBlobAppend(&sMsg,")",1);` |
|      22 | 6588 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 | 6589 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      15 | 6590 | `	SyBlobRelease(&sMsg);` |
|      15 | 6591 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 6592 | `		return SXERR_ABORT;` |
|       - | 6593 | `	}` |
|      15 | 6594 | `	return SXRET_OK;` |
|   19115 | 6595 |  |
|   38230 | 6596 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 | 6597 |  |
|   38232 | 6598 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6599 | `	ph7_class *pClass,*pBase;` |
|       - | 6600 | `	SyToken *pEnd,*pTmp;` |
|       - | 6601 | `	sxi32 iProtection;` |
|       - | 6602 | `	SySet aInterfaces;` |
|       - | 6603 | `	SySet aUseEntries;` |
|       - | 6604 | `	sxi32 iAttrflags;` |
|       - | 6605 | `	SyString *pName;` |
|       - | 6606 | `	sxi32 nKwrd;` |
|       - | 6607 | `	sxi32 rc;` |
|       - | 6608 | `	/* Jump the 'class' keyword */` |
|   38232 | 6609 | `	pGen->pIn++;` |
|   38232 | 6610 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 6611 | `		/* Syntax error */` |
|     ! 0 | 6612 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 | 6613 | `		if( rc == SXERR_ABORT ){` |
|       - | 6614 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6615 | `			return SXERR_ABORT;` |
|       - | 6616 | `		}` |
|       - | 6617 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 | 6618 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 | 6619 | `			pGen->pIn++;` |
|     ! 0 | 6620 | `		}` |
|     ! 0 | 6621 | `		return SXRET_OK;` |
|       - | 6622 | `	}` |
|       - | 6623 | `	/* Extract class name */` |
|   38232 | 6624 | `	pName = &pGen->pIn->sData;` |
|       - | 6625 | `	/* Advance the stream cursor */` |
|   38232 | 6626 | `	pGen->pIn++;` |
|       - | 6627 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 6628 | `		SyBlob sFQN;` |
|       - | 6629 | `		SyString sFQNStr;` |
|   38232 | 6630 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   38232 | 6631 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   38232 | 6632 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   38232 | 6633 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   38232 | 6634 | `		SyBlobRelease(&sFQN);` |
|       - | 6635 | `	}` |
|   38232 | 6636 | `	if( pClass == 0 ){` |
|     ! 0 | 6637 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6638 | `		return SXERR_ABORT;` |
|       - | 6639 | `	}` |
|       - | 6640 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   38232 | 6641 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   38232 | 6642 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - | 6643 | `	/* Assume a standalone class */` |
|   38232 | 6644 | `	pBase = 0;` |
|   38232 | 6645 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 6646 | `		SyString *pBaseName;` |
|   27110 | 6647 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   27110 | 6648 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|   24378 | 6649 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   24378 | 6650 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 6651 | `				/* Syntax error */` |
|     ! 0 | 6652 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 6653 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 | 6654 | `					pName);` |
|     ! 0 | 6655 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6656 | `				if( rc == SXERR_ABORT ){` |
|       - | 6657 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 6658 | `					return SXERR_ABORT;` |
|       - | 6659 | `				}` |
|     ! 0 | 6660 | `				return SXRET_OK;` |
|       - | 6661 | `			}` |
|       - | 6662 | `			/* Extract base class name and resolve through namespace/imports */` |
|   24378 | 6663 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 6664 | `			{` |
|       - | 6665 | `				SyBlob sResolved;` |
|   24378 | 6666 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   24378 | 6667 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|   36566 | 6668 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|   24376 | 6669 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   24378 | 6670 | `				SyBlobRelease(&sResolved);` |
|       - | 6671 | `			}` |
|       - | 6672 | `			/* Interfaces are not allowed */` |
|   24378 | 6673 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 | 6674 | `				pBase = pBase->pNextName;` |
|     ! 0 | 6675 | `			}` |
|   24378 | 6676 | `			if( pBase == 0 ){` |
|       - | 6677 | `				/* Inexistant base class */` |
|     ! 0 | 6678 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 | 6679 | `				if( rc == SXERR_ABORT ){` |
|       - | 6680 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 6681 | `					return SXERR_ABORT;` |
|       - | 6682 | `				}` |
|     ! 0 | 6683 | `			}else{` |
|   24378 | 6684 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 | 6685 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 6686 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 | 6687 | `					if( rc == SXERR_ABORT ){` |
|       - | 6688 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 6689 | `						return SXERR_ABORT;` |
|       - | 6690 | `					}` |
|     ! 0 | 6691 | `				}` |
|       - | 6692 | `			}` |
|       - | 6693 | `			/* Advance the stream cursor */` |
|   24378 | 6694 | `			pGen->pIn++;` |
|   12188 | 6695 | `		}` |
|   27110 | 6696 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - | 6697 | `			ph7_class *pInterface;` |
|       - | 6698 | `			SyString *pIntName;` |
|       - | 6699 | `			/* Interface implementation */` |
|    2736 | 6700 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    1367 | 6701 | `			for(;;){` |
|    2736 | 6702 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 6703 | `					/* Syntax error */` |
|     ! 0 | 6704 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 6705 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 | 6706 | `						pName);` |
|     ! 0 | 6707 | `					if( rc == SXERR_ABORT ){` |
|       - | 6708 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 6709 | `						return SXERR_ABORT;` |
|       - | 6710 | `					}` |
|     ! 0 | 6711 | `					break;` |
|       - | 6712 | `				}` |
|       - | 6713 | `				/* Extract interface name and resolve through namespace/imports */` |
|    2736 | 6714 | `				pIntName = &pGen->pIn->sData;` |
|       - | 6715 | `				{` |
|       - | 6716 | `					SyBlob sResolved;` |
|    2736 | 6717 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    2736 | 6718 | `					GenStateResolveName(pGen,pIntName,&sResolved);` |
|    5470 | 6719 | `					pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    2734 | 6720 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    2736 | 6721 | `					SyBlobRelease(&sResolved);` |
|       - | 6722 | `				}` |
|       - | 6723 | `				/* Only interfaces are allowed */` |
|    2736 | 6724 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 6725 | `					pInterface = pInterface->pNextName;` |
|     ! 0 | 6726 | `				}` |
|    2736 | 6727 | `				if( pInterface == 0 ){` |
|       - | 6728 | `					/* Inexistant interface */` |
|     ! 0 | 6729 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 | 6730 | `					if( rc == SXERR_ABORT ){` |
|       - | 6731 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 6732 | `						return SXERR_ABORT;` |
|       - | 6733 | `					}` |
|     ! 0 | 6734 | `				}else{` |
|       - | 6735 | `					/* Register interface */` |
|    2736 | 6736 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - | 6737 | `				}` |
|       - | 6738 | `				/* Advance the stream cursor */` |
|    2736 | 6739 | `				pGen->pIn++;` |
|    2736 | 6740 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    1369 | 6741 | `					break;` |
|       - | 6742 | `				}` |
|     ! 0 | 6743 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 | 6744 | `			}` |
|    1367 | 6745 | `		}` |
|   13554 | 6746 | `	}` |
|   38232 | 6747 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 6748 | `		/* Syntax error */` |
|     ! 0 | 6749 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 | 6750 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6751 | `		if( rc == SXERR_ABORT ){` |
|       - | 6752 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6753 | `			return SXERR_ABORT;` |
|       - | 6754 | `		}` |
|     ! 0 | 6755 | `		return SXRET_OK;` |
|       - | 6756 | `	}` |
|   38232 | 6757 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   38232 | 6758 | `	pEnd = 0; /* cc warning */` |
|       - | 6759 | `	/* Delimit the class body */` |
|   38232 | 6760 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   38232 | 6761 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 6762 | `		/* Syntax error */` |
|     ! 0 | 6763 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 | 6764 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6765 | `		if( rc == SXERR_ABORT ){` |
|       - | 6766 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6767 | `			return SXERR_ABORT;` |
|       - | 6768 | `		}` |
|     ! 0 | 6769 | `		return SXRET_OK;` |
|       - | 6770 | `	}` |
|       - | 6771 | `	/* Swap token stream */` |
|   38232 | 6772 | `	pTmp = pGen->pEnd;` |
|   38232 | 6773 | `	pGen->pEnd = pEnd;` |
|       - | 6774 | `	/* Set the inherited flags */` |
|   38232 | 6775 | `	pClass->iFlags = iFlags;` |
|       - | 6776 | `	/* Start the parse process */` |
|   73249 | 6777 | `	for(;;){` |
|       - | 6778 | `		/* Jump leading/trailing semi-colons */` |
|  217126 | 6779 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   35332 | 6780 | `			pGen->pIn++;` |
|       2 | 6781 | `		}` |
|  181796 | 6782 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 6783 | `			/* End of class body */` |
|   38228 | 6784 | `			break;` |
|       - | 6785 | `		}` |
|  143570 | 6786 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6787 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6788 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 6789 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 6790 | `			if( rc == SXERR_ABORT ){` |
|       - | 6791 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 6792 | `				return SXERR_ABORT;` |
|       - | 6793 | `			}` |
|     ! 0 | 6794 | `			goto done;` |
|       - | 6795 | `		}` |
|       - | 6796 | `		/* Assume public visibility */` |
|  143570 | 6797 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  143570 | 6798 | `		iAttrflags = 0;` |
|  143570 | 6799 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 6800 | `			/* Extract the current keyword */` |
|  143570 | 6801 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  143570 | 6802 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 6803 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - | 6804 | `				TraitUseEntry sUse;` |
|      41 | 6805 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      41 | 6806 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      41 | 6807 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      28 | 6808 | `				for(;;){` |
|       - | 6809 | `					ph7_class *pTrait;` |
|       - | 6810 | `					SyString *pTraitName;` |
|      49 | 6811 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6812 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6813 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 | 6814 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6815 | `							return SXERR_ABORT;` |
|       - | 6816 | `						}` |
|     ! 0 | 6817 | `						break;` |
|       - | 6818 | `					}` |
|      49 | 6819 | `					pTraitName = &pGen->pIn->sData;` |
|       - | 6820 | `					/* Resolve trait name through namespace/imports */ {` |
|       - | 6821 | `						SyBlob sResolved;` |
|      49 | 6822 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      49 | 6823 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|      97 | 6824 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      48 | 6825 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      49 | 6826 | `						SyBlobRelease(&sResolved);` |
|       - | 6827 | `					}` |
|       - | 6828 | `					/* Only traits are allowed */` |
|      49 | 6829 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 6830 | `						pTrait = pTrait->pNextName;` |
|     ! 0 | 6831 | `					}` |
|      49 | 6832 | `					if( pTrait == 0 ){` |
|     ! 0 | 6833 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6834 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 | 6835 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6836 | `							return SXERR_ABORT;` |
|       - | 6837 | `						}` |
|     ! 0 | 6838 | `					}else{` |
|      49 | 6839 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - | 6840 | `					}` |
|      49 | 6841 | `					pGen->pIn++; /* Advance past trait name */` |
|      49 | 6842 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      21 | 6843 | `						break;` |
|       - | 6844 | `					}` |
|       9 | 6845 | `					pGen->pIn++; /* Jump the comma */` |
|       1 | 6846 | `				}` |
|       - | 6847 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      41 | 6848 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 6849 | `					SyToken *pBlock;` |
|       9 | 6850 | `					pGen->pIn++; /* Jump '{' */` |
|       9 | 6851 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 | 6852 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 | 6853 | `					sUse.pResolvEnd = pBlock;` |
|       9 | 6854 | `					if( pBlock < pGen->pEnd ){` |
|       9 | 6855 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 | 6856 | `					}else{` |
|     ! 0 | 6857 | `						pGen->pIn = pGen->pEnd;` |
|       - | 6858 | `					}` |
|       4 | 6859 | `				}` |
|      41 | 6860 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - | 6861 | `				/* The semicolon will be consumed by the outer loop */` |
|      41 | 6862 | `				continue;` |
|       - | 6863 | `			}` |
|  143530 | 6864 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  140724 | 6865 | `				iProtection = nKwrd;` |
|  140724 | 6866 | `				pGen->pIn++; /* Jump the visibility token */` |
|  140724 | 6867 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6868 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6869 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 6870 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6871 | `					if( rc == SXERR_ABORT ){` |
|       - | 6872 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 6873 | `						return SXERR_ABORT;` |
|       - | 6874 | `					}` |
|     ! 0 | 6875 | `					goto done;` |
|       - | 6876 | `				}` |
|  140724 | 6877 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 6878 | `					/* Attribute declaration */` |
|   35256 | 6879 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   35256 | 6880 | `					if( rc != SXRET_OK ){` |
|       3 | 6881 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6882 | `							return SXERR_ABORT;` |
|       - | 6883 | `						}` |
|       3 | 6884 | `						goto done;` |
|       - | 6885 | `					}` |
|   35254 | 6886 | `					continue;` |
|       - | 6887 | `				}` |
|       - | 6888 | `				/* Extract the keyword */` |
|  105470 | 6889 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   52734 | 6890 | `			}` |
|  108276 | 6891 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 6892 | `				/* Process constant declaration */` |
|      30 | 6893 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      30 | 6894 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6895 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6896 | `						return SXERR_ABORT;` |
|       - | 6897 | `					}` |
|     ! 0 | 6898 | `					goto done;` |
|       - | 6899 | `				}` |
|      16 | 6900 | `			}else{` |
|  108248 | 6901 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 6902 | `					/* Static method or attribute,record that */` |
|    2724 | 6903 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    2724 | 6904 | `					pGen->pIn++; /* Jump the static keyword */` |
|    2724 | 6905 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 6906 | `						/* Extract the keyword */` |
|    2720 | 6907 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    2720 | 6908 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 6909 | `							iProtection = nKwrd;` |
|     ! 0 | 6910 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 | 6911 | `						}` |
|    1359 | 6912 | `					}` |
|    2724 | 6913 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6914 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6915 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 | 6916 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6917 | `						if( rc == SXERR_ABORT ){` |
|       - | 6918 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 6919 | `							return SXERR_ABORT;` |
|       - | 6920 | `						}` |
|     ! 0 | 6921 | `						goto done;` |
|       - | 6922 | `					}` |
|    2724 | 6923 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 6924 | `						/* Attribute declaration */` |
|       5 | 6925 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 6926 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 6927 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 6928 | `								return SXERR_ABORT;` |
|       - | 6929 | `							}` |
|     ! 0 | 6930 | `							goto done;` |
|       - | 6931 | `						}` |
|       5 | 6932 | `						continue;` |
|       - | 6933 | `					}` |
|       - | 6934 | `					/* Extract the keyword */` |
|    2720 | 6935 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  106885 | 6936 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - | 6937 | `					/* Abstract method,record that */` |
|      10 | 6938 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - | 6939 | `					/* Mark the whole class as abstract */` |
|      10 | 6940 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - | 6941 | `					/* Advance the stream cursor */` |
|      10 | 6942 | `					pGen->pIn++;` |
|      10 | 6943 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      10 | 6944 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      10 | 6945 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       8 | 6946 | `							iProtection = nKwrd;` |
|       8 | 6947 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 | 6948 | `						}` |
|       4 | 6949 | `					}` |
|      10 | 6950 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 6951 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 6952 | `							/* Static method */` |
|     ! 0 | 6953 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 6954 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 6955 | `					}` |
|      10 | 6956 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       8 | 6957 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6958 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6959 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 | 6960 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 6961 | `							if( rc == SXERR_ABORT ){` |
|       - | 6962 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 6963 | `								return SXERR_ABORT;` |
|       - | 6964 | `							}` |
|     ! 0 | 6965 | `							goto done;` |
|       - | 6966 | `					}` |
|      10 | 6967 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  105522 | 6968 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - | 6969 | `					/* final method ,record that */` |
|       5 | 6970 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 | 6971 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 | 6972 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 6973 | `						/* Extract the keyword */` |
|       5 | 6974 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 6975 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 6976 | `							iProtection = nKwrd;` |
|       5 | 6977 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 6978 | `						}` |
|       2 | 6979 | `					}` |
|       5 | 6980 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 | 6981 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 6982 | `							/* Static method */` |
|     ! 0 | 6983 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 6984 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 6985 | `					}` |
|       5 | 6986 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 6987 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6988 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6989 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 | 6990 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 6991 | `							if( rc == SXERR_ABORT ){` |
|       - | 6992 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 6993 | `								return SXERR_ABORT;` |
|       - | 6994 | `							}` |
|     ! 0 | 6995 | `							goto done;` |
|       - | 6996 | `					}` |
|       5 | 6997 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 6998 | `				}` |
|  108244 | 6999 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 7000 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7001 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 | 7002 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 7003 | `						if( rc == SXERR_ABORT ){` |
|       - | 7004 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 7005 | `							return SXERR_ABORT;` |
|       - | 7006 | `						}` |
|     ! 0 | 7007 | `						goto done;` |
|       - | 7008 | `				}` |
|  108244 | 7009 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 | 7010 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 | 7011 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 | 7012 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7013 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 7014 | `						if( rc == SXERR_ABORT ){` |
|       - | 7015 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 7016 | `							return SXERR_ABORT;` |
|       - | 7017 | `						}` |
|     ! 0 | 7018 | `						goto done;` |
|       - | 7019 | `					}` |
|       - | 7020 | `					/* Attribute declaration */` |
|       7 | 7021 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 | 7022 | `				}else{` |
|       - | 7023 | `					/* Process method declaration */` |
|  108238 | 7024 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 7025 | `				}` |
|  108244 | 7026 | `				if( rc != SXRET_OK ){` |
|       3 | 7027 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7028 | `						return SXERR_ABORT;` |
|       - | 7029 | `					}` |
|       3 | 7030 | `					goto done;` |
|       - | 7031 | `				}` |
|       - | 7032 | `			}` |
|   54136 | 7033 | `		}else{` |
|       - | 7034 | `			/* Attribute declaration */` |
|     ! 0 | 7035 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 7036 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7037 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7038 | `					return SXERR_ABORT;` |
|       - | 7039 | `				}` |
|     ! 0 | 7040 | `				goto done;` |
|       - | 7041 | `			}` |
|       - | 7042 | `		}` |
|       2 | 7043 | `	}` |
|       - | 7044 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - | 7045 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - | 7046 | `	 */` |
|       - | 7047 | `	{` |
|       - | 7048 | `		TraitUseEntry *apUse;` |
|       - | 7049 | `		sxu32 nU;` |
|   38228 | 7050 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   38268 | 7051 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      41 | 7052 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      41 | 7053 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      41 | 7054 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      41 | 7055 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - | 7056 | `			sxu32 nT;` |
|      41 | 7057 | `			if( !hasResolution ){` |
|       - | 7058 | `				/* No conflict resolution block: use standard trait application */` |
|      71 | 7059 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      39 | 7060 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      39 | 7061 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 7062 | `						break;` |
|       - | 7063 | `					}` |
|      20 | 7064 | `				}` |
|      17 | 7065 | `			}else{` |
|       - | 7066 | `				/* With resolution block: copy attributes, record traits,` |
|       - | 7067 | `				 * then use the block to resolve method conflicts.` |
|       - | 7068 | `				 */` |
|       - | 7069 | `				SyToken *pR;` |
|      19 | 7070 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 | 7071 | `					ph7_class *pTR = apTrait[nT];` |
|       - | 7072 | `					ph7_class_attr *pAR;` |
|       - | 7073 | `					SyHashEntry *pER;` |
|       - | 7074 | `					SyString *pNR;` |
|      11 | 7075 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 | 7076 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 | 7077 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 | 7078 | `						pNR = &pAR->sName;` |
|     ! 0 | 7079 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 | 7080 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 | 7081 | `						}` |
|     ! 0 | 7082 | `					}` |
|      11 | 7083 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 | 7084 | `				}` |
|       - | 7085 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 | 7086 | `				pR = pUse->pResolvStart;` |
|      21 | 7087 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 7088 | `					SyString sTrait,sMethod;` |
|       - | 7089 | `					ph7_class *pSrcTrait;` |
|       - | 7090 | `					ph7_class_method *pMeth;` |
|       - | 7091 | `					sxi32 nRKwrd;` |
|      33 | 7092 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 7093 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 7094 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 7095 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 7096 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 7097 | `					sMethod = pR->sData;` |
|      13 | 7098 | `					pR++;` |
|      13 | 7099 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 7100 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 7101 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 7102 | `							sTrait = sMethod;` |
|       7 | 7103 | `							pR++;` |
|       7 | 7104 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 7105 | `							sMethod = pR->sData;` |
|       7 | 7106 | `							pR++;` |
|       3 | 7107 | `						}` |
|       3 | 7108 | `					}` |
|      13 | 7109 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 7110 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 7111 | `						continue;` |
|       - | 7112 | `					}` |
|      13 | 7113 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 7114 | `					pR++;` |
|      13 | 7115 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 | 7116 | `						pSrcTrait = 0;` |
|       7 | 7117 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 | 7118 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 | 7119 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 | 7120 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 | 7121 | `								pSrcTrait = apTrait[nT];` |
|       5 | 7122 | `								break;` |
|       - | 7123 | `							}` |
|       2 | 7124 | `						}` |
|       5 | 7125 | `						if( pSrcTrait ){` |
|       5 | 7126 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 | 7127 | `							if( pMeth ){` |
|       5 | 7128 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 | 7129 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 | 7130 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 | 7131 | `								}` |
|       2 | 7132 | `							}` |
|       2 | 7133 | `						}` |
|       2 | 7134 | `					}` |
|      29 | 7135 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 7136 | `				}` |
|       - | 7137 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 | 7138 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - | 7139 | `					ph7_class_method *pMR;` |
|       - | 7140 | `					SyHashEntry *pER;` |
|       - | 7141 | `					SyString *pNR;` |
|      11 | 7142 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 | 7143 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 | 7144 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 | 7145 | `						pNR = &pMR->sFunc.sName;` |
|      19 | 7146 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 | 7147 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 | 7148 | `						}` |
|       1 | 7149 | `					}` |
|       6 | 7150 | `				}` |
|       - | 7151 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 | 7152 | `				pR = pUse->pResolvStart;` |
|      21 | 7153 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 7154 | `					SyString sTrait,sMethod,sAlias;` |
|       - | 7155 | `					ph7_class *pSrcTrait;` |
|       - | 7156 | `					ph7_class_method *pMeth;` |
|      21 | 7157 | `					int hasQual = 0;` |
|       - | 7158 | `					sxi32 nRKwrd;` |
|      33 | 7159 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 7160 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 7161 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 7162 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 7163 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 | 7164 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 7165 | `					sMethod = pR->sData;` |
|      13 | 7166 | `					pR++;` |
|      13 | 7167 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 7168 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 7169 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 7170 | `							sTrait = sMethod;` |
|       7 | 7171 | `							hasQual = 1;` |
|       7 | 7172 | `							pR++;` |
|       7 | 7173 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 7174 | `							sMethod = pR->sData;` |
|       7 | 7175 | `							pR++;` |
|       3 | 7176 | `						}` |
|       3 | 7177 | `					}` |
|      13 | 7178 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 7179 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 7180 | `						continue;` |
|       - | 7181 | `					}` |
|      13 | 7182 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 7183 | `					pR++;` |
|      13 | 7184 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 | 7185 | `						sxi32 iNewVis = -1;` |
|       9 | 7186 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 | 7187 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 | 7188 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 | 7189 | `								iNewVis = nAK;` |
|       7 | 7190 | `								pR++;` |
|       3 | 7191 | `							}` |
|       3 | 7192 | `						}` |
|       9 | 7193 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 | 7194 | `							sAlias = pR->sData;` |
|       7 | 7195 | `							pR++;` |
|       3 | 7196 | `						}` |
|       9 | 7197 | `						pMeth = 0;` |
|       9 | 7198 | `						if( hasQual ){` |
|       3 | 7199 | `							pSrcTrait = 0;` |
|       5 | 7200 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 | 7201 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 | 7202 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 | 7203 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 | 7204 | `									pSrcTrait = apTrait[nT];` |
|       3 | 7205 | `									break;` |
|       - | 7206 | `								}` |
|       2 | 7207 | `							}` |
|       3 | 7208 | `							if( pSrcTrait ){` |
|       3 | 7209 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 | 7210 | `							}` |
|       2 | 7211 | `						}else{` |
|       7 | 7212 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - | 7213 | `						}` |
|       9 | 7214 | `						if( pMeth ){` |
|       9 | 7215 | `							if( sAlias.nByte > 0 ){` |
|       - | 7216 | `								/* Create a shallow copy of the method struct for the alias` |
|       - | 7217 | `								 * so it can carry its own visibility without affecting the original.` |
|       - | 7218 | `								 */` |
|       - | 7219 | `								ph7_class_method *pAlias;` |
|       - | 7220 | `								char *zAliasDup;` |
|       7 | 7221 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 | 7222 | `								if( pAlias ){` |
|       7 | 7223 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 | 7224 | `									if( iNewVis >= 0 ){` |
|       5 | 7225 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 7226 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 7227 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 | 7228 | `									}` |
|       7 | 7229 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 | 7230 | `									if( zAliasDup ){` |
|       7 | 7231 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 | 7232 | `									}` |
|       4 | 7233 | `								}` |
|       6 | 7234 | `							}else if( iNewVis >= 0 ){` |
|       - | 7235 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - | 7236 | `								ph7_class_method *pCopy;` |
|       3 | 7237 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 | 7238 | `								if( pCopy ){` |
|       3 | 7239 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 | 7240 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 | 7241 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 7242 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 7243 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - | 7244 | `									/* Replace the method in the class hash */` |
|       3 | 7245 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 | 7246 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 | 7247 | `								}` |
|       1 | 7248 | `							}` |
|       4 | 7249 | `						}` |
|       4 | 7250 | `						SXUNUSED(hasQual);` |
|       4 | 7251 | `					}` |
|      17 | 7252 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 7253 | `				}` |
|       - | 7254 | `			}` |
|      41 | 7255 | `			SySetRelease(&pUse->aTraits);` |
|      21 | 7256 | `		}` |
|       - | 7257 | `	}` |
|       - | 7258 | `	/* Install the class */` |
|   38228 | 7259 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   38228 | 7260 | `	if( rc == SXRET_OK ){` |
|       - | 7261 | `		ph7_class **apInterface;` |
|       - | 7262 | `		sxu32 n;` |
|   38228 | 7263 | `		if( pBase ){` |
|       - | 7264 | `			/* Inherit from base class and mark as a subclass */` |
|   24378 | 7265 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   12188 | 7266 | `		}` |
|   38228 | 7267 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   40962 | 7268 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 7269 | `			/* Implements one or more interface */` |
|    2736 | 7270 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    2736 | 7271 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7272 | `				break;` |
|       - | 7273 | `			}` |
|    1369 | 7274 | `		}` |
|       - | 7275 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   38228 | 7276 | `		if( rc == SXRET_OK ){` |
|   38228 | 7277 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   38228 | 7278 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 7279 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 7280 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 7281 | `				return SXERR_ABORT;` |
|       - | 7282 | `			}` |
|   19113 | 7283 | `		}` |
|       - | 7284 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   38228 | 7285 | `		if( rc == SXRET_OK ){` |
|   38228 | 7286 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   38228 | 7287 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 7288 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 7289 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 7290 | `				return SXERR_ABORT;` |
|       - | 7291 | `			}` |
|   19113 | 7292 | `		}` |
|   19113 | 7293 | `	}` |
|   38228 | 7294 | `	SySetRelease(&aUseEntries);` |
|   38228 | 7295 | `	SySetRelease(&aInterfaces);` |
|   38228 | 7296 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7297 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 7298 | `		return SXERR_ABORT;` |
|       - | 7299 | `	}` |
|   19113 | 7300 | `done:` |
|       - | 7301 | `	/* Point beyond the class body */` |
|   38232 | 7302 | `	pGen->pIn = &pEnd[1];` |
|   38232 | 7303 | `	pGen->pEnd = pTmp;` |
|   38232 | 7304 | `	return PH7_OK;` |
|   19117 | 7305 |  |
|       - | 7306 | `/*` |
|       - | 7307 | ` * Compile a user-defined abstract class.` |
|       - | 7308 | ` *  According to the PHP language reference manual` |
|       - | 7309 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 7310 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 7311 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 7312 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 7313 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 7314 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 7315 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 7316 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 7317 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 7318 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 7319 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 7320 | ` *   could differ.` |
|       - | 7321 | ` */` |
|      16 | 7322 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 | 7323 |  |
|       - | 7324 | `	sxi32 rc;` |
|      18 | 7325 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      18 | 7326 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      18 | 7327 | `	return rc;` |
|       2 | 7328 |  |
|       - | 7329 | `/*` |
|       - | 7330 | ` * Compile a user-defined final class.` |
|       - | 7331 | ` *  According to the PHP language reference manual` |
|       - | 7332 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - | 7333 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - | 7334 | ` *    final then it cannot be extended.` |
|       - | 7335 | ` */` |
|       2 | 7336 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 | 7337 |  |
|       - | 7338 | `	sxi32 rc;` |
|       3 | 7339 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 | 7340 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 | 7341 | `	return rc;` |
|       1 | 7342 |  |
|       - | 7343 | `/*` |
|       - | 7344 | ` * Compile a user-defined trait.` |
|       - | 7345 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - | 7346 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - | 7347 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - | 7348 | ` */` |
|      52 | 7349 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       2 | 7350 |  |
|      54 | 7351 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 7352 | `	ph7_class *pClass;` |
|       - | 7353 | `	SyToken *pEnd,*pTmp;` |
|       - | 7354 | `	sxi32 iProtection;` |
|       - | 7355 | `	sxi32 iAttrflags;` |
|       - | 7356 | `	SyString *pName;` |
|       - | 7357 | `	sxi32 nKwrd;` |
|       - | 7358 | `	sxi32 rc;` |
|       - | 7359 | `	/* Jump the 'trait' keyword */` |
|      54 | 7360 | `	pGen->pIn++;` |
|      54 | 7361 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 7362 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 | 7363 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7364 | `			return SXERR_ABORT;` |
|       - | 7365 | `		}` |
|     ! 0 | 7366 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 7367 | `			pGen->pIn++;` |
|     ! 0 | 7368 | `		}` |
|     ! 0 | 7369 | `		return SXRET_OK;` |
|       - | 7370 | `	}` |
|       - | 7371 | `	/* Extract trait name */` |
|      54 | 7372 | `	pName = &pGen->pIn->sData;` |
|      54 | 7373 | `	pGen->pIn++;` |
|       - | 7374 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 7375 | `		SyBlob sFQN;` |
|       - | 7376 | `		SyString sFQNStr;` |
|      54 | 7377 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      54 | 7378 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      54 | 7379 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      54 | 7380 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      54 | 7381 | `		SyBlobRelease(&sFQN);` |
|       - | 7382 | `	}` |
|      54 | 7383 | `	if( pClass == 0 ){` |
|     ! 0 | 7384 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 7385 | `		return SXERR_ABORT;` |
|       - | 7386 | `	}` |
|       - | 7387 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      54 | 7388 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 7389 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 | 7390 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 7391 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7392 | `			return SXERR_ABORT;` |
|       - | 7393 | `		}` |
|     ! 0 | 7394 | `		return SXRET_OK;` |
|       - | 7395 | `	}` |
|      54 | 7396 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      54 | 7397 | `	pEnd = 0;` |
|      54 | 7398 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      54 | 7399 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 7400 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 7401 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 7402 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7403 | `			return SXERR_ABORT;` |
|       - | 7404 | `		}` |
|     ! 0 | 7405 | `		return SXRET_OK;` |
|       - | 7406 | `	}` |
|       - | 7407 | `	/* Swap token stream */` |
|      54 | 7408 | `	pTmp = pGen->pEnd;` |
|      54 | 7409 | `	pGen->pEnd = pEnd;` |
|       - | 7410 | `	/* Mark as trait */` |
|      54 | 7411 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - | 7412 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      53 | 7413 | `	for(;;){` |
|     144 | 7414 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      21 | 7415 | `			pGen->pIn++;` |
|       1 | 7416 | `		}` |
|     124 | 7417 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      54 | 7418 | `			break;` |
|       - | 7419 | `		}` |
|      71 | 7420 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 7421 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7422 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 7423 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 7424 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7425 | `				return SXERR_ABORT;` |
|       - | 7426 | `			}` |
|     ! 0 | 7427 | `			goto done;` |
|       - | 7428 | `		}` |
|      71 | 7429 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      71 | 7430 | `		iAttrflags = 0;` |
|      71 | 7431 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      71 | 7432 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      71 | 7433 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 7434 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 | 7435 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 | 7436 | `				for(;;){` |
|       - | 7437 | `					ph7_class *pUsedTrait;` |
|       - | 7438 | `					SyString *pUsedName;` |
|       5 | 7439 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 7440 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 7441 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 7442 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 7443 | `							return SXERR_ABORT;` |
|       - | 7444 | `						}` |
|     ! 0 | 7445 | `						break;` |
|       - | 7446 | `					}` |
|       5 | 7447 | `					pUsedName = &pGen->pIn->sData;` |
|       - | 7448 | `					{` |
|       - | 7449 | `						SyBlob sResolved;` |
|       5 | 7450 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 | 7451 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 | 7452 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 | 7453 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 | 7454 | `						SyBlobRelease(&sResolved);` |
|       - | 7455 | `					}` |
|       5 | 7456 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 7457 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 7458 | `					}` |
|       5 | 7459 | `					if( pUsedTrait == 0 ){` |
|       4 | 7460 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 | 7461 | `							"'%z' is not a trait",pUsedName);` |
|       3 | 7462 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 7463 | `							return SXERR_ABORT;` |
|       - | 7464 | `						}` |
|       2 | 7465 | `					}else{` |
|       3 | 7466 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - | 7467 | `					}` |
|       5 | 7468 | `					pGen->pIn++;` |
|       5 | 7469 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 | 7470 | `						break;` |
|       - | 7471 | `					}` |
|     ! 0 | 7472 | `					pGen->pIn++;` |
|     ! 0 | 7473 | `				}` |
|       5 | 7474 | `				continue;` |
|       - | 7475 | `			}` |
|      67 | 7476 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      63 | 7477 | `				iProtection = nKwrd;` |
|      63 | 7478 | `				pGen->pIn++;` |
|      63 | 7479 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 7480 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7481 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 7482 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 7483 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7484 | `						return SXERR_ABORT;` |
|       - | 7485 | `					}` |
|     ! 0 | 7486 | `					goto done;` |
|       - | 7487 | `				}` |
|      63 | 7488 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 | 7489 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 | 7490 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 7491 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 7492 | `							return SXERR_ABORT;` |
|       - | 7493 | `						}` |
|     ! 0 | 7494 | `						goto done;` |
|       - | 7495 | `					}` |
|      11 | 7496 | `					continue;` |
|       - | 7497 | `				}` |
|      53 | 7498 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      26 | 7499 | `			}` |
|      57 | 7500 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 7501 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7502 | `					"Traits cannot have constants");` |
|     ! 0 | 7503 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7504 | `					return SXERR_ABORT;` |
|       - | 7505 | `				}` |
|     ! 0 | 7506 | `				goto done;` |
|     ! 0 | 7507 | `			}else{` |
|      57 | 7508 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 | 7509 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 | 7510 | `					pGen->pIn++;` |
|       5 | 7511 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 7512 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 7513 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 7514 | `							iProtection = nKwrd;` |
|     ! 0 | 7515 | `							pGen->pIn++;` |
|     ! 0 | 7516 | `						}` |
|       1 | 7517 | `					}` |
|       5 | 7518 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 7519 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7520 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 7521 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 7522 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 7523 | `							return SXERR_ABORT;` |
|       - | 7524 | `						}` |
|     ! 0 | 7525 | `						goto done;` |
|       - | 7526 | `					}` |
|       5 | 7527 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 7528 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 7529 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 7530 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 7531 | `								return SXERR_ABORT;` |
|       - | 7532 | `							}` |
|     ! 0 | 7533 | `							goto done;` |
|       - | 7534 | `						}` |
|       3 | 7535 | `						continue;` |
|       - | 7536 | `					}` |
|       3 | 7537 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      54 | 7538 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       5 | 7539 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       5 | 7540 | `					pGen->pIn++;` |
|       5 | 7541 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 | 7542 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 7543 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 7544 | `							iProtection = nKwrd;` |
|       5 | 7545 | `							pGen->pIn++;` |
|       2 | 7546 | `						}` |
|       2 | 7547 | `					}` |
|       5 | 7548 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 7549 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 7550 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7551 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 7552 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 7553 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 7554 | `							return SXERR_ABORT;` |
|       - | 7555 | `						}` |
|     ! 0 | 7556 | `						goto done;` |
|       - | 7557 | `					}` |
|       5 | 7558 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 7559 | `				}` |
|      55 | 7560 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 7561 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7562 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 7563 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 7564 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7565 | `						return SXERR_ABORT;` |
|       - | 7566 | `					}` |
|     ! 0 | 7567 | `					goto done;` |
|       - | 7568 | `				}` |
|      55 | 7569 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 7570 | `					pGen->pIn++;` |
|     ! 0 | 7571 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 7572 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7573 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 7574 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 7575 | `							return SXERR_ABORT;` |
|       - | 7576 | `						}` |
|     ! 0 | 7577 | `						goto done;` |
|       - | 7578 | `					}` |
|     ! 0 | 7579 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 7580 | `				}else{` |
|      55 | 7581 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 7582 | `				}` |
|      55 | 7583 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7584 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7585 | `						return SXERR_ABORT;` |
|       - | 7586 | `					}` |
|     ! 0 | 7587 | `					goto done;` |
|       - | 7588 | `				}` |
|       - | 7589 | `			}` |
|      28 | 7590 | `		}else{` |
|     ! 0 | 7591 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 7592 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7593 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7594 | `					return SXERR_ABORT;` |
|       - | 7595 | `				}` |
|     ! 0 | 7596 | `				goto done;` |
|       - | 7597 | `			}` |
|       - | 7598 | `		}` |
|       1 | 7599 | `	}` |
|       - | 7600 | `	/* Install the trait */` |
|      54 | 7601 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      54 | 7602 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7603 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 7604 | `		return SXERR_ABORT;` |
|       - | 7605 | `	}` |
|      26 | 7606 | `done:` |
|       - | 7607 | `	/* Point beyond the trait body */` |
|      54 | 7608 | `	pGen->pIn = &pEnd[1];` |
|      54 | 7609 | `	pGen->pEnd = pTmp;` |
|      54 | 7610 | `	return PH7_OK;` |
|      28 | 7611 |  |
|       - | 7612 | `/*` |
|       - | 7613 | ` * Compile a user-defined class.` |
|       - | 7614 | ` *  According to the PHP language reference manual` |
|       - | 7615 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 7616 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 7617 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 7618 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 7619 | ` *   and functions (called "methods").` |
|       - | 7620 | ` */` |
|   38212 | 7621 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 | 7622 |  |
|       - | 7623 | `	sxi32 rc;` |
|   38214 | 7624 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   38214 | 7625 | `	return rc;` |
|       2 | 7626 |  |
|       - | 7627 | `/*` |
|       - | 7628 | ` * Exception handling.` |
|       - | 7629 | ` *  According to the PHP language reference manual` |
|       - | 7630 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 7631 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 7632 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 7633 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 7634 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 7635 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 7636 | ` *    (or re-thrown) within a catch block.` |
|       - | 7637 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 7638 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 7639 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 7640 | ` *    been defined with set_exception_handler().` |
|       - | 7641 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 7642 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 7643 | ` */` |
|       - | 7644 | `/*` |
|       - | 7645 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 7646 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 7647 | ` * indicates failure.` |
|       - | 7648 | ` */` |
|    8148 | 7649 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 7650 |  |
|    8150 | 7651 | `	sxi32 rc = SXRET_OK;` |
|    8150 | 7652 | `	if( pRoot->pOp ){` |
|    8144 | 7653 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */` |
|    4074 | 7654 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 7655 | `			/* Unexpected expression */` |
|     ! 0 | 7656 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 7657 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 7658 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 7659 | `				rc = SXERR_INVALID;` |
|     ! 0 | 7660 | `			}` |
|       2 | 7661 | `		}` |
|    4077 | 7662 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 7663 | `		/* Unexpected expression */` |
|     ! 0 | 7664 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 7665 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 7666 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 7667 | `			rc = SXERR_INVALID;` |
|     ! 0 | 7668 | `		}` |
|     ! 0 | 7669 | `	}` |
|    8150 | 7670 | `	return rc;` |
|       2 | 7671 |  |
|       - | 7672 | `/*` |
|       - | 7673 | ` * Compile a 'throw' statement.` |
|       - | 7674 | ` * throw: This is how you trigger an exception.` |
|       - | 7675 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 7676 | ` */` |
|    8148 | 7677 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 | 7678 |  |
|    8150 | 7679 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 7680 | `	GenBlock *pBlock;` |
|       - | 7681 | `	sxu32 nIdx;` |
|       - | 7682 | `	sxi32 rc;` |
|    8150 | 7683 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 7684 | `	/* Compile the expression */` |
|    8150 | 7685 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    8150 | 7686 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 7687 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 7688 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7689 | `			return SXERR_ABORT;` |
|       - | 7690 | `		}` |
|     ! 0 | 7691 | `		return SXRET_OK;` |
|       - | 7692 | `	}` |
|    8150 | 7693 | `	pBlock = pGen->pCurrent;` |
|       - | 7694 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   37866 | 7695 | `	while(pBlock->pParent){` |
|   37862 | 7696 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    8146 | 7697 | `			break;` |
|       - | 7698 | `		}` |
|       - | 7699 | `		/* Point to the parent block */` |
|   29718 | 7700 | `		pBlock = pBlock->pParent;` |
|       2 | 7701 | `	}` |
|       - | 7702 | `	/* Emit the throw instruction */` |
|    8150 | 7703 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 7704 | `	/* Emit the jump */` |
|    8150 | 7705 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    8150 | 7706 | `	return SXRET_OK;` |
|    4076 | 7707 |  |
|       - | 7708 | `/*` |
|       - | 7709 | ` * Compile a 'catch' block.` |
|       - | 7710 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 7711 | ` * an object containing the exception information.` |
|       - | 7712 | ` */` |
|      98 | 7713 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 | 7714 |  |
|     100 | 7715 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 7716 | `	ph7_exception_block sCatch;` |
|       - | 7717 | `	SySet *pInstrContainer;` |
|       - | 7718 | `	SyString sClassName;` |
|       - | 7719 | `	GenBlock *pCatch;` |
|       - | 7720 | `	SyToken *pToken;` |
|       - | 7721 | `	SyString *pName;` |
|       - | 7722 | `	char *zDup;` |
|       - | 7723 | `	sxi32 rc;` |
|     100 | 7724 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 7725 | `	/* Zero the structure */` |
|     100 | 7726 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 7727 | `	/* Initialize fields */` |
|     100 | 7728 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     100 | 7729 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     100 | 7730 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 7731 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 7732 | `			pToken = pGen->pIn;` |
|     ! 0 | 7733 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 7734 | `				pToken--;` |
|     ! 0 | 7735 | `			}` |
|     ! 0 | 7736 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 7737 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 7738 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 7739 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7740 | `				return SXERR_ABORT;` |
|       - | 7741 | `			}` |
|     ! 0 | 7742 | `			return SXERR_INVALID;` |
|       - | 7743 | `	}` |
|       - | 7744 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     100 | 7745 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|      61 | 7746 | `	for(;;){` |
|     124 | 7747 | `		int isAbsolute = 0;` |
|       - | 7748 | `		SyBlob sName;` |
|     124 | 7749 | `		SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|       - | 7750 | `		/* Accept optional leading '\' for fully-qualified names */` |
|     124 | 7751 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|       7 | 7752 | `			isAbsolute = 1;` |
|       7 | 7753 | `			pGen->pIn++;` |
|       3 | 7754 | `		}` |
|     124 | 7755 | `		if( pGen->pIn >= pGen->pEnd \|\|` |
|     122 | 7756 | `			(pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       5 | 7757 | `			SyBlobRelease(&sName);` |
|       5 | 7758 | `			pToken = pGen->pIn;` |
|       5 | 7759 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 7760 | `				pToken--;` |
|     ! 0 | 7761 | `			}` |
|       7 | 7762 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 7763 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 7764 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       5 | 7765 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7766 | `				return SXERR_ABORT;` |
|       - | 7767 | `			}` |
|       5 | 7768 | `			return SXERR_INVALID;` |
|       - | 7769 | `		}` |
|       - | 7770 | `		/* Collect namespace-qualified name: ID [\ ID]* */` |
|     120 | 7771 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|     120 | 7772 | `		pGen->pIn++;` |
|     183 | 7773 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|      67 | 7774 | `			&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       5 | 7775 | `			SyBlobAppend(&sName,"\\",1);` |
|       5 | 7776 | `			pGen->pIn++; /* Skip '\' separator */` |
|       5 | 7777 | `			SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       5 | 7778 | `			pGen->pIn++;` |
|       1 | 7779 | `		}` |
|       - | 7780 | `		/* Resolve through namespace/imports for non-absolute names */` |
|     120 | 7781 | `		if( !isAbsolute ){` |
|       - | 7782 | `			SyString sRaw;` |
|       - | 7783 | `			SyBlob sResolved;` |
|     114 | 7784 | `			SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     114 | 7785 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     114 | 7786 | `			GenStateResolveName(pGen,&sRaw,&sResolved);` |
|     170 | 7787 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     112 | 7788 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     114 | 7789 | `			SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     114 | 7790 | `			SyBlobRelease(&sResolved);` |
|      58 | 7791 | `		}else{` |
|       - | 7792 | `			/* Absolute name: use as-is without namespace prefix */` |
|      10 | 7793 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       6 | 7794 | `				(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|       7 | 7795 | `			SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sName));` |
|       - | 7796 | `		}` |
|     120 | 7797 | `		SyBlobRelease(&sName);` |
|     120 | 7798 | `		if( zDup == 0 ){` |
|     ! 0 | 7799 | `			goto Mem;` |
|       - | 7800 | `		}` |
|     120 | 7801 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     120 | 7802 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7803 | `			goto Mem;` |
|       - | 7804 | `		}` |
|       - | 7805 | `		/* Check for '\|' (multi-catch separator) */` |
|     130 | 7806 | `		if( pGen->pIn < pGen->pEnd &&` |
|     118 | 7807 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      26 | 7808 | `			pGen->pIn->sData.nByte == 1 &&` |
|      24 | 7809 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      26 | 7810 | `			pGen->pIn++; /* Consume the '\|' */` |
|      26 | 7811 | `			continue;` |
|       - | 7812 | `		}` |
|      96 | 7813 | `		break;` |
|     ! 0 | 7814 | `	}` |
|     141 | 7815 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|      96 | 7816 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 7817 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 7818 | `			pToken = pGen->pIn;` |
|     ! 0 | 7819 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 7820 | `				pToken--;` |
|     ! 0 | 7821 | `			}` |
|     ! 0 | 7822 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 7823 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 7824 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 7825 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7826 | `				return SXERR_ABORT;` |
|       - | 7827 | `			}` |
|     ! 0 | 7828 | `			return SXERR_INVALID;` |
|       - | 7829 | `	}` |
|      96 | 7830 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 7831 | `	/* Duplicate instance name */` |
|      96 | 7832 | `	pName = &pGen->pIn->sData;` |
|      96 | 7833 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      96 | 7834 | `	if( zDup == 0 ){` |
|     ! 0 | 7835 | `		goto Mem;` |
|       - | 7836 | `	}` |
|      96 | 7837 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|      96 | 7838 | `	pGen->pIn++;` |
|      96 | 7839 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 7840 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 7841 | `		pToken = pGen->pIn;` |
|     ! 0 | 7842 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 7843 | `			pToken--;` |
|     ! 0 | 7844 | `		}` |
|     ! 0 | 7845 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 7846 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 7847 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 7848 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7849 | `			return SXERR_ABORT;` |
|       - | 7850 | `		}` |
|     ! 0 | 7851 | `		return SXERR_INVALID;` |
|       - | 7852 | `	}` |
|       - | 7853 | `	/* Compile the block */` |
|      96 | 7854 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 7855 | `	/* Create the catch block */` |
|      96 | 7856 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|      96 | 7857 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7858 | `		return SXERR_ABORT;` |
|       - | 7859 | `	}` |
|       - | 7860 | `	/* Swap bytecode container */` |
|      96 | 7861 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      96 | 7862 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 7863 | `	/* Compile the block */` |
|      96 | 7864 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 7865 | `	/* Fix forward jumps now the destination is resolved  */` |
|      96 | 7866 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7867 | `	/* Emit the DONE instruction */` |
|      96 | 7868 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 7869 | `	/* Leave the block */` |
|      96 | 7870 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 7871 | `	/* Restore the default container */` |
|      96 | 7872 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 7873 | `	/* Install the catch block */` |
|      96 | 7874 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      96 | 7875 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7876 | `		goto Mem;` |
|       - | 7877 | `	}` |
|      96 | 7878 | `	return SXRET_OK;` |
|     ! 0 | 7879 | `Mem:` |
|     ! 0 | 7880 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 7881 | `	return SXERR_ABORT;` |
|      51 | 7882 |  |
|       - | 7883 | `/*` |
|       - | 7884 | ` * Compile a 'try' block.` |
|       - | 7885 | ` * A function using an exception should be in a "try" block.` |
|       - | 7886 | ` * If the exception does not trigger, the code will continue` |
|       - | 7887 | ` * as normal. However if the exception triggers, an exception` |
|       - | 7888 | ` * is "thrown".` |
|       - | 7889 | ` */` |
|     106 | 7890 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 | 7891 |  |
|       - | 7892 | `	ph7_exception *pException;` |
|     108 | 7893 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 7894 | `	GenBlock *pTry;` |
|       - | 7895 | `	sxu32 nJmpIdx;` |
|       - | 7896 | `	sxi32 rc;` |
|       - | 7897 | `	/* Create the exception container */` |
|     108 | 7898 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     108 | 7899 | `	if( pException == 0 ){` |
|     ! 0 | 7900 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 7901 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 7902 | `		return SXERR_ABORT;` |
|       - | 7903 | `	}` |
|       - | 7904 | `	/* Zero the structure */` |
|     108 | 7905 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 7906 | `	/* Initialize fields */` |
|     108 | 7907 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     108 | 7908 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     108 | 7909 | `	pException->iHasFinally = 0;` |
|     108 | 7910 | `	pException->iFinallyDone = 0;` |
|     108 | 7911 | `	pException->pVm = pGen->pVm;` |
|       - | 7912 | `	/* Create the try block */` |
|     108 | 7913 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     108 | 7914 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7915 | `		return SXERR_ABORT;` |
|       - | 7916 | `	}` |
|       - | 7917 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     108 | 7918 | `	pTry->pUserData = pException;` |
|       - | 7919 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     108 | 7920 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 7921 | `	/* Fix the jump later when the destination is resolved */` |
|     108 | 7922 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     108 | 7923 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 7924 | `	/* Compile the block */` |
|     108 | 7925 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     108 | 7926 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 7927 | `		return SXERR_ABORT;` |
|       - | 7928 | `	}` |
|       - | 7929 | `	/* Fix forward jumps now the destination is resolved */` |
|     108 | 7930 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7931 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     108 | 7932 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 7933 | `	/* Leave the block */` |
|     108 | 7934 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 7935 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     108 | 7936 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     104 | 7937 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 7938 | `		/* Compile one or more catch blocks */` |
|      96 | 7939 | `		for(;;){` |
|     192 | 7940 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     154 | 7941 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      49 | 7942 | `					break;` |
|       - | 7943 | `			}` |
|     100 | 7944 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     100 | 7945 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7946 | `				return SXERR_ABORT;` |
|       - | 7947 | `			}` |
|       2 | 7948 | `		}` |
|      47 | 7949 | `	}` |
|       - | 7950 | `	/* Compile optional finally block */` |
|     108 | 7951 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      56 | 7952 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 7953 | `		SySet *pInstrContainer;` |
|       - | 7954 | `		GenBlock *pFinBlock;` |
|      32 | 7955 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 7956 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      32 | 7957 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      32 | 7958 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7959 | `			return SXERR_ABORT;` |
|       - | 7960 | `		}` |
|       - | 7961 | `		/* Swap bytecode container */` |
|      32 | 7962 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 | 7963 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 7964 | `		/* Compile the finally body */` |
|      32 | 7965 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      32 | 7966 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7967 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 7968 | `			return SXERR_ABORT;` |
|       - | 7969 | `		}` |
|       - | 7970 | `		/* Fix forward jumps now the destination is resolved */` |
|      32 | 7971 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7972 | `		/* Emit DONE to terminate the finally block */` |
|      32 | 7973 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 7974 | `		/* Leave the block */` |
|      32 | 7975 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 7976 | `		/* Restore the default container */` |
|      32 | 7977 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 | 7978 | `		pException->iHasFinally = 1;` |
|      15 | 7979 | `	}` |
|       - | 7980 | `	/* Must have at least one catch or finally */` |
|     108 | 7981 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       7 | 7982 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 7983 | `			"Cannot use try without catch or finally");` |
|       7 | 7984 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7985 | `			return SXERR_ABORT;` |
|       - | 7986 | `		}` |
|       3 | 7987 | `	}` |
|     108 | 7988 | `	return SXRET_OK;` |
|      55 | 7989 |  |
|       - | 7990 | `/*` |
|       - | 7991 | ` * Compile a switch block.` |
|       - | 7992 | ` *  (See block-comment below for more information)` |
|       - | 7993 | ` */` |
|     108 | 7994 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 | 7995 |  |
|     110 | 7996 | `	sxi32 rc = SXRET_OK;` |
|     110 | 7997 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 7998 | `		/* Unexpected token */` |
|     ! 0 | 7999 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 8000 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 8001 | `			return SXERR_ABORT;` |
|       - | 8002 | `		}` |
|     ! 0 | 8003 | `		pGen->pIn++;` |
|     ! 0 | 8004 | `	}` |
|     110 | 8005 | `	pGen->pIn++;` |
|       - | 8006 | `	/* First instruction to execute in this block. */` |
|     110 | 8007 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 8008 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 8009 | `	 * or the '}' token */` |
|     182 | 8010 | `	for(;;){` |
|     366 | 8011 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 8012 | `			/* No more input to process */` |
|     ! 0 | 8013 | `			break;` |
|       - | 8014 | `		}` |
|     366 | 8015 | `		rc = SXRET_OK;` |
|     366 | 8016 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      70 | 8017 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      28 | 8018 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 8019 | `					/* Unexpected token */` |
|     ! 0 | 8020 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 8021 | `						&pGen->pIn->sData);` |
|     ! 0 | 8022 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 8023 | `						return SXERR_ABORT;` |
|       - | 8024 | `					}` |
|       - | 8025 | `					/* FALL THROUGH */` |
|     ! 0 | 8026 | `				}` |
|      28 | 8027 | `				rc = SXERR_EOF;` |
|      28 | 8028 | `				break;` |
|       - | 8029 | `			}` |
|      23 | 8030 | `		}else{` |
|       - | 8031 | `			sxi32 nKwrd;` |
|       - | 8032 | `			/* Extract the keyword */` |
|     298 | 8033 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     298 | 8034 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      42 | 8035 | `				break;` |
|       - | 8036 | `			}` |
|     218 | 8037 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 8038 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 8039 | `					/* Unexpected token */` |
|     ! 0 | 8040 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 8041 | `						&pGen->pIn->sData);` |
|     ! 0 | 8042 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 8043 | `						return SXERR_ABORT;` |
|       - | 8044 | `					}` |
|       - | 8045 | `					/* FALL THROUGH */` |
|     ! 0 | 8046 | `				}` |
|       - | 8047 | `				/* Block compiled */` |
|       3 | 8048 | `				break;` |
|       - | 8049 | `			}` |
|       - | 8050 | `		}` |
|       - | 8051 | `		/* Compile block */` |
|     258 | 8052 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     258 | 8053 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 8054 | `			return SXERR_ABORT;` |
|       - | 8055 | `		}` |
|       2 | 8056 | `	}` |
|     110 | 8057 | `	return rc;` |
|      56 | 8058 |  |
|       - | 8059 | `/*` |
|       - | 8060 | ` * Compile a case eXpression.` |
|       - | 8061 | ` *  (See block-comment below for more information)` |
|       - | 8062 | ` */` |
|      88 | 8063 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 | 8064 |  |
|       - | 8065 | `	SySet *pInstrContainer;` |
|       - | 8066 | `	SyToken *pEnd,*pTmp;` |
|      90 | 8067 | `	sxi32 iNest = 0;` |
|       - | 8068 | `	sxi32 rc;` |
|       - | 8069 | `	/* Delimit the expression */` |
|      90 | 8070 | `	pEnd = pGen->pIn;` |
|     186 | 8071 | `	while( pEnd < pGen->pEnd ){` |
|     186 | 8072 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 8073 | `			/* Increment nesting level */` |
|       3 | 8074 | `			iNest++;` |
|     185 | 8075 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 8076 | `			/* Decrement nesting level */` |
|       3 | 8077 | `			iNest--;` |
|     183 | 8078 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      90 | 8079 | `			break;` |
|       - | 8080 | `		}` |
|      98 | 8081 | `		pEnd++;` |
|       2 | 8082 | `	}` |
|      90 | 8083 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 8084 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 8085 | `		if( rc == SXERR_ABORT ){` |
|       - | 8086 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 8087 | `			return SXERR_ABORT;` |
|       - | 8088 | `		}` |
|     ! 0 | 8089 | `	}` |
|       - | 8090 | `	/* Swap token stream */` |
|      90 | 8091 | `	pTmp = pGen->pEnd;` |
|      90 | 8092 | `	pGen->pEnd = pEnd;` |
|      90 | 8093 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      90 | 8094 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      90 | 8095 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 8096 | `	/* Emit the done instruction */` |
|      90 | 8097 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      90 | 8098 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 8099 | `	/* Update token stream */` |
|      90 | 8100 | `	pGen->pIn  = pEnd;` |
|      90 | 8101 | `	pGen->pEnd = pTmp;` |
|      90 | 8102 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 8103 | `		return SXERR_ABORT;` |
|       - | 8104 | `	}` |
|      90 | 8105 | `	return SXRET_OK;` |
|      46 | 8106 |  |
|       - | 8107 | `/*` |
|       - | 8108 | ` * Compile the smart switch statement.` |
|       - | 8109 | ` * According to the PHP language reference manual` |
|       - | 8110 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 8111 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 8112 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 8113 | ` *  This is exactly what the switch statement is for.` |
|       - | 8114 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 8115 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 8116 | ` *  of the outer loop, use continue 2.` |
|       - | 8117 | ` *  Note that switch/case does loose comparision.` |
|       - | 8118 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 8119 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 8120 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 8121 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 8122 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 8123 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 8124 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 8125 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 8126 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 8127 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 8128 | ` *  list for the next case.` |
|       - | 8129 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 8130 | ` *  or floating-point numbers and strings.` |
|       - | 8131 | ` */` |
|      28 | 8132 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 | 8133 |  |
|       - | 8134 | `	GenBlock *pSwitchBlock;` |
|       - | 8135 | `	SyToken *pTmp,*pEnd;` |
|       - | 8136 | `	ph7_switch *pSwitch;` |
|       - | 8137 | `	sxu32 nToken;` |
|       - | 8138 | `	sxu32 nLine;` |
|       - | 8139 | `	sxi32 rc;` |
|      30 | 8140 | `	nLine = pGen->pIn->nLine;` |
|       - | 8141 | `	/* Jump the 'switch' keyword */` |
|      30 | 8142 | `	pGen->pIn++;` |
|      30 | 8143 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 8144 | `		/* Syntax error */` |
|     ! 0 | 8145 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 8146 | `		if( rc == SXERR_ABORT ){` |
|       - | 8147 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 8148 | `			return SXERR_ABORT;` |
|       - | 8149 | `		}` |
|     ! 0 | 8150 | `		goto Synchronize;` |
|       - | 8151 | `	}` |
|       - | 8152 | `	/* Jump the left parenthesis '(' */` |
|      30 | 8153 | `	pGen->pIn++;` |
|      30 | 8154 | `	pEnd = 0; /* cc warning */` |
|       - | 8155 | `	/* Create the loop block */` |
|      44 | 8156 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 | 8157 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      30 | 8158 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 8159 | `		return SXERR_ABORT;` |
|       - | 8160 | `	}` |
|       - | 8161 | `	/* Delimit the condition */` |
|      30 | 8162 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      30 | 8163 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 8164 | `		/* Empty expression */` |
|     ! 0 | 8165 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 8166 | `		if( rc == SXERR_ABORT ){` |
|       - | 8167 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 8168 | `			return SXERR_ABORT;` |
|       - | 8169 | `		}` |
|     ! 0 | 8170 | `	}` |
|       - | 8171 | `	/* Swap token streams */` |
|      30 | 8172 | `	pTmp = pGen->pEnd;` |
|      30 | 8173 | `	pGen->pEnd = pEnd;` |
|       - | 8174 | `	/* Compile the expression */` |
|      30 | 8175 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 | 8176 | `	if( rc == SXERR_ABORT ){` |
|       - | 8177 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 8178 | `		return SXERR_ABORT;` |
|       - | 8179 | `	}` |
|       - | 8180 | `	/* Update token stream */` |
|      30 | 8181 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 8182 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 8183 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 8184 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 8185 | `			return SXERR_ABORT;` |
|       - | 8186 | `		}` |
|     ! 0 | 8187 | `		pGen->pIn++;` |
|     ! 0 | 8188 | `	}` |
|      30 | 8189 | `	pGen->pIn  = &pEnd[1];` |
|      30 | 8190 | `	pGen->pEnd = pTmp;` |
|      30 | 8191 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 | 8192 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 8193 | `			pTmp = pGen->pIn;` |
|     ! 0 | 8194 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 8195 | `				pTmp--;` |
|     ! 0 | 8196 | `			}` |
|       - | 8197 | `			/* Unexpected token */` |
|     ! 0 | 8198 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 8199 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 8200 | `				return SXERR_ABORT;` |
|       - | 8201 | `			}` |
|     ! 0 | 8202 | `			goto Synchronize;` |
|       - | 8203 | `	}` |
|       - | 8204 | `	/* Set the delimiter token */` |
|      30 | 8205 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 8206 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 8207 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 8208 | `	}else{` |
|      28 | 8209 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 8210 | `	}` |
|      30 | 8211 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 8212 | `	/* Create the switch blocks container */` |
|      30 | 8213 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      30 | 8214 | `	if( pSwitch == 0 ){` |
|       - | 8215 | `		/* Abort compilation */` |
|     ! 0 | 8216 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 8217 | `		return SXERR_ABORT;` |
|       - | 8218 | `	}` |
|       - | 8219 | `	/* Zero the structure */` |
|      30 | 8220 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 8221 | `	/* Initialize fields */` |
|      30 | 8222 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 8223 | `	/* Emit the switch instruction */` |
|      30 | 8224 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 8225 | `	/* Compile case blocks */` |
|      96 | 8226 | `	for(;;){` |
|       - | 8227 | `		sxu32 nKwrd;` |
|     112 | 8228 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 8229 | `			/* No more input to process */` |
|     ! 0 | 8230 | `			break;` |
|       - | 8231 | `		}` |
|     112 | 8232 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 8233 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 8234 | `				/* Unexpected token */` |
|     ! 0 | 8235 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 8236 | `					&pGen->pIn->sData);` |
|     ! 0 | 8237 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 8238 | `					return SXERR_ABORT;` |
|       - | 8239 | `				}` |
|       - | 8240 | `				/* FALL THROUGH */` |
|     ! 0 | 8241 | `			}` |
|       - | 8242 | `			/* Block compiled */` |
|     ! 0 | 8243 | `			break;` |
|       - | 8244 | `		}` |
|       - | 8245 | `		/* Extract the keyword */` |
|     112 | 8246 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     112 | 8247 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 8248 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 8249 | `				/* Unexpected token */` |
|     ! 0 | 8250 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 8251 | `					&pGen->pIn->sData);` |
|     ! 0 | 8252 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 8253 | `					return SXERR_ABORT;` |
|       - | 8254 | `				}` |
|       - | 8255 | `				/* FALL THROUGH */` |
|     ! 0 | 8256 | `			}` |
|       - | 8257 | `			/* Block compiled */` |
|       3 | 8258 | `			break;` |
|       - | 8259 | `		}` |
|     110 | 8260 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 8261 | `			/*` |
|       - | 8262 | `			 * Accroding to the PHP language reference manual` |
|       - | 8263 | `			 *  A special case is the default case. This case matches anything` |
|       - | 8264 | `			 *  that wasn't matched by the other cases.` |
|       - | 8265 | `			 */` |
|      22 | 8266 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 8267 | `				/* Default case already compiled */` |
|     ! 0 | 8268 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 8269 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 8270 | `					return SXERR_ABORT;` |
|       - | 8271 | `				}` |
|     ! 0 | 8272 | `			}` |
|      22 | 8273 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 8274 | `			/* Compile the default block */` |
|      22 | 8275 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      22 | 8276 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 8277 | `				return SXERR_ABORT;` |
|      22 | 8278 | `			}else if( rc == SXERR_EOF ){` |
|      20 | 8279 | `				break;` |
|       1 | 8280 | `			}` |
|      91 | 8281 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 8282 | `			ph7_case_expr sCase;` |
|       - | 8283 | `			/* Standard case block */` |
|      90 | 8284 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 8285 | `			/* initialize the structure */` |
|      90 | 8286 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 8287 | `			/* Compile the case expression */` |
|      90 | 8288 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      90 | 8289 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 8290 | `				return SXERR_ABORT;` |
|       - | 8291 | `			}` |
|       - | 8292 | `			/* Compile the case block */` |
|      90 | 8293 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 8294 | `			/* Insert in the switch container */` |
|      90 | 8295 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      90 | 8296 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 8297 | `				return SXERR_ABORT;` |
|      90 | 8298 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 8299 | `				break;` |
|       - | 8300 | `			}` |
|      42 | 8301 | `		}else{` |
|       - | 8302 | `			/* Unexpected token */` |
|     ! 0 | 8303 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 8304 | `				&pGen->pIn->sData);` |
|     ! 0 | 8305 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 8306 | `				return SXERR_ABORT;` |
|       - | 8307 | `			}` |
|     ! 0 | 8308 | `			break;` |
|       - | 8309 | `		}` |
|       2 | 8310 | `	}` |
|       - | 8311 | `	/* Fix all jumps now the destination is resolved */` |
|      30 | 8312 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      30 | 8313 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 8314 | `	/* Release the loop block */` |
|      30 | 8315 | `	GenStateLeaveBlock(pGen,0);` |
|      30 | 8316 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 8317 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      30 | 8318 | `		pGen->pIn++;` |
|      14 | 8319 | `	}` |
|       - | 8320 | `	/* Statement successfully compiled */` |
|      30 | 8321 | `	return SXRET_OK;` |
|     ! 0 | 8322 | `Synchronize:` |
|       - | 8323 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 8324 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 8325 | `		pGen->pIn++;` |
|     ! 0 | 8326 | `	}` |
|     ! 0 | 8327 | `	return SXRET_OK;` |
|      16 | 8328 |  |
|       - | 8329 | `/*` |
|       - | 8330 | ` * Generate bytecode for a given expression tree.` |
|       - | 8331 | ` * If something goes wrong while generating bytecode` |
|       - | 8332 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 8333 | ` * this function takes care of generating the appropriate` |
|       - | 8334 | ` * error message.` |
|       - | 8335 | ` */` |
| 2426674 | 8336 | `static sxi32 GenStateEmitExprCode(` |
|       - | 8337 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 8338 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 8339 | `	sxi32 iFlags /* Control flags */` |
|       - | 8340 | `	)` |
|       2 | 8341 |  |
|       - | 8342 | `	VmInstr *pInstr;` |
|       - | 8343 | `	sxu32 nJmpIdx;` |
| 2426676 | 8344 | `	sxi32 iP1 = 0;` |
| 2426676 | 8345 | `	sxu32 iP2 = 0;` |
| 2426676 | 8346 | `	void *p3  = 0;` |
|       - | 8347 | `	sxi32 iVmOp;` |
|       - | 8348 | `	sxi32 rc;` |
| 2426676 | 8349 | `	if( pNode->xCode ){` |
|       - | 8350 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 8351 | `		/* Compile node */` |
| 1504198 | 8352 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1504198 | 8353 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1504198 | 8354 | `		RE_SWAP_DELIMITER(pGen);` |
| 1504198 | 8355 | `		return rc;` |
|       - | 8356 | `	}` |
|  922480 | 8357 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 8358 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 8359 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 8360 | `		return SXERR_ABORT;` |
|       - | 8361 | `	}` |
|  922480 | 8362 | `	iVmOp = pNode->pOp->iVmOp;` |
|  922480 | 8363 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      47 | 8364 | `		sxu32 nJmp = 0;` |
|       - | 8365 | `		VmInstr *pInstrFix;` |
|       - | 8366 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 8367 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 8368 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 8369 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 8370 | `		 * stack slot carries a writable nIdx. */` |
|      47 | 8371 | `		if( pNode->pRight ){` |
|      47 | 8372 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE);` |
|      47 | 8373 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 8374 | `				return rc;` |
|       - | 8375 | `			}` |
|       - | 8376 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 8377 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 8378 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 8379 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 8380 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 8381 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 8382 | `			 * cascade for the actual write path stays correct. */` |
|      47 | 8383 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      47 | 8384 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      19 | 8385 | `				pInstrFix->iP2 = 3;` |
|       9 | 8386 | `			}` |
|      23 | 8387 | `		}` |
|       - | 8388 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      47 | 8389 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 8390 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      47 | 8391 | `		if( pNode->pLeft ){` |
|      47 | 8392 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      47 | 8393 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 8394 | `				return rc;` |
|       - | 8395 | `			}` |
|      23 | 8396 | `		}` |
|       - | 8397 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      47 | 8398 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 8399 | `		/* Patch the short-circuit jump to land after the store. */` |
|      47 | 8400 | `		if( nJmp > 0 ){` |
|      47 | 8401 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      47 | 8402 | `			if( pInstrFix ){` |
|      47 | 8403 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      23 | 8404 | `			}` |
|      23 | 8405 | `		}` |
|      47 | 8406 | `		return SXRET_OK;` |
|       - | 8407 | `	}` |
|  922434 | 8408 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 8409 | `		sxu32 nJz,nJmp;` |
|       - | 8410 | `		/* Ternary operator require special handling */` |
|       - | 8411 | `		/* Phase#1: Compile the condition */` |
|    1900 | 8412 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    1900 | 8413 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 8414 | `			return rc;` |
|       - | 8415 | `		}` |
|    1900 | 8416 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    1900 | 8417 | `		if( pNode->pLeft ){` |
|       - | 8418 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 8419 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1832 | 8420 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 8421 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1832 | 8422 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1832 | 8423 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 8424 | `				return rc;` |
|       - | 8425 | `			}` |
|     917 | 8426 | `		}else{` |
|       - | 8427 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 8428 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 8429 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 8430 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 8431 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 8432 | `		}` |
|       - | 8433 | `		/* Phase#4: Emit the unconditional jump */` |
|    1900 | 8434 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 8435 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    1900 | 8436 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    1900 | 8437 | `		if( pInstr ){` |
|    1900 | 8438 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     949 | 8439 | `		}` |
|    1900 | 8440 | `		if( !pNode->pLeft ){` |
|       - | 8441 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 8442 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 8443 | `		}` |
|       - | 8444 | `		/* Phase#6: Compile the 'else' expression */` |
|    1900 | 8445 | `		if( pNode->pRight ){` |
|    1900 | 8446 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    1900 | 8447 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 8448 | `				return rc;` |
|       - | 8449 | `			}` |
|     949 | 8450 | `		}` |
|    1900 | 8451 | `		if( nJmp > 0 ){` |
|       - | 8452 | `			/* Phase#7: Fix the unconditional jump */` |
|    1900 | 8453 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    1900 | 8454 | `			if( pInstr ){` |
|    1900 | 8455 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     949 | 8456 | `			}` |
|     949 | 8457 | `		}` |
|       - | 8458 | `		/* All done */` |
|    1900 | 8459 | `		return SXRET_OK;` |
|       - | 8460 | `	}` |
|       - | 8461 | `	/* Generate code for the left tree */` |
|  920536 | 8462 | `	if( pNode->pLeft ){` |
|  920500 | 8463 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 8464 | `			ph7_expr_node **apNode;` |
|  308950 | 8465 | `			int hasSpread = 0;` |
|       - | 8466 | `			sxi32 n;` |
|       - | 8467 | `			/* Recurse and generate bytecodes for function arguments */` |
|  308950 | 8468 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       - | 8469 | `			/* Read-only load */` |
|  308950 | 8470 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  617280 | 8471 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  308332 | 8472 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  308332 | 8473 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 8474 | `					return rc;` |
|       - | 8475 | `				}` |
|  308332 | 8476 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 8477 | `					/* Emit spread opcode to unpack this array argument */` |
|      15 | 8478 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      15 | 8479 | `					hasSpread = 1;` |
|       7 | 8480 | `				}` |
|  154167 | 8481 | `			}` |
|       - | 8482 | `			/* Total number of given arguments */` |
|  308950 | 8483 | `			iP1 = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|  308950 | 8484 | `			iP2 = hasSpread;` |
|       - | 8485 | `			/* Remove stale flags now */` |
|  308950 | 8486 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  154474 | 8487 | `		}` |
|  920500 | 8488 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|  920500 | 8489 | `		if( rc != SXRET_OK ){` |
|      27 | 8490 | `			return rc;` |
|       - | 8491 | `		}` |
|  920474 | 8492 | `		if( iVmOp == PH7_OP_CALL ){` |
|  308950 | 8493 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  308950 | 8494 | `			if( pInstr ){` |
|  308950 | 8495 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  308282 | 8496 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 8497 | `					sxu32 nQual;` |
|       - | 8498 | `					/* Prevent constant expansion */` |
|  308282 | 8499 | `					pInstr->iP1 = 0;` |
|       - | 8500 | `					/* Namespace-qualify the function name for CALL.` |
|       - | 8501 | `					 * Only check function imports — class imports must NOT` |
|       - | 8502 | ``					 * affect function resolution.  For `new Foo()`, the CALL`` |
|       - | 8503 | `					 * handler fires before NEW; we store the original literal` |
|       - | 8504 | `					 * index in the CALL instruction's iP2 so the NEW handler` |
|       - | 8505 | `					 * can recover the unqualified name and re-qualify with` |
|       - | 8506 | `					 * class imports. */ {` |
|  308282 | 8507 | `						int fromImport = 0;` |
|  308282 | 8508 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  308282 | 8509 | `						pInstr->iP2 = (sxi32)nQual;` |
|  308282 | 8510 | `						if( nQual != nOrig ){` |
|       - | 8511 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 8512 | `							 * NEW handler can recover the unqualified name. */` |
|      68 | 8513 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      68 | 8514 | `							if( !fromImport ){` |
|      58 | 8515 | `								p3 = (void *)1;` |
|      28 | 8516 | `							}` |
|      35 | 8517 | `						}` |
|       - | 8518 | `					}` |
|  154810 | 8519 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 8520 | `					/* Method call,flag that */` |
|     554 | 8521 | `					pInstr->iP2 = 1;` |
|     276 | 8522 | `				}` |
|  154476 | 8523 | `			}` |
|  766000 | 8524 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 8525 | `			ph7_expr_node **apNode;` |
|       - | 8526 | `			sxi32 n;` |
|       - | 8527 | `			/* Recurse and generate bytecodes for array index */` |
|   69270 | 8528 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  124998 | 8529 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   55730 | 8530 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   55730 | 8531 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 8532 | `					return rc;` |
|       - | 8533 | `				}` |
|   27866 | 8534 | `			}` |
|   69270 | 8535 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   55730 | 8536 | `				iP1 = 1; /* Node have an index associated with it */` |
|   27864 | 8537 | `			}` |
|   69270 | 8538 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 8539 | `				/* Create an empty entry when the desired index is not found */` |
|   27350 | 8540 | `				iP2 = 1;` |
|   13676 | 8541 | `			}` |
|  576892 | 8542 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 8543 | `			/* POP the left node */` |
|      32 | 8544 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 8545 | `		}` |
|  460236 | 8546 | `	}` |
|  920510 | 8547 | `	rc = SXRET_OK;` |
|  920510 | 8548 | `	nJmpIdx = 0;` |
|       - | 8549 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 8550 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 8551 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  920510 | 8552 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     236 | 8553 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     236 | 8554 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     236 | 8555 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     236 | 8556 | `			int isSpecial = 0;` |
|     236 | 8557 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     152 | 8558 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     152 | 8559 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     163 | 8560 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     131 | 8561 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      67 | 8562 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      86 | 8563 | `					isSpecial = 1;` |
|      42 | 8564 | `				}` |
|      96 | 8565 | `			}` |
|     278 | 8566 | `			pInstr->iP1 = 0;` |
|     278 | 8567 | `			if( !isSpecial ){` |
|     110 | 8568 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      54 | 8569 | `			}` |
|       - | 8570 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 8571 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     194 | 8572 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     110 | 8573 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     110 | 8574 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 | 8575 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      44 | 8576 | `					return SXRET_OK;` |
|       - | 8577 | `				}` |
|      33 | 8578 | `			}` |
|      75 | 8579 | `		}` |
|     147 | 8580 | `	}` |
|       - | 8581 | `	/* Generate code for the right tree */` |
|  920434 | 8582 | `	if( pNode->pRight ){` |
|  480910 | 8583 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 8584 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    8510 | 8585 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  476656 | 8586 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 8587 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    2842 | 8588 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  470982 | 8589 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 8590 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      32 | 8591 | `			iVmOp = 0; /* No binary operator to emit */` |
|      32 | 8592 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  469547 | 8593 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  209832 | 8594 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  104915 | 8595 | `		}` |
|  480910 | 8596 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  480910 | 8597 | `		if( iVmOp == PH7_OP_STORE ){` |
|  206962 | 8598 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  206936 | 8599 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 8600 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 8601 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 8602 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 8603 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 8604 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 8605 | `				 */` |
|      54 | 8606 | `				iVmOp = 0;` |
|  206936 | 8607 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  206910 | 8608 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 8609 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   45924 | 8610 | `					iP2 = 1;` |
|   22963 | 8611 | `				}else{` |
|  160988 | 8612 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 8613 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   27288 | 8614 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   27288 | 8615 | `						iP1 = pInstr->iP1;` |
|   13645 | 8616 | `					}else{` |
|  133702 | 8617 | `						p3 = pInstr->p3;` |
|       - | 8618 | `					}` |
|       - | 8619 | `					/* POP the last dynamic load instruction */` |
|  160988 | 8620 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 8621 | `				}` |
|  103456 | 8622 | `			}` |
|  377430 | 8623 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      48 | 8624 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      48 | 8625 | `			if( pInstr ){` |
|      48 | 8626 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 8627 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 8628 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 8629 | `					 */` |
|      15 | 8630 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 8631 | `					iP1 = pInstr->iP1;` |
|      15 | 8632 | `					iP2 = pInstr->iP2;` |
|      15 | 8633 | `					p3  = pInstr->p3;` |
|       8 | 8634 | `				}else{` |
|      34 | 8635 | `					p3 = pInstr->p3;` |
|       - | 8636 | `				}` |
|      23 | 8637 | `			}` |
|      23 | 8638 | `		}` |
|  240454 | 8639 | `	}` |
|  920434 | 8640 | `	if( iVmOp > 0 ){` |
|  920322 | 8641 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   11038 | 8642 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 8643 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    8096 | 8644 | `				iP1 = 1;` |
|    4049 | 8645 | `			}` |
|  914804 | 8646 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 8647 | `			/* Namespace-qualify the class name for NEW */ {` |
|   13934 | 8648 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   13934 | 8649 | `				VmInstr *pCallInstr = 0;` |
|   13934 | 8650 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   13918 | 8651 | `					pCallInstr = pPeek;` |
|   13918 | 8652 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    6958 | 8653 | `				}` |
|   13934 | 8654 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|       - | 8655 | `					sxu32 nLitForClass;` |
|       - | 8656 | `					/* If the CALL handler already qualified the name using` |
|       - | 8657 | `					 * function imports, recover the original unqualified` |
|       - | 8658 | `					 * literal so we can re-qualify with class imports. */` |
|   13932 | 8659 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      32 | 8660 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      17 | 8661 | `					}else{` |
|   13902 | 8662 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 8663 | `					}` |
|   13932 | 8664 | `					pPeek->iP1 = 0;` |
|   13932 | 8665 | `					pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    6965 | 8666 | `				}` |
|       - | 8667 | `			}` |
|   13934 | 8668 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   13934 | 8669 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 8670 | `				VmInstr *pPrev;` |
|   13918 | 8671 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   13918 | 8672 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 8673 | `					/* Pop the call instruction */` |
|   13918 | 8674 | `					iP1 = pInstr->iP1;` |
|   13918 | 8675 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    6958 | 8676 | `				}` |
|    6960 | 8677 | `			}` |
|  902320 | 8678 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 8679 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 8680 | `			 * Namespace-qualify it, but skip self/static/parent. */` |
|      50 | 8681 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      50 | 8682 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      50 | 8683 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      50 | 8684 | `				int isSpecialIs = 0;` |
|      50 | 8685 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      46 | 8686 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      46 | 8687 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      49 | 8688 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      41 | 8689 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      23 | 8690 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 8691 | `						isSpecialIs = 1;` |
|       5 | 8692 | `					}` |
|      23 | 8693 | `				}` |
|      52 | 8694 | `				pInstr->iP1 = 0;` |
|      52 | 8695 | `				if( !isSpecialIs ){` |
|      38 | 8696 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      18 | 8697 | `				}` |
|      25 | 8698 | `			}` |
|  895333 | 8699 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 8700 | `			/* Prevent constant expansion for member/property names.` |
|       - | 8701 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 8702 | `			 * should not trigger constant lookup. */` |
|  103432 | 8703 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  103432 | 8704 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  103416 | 8705 | `				pInstr->iP1 = 0;` |
|   51707 | 8706 | `			}` |
|  103432 | 8707 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 8708 | `				/* Static member access,remember that */` |
|     160 | 8709 | `				iP1 = 1;` |
|     160 | 8710 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     160 | 8711 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      10 | 8712 | `					p3 = pInstr->p3;` |
|      10 | 8713 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       4 | 8714 | `				}` |
|      79 | 8715 | `			}` |
|   51715 | 8716 | `		}` |
|       - | 8717 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  920320 | 8718 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  460159 | 8719 | `	}` |
|  920432 | 8720 | `	if( nJmpIdx > 0 ){` |
|       - | 8721 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   11380 | 8722 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   11380 | 8723 | `		if( pInstr ){` |
|   11380 | 8724 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    5689 | 8725 | `		}` |
|    5689 | 8726 | `	}` |
|  920432 | 8727 | `	return rc;` |
| 1213321 | 8728 |  |
|       - | 8729 | `/*` |
|       - | 8730 | ` * Compile a PHP expression.` |
|       - | 8731 | ` * According to the PHP language reference manual:` |
|       - | 8732 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 8733 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 8734 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 8735 | ` *  is "anything that has a value".` |
|       - | 8736 | ` * If something goes wrong while compiling the expression,this` |
|       - | 8737 | ` * function takes care of generating the appropriate error` |
|       - | 8738 | ` * message.` |
|       - | 8739 | ` */` |
|  655636 | 8740 | `static sxi32 PH7_CompileExpr(` |
|       - | 8741 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 8742 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 8743 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 8744 | `	)` |
|       2 | 8745 |  |
|       - | 8746 | `	ph7_expr_node *pRoot;` |
|       - | 8747 | `	SySet sExprNode;` |
|       - | 8748 | `	SyToken *pEnd;` |
|       - | 8749 | `	sxi32 nExpr;` |
|       - | 8750 | `	sxi32 iNest;` |
|       - | 8751 | `	sxi32 rc;` |
|       - | 8752 | `	/* Initialize worker variables */` |
|  655638 | 8753 | `	nExpr = 0;` |
|  655638 | 8754 | `	pRoot = 0;` |
|  655638 | 8755 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  655638 | 8756 | `	SySetAlloc(&sExprNode,0x10);` |
|  655638 | 8757 | `	rc = SXRET_OK;` |
|       - | 8758 | `	/* Delimit the expression */` |
|  655638 | 8759 | `	pEnd = pGen->pIn;` |
|  655638 | 8760 | `	iNest = 0;` |
| 4419700 | 8761 | `	while( pEnd < pGen->pEnd ){` |
| 4190854 | 8762 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 8763 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     236 | 8764 | `			iNest++;` |
| 4190737 | 8765 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     244 | 8766 | `			iNest--;` |
| 4190499 | 8767 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  426998 | 8768 | `			if( iNest <= 0 ){` |
|  426792 | 8769 | `				break;` |
|       - | 8770 | `			}` |
|     103 | 8771 | `		}` |
| 3764064 | 8772 | `		pEnd++;` |
|       2 | 8773 | `	}` |
|  655638 | 8774 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   10988 | 8775 | `		SyToken *pEnd2 = pGen->pIn;` |
|   10988 | 8776 | `		iNest = 0;` |
|       - | 8777 | `		/* Stop at the first comma */` |
|   21998 | 8778 | `		while( pEnd2 < pEnd ){` |
|   11012 | 8779 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|       6 | 8780 | `				iNest++;` |
|   11010 | 8781 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|       6 | 8782 | `				iNest--;` |
|   11006 | 8783 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       5 | 8784 | `				if( iNest <= 0 ){` |
|     ! 0 | 8785 | `					break;` |
|       - | 8786 | `				}` |
|       2 | 8787 | `			}` |
|   11012 | 8788 | `			pEnd2++;` |
|       2 | 8789 | `		}` |
|   10988 | 8790 | `		if( pEnd2 <pEnd ){` |
|     ! 0 | 8791 | `			pEnd = pEnd2;` |
|     ! 0 | 8792 | `		}` |
|    5493 | 8793 | `	}` |
|  655638 | 8794 | `	if( pEnd > pGen->pIn ){` |
|  655628 | 8795 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 8796 | `		/* Swap delimiter */` |
|  655628 | 8797 | `		pGen->pEnd = pEnd;` |
|       - | 8798 | `		/* Try to get an expression tree */` |
|  655628 | 8799 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  655628 | 8800 | `		if( rc == SXRET_OK && pRoot ){` |
|  655458 | 8801 | `			rc = SXRET_OK;` |
|  655458 | 8802 | `			if( xTreeValidator ){` |
|       - | 8803 | `				/* Call the upper layer validator callback */` |
|   14100 | 8804 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    7049 | 8805 | `			}` |
|  655458 | 8806 | `			if( rc != SXERR_ABORT ){` |
|       - | 8807 | `				/* Generate code for the given tree */` |
|  655458 | 8808 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|  327728 | 8809 | `			}` |
|  655458 | 8810 | `			nExpr = 1;` |
|  327728 | 8811 | `		}` |
|       - | 8812 | `		/* Release the whole tree */` |
|  655628 | 8813 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 8814 | `		/* Synchronize token stream */` |
|  655628 | 8815 | `		pGen->pEnd = pTmp;` |
|  655628 | 8816 | `		pGen->pIn  = pEnd;` |
|  655628 | 8817 | `		if( rc == SXERR_ABORT ){` |
|      11 | 8818 | `			SySetRelease(&sExprNode);` |
|      11 | 8819 | `			return SXERR_ABORT;` |
|       - | 8820 | `		}` |
|  327808 | 8821 | `	}` |
|  655628 | 8822 | `	SySetRelease(&sExprNode);` |
|  655628 | 8823 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  327820 | 8824 |  |
|       - | 8825 | `/*` |
|       - | 8826 | ` * Return a pointer to the node construct handler associated` |
|       - | 8827 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 8828 | ` */` |
|  163658 | 8829 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 8830 |  |
|  163660 | 8831 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 8832 | `		/* Numeric literal: Either real or integer */` |
|   89540 | 8833 | `		return PH7_CompileNumLiteral;` |
|   74122 | 8834 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 8835 | `		/* Double quoted string */` |
|   15870 | 8836 | `		return PH7_CompileString;` |
|   58254 | 8837 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 8838 | `		/* Single quoted string */` |
|   58142 | 8839 | `		return PH7_CompileSimpleString;` |
|     114 | 8840 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 8841 | `		/* Heredoc */` |
|      66 | 8842 | `		return PH7_CompileHereDoc;` |
|      50 | 8843 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 8844 | `		/* Nowdoc */` |
|      44 | 8845 | `		return PH7_CompileNowDoc;` |
|       7 | 8846 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 8847 | `		/* Backtick quoted string */` |
|       5 | 8848 | `		return PH7_CompileBacktic;` |
|       - | 8849 | `	}` |
|       3 | 8850 | `	return 0;` |
|   81831 | 8851 |  |
|       - | 8852 | `/*` |
|       - | 8853 | ` * Compile an unset() statement.` |
|       - | 8854 | ` * unset($var, $arr[$key], ...);` |
|       - | 8855 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 8856 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 8857 | ` * parent array before extracting the element to unset.` |
|       - | 8858 | ` */` |
|    2646 | 8859 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       2 | 8860 |  |
|    2648 | 8861 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2648 | 8862 | `	sxu32 nIdx = 0;` |
|       - | 8863 | `	SyString sName;` |
|       - | 8864 | `	sxi32 rc;` |
|       - | 8865 | `	/* Jump the 'unset' keyword */` |
|    2648 | 8866 | `	pGen->pIn++;` |
|       - | 8867 | `	/* Save delimiter */` |
|    2648 | 8868 | `	pTmp = pGen->pEnd;` |
|       - | 8869 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2648 | 8870 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2648 | 8871 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 8872 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 8873 | `		SyToken *pClose;` |
|    2648 | 8874 | `		pGen->pIn++;   /* Skip '(' */` |
|    2648 | 8875 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2648 | 8876 | `		pEnd = pClose; /* Stop at ')' */` |
|    1323 | 8877 | `	}` |
|    2648 | 8878 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 8879 | `	/* Resolve the 'unset' builtin name once */` |
|    2648 | 8880 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     304 | 8881 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     304 | 8882 | `		if( pObj == 0 ){` |
|     ! 0 | 8883 | `			return SXERR_ABORT;` |
|       - | 8884 | `		}` |
|     304 | 8885 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     304 | 8886 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     151 | 8887 | `	}` |
|       - | 8888 | `	/* Compile each comma-separated argument */` |
|    8860 | 8889 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6214 | 8890 | `		if( pGen->pIn < pNext ){` |
|    6214 | 8891 | `			pGen->pEnd = pNext;` |
|    6214 | 8892 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 8893 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_STORE,0);` |
|    6214 | 8894 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 8895 | `				return SXERR_ABORT;` |
|       - | 8896 | `			}` |
|    6214 | 8897 | `			if( rc != SXERR_EMPTY ){` |
|       - | 8898 | `				/* Emit call for this single argument */` |
|    6212 | 8899 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6212 | 8900 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,0,0);` |
|    6212 | 8901 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3105 | 8902 | `			}` |
|    3106 | 8903 | `		}` |
|       - | 8904 | `		/* Jump trailing commas */` |
|    9782 | 8905 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3570 | 8906 | `			pNext++;` |
|       2 | 8907 | `		}` |
|    6214 | 8908 | `		pGen->pIn = pNext;` |
|       2 | 8909 | `	}` |
|       - | 8910 | `	/* Skip past the closing ')' if present */` |
|    2648 | 8911 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2648 | 8912 | `		pGen->pIn++;` |
|    1323 | 8913 | `	}` |
|       - | 8914 | `	/* Restore token stream */` |
|    2648 | 8915 | `	pGen->pEnd = pTmp;` |
|    2648 | 8916 | `	return SXRET_OK;` |
|    1325 | 8917 |  |
|       - | 8918 | `/*` |
|       - | 8919 | ` * PHP Language construct table.` |
|       - | 8920 | ` */` |
|       - | 8921 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 8922 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 8923 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 8924 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 8925 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 8926 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 8927 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 8928 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 8929 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 8930 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 8931 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 8932 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 8933 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 8934 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 8935 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 8936 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 8937 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 8938 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 8939 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 8940 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 8941 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 8942 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 8943 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 8944 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 8945 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 8946 | `};` |
|       - | 8947 | `/*` |
|       - | 8948 | ` * Return a pointer to the statement handler routine associated` |
|       - | 8949 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 8950 | ` */` |
|  397590 | 8951 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 8952 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 8953 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 8954 | `	)` |
|       2 | 8955 |  |
|  397592 | 8956 | `	sxu32 n = 0;` |
| 1671040 | 8957 | `	for(;;){` |
| 3342082 | 8958 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   46544 | 8959 | `			break;` |
|       - | 8960 | `		}` |
| 3295540 | 8961 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  351050 | 8962 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 8963 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 8964 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 8965 | `					/* 'static' (class context),return null */` |
|     ! 0 | 8966 | `					return 0;` |
|       - | 8967 | `				}` |
|     ! 0 | 8968 | `			}` |
|  351048 | 8969 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       2 | 8970 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       3 | 8971 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 8972 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 8973 | `				return 0;` |
|       - | 8974 | `			}` |
|       - | 8975 | `			/* Return a pointer to the handler.` |
|       - | 8976 | `			*/` |
|  351050 | 8977 | `			return aLangConstruct[n].xConstruct;` |
|       - | 8978 | `		}` |
| 2944492 | 8979 | `		n++;` |
|       2 | 8980 | `	}` |
|   46544 | 8981 | `	if( pLookahed ){` |
|   46544 | 8982 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    8128 | 8983 | `			return PH7_CompileClassInterface;` |
|   38418 | 8984 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   38214 | 8985 | `			return PH7_CompileClass;` |
|     206 | 8986 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      54 | 8987 | `			return PH7_CompileTrait;` |
|     152 | 8988 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      19 | 8989 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      18 | 8990 | `				return PH7_CompileAbstractClass;` |
|     136 | 8991 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 8992 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 8993 | `				return PH7_CompileFinalClass;` |
|       - | 8994 | `		}` |
|      67 | 8995 | `	}` |
|       - | 8996 | `	/* Not a language construct */` |
|     136 | 8997 | `	return 0;` |
|  198797 | 8998 |  |
|       - | 8999 | `/*` |
|       - | 9000 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 9001 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 9002 | ` */` |
|     134 | 9003 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 9004 |  |
|       - | 9005 | `	int rc;` |
|     136 | 9006 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     136 | 9007 | `	if( rc == FALSE ){` |
|      40 | 9008 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      38 | 9009 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 9010 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 9011 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 9012 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 9013 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 9014 | `			*/` |
|       - | 9015 | `			){` |
|      34 | 9016 | `				rc = TRUE;` |
|      16 | 9017 | `		}` |
|      20 | 9018 | `	}` |
|     136 | 9019 | `	return rc;` |
|       2 | 9020 |  |
|       - | 9021 | `/*` |
|       - | 9022 | ` * Compile a PHP chunk.` |
|       - | 9023 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 9024 | ` * takes care of generating the appropriate error message.` |
|       - | 9025 | ` */` |
|  533754 | 9026 | `static sxi32 GenStateCompileChunk(` |
|       - | 9027 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 9028 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 9029 | `	)` |
|       2 | 9030 |  |
|       - | 9031 | `	ProcLangConstruct xCons;` |
|       - | 9032 | `	sxi32 rc;` |
|  533756 | 9033 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  319012 | 9034 | `	for(;;){` |
|  638026 | 9035 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 9036 | `			/* No more input to process */` |
|   11694 | 9037 | `			break;` |
|       - | 9038 | `		}` |
|  626334 | 9039 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 9040 | `			/* Compile block */` |
|      12 | 9041 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      12 | 9042 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 9043 | `				break;` |
|       - | 9044 | `			}` |
|       7 | 9045 | `		}else{` |
|  626324 | 9046 | `			xCons = 0;` |
|  626324 | 9047 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  397592 | 9048 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 9049 | `				/* Try to extract a language construct handler */` |
|  397592 | 9050 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  397592 | 9051 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 9052 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 9053 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 9054 | `						&pGen->pIn->sData);` |
|       9 | 9055 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 9056 | `						break;` |
|       - | 9057 | `					}` |
|       - | 9058 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 9059 | `					 * this erroneous statement.` |
|       - | 9060 | `					 */` |
|       9 | 9061 | `					xCons = PH7_ErrorRecover;` |
|       4 | 9062 | `				}` |
|  427529 | 9063 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   40010 | 9064 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 9065 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 9066 | `				xCons = PH7_CompileLabel;` |
|      56 | 9067 | `			}` |
|  626324 | 9068 | `			if( xCons == 0 ){` |
|       - | 9069 | `				/* Assume an expression an try to compile it */` |
|  228748 | 9070 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  228748 | 9071 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 9072 | `					/* Pop l-value */` |
|  228610 | 9073 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  114304 | 9074 | `				}` |
|  114375 | 9075 | `			}else{` |
|       - | 9076 | `				/* Go compile the sucker */` |
|  397578 | 9077 | `				rc = xCons(&(*pGen));` |
|       - | 9078 | `			}` |
|  626324 | 9079 | `			if( rc == SXERR_ABORT ){` |
|       - | 9080 | `				/* Request to abort compilation */` |
|      11 | 9081 | `				break;` |
|       - | 9082 | `			}` |
|       - | 9083 | `		}` |
|       - | 9084 | `		/* Ignore trailing semi-colons ';' */` |
| 1037436 | 9085 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  411114 | 9086 | `			pGen->pIn++;` |
|       2 | 9087 | `		}` |
|  626324 | 9088 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 9089 | `			/* Compile a single statement and return */` |
|  522054 | 9090 | `			break;` |
|       - | 9091 | `		}` |
|       - | 9092 | `		/* LOOP ONE */` |
|       - | 9093 | `		/* LOOP TWO */` |
|       - | 9094 | `		/* LOOP THREE */` |
|       - | 9095 | `		/* LOOP FOUR */` |
|       2 | 9096 | `	}` |
|       - | 9097 | `	/* Return compilation status */` |
|  533756 | 9098 | `	return rc;` |
|       2 | 9099 |  |
|       - | 9100 | `/*` |
|       - | 9101 | ` * Compile a Raw PHP chunk.` |
|       - | 9102 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 9103 | ` * takes care of generating the appropriate error message.` |
|       - | 9104 | ` */` |
|   11704 | 9105 | `static sxi32 PH7_CompilePHP(` |
|       - | 9106 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 9107 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 9108 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 9109 | `	)` |
|       2 | 9110 |  |
|   11706 | 9111 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 9112 | `	sxi32 rc;` |
|       - | 9113 | `	/* Reset the token set */` |
|   11706 | 9114 | `	SySetReset(&(*pTokenSet));` |
|       - | 9115 | `	/* Mark as the default token set */` |
|   11706 | 9116 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 9117 | `	/* Advance the stream cursor */` |
|   11706 | 9118 | `	pGen->pRawIn++;` |
|       - | 9119 | `	/* Tokenize the PHP chunk first */` |
|   11706 | 9120 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 9121 | `	/* Point to the head and tail of the token stream. */` |
|   11706 | 9122 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   11706 | 9123 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   11706 | 9124 | `	if( is_expr ){` |
|     ! 0 | 9125 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 9126 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 9127 | `			/* A simple expression,compile it */` |
|     ! 0 | 9128 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 9129 | `		}` |
|       - | 9130 | `		/* Emit the DONE instruction */` |
|     ! 0 | 9131 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 9132 | `		return SXRET_OK;` |
|       - | 9133 | `	}` |
|   11706 | 9134 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 9135 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 9136 | `		/*` |
|       - | 9137 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 9138 | `		 * According to the PHP reference manual:` |
|       - | 9139 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 9140 | `		 *  immediately follow` |
|       - | 9141 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 9142 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 9143 | `		 * Symisc extension:` |
|       - | 9144 | `		 *   This short syntax works with all PHP opening` |
|       - | 9145 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 9146 | `		 *   only short tag.` |
|       - | 9147 | `		 */` |
|       - | 9148 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 9149 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 9150 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 9151 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 9152 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 9153 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 9154 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 9155 | `		}` |
|       3 | 9156 | `		return SXRET_OK;` |
|       - | 9157 | `	}` |
|       - | 9158 | `	/* Compile the PHP chunk */` |
|   11704 | 9159 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 9160 | `	/* Fix exceptions jumps */` |
|   11704 | 9161 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 9162 | `	/* Fix gotos now, the jump destination is resolved */` |
|   11704 | 9163 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 9164 | `		rc = SXERR_ABORT;` |
|       1 | 9165 | `	}` |
|       - | 9166 | `	/* Reset container */` |
|   11704 | 9167 | `	SySetReset(&pGen->aGoto);` |
|   11704 | 9168 | `	SySetReset(&pGen->aLabel);` |
|       - | 9169 | `	/* Compilation result */` |
|   11704 | 9170 | `	return rc;` |
|    5854 | 9171 |  |
|       - | 9172 | `/*` |
|       - | 9173 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 9174 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 9175 | ` * This is the only compile interface exported from this file.` |
|       - | 9176 | ` */` |
|   13844 | 9177 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 9178 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 9179 | `	SyString *pScript,  /* Script to compile */` |
|       - | 9180 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 9181 | `	)` |
|       2 | 9182 |  |
|       - | 9183 | `	SySet aPhpToken,aRawToken;` |
|       - | 9184 | `	ph7_gen_state *pCodeGen;` |
|       - | 9185 | `	ph7_value *pRawObj;` |
|       - | 9186 | `	sxu32 nObjIdx;` |
|       - | 9187 | `	sxi32 nRawObj;` |
|       - | 9188 | `	int is_expr;` |
|       - | 9189 | `	sxi32 rc;` |
|   13846 | 9190 | `	if( pScript->nByte < 1 ){` |
|       - | 9191 | `		/* Nothing to compile */` |
|     ! 0 | 9192 | `		return PH7_OK;` |
|       - | 9193 | `	}` |
|       - | 9194 | `	/* Initialize the tokens containers */` |
|   13846 | 9195 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   13846 | 9196 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   13846 | 9197 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   13846 | 9198 | `	is_expr = 0;` |
|   13846 | 9199 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 9200 | `		SyToken sTmp;` |
|       - | 9201 | `		/* PHP only: -*/` |
|    2726 | 9202 | `		sTmp.nLine = 1;` |
|    2726 | 9203 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2726 | 9204 | `		sTmp.pUserData = 0;` |
|    2726 | 9205 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2726 | 9206 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2726 | 9207 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 9208 | `			/* A simple PHP expression */` |
|     ! 0 | 9209 | `			is_expr = 1;` |
|     ! 0 | 9210 | `		}` |
|    1364 | 9211 | `	}else{` |
|       - | 9212 | `		/* Tokenize raw text */` |
|   11122 | 9213 | `		SySetAlloc(&aRawToken,32);` |
|   11122 | 9214 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 9215 | `	}` |
|   13846 | 9216 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 9217 | `	/* Process high-level tokens */` |
|   13846 | 9218 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   13846 | 9219 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   13846 | 9220 | `	rc = PH7_OK;` |
|   13846 | 9221 | `	if( is_expr ){` |
|       - | 9222 | `		/* Compile the expression */` |
|     ! 0 | 9223 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 9224 | `		goto cleanup;` |
|       - | 9225 | `	}` |
|   13846 | 9226 | `	nObjIdx = 0;` |
|       - | 9227 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 9228 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 9229 | `	 * preventing namespace bleeding across include()d files. */` |
|   13846 | 9230 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 9231 | `	/* Start the compilation process */` |
|   12486 | 9232 | `	for(;;){` |
|   36666 | 9233 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   13834 | 9234 | `			break; /* No more tokens to process */` |
|       - | 9235 | `		}` |
|   22834 | 9236 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 9237 | `			/* Compile the PHP chunk */` |
|   11706 | 9238 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   11706 | 9239 | `			if( rc == SXERR_ABORT ){` |
|      13 | 9240 | `				break;` |
|       - | 9241 | `			}` |
|   11694 | 9242 | `			continue;` |
|       - | 9243 | `		}` |
|       - | 9244 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   11130 | 9245 | `		nRawObj = 0;` |
|   22258 | 9246 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 9247 | `			/* Consume the raw chunk without any processing */` |
|   11130 | 9248 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   11130 | 9249 | `			if( pRawObj == 0 ){` |
|     ! 0 | 9250 | `				rc = SXERR_MEM;` |
|     ! 0 | 9251 | `				break;` |
|       - | 9252 | `			}` |
|       - | 9253 | `			/* Mark as constant and emit the load constant instruction */` |
|   11130 | 9254 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   11130 | 9255 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   11130 | 9256 | `			++nRawObj;` |
|   11130 | 9257 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 9258 | `		}` |
|   11130 | 9259 | `		if( nRawObj > 0 ){` |
|       - | 9260 | `			/* Emit the consume instruction */` |
|   11130 | 9261 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    5564 | 9262 | `		}` |
|    6924 | 9263 | `	}` |
|    6922 | 9264 | `cleanup:` |
|   13846 | 9265 | `	SySetRelease(&aRawToken);` |
|   13846 | 9266 | `	SySetRelease(&aPhpToken);` |
|   13846 | 9267 | `	return rc;` |
|    6924 | 9268 |  |
|       - | 9269 | `/*` |
|       - | 9270 | ` * Utility routines.Initialize the code generator.` |
|       - | 9271 | ` */` |
|    2696 | 9272 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 9273 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 9274 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 9275 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 9276 | `	)` |
|       2 | 9277 |  |
|    2698 | 9278 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 9279 | `	/* Zero the structure */` |
|    2698 | 9280 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 9281 | `	/* Initial state */` |
|    2698 | 9282 | `	pGen->pVm  = &(*pVm);` |
|    2698 | 9283 | `	pGen->xErr = xErr;` |
|    2698 | 9284 | `	pGen->pErrData = pErrData;` |
|    2698 | 9285 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2698 | 9286 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2698 | 9287 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2698 | 9288 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 9289 | `	/* Error log buffer */` |
|    2698 | 9290 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 9291 | `	/* General purpose working buffer */` |
|    2698 | 9292 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 9293 | `	/* Namespace state */` |
|    2698 | 9294 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2698 | 9295 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    2698 | 9296 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    2698 | 9297 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 9298 | `	/* Create the global scope */` |
|    2698 | 9299 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 9300 | `	/* Point to the global scope */` |
|    2698 | 9301 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2698 | 9302 | `	return SXRET_OK;` |
|       2 | 9303 |  |
|       - | 9304 | `/*` |
|       - | 9305 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 9306 | ` */` |
|   16266 | 9307 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 9308 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 9309 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 9310 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 9311 | `	)` |
|       2 | 9312 |  |
|   16268 | 9313 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 9314 | `	GenBlock *pBlock,*pParent;` |
|       - | 9315 | `	/* Reset state */` |
|   16268 | 9316 | `	SySetReset(&pGen->aLabel);` |
|   16268 | 9317 | `	SySetReset(&pGen->aGoto);` |
|   16268 | 9318 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   16268 | 9319 | `	SyBlobRelease(&pGen->sWorker);` |
|   16268 | 9320 | `	SyBlobRelease(&pGen->sNamespace);` |
|   16268 | 9321 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   16268 | 9322 | `	SyHashRelease(&pGen->hUseImports);` |
|   16268 | 9323 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   16268 | 9324 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   16268 | 9325 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   16268 | 9326 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   16268 | 9327 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 9328 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 9329 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 9330 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 9331 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 9332 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 9333 | `	 * number of unique names, which is acceptable. */` |
|       - | 9334 | `	/* Point to the global scope */` |
|   16268 | 9335 | `	pBlock = pGen->pCurrent;` |
|   16268 | 9336 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 9337 | `		pParent = pBlock->pParent;` |
|     ! 0 | 9338 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 9339 | `		pBlock = pParent;` |
|     ! 0 | 9340 | `	}` |
|   16268 | 9341 | `	pGen->xErr = xErr;` |
|   16268 | 9342 | `	pGen->pErrData = pErrData;` |
|   16268 | 9343 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   16268 | 9344 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   16268 | 9345 | `	pGen->pIn = pGen->pEnd = 0;` |
|   16268 | 9346 | `	pGen->nErr = 0;` |
|   16268 | 9347 | `	return SXRET_OK;` |
|       2 | 9348 |  |
|       - | 9349 | `/*` |
|       - | 9350 | ` * Generate a compile-time error message.` |
|       - | 9351 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 9352 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 9353 | ` * abort compilation immediately.` |
|       - | 9354 | ` */` |
|     506 | 9355 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 9356 |  |
|     508 | 9357 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     508 | 9358 | `	const char *zErr = "Error";` |
|       - | 9359 | `	SyString *pFile;` |
|       - | 9360 | `	va_list ap;` |
|       - | 9361 | `	sxi32 rc;` |
|       - | 9362 | `	/* Reset the working buffer */` |
|     508 | 9363 | `	SyBlobReset(pWorker);` |
|       - | 9364 | `	/* Peek the processed file path if available */` |
|     508 | 9365 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     508 | 9366 | `	if( nErrType == E_ERROR ){` |
|       - | 9367 | `		/* Increment the error counter */` |
|     430 | 9368 | `		pGen->nErr++;` |
|     430 | 9369 | `		if( pGen->nErr > 15 ){` |
|       - | 9370 | `			/* Error count limit reached */` |
|       5 | 9371 | `			if( pGen->xErr ){` |
|       5 | 9372 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 9373 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 9374 | `				if( pFile ){` |
|       5 | 9375 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 9376 | `				}` |
|       5 | 9377 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 9378 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 9379 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 9380 | `				}` |
|       2 | 9381 | `			}` |
|       - | 9382 | `			/* Abort immediately */` |
|       5 | 9383 | `			return SXERR_ABORT;` |
|       - | 9384 | `		}` |
|     212 | 9385 | `	}` |
|     504 | 9386 | `	if( pGen->xErr == 0 ){` |
|       - | 9387 | `		/* No available error consumer,return immediately */` |
|       3 | 9388 | `		return SXRET_OK;` |
|       - | 9389 | `	}` |
|     501 | 9390 | `	switch(nErrType){` |
|     423 | 9391 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      29 | 9392 | `	case E_WARNING: zErr = "Warning";     break;` |
|      43 | 9393 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 9394 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 9395 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 9396 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 9397 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 9398 | `	default:` |
|     ! 0 | 9399 | `		break;` |
|       - | 9400 | `	}` |
|     501 | 9401 | `	rc = SXRET_OK;` |
|       - | 9402 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     501 | 9403 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     501 | 9404 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     501 | 9405 | `	va_start(ap,zFormat);` |
|     501 | 9406 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     501 | 9407 | `	va_end(ap);` |
|     501 | 9408 | `	if( pFile ){` |
|     501 | 9409 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     250 | 9410 | `	}` |
|       - | 9411 | `	/* Append a new line */` |
|     501 | 9412 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     501 | 9413 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 9414 | `		/* Consume the generated error message */` |
|     501 | 9415 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     250 | 9416 | `	}` |
|     501 | 9417 | `	return rc;` |
|     255 | 9418 |  |
|       - | 9419 |  |
