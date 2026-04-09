# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3781/4897 lines (77.21%)

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
|    2880 |  128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |  129 |  |
|    2882 |  130 | `	GenBlock *pBlock = pCurrent;` |
|    8083 |  131 | `	for(;;){` |
|   16168 |  132 | `		if( pBlock->iFlags & iBlockType ){` |
|    2770 |  133 | `			iCount--; /* Decrement nesting level */` |
|    2770 |  134 | `			if( iCount < 1 ){` |
|       - |  135 | `				/* Block meet with the desired criteria */` |
|    2748 |  136 | `				return pBlock;` |
|       - |  137 | `			}` |
|      11 |  138 | `		}` |
|       - |  139 | `		/* Point to the upper block */` |
|   13422 |  140 | `		pBlock = pBlock->pParent;` |
|   13422 |  141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |  142 | `			/* Forbidden */` |
|      69 |  143 | `			break;` |
|       - |  144 | `		}` |
|       2 |  145 | `	}` |
|       - |  146 | `	/* No such block */` |
|     136 |  147 | `	return 0;` |
|    1442 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Initialize a freshly allocated block instance.` |
|       - |  151 | ` */` |
|  559778 |  152 | `static void GenStateInitBlock(` |
|       - |  153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |  155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |  157 | `	void *pUserData      /* Upper layer private data */` |
|       - |  158 | `	)` |
|       2 |  159 |  |
|       - |  160 | `	/* Initialize block fields */` |
|  559780 |  161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  559780 |  162 | `	pBlock->pUserData   = pUserData;` |
|  559780 |  163 | `	pBlock->pGen        = pGen;` |
|  559780 |  164 | `	pBlock->iFlags      = iType;` |
|  559780 |  165 | `	pBlock->pParent     = 0;` |
|  559780 |  166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  559780 |  167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  559780 |  168 |  |
|       - |  169 | `/*` |
|       - |  170 | ` * Allocate a new block instance.` |
|       - |  171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |  172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |  173 | ` * processing on failure.` |
|       - |  174 | ` */` |
|  557156 |  175 | `static sxi32 GenStateEnterBlock(` |
|       - |  176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |  179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |  180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |  181 | `	)` |
|       2 |  182 |  |
|       - |  183 | `	GenBlock *pBlock;` |
|       - |  184 | `	/* Allocate a new block instance */` |
|  557158 |  185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  557158 |  186 | `	if( pBlock == 0 ){` |
|       - |  187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  189 | `		 */` |
|     ! 0 |  190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |  191 | `		/* Abort processing immediately */` |
|     ! 0 |  192 | `		return SXERR_ABORT;` |
|       - |  193 | `	}` |
|       - |  194 | `	/* Zero the structure */` |
|  557158 |  195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  557158 |  196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |  197 | `	/* Link to the parent block */` |
|  557158 |  198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |  199 | `	/* Mark as the current block */` |
|  557158 |  200 | `	pGen->pCurrent = pBlock;` |
|  557158 |  201 | `	if( ppBlock ){` |
|       - |  202 | `		/* Write a pointer to the new instance */` |
|  269616 |  203 | `		*ppBlock = pBlock;` |
|  134807 |  204 | `	}` |
|  557158 |  205 | `	return SXRET_OK;` |
|  278580 |  206 |  |
|       - |  207 | `/*` |
|       - |  208 | ` * Release block fields without freeing the whole instance.` |
|       - |  209 | ` */` |
|  557148 |  210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |  211 |  |
|  557150 |  212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  557150 |  213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  557150 |  214 |  |
|       - |  215 | `/*` |
|       - |  216 | ` * Release a block.` |
|       - |  217 | ` */` |
|  557148 |  218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |  219 |  |
|  557150 |  220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  557150 |  221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |  222 | `	/* Free the instance */` |
|  557150 |  223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  557150 |  224 |  |
|       - |  225 | `/*` |
|       - |  226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |  227 | ` */` |
|  557148 |  228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |  229 |  |
|  557150 |  230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  557150 |  231 | `	if( pBlock == 0 ){` |
|       - |  232 | `		/* No more block to pop */` |
|     ! 0 |  233 | `		return SXERR_EMPTY;` |
|       - |  234 | `	}` |
|       - |  235 | `	/* Point to the upper block */` |
|  557150 |  236 | `	pGen->pCurrent = pBlock->pParent;` |
|  557150 |  237 | `	if( ppBlock ){` |
|       - |  238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |  239 | `		*ppBlock = pBlock;` |
|     ! 0 |  240 | `	}else{` |
|       - |  241 | `		/* Safely release the block */` |
|  557150 |  242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |  243 | `	}` |
|  557150 |  244 | `	return SXRET_OK;` |
|  278576 |  245 |  |
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
|  169916 |  256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |  257 |  |
|       - |  258 | `	JumpFixup sJumpFix;` |
|       - |  259 | `	sxi32 rc;` |
|       - |  260 | `	/* Init the JumpFixup structure */` |
|  169918 |  261 | `	sJumpFix.nJumpType = nJumpType;` |
|  169918 |  262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |  263 | `	/* Insert in the jump fixup table */` |
|  169918 |  264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  169918 |  265 | `	return rc;` |
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
|  396802 |  278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |  279 |  |
|       - |  280 | `	JumpFixup *aFix;` |
|       - |  281 | `	VmInstr *pInstr;` |
|       - |  282 | `	sxu32 nFixed;` |
|       - |  283 | `	sxu32 n;` |
|       - |  284 | `	/* Point to the jump fixup table */` |
|  396804 |  285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |  286 | `	/* Fix the desired jumps */` |
|  727914 |  287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  331112 |  288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |  289 | `			/* Already fixed */` |
|  128950 |  290 | `			continue;` |
|       - |  291 | `		}` |
|  202164 |  292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |  293 | `			/* Not of our interest */` |
|   32250 |  294 | `			continue;` |
|       - |  295 | `		}` |
|       - |  296 | `		/* Point to the instruction to fix */` |
|  169916 |  297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  169916 |  298 | `		if( pInstr ){` |
|  169916 |  299 | `			pInstr->iP2 = nJumpDest;` |
|  169916 |  300 | `			nFixed++;` |
|       - |  301 | `			/* Mark as fixed */` |
|  169916 |  302 | `			aFix[n].nJumpType = -1;` |
|   84957 |  303 | `		}` |
|   84959 |  304 | `	}` |
|       - |  305 | `	/* Total number of fixed jumps */` |
|  396804 |  306 | `	return nFixed;` |
|       2 |  307 |  |
|       - |  308 | `/*` |
|       - |  309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |  310 | ` * The goto statement can be used to jump to another section` |
|       - |  311 | ` * in the program.` |
|       - |  312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |  313 | ` * statement for more information.` |
|       - |  314 | ` */` |
|  151446 |  315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |  316 |  |
|       - |  317 | `	JumpFixup *pJump,*aJumps;` |
|       - |  318 | `	Label *pLabel,*aLabel;` |
|       - |  319 | `	VmInstr *pInstr;` |
|       - |  320 | `	sxi32 rc;` |
|       - |  321 | `	sxu32 n;` |
|       - |  322 | `	/* Point to the goto table */` |
|  151448 |  323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |  324 | `	/* Fix */` |
|  151594 |  325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  151446 |  350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  151578 |  351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |  352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |  353 | `			/* Emit a warning */` |
|      37 |  354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |  355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |  356 | `		}` |
|      68 |  357 | `	}` |
|  151446 |  358 | `	return SXRET_OK;` |
|   75725 |  359 |  |
|       - |  360 | `/*` |
|       - |  361 | ` * Check if a given token value is installed in the literal table.` |
|       - |  362 | ` */` |
|  493226 |  363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |  364 |  |
|       - |  365 | `	SyHashEntry *pEntry;` |
|  493228 |  366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  493228 |  367 | `	if( pEntry == 0 ){` |
|  243176 |  368 | `		return SXERR_NOTFOUND;` |
|       - |  369 | `	}` |
|  250054 |  370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  250054 |  371 | `	return SXRET_OK;` |
|  246615 |  372 |  |
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
|  243174 |  383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |  384 |  |
|  243176 |  385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  243176 |  386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  121587 |  387 | `	}` |
|  243176 |  388 | `	return SXRET_OK;` |
|       2 |  389 |  |
|       - |  390 | `/*` |
|       - |  391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |  392 | ` * in the constant table.` |
|       - |  393 | ` */` |
|   86240 |  394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |  395 |  |
|       - |  396 | `	ph7_value *pObj;` |
|   86242 |  397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |  398 | `	/* Reserve a new constant */` |
|   86242 |  399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   86242 |  400 | `	if( pObj == 0 ){` |
|     ! 0 |  401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  402 | `		return 0;` |
|       - |  403 | `	}` |
|   86242 |  404 | `	*pIdx = nIdx;` |
|       - |  405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |  406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |  407 | `	 */` |
|   86242 |  408 | `	return pObj;` |
|   43122 |  409 |  |
|       - |  410 | `/*` |
|       - |  411 | ` * Implementation of the PHP language constructs.` |
|       - |  412 | ` */` |
|       - |  413 | `/* Forward declaration */` |
|       - |  414 | `static sxi32 GenStateCompileChunk(ph7_gen_state *pGen,sxi32 iFlags);` |
|       - |  415 | `/*` |
|       - |  416 | ` * Compile a numeric [i.e: integer or real] literal.` |
|       - |  417 | ` * Notes on the integer type.` |
|       - |  418 | ` *  According to the PHP language reference manual` |
|       - |  419 | ` *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)` |
|       - |  420 | ` *  or binary (base 2) notation, optionally preceded by a sign (- or +).` |
|       - |  421 | ` *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal` |
|       - |  422 | ` *  notation precede the number with 0x. To use binary notation precede the number with 0b.` |
|       - |  423 | ` * Symisc eXtension to the integer type.` |
|       - |  424 | ` *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine` |
|       - |  425 | ` *  where the size of an integer is platform-dependent.That is,the size of an integer` |
|       - |  426 | ` *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms` |
|       - |  427 | ` *  [i.e: either 32bit or 64bit].` |
|       - |  428 | ` *  For more information on this powerfull extension please refer to the official` |
|       - |  429 | ` *  documentation.` |
|       - |  430 | ` */` |
|   86654 |  431 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  432 |  |
|   86656 |  433 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   86656 |  434 | `	sxu32 nIdx = 0;` |
|   86656 |  435 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |  436 | `		ph7_value *pObj;` |
|       - |  437 | `		sxi64 iValue;` |
|   86242 |  438 | `		iValue = PH7_TokenValueToInt64(&pToken->sData);` |
|   86242 |  439 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   86242 |  440 | `		if( pObj == 0 ){` |
|     ! 0 |  441 | `			SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  442 | `			return SXERR_ABORT;` |
|       - |  443 | `		}` |
|   86242 |  444 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   43122 |  445 | `	}else{` |
|       - |  446 | `		/* Real number */` |
|       - |  447 | `		ph7_value *pObj;` |
|       - |  448 | `		/* Reserve a new constant */` |
|     416 |  449 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     416 |  450 | `		if( pObj == 0 ){` |
|     ! 0 |  451 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  452 | `			return SXERR_ABORT;` |
|       - |  453 | `		}` |
|     416 |  454 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&pToken->sData);` |
|     416 |  455 | `		PH7_MemObjToReal(pObj);` |
|       - |  456 | `	}` |
|       - |  457 | `	/* Emit the load constant instruction */` |
|   86656 |  458 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  459 | `	/* Node successfully compiled */` |
|   86656 |  460 | `	return SXRET_OK;` |
|   43329 |  461 |  |
|       - |  462 | `/*` |
|       - |  463 | ` * Compile a single quoted string.` |
|       - |  464 | ` * According to the PHP language reference manual:` |
|       - |  465 | ` *` |
|       - |  466 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|       - |  467 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|       - |  468 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|       - |  469 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|       - |  470 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|       - |  471 | ` *` |
|       - |  472 | ` */` |
|   56640 |  473 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  474 |  |
|   56642 |  475 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  476 | `	const char *zIn,*zCur,*zEnd;` |
|       - |  477 | `	ph7_value *pObj;` |
|       - |  478 | `	sxu32 nIdx;` |
|   56642 |  479 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |  480 | `	/* Delimit the string */` |
|   56642 |  481 | `	zIn  = pStr->zString;` |
|   56642 |  482 | `	zEnd = &zIn[pStr->nByte];` |
|   56642 |  483 | `	if( zIn >= zEnd ){` |
|       - |  484 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |  485 | `		 * rather than reserving a new object each time. */` |
|     138 |  486 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     138 |  487 | `		return SXRET_OK;` |
|       - |  488 | `	}` |
|   56506 |  489 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |  490 | `		/* Already processed,emit the load constant instruction` |
|       - |  491 | `		 * and return.` |
|       - |  492 | `		 */` |
|   16744 |  493 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   16744 |  494 | `		return SXRET_OK;` |
|       - |  495 | `	}` |
|       - |  496 | `	/* Reserve a new constant */` |
|   39764 |  497 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   39764 |  498 | `	if( pObj == 0 ){` |
|     ! 0 |  499 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  500 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  501 | `		return SXERR_ABORT;` |
|       - |  502 | `	}` |
|   39764 |  503 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  504 | `	/* Compile the node */` |
|   39804 |  505 | `	for(;;){` |
|   79610 |  506 | `		if( zIn >= zEnd ){` |
|       - |  507 | `			/* End of input */` |
|   39764 |  508 | `			break;` |
|       - |  509 | `		}` |
|   39848 |  510 | `		zCur = zIn;` |
|  632650 |  511 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  592804 |  512 | `			zIn++;` |
|       2 |  513 | `		}` |
|   39848 |  514 | `		if( zIn > zCur ){` |
|       - |  515 | `			/* Append raw contents*/` |
|   39828 |  516 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   19913 |  517 | `		}` |
|   39848 |  518 | `		zIn++;` |
|   39848 |  519 | `		if( zIn < zEnd ){` |
|     105 |  520 | `			if( zIn[0] == '\\' ){` |
|       - |  521 | `				/* A literal backslash */` |
|      23 |  522 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      94 |  523 | `			}else if( zIn[0] == '\'' ){` |
|       - |  524 | `				/* A single quote */` |
|      11 |  525 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       6 |  526 | `			}else{` |
|       - |  527 | `				/* verbatim copy */` |
|      73 |  528 | `				zIn--;` |
|      73 |  529 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|      73 |  530 | `				zIn++;` |
|       - |  531 | `			}` |
|      52 |  532 | `		}` |
|       - |  533 | `		/* Advance the stream cursor */` |
|   39848 |  534 | `		zIn++;` |
|       2 |  535 | `	}` |
|       - |  536 | `	/* Emit the load constant instruction */` |
|   39764 |  537 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   39764 |  538 | `	if( pStr->nByte < 1024 ){` |
|       - |  539 | `		/* Install in the literal table */` |
|   39764 |  540 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   19881 |  541 | `	}` |
|       - |  542 | `	/* Node successfully compiled */` |
|   39764 |  543 | `	return SXRET_OK;` |
|   28322 |  544 |  |
|       - |  545 | `/*` |
|       - |  546 | ` * Compile a nowdoc string.` |
|       - |  547 | ` * According to the PHP language reference manual:` |
|       - |  548 | ` *` |
|       - |  549 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|       - |  550 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|       - |  551 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|       - |  552 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|       - |  553 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|       - |  554 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|       - |  555 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|       - |  556 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|       - |  557 | ` *  of the closing identifier.` |
|       - |  558 | ` */` |
|      28 |  559 | `static sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 |  560 |  |
|      29 |  561 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  562 | `	ph7_value *pObj;` |
|       - |  563 | `	sxu32 nIdx;` |
|      29 |  564 | `	nIdx = 0; /* Prevent compiler warning */` |
|      29 |  565 | `	if( pStr->nByte <= 0 ){` |
|       - |  566 | `		/* Empty string,load NULL */` |
|     ! 0 |  567 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     ! 0 |  568 | `		return SXRET_OK;` |
|       - |  569 | `	}` |
|       - |  570 | `	/* Reserve a new constant */` |
|      29 |  571 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      29 |  572 | `	if( pObj == 0 ){` |
|     ! 0 |  573 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  574 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  575 | `		return SXERR_ABORT;` |
|       - |  576 | `	}` |
|       - |  577 | `	/* No processing is done here, simply a memcpy() operation */` |
|      29 |  578 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|       - |  579 | `	/* Emit the load constant instruction */` |
|      29 |  580 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  581 | `	/* Node successfully compiled */` |
|      29 |  582 | `	return SXRET_OK;` |
|      15 |  583 |  |
|       - |  584 | `/*` |
|       - |  585 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|       - |  586 | ` * According to the PHP language reference manual` |
|       - |  587 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|       - |  588 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|       - |  589 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|       - |  590 | ` *  property in a string with a minimum of effort.` |
|       - |  591 | ` *  Simple syntax` |
|       - |  592 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|       - |  593 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|       - |  594 | ` *   the end of the name.` |
|       - |  595 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|       - |  596 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|       - |  597 | ` *   as to simple variables.` |
|       - |  598 | ` *  Complex (curly) syntax` |
|       - |  599 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|       - |  600 | ` *   of complex expressions.` |
|       - |  601 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|       - |  602 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|       - |  603 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|       - |  604 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|       - |  605 | ` */` |
|    1710 |  606 | `static sxi32 GenStateProcessStringExpression(` |
|       - |  607 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  608 | `	sxu32 nLine,         /* Line number */` |
|       - |  609 | `	const char *zIn,     /* Raw expression */` |
|       - |  610 | `	const char *zEnd     /* End of the expression */` |
|       - |  611 | `	)` |
|       2 |  612 |  |
|       - |  613 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  614 | `	SySet sToken;` |
|       - |  615 | `	sxi32 rc;` |
|       - |  616 | `	/* Initialize the token set */` |
|    1712 |  617 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |  618 | `	/* Preallocate some slots */` |
|    1712 |  619 | `	SySetAlloc(&sToken,0x08);` |
|       - |  620 | `	/* Tokenize the text */` |
|    1712 |  621 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |  622 | `	/* Swap delimiter */` |
|    1712 |  623 | `	pTmpIn  = pGen->pIn;` |
|    1712 |  624 | `	pTmpEnd = pGen->pEnd;` |
|    1712 |  625 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1712 |  626 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |  627 | `	/* Compile the expression */` |
|    1712 |  628 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  629 | `	/* Restore token stream */` |
|    1712 |  630 | `	pGen->pIn  = pTmpIn;` |
|    1712 |  631 | `	pGen->pEnd = pTmpEnd;` |
|       - |  632 | `	/* Release the token set */` |
|    1712 |  633 | `	SySetRelease(&sToken);` |
|       - |  634 | `	/* Compilation result */` |
|    1712 |  635 | `	return rc;` |
|       2 |  636 |  |
|       - |  637 | `/*` |
|       - |  638 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |  639 | ` */` |
|   16510 |  640 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |  641 |  |
|       - |  642 | `	ph7_value *pConstObj;` |
|   16512 |  643 | `	sxu32 nIdx = 0;` |
|       - |  644 | `	/* Reserve a new constant */` |
|   16512 |  645 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   16512 |  646 | `	if( pConstObj == 0 ){` |
|     ! 0 |  647 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  648 | `		return 0;` |
|       - |  649 | `	}` |
|   16512 |  650 | `	(*pCount)++;` |
|   16512 |  651 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  652 | `	/* Emit the load constant instruction */` |
|   16512 |  653 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   16512 |  654 | `	return pConstObj;` |
|    8257 |  655 |  |
|       - |  656 | `/*` |
|       - |  657 | ` * Compile a double quoted/heredoc string.` |
|       - |  658 | ` * According to the PHP language reference manual` |
|       - |  659 | ` * Heredoc` |
|       - |  660 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|       - |  661 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|       - |  662 | ` *  to close the quotation.` |
|       - |  663 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|       - |  664 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|       - |  665 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|       - |  666 | ` *  Warning` |
|       - |  667 | ` *  It is very important to note that the line with the closing identifier must contain` |
|       - |  668 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|       - |  669 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|       - |  670 | ` *  It's also important to realize that the first character before the closing identifier must` |
|       - |  671 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|       - |  672 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|       - |  673 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|       - |  674 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|       - |  675 | ` *  the end of the current file, a parse error will result at the last line.` |
|       - |  676 | ` *  Heredocs can not be used for initializing class properties.` |
|       - |  677 | ` * Double quoted` |
|       - |  678 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|       - |  679 | ` *  Escaped characters Sequence 	Meaning` |
|       - |  680 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|       - |  681 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|       - |  682 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|       - |  683 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|       - |  684 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|       - |  685 | ` *  \\ backslash` |
|       - |  686 | ` *  \$ dollar sign` |
|       - |  687 | ` *  \" double-quote` |
|       - |  688 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation` |
|       - |  689 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|       - |  690 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|       - |  691 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|       - |  692 | ` * See string parsing for details.` |
|       - |  693 | ` */` |
|   15306 |  694 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |  695 |  |
|   15308 |  696 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  697 | `	const char *zIn,*zCur,*zEnd;` |
|   15308 |  698 | `	ph7_value *pObj = 0;` |
|       - |  699 | `	sxi32 iCons;` |
|       - |  700 | `	sxi32 rc;` |
|       - |  701 | `	/* Delimit the string */` |
|   15308 |  702 | `	zIn  = pStr->zString;` |
|   15308 |  703 | `	zEnd = &zIn[pStr->nByte];` |
|   15308 |  704 | `	if( zIn >= zEnd ){` |
|       - |  705 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  706 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  707 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  708 | `		 */` |
|     226 |  709 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     226 |  710 | `		return SXRET_OK;` |
|       - |  711 | `	}` |
|   15084 |  712 | `	zCur = 0;` |
|       - |  713 | `	/* Compile the node */` |
|   15084 |  714 | `	iCons = 0;` |
|    8396 |  715 | `	for(;;){` |
|   25352 |  716 | `		zCur = zIn;` |
|  137946 |  717 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  114306 |  718 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      44 |  719 | `				break;` |
|  114222 |  720 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1628 |  721 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     814 |  722 | `					break;` |
|       - |  723 | `			}` |
|  112596 |  724 | `			zIn++;` |
|       2 |  725 | `		}` |
|   25352 |  726 | `		if( zIn > zCur ){` |
|   11850 |  727 | `			if( pObj == 0 ){` |
|   11574 |  728 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   11574 |  729 | `				if( pObj == 0 ){` |
|     ! 0 |  730 | `					return SXERR_ABORT;` |
|       - |  731 | `				}` |
|    5786 |  732 | `			}` |
|   11850 |  733 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    5924 |  734 | `		}` |
|   25352 |  735 | `		if( zIn >= zEnd ){` |
|   15084 |  736 | `			break;` |
|       - |  737 | `		}` |
|   10270 |  738 | `		if( zIn[0] == '\\' ){` |
|    8560 |  739 | `			const char *zPtr = 0;` |
|       - |  740 | `			sxu32 n;` |
|    8560 |  741 | `			zIn++;` |
|    8560 |  742 | `			if( zIn >= zEnd ){` |
|     ! 0 |  743 | `				break;` |
|       - |  744 | `			}` |
|    8560 |  745 | `			if( pObj == 0 ){` |
|    4940 |  746 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    4940 |  747 | `				if( pObj == 0 ){` |
|     ! 0 |  748 | `					return SXERR_ABORT;` |
|       - |  749 | `				}` |
|    2469 |  750 | `			}` |
|    8560 |  751 | `			n = sizeof(char); /* size of conversion */` |
|    8560 |  752 | `			switch( zIn[0] ){` |
|       3 |  753 | `			case '$':` |
|       - |  754 | `				/* Dollar sign */` |
|       7 |  755 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|       7 |  756 | `				break;` |
|      38 |  757 | `			case '\\':` |
|       - |  758 | `				/* A literal backslash */` |
|      78 |  759 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      78 |  760 | `				break;` |
|       2 |  761 | `			case 'a':` |
|       - |  762 | `				/* The "alert" character (BEL)[ctrl+g] ASCII code 7 */` |
|       5 |  763 | `				PH7_MemObjStringAppend(pObj,"\a",sizeof(char));` |
|       5 |  764 | `				break;` |
|       2 |  765 | `			case 'b':` |
|       - |  766 | `				/* Backspace (BS)[ctrl+h] ASCII code 8 */` |
|       5 |  767 | `				PH7_MemObjStringAppend(pObj,"\b",sizeof(char));` |
|       5 |  768 | `				break;` |
|       4 |  769 | `			case 'f':` |
|       - |  770 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|       9 |  771 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|       9 |  772 | `				break;` |
|    3914 |  773 | `			case 'n':` |
|       - |  774 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    7830 |  775 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    7830 |  776 | `				break;` |
|      19 |  777 | `			case 'r':` |
|       - |  778 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|      40 |  779 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|      40 |  780 | `				break;` |
|      24 |  781 | `			case 't':` |
|       - |  782 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      50 |  783 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      50 |  784 | `				break;` |
|       3 |  785 | `			case 'v':` |
|       - |  786 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|       7 |  787 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|       7 |  788 | `				break;` |
|       1 |  789 | `			case '\'':` |
|       - |  790 | `				/* Single quote */` |
|       3 |  791 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       3 |  792 | `				break;` |
|      50 |  793 | `			case '"':` |
|       - |  794 | `				/* Double quote */` |
|     102 |  795 | `				PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|     102 |  796 | `				break;` |
|       5 |  797 | `			case '0':` |
|       - |  798 | `				/* NUL byte */` |
|      11 |  799 | `				PH7_MemObjStringAppend(pObj,"\0",sizeof(char));` |
|      11 |  800 | `				break;` |
|     188 |  801 | `			case 'x':` |
|     377 |  802 | `				if((unsigned char)zIn[1] < 0xc0 && SyisHex(zIn[1]) ){` |
|       - |  803 | `					int c;` |
|       - |  804 | `					/* Hex digit */` |
|     363 |  805 | `					c = SyHexToint(zIn[1]) << 4;` |
|     363 |  806 | `					if( &zIn[2] < zEnd ){` |
|     363 |  807 | `						c +=  SyHexToint(zIn[2]);` |
|     181 |  808 | `					}` |
|       - |  809 | `					/* Output char */` |
|     363 |  810 | `					PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|     363 |  811 | `					n += sizeof(char) * 2;` |
|     182 |  812 | `				}else{` |
|       - |  813 | `					/* Output literal character  */` |
|      15 |  814 | `					PH7_MemObjStringAppend(pObj,"x",sizeof(char));` |
|       - |  815 | `				}` |
|     377 |  816 | `				break;` |
|      15 |  817 | `			case 'o':` |
|      31 |  818 | `				if( &zIn[1] < zEnd && (unsigned char)zIn[1] < 0xc0 && SyisDigit(zIn[1]) && (zIn[1] - '0') < 8 ){` |
|       - |  819 | `					/* Octal digit stream */` |
|       - |  820 | `					int c;` |
|      21 |  821 | `					c = 0;` |
|      21 |  822 | `					zIn++;` |
|      61 |  823 | `					for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|      55 |  824 | `						if( zPtr >= zEnd \|\| (unsigned char)zPtr[0] >= 0xc0 \|\| !SyisDigit(zPtr[0]) \|\| (zPtr[0] - '0') > 7 ){` |
|       8 |  825 | `							break;` |
|       - |  826 | `						}` |
|      41 |  827 | `						c = c * 8 + (zPtr[0] - '0');` |
|      21 |  828 | `					}` |
|      21 |  829 | `					if ( c > 0 ){` |
|      15 |  830 | `						PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|       7 |  831 | `					}` |
|      21 |  832 | `					n = (sxu32)(zPtr-zIn);` |
|      11 |  833 | `				}else{` |
|       - |  834 | `					/* Output literal character  */` |
|      11 |  835 | `					PH7_MemObjStringAppend(pObj,"o",sizeof(char));` |
|       - |  836 | `				}` |
|      31 |  837 | `				break;` |
|      11 |  838 | `			default:` |
|       - |  839 | `				/* Output without a slash */` |
|      23 |  840 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char));` |
|      22 |  841 | `				break;` |
|       - |  842 | `			}` |
|       - |  843 | `			/* Advance the stream cursor */` |
|    8560 |  844 | `			zIn += n;` |
|    8560 |  845 | `			continue;` |
|       - |  846 | `		}` |
|    1712 |  847 | `		if( zIn[0] == '{' ){` |
|       - |  848 | `			/* Curly syntax */` |
|       - |  849 | `			const char *zExpr;` |
|      87 |  850 | `			sxi32 iNest = 1;` |
|      87 |  851 | `			zIn++;` |
|      87 |  852 | `			zExpr = zIn;` |
|       - |  853 | `			/* Synchronize with the next closing curly braces */` |
|     985 |  854 | `			while( zIn < zEnd ){` |
|     985 |  855 | `				if( zIn[0] == '{' ){` |
|       - |  856 | `					/* Increment nesting level */` |
|       9 |  857 | `					iNest++;` |
|     981 |  858 | `				}else if(zIn[0] == '}' ){` |
|       - |  859 | `					/* Decrement nesting level */` |
|      95 |  860 | `					iNest--;` |
|      95 |  861 | `					if( iNest <= 0 ){` |
|      87 |  862 | `						break;` |
|       - |  863 | `					}` |
|       4 |  864 | `				}` |
|     899 |  865 | `				zIn++;` |
|       1 |  866 | `			}` |
|       - |  867 | `			/* Process the expression */` |
|      87 |  868 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      87 |  869 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  870 | `				return SXERR_ABORT;` |
|       - |  871 | `			}` |
|      87 |  872 | `			if( rc != SXERR_EMPTY ){` |
|      87 |  873 | `				++iCons;` |
|      43 |  874 | `			}` |
|      87 |  875 | `			if( zIn < zEnd ){` |
|       - |  876 | `				/* Jump the trailing curly */` |
|      87 |  877 | `				zIn++;` |
|      43 |  878 | `			}` |
|      44 |  879 | `		}else{` |
|       - |  880 | `			/* Simple syntax */` |
|    1626 |  881 | `			const char *zExpr = zIn;` |
|       - |  882 | `			/* Assemble variable name */` |
|     812 |  883 | `			for(;;){` |
|       - |  884 | `				/* Jump leading dollars */` |
|    3250 |  885 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1626 |  886 | `					zIn++;` |
|       2 |  887 | `				}` |
|     812 |  888 | `				for(;;){` |
|   10066 |  889 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    7630 |  890 | `						zIn++;` |
|       2 |  891 | `					}` |
|    1626 |  892 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  893 | `						/* UTF-8 stream */` |
|     ! 0 |  894 | `						zIn++;` |
|     ! 0 |  895 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  896 | `							zIn++;` |
|     ! 0 |  897 | `						}` |
|     ! 0 |  898 | `						continue;` |
|       - |  899 | `					}` |
|    1626 |  900 | `					break;` |
|     ! 0 |  901 | `				}` |
|    1626 |  902 | `				if( zIn >= zEnd ){` |
|      96 |  903 | `					break;` |
|       - |  904 | `				}` |
|    1532 |  905 | `				if( zIn[0] == '[' ){` |
|       9 |  906 | `					sxi32 iSquare = 1;` |
|       9 |  907 | `					zIn++;` |
|      17 |  908 | `					while( zIn < zEnd ){` |
|      17 |  909 | `						if( zIn[0] == '[' ){` |
|     ! 0 |  910 | `							iSquare++;` |
|      17 |  911 | `						}else if (zIn[0] == ']' ){` |
|       9 |  912 | `							iSquare--;` |
|       9 |  913 | `							if( iSquare <= 0 ){` |
|       9 |  914 | `								break;` |
|       - |  915 | `							}` |
|     ! 0 |  916 | `						}` |
|       9 |  917 | `						zIn++;` |
|       1 |  918 | `					}` |
|       9 |  919 | `					if( zIn < zEnd ){` |
|       9 |  920 | `						zIn++;` |
|       4 |  921 | `					}` |
|       9 |  922 | `					break;` |
|    1524 |  923 | `				}else if(zIn[0] == '{' ){` |
|       6 |  924 | `					sxi32 iCurly = 1;` |
|       6 |  925 | `					zIn++;` |
|      18 |  926 | `					while( zIn < zEnd ){` |
|      16 |  927 | `						if( zIn[0] == '{' ){` |
|     ! 0 |  928 | `							iCurly++;` |
|      16 |  929 | `						}else if (zIn[0] == '}' ){` |
|       3 |  930 | `							iCurly--;` |
|       3 |  931 | `							if( iCurly <= 0 ){` |
|       3 |  932 | `								break;` |
|       - |  933 | `							}` |
|     ! 0 |  934 | `						}` |
|      14 |  935 | `						zIn++;` |
|       2 |  936 | `					}` |
|       6 |  937 | `					if( zIn < zEnd ){` |
|       3 |  938 | `						zIn++;` |
|       1 |  939 | `					}` |
|       6 |  940 | `					break;` |
|    1520 |  941 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  942 | `					/* Member access operator '->' */` |
|     ! 0 |  943 | `					zIn += 2;` |
|    1520 |  944 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  945 | `					/* Static member access operator '::' */` |
|     ! 0 |  946 | `					zIn += 2;` |
|     ! 0 |  947 | `				}else{` |
|     761 |  948 | `					break;` |
|       - |  949 | `				}` |
|     ! 0 |  950 | `			}` |
|       - |  951 | `			/* Process the expression */` |
|    1626 |  952 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1626 |  953 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  954 | `				return SXERR_ABORT;` |
|       - |  955 | `			}` |
|    1626 |  956 | `			if( rc != SXERR_EMPTY ){` |
|    1624 |  957 | `				++iCons;` |
|     811 |  958 | `			}` |
|       - |  959 | `		}` |
|       - |  960 | `		/* Invalidate the previously used constant */` |
|    1712 |  961 | `		pObj = 0;` |
|       2 |  962 | `	}/*for(;;)*/` |
|   15084 |  963 | `	if( iCons > 1 ){` |
|       - |  964 | `		/* Concatenate all compiled constants */` |
|    1286 |  965 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     642 |  966 | `	}` |
|       - |  967 | `	/* Node successfully compiled */` |
|   15084 |  968 | `	return SXRET_OK;` |
|    7655 |  969 |  |
|       - |  970 | `/*` |
|       - |  971 | ` * Compile a double quoted string.` |
|       - |  972 | ` *  See the block-comment above for more information.` |
|       - |  973 | ` */` |
|   15280 |  974 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  975 |  |
|       - |  976 | `	sxi32 rc;` |
|   15282 |  977 | `	rc = GenStateCompileString(&(*pGen));` |
|    7640 |  978 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  979 | `	/* Compilation result */` |
|   15282 |  980 | `	return rc;` |
|       2 |  981 |  |
|       - |  982 | `/*` |
|       - |  983 | ` * Compile a Heredoc string.` |
|       - |  984 | ` *  See the block-comment above for more information.` |
|       - |  985 | ` */` |
|      26 |  986 | `static sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  987 |  |
|      28 |  988 | `	GenStateCompileString(&(*pGen));` |
|      13 |  989 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  990 | `	/* Compilation result */` |
|      28 |  991 | `	return SXRET_OK;` |
|       2 |  992 |  |
|       - |  993 | `/*` |
|       - |  994 | ` * Compile an array entry whether it is a key or a value.` |
|       - |  995 | ` *  Notes on array entries.` |
|       - |  996 | ` *  According to the PHP language reference manual` |
|       - |  997 | ` *  An array can be created by the array() language construct.` |
|       - |  998 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|       - |  999 | ` *  array(  key =>  value` |
|       - | 1000 | ` *    , ...` |
|       - | 1001 | ` *    )` |
|       - | 1002 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|       - | 1003 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|       - | 1004 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|       - | 1005 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|       - | 1006 | ` *  contain integer and string indices.` |
|       - | 1007 | ` *  A value can be any PHP type.` |
|       - | 1008 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|       - | 1009 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|       - | 1010 | ` *  is specified, that value will be overwritten.` |
|       - | 1011 | ` */` |
|   15744 | 1012 | `static sxi32 GenStateCompileArrayEntry(` |
|       - | 1013 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 1014 | `	SyToken *pIn,        /* Token stream */` |
|       - | 1015 | `	SyToken *pEnd,       /* End of the token stream */` |
|       - | 1016 | `	sxi32 iFlags,        /* Compilation flags */` |
|       - | 1017 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|       - | 1018 | `	)` |
|       2 | 1019 |  |
|       - | 1020 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 1021 | `	sxi32 rc;` |
|       - | 1022 | `	/* Swap token stream */` |
|   15746 | 1023 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - | 1024 | `	/* Compile the expression*/` |
|   15746 | 1025 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - | 1026 | `	/* Restore token stream */` |
|   15746 | 1027 | `	RE_SWAP_DELIMITER(pGen);` |
|   15746 | 1028 | `	return rc;` |
|       2 | 1029 |  |
|       - | 1030 | `/*` |
|       - | 1031 | ` * Expression tree validator callback for the 'array' language construct.` |
|       - | 1032 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1033 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1034 | ` * error message.` |
|       - | 1035 | ` * See the routine responible of compiling the array language construct` |
|       - | 1036 | ` * for more inforation.` |
|       - | 1037 | ` */` |
|      30 | 1038 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 1039 |  |
|      32 | 1040 | `	sxi32 rc = SXRET_OK;` |
|      32 | 1041 | `	if( pRoot->pOp ){` |
|      19 | 1042 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 | 1043 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      14 | 1044 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 1045 | `			/* Unexpected expression */` |
|      11 | 1046 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1047 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|      11 | 1048 | `			if( rc != SXERR_ABORT ){` |
|      11 | 1049 | `				rc = SXERR_INVALID;` |
|       5 | 1050 | `			}` |
|       7 | 1051 | `		}` |
|      25 | 1052 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1053 | `		/* Unexpected expression */` |
|       3 | 1054 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1055 | `			"array(): Expecting a variable after reference operator '&'");` |
|       3 | 1056 | `		if( rc != SXERR_ABORT ){` |
|       3 | 1057 | `			rc = SXERR_INVALID;` |
|       1 | 1058 | `		}` |
|       1 | 1059 | `	}` |
|      32 | 1060 | `	return rc;` |
|       2 | 1061 |  |
|       - | 1062 | `/*` |
|       - | 1063 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - | 1064 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - | 1065 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - | 1066 | ` */` |
|   22964 | 1067 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 | 1068 |  |
|       - | 1069 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - | 1070 | `	SyToken *pKey,*pCur;` |
|   22966 | 1071 | `	sxi32 iEmitRef = 0;` |
|   22966 | 1072 | `	sxi32 nPair = 0;` |
|       - | 1073 | `	sxi32 iNest;` |
|       - | 1074 | `	sxi32 rc;` |
|   22966 | 1075 | `	xValidator = 0;` |
|   18700 | 1076 | `	for(;;){` |
|       - | 1077 | `		/* Jump leading commas */` |
|   42320 | 1078 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    4920 | 1079 | `			pGen->pIn++;` |
|       2 | 1080 | `		}` |
|   37402 | 1081 | `		pCur = pGen->pIn;` |
|   37402 | 1082 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - | 1083 | `			/* No more entry to process */` |
|   22954 | 1084 | `			break;` |
|       - | 1085 | `		}` |
|   14450 | 1086 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 | 1087 | `			continue;` |
|       - | 1088 | `		}` |
|       - | 1089 | `		/* Compile the key if available */` |
|   14450 | 1090 | `		pKey = pCur;` |
|   14450 | 1091 | `		iNest = 0;` |
|   40066 | 1092 | `		while( pCur < pGen->pIn ){` |
|   26808 | 1093 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1192 | 1094 | `				break;` |
|       - | 1095 | `			}` |
|   25618 | 1096 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      78 | 1097 | `				iNest++;` |
|   25580 | 1098 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - | 1099 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - | 1100 | `				 * parser will shortly detect any syntax error.` |
|       - | 1101 | `				 */` |
|      78 | 1102 | `				iNest--;` |
|      38 | 1103 | `			}` |
|   25618 | 1104 | `			pCur++;` |
|       2 | 1105 | `		}` |
|   14450 | 1106 | `		rc = SXERR_EMPTY;` |
|   14450 | 1107 | `		if( pCur < pGen->pIn ){` |
|    1192 | 1108 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - | 1109 | `				/* Missing value */` |
|      11 | 1110 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      11 | 1111 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1112 | `					return SXERR_ABORT;` |
|       - | 1113 | `				}` |
|      11 | 1114 | `				return SXRET_OK;` |
|       - | 1115 | `			}` |
|       - | 1116 | `			/* Compile the expression holding the key */` |
|    1182 | 1117 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - | 1118 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1182 | 1119 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1120 | `				return SXERR_ABORT;` |
|       - | 1121 | `			}` |
|    1182 | 1122 | `			pCur++; /* Jump the '=>' operator */` |
|   13850 | 1123 | `		}else if( pKey == pCur ){` |
|       - | 1124 | `			/* Key is omitted,emit a warning */` |
|     ! 0 | 1125 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 | 1126 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 | 1127 | `		}else{` |
|       - | 1128 | `			/* Reset back the cursor and point to the entry value */` |
|   13260 | 1129 | `			pCur = pKey;` |
|       - | 1130 | `		}` |
|   14440 | 1131 | `		if( rc == SXERR_EMPTY ){` |
|       - | 1132 | `			/* No available key,load NULL */` |
|   13262 | 1133 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    6630 | 1134 | `		}` |
|   14440 | 1135 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - | 1136 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      34 | 1137 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      34 | 1138 | `			iEmitRef = 1;` |
|      34 | 1139 | `			pCur++; /* Jump the '&' token */` |
|      34 | 1140 | `			if( pCur >= pGen->pIn ){` |
|       - | 1141 | `				/* Missing value */` |
|       3 | 1142 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|       3 | 1143 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1144 | `					return SXERR_ABORT;` |
|       - | 1145 | `				}` |
|       3 | 1146 | `				return SXRET_OK;` |
|       - | 1147 | `			}` |
|      15 | 1148 | `		}` |
|       - | 1149 | `		/* Compile indice value */` |
|   14438 | 1150 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   14438 | 1151 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1152 | `			return SXERR_ABORT;` |
|       - | 1153 | `		}` |
|   14438 | 1154 | `		if( iEmitRef ){` |
|       - | 1155 | `			/* Emit the load reference instruction */` |
|      32 | 1156 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 | 1157 | `		}` |
|   14438 | 1158 | `		xValidator = 0;` |
|   14438 | 1159 | `		iEmitRef = 0;` |
|   14438 | 1160 | `		nPair++;` |
|       2 | 1161 | `	}` |
|       - | 1162 | `	/* Emit the load map instruction */` |
|   22954 | 1163 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - | 1164 | `	/* Node successfully compiled */` |
|   22954 | 1165 | `	return SXRET_OK;` |
|   11484 | 1166 |  |
|       - | 1167 | `/*` |
|       - | 1168 | ` * Compile the 'array' language construct.` |
|       - | 1169 | ` *	 According to the PHP language reference manual` |
|       - | 1170 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - | 1171 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - | 1172 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - | 1173 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - | 1174 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - | 1175 | ` */` |
|   22732 | 1176 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1177 |  |
|       - | 1178 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   22734 | 1179 | `	pGen->pIn += 2;` |
|   22734 | 1180 | `	pGen->pEnd--;` |
|   11366 | 1181 | `	SXUNUSED(iCompileFlag);` |
|   22734 | 1182 | `	return GenStateCompileArrayBody(pGen);` |
|       2 | 1183 |  |
|       - | 1184 | `/*` |
|       - | 1185 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - | 1186 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - | 1187 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - | 1188 | ` */` |
|     232 | 1189 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1190 |  |
|       - | 1191 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     234 | 1192 | `	pGen->pIn++;` |
|     234 | 1193 | `	pGen->pEnd--;` |
|     116 | 1194 | `	SXUNUSED(iCompileFlag);` |
|     234 | 1195 | `	return GenStateCompileArrayBody(pGen);` |
|       2 | 1196 |  |
|       - | 1197 | `/*` |
|       - | 1198 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - | 1199 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1200 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1201 | ` * error message.` |
|       - | 1202 | ` * See the routine responible of compiling the list language construct` |
|       - | 1203 | ` * for more inforation.` |
|       - | 1204 | ` */` |
|     128 | 1205 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 1206 |  |
|     130 | 1207 | `	sxi32 rc = SXRET_OK;` |
|     130 | 1208 | `	if( pRoot->pOp ){` |
|     ! 0 | 1209 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|     ! 0 | 1210 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - | 1211 | `				/* Unexpected expression */` |
|     ! 0 | 1212 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1213 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 | 1214 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 | 1215 | `					rc = SXERR_INVALID;` |
|     ! 0 | 1216 | `				}` |
|     ! 0 | 1217 | `		}` |
|     130 | 1218 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1219 | `		/* Unexpected expression */` |
|       5 | 1220 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1221 | `			"list(): Expecting a variable not an expression");` |
|       5 | 1222 | `		if( rc != SXERR_ABORT ){` |
|       5 | 1223 | `			rc = SXERR_INVALID;` |
|       2 | 1224 | `		}` |
|       2 | 1225 | `	}` |
|     130 | 1226 | `	return rc;` |
|       2 | 1227 |  |
|       - | 1228 | `/*` |
|       - | 1229 | ` * Compile the 'list' language construct.` |
|       - | 1230 | ` *  According to the PHP language reference` |
|       - | 1231 | ` *  list(): Assign variables as if they were an array.` |
|       - | 1232 | ` *  list() is used to assign a list of variables in one operation.` |
|       - | 1233 | ` *  Description` |
|       - | 1234 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - | 1235 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - | 1236 | ` *   list() is used to assign a list of variables in one operation.` |
|       - | 1237 | ` *  Parameters` |
|       - | 1238 | ` *   $varname: A variable.` |
|       - | 1239 | ` *  Return Values` |
|       - | 1240 | ` *   The assigned array.` |
|       - | 1241 | ` */` |
|       - | 1242 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - | 1243 | `struct NestedListEntry {` |
|       - | 1244 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - | 1245 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - | 1246 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - | 1247 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - | 1248 | `};` |
|       - | 1249 | `/*` |
|       - | 1250 | ` * Shared body for list() and short list [...] compilation.` |
|       - | 1251 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - | 1252 | ` * the opening delimiter and before the closing delimiter.` |
|       - | 1253 | ` */` |
|      74 | 1254 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       2 | 1255 |  |
|       - | 1256 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - | 1257 | `	SyToken *pNext;` |
|       - | 1258 | `	sxi32 nExpr;` |
|       - | 1259 | `	sxi32 rc;` |
|      76 | 1260 | `	nExpr = 0;` |
|      76 | 1261 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     230 | 1262 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     156 | 1263 | `		if( pGen->pIn < pNext ){` |
|       - | 1264 | `			/* Check for nested list() */` |
|     144 | 1265 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 | 1266 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 1267 | `				/* Record this nested list for post-processing */` |
|       3 | 1268 | `				SyToken *pListEnd = 0;` |
|       3 | 1269 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 | 1270 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 | 1271 | `				}` |
|       3 | 1272 | `				if( pListEnd ){` |
|       - | 1273 | `					struct NestedListEntry sEntry;` |
|       3 | 1274 | `					sEntry.nIndex = nExpr;` |
|       3 | 1275 | `					sEntry.pStart = pGen->pIn;` |
|       3 | 1276 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 | 1277 | `					sEntry.isShort = 0;` |
|       3 | 1278 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 | 1279 | `				}` |
|       - | 1280 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 | 1281 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     143 | 1282 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - | 1283 | `				/* Nested short destructuring [...] */` |
|      13 | 1284 | `				SyToken *pBracketEnd = 0;` |
|      13 | 1285 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 | 1286 | `				if( pBracketEnd ){` |
|       - | 1287 | `					struct NestedListEntry sEntry;` |
|      13 | 1288 | `					sEntry.nIndex = nExpr;` |
|      13 | 1289 | `					sEntry.pStart = pGen->pIn;` |
|      13 | 1290 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 | 1291 | `					sEntry.isShort = 1;` |
|      13 | 1292 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 | 1293 | `				}` |
|       - | 1294 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 | 1295 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 | 1296 | `			}else{` |
|       - | 1297 | `				/* Compile the expression holding the variable */` |
|     130 | 1298 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     130 | 1299 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 1300 | `					SySetRelease(&sNested);` |
|     ! 0 | 1301 | `					return SXRET_OK;` |
|       - | 1302 | `				}` |
|       - | 1303 | `			}` |
|      73 | 1304 | `		}else{` |
|       - | 1305 | `			/* Empty entry,load NULL */` |
|      13 | 1306 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - | 1307 | `		}` |
|     156 | 1308 | `		nExpr++;` |
|       - | 1309 | `		/* Advance the stream cursor */` |
|     156 | 1310 | `		pGen->pIn = &pNext[1];` |
|       2 | 1311 | `	}` |
|       - | 1312 | `	/* Emit the LOAD_LIST instruction */` |
|      76 | 1313 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - | 1314 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - | 1315 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - | 1316 | `	 * at the corresponding index and recursively destructure it.` |
|       - | 1317 | `	 */` |
|      76 | 1318 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 | 1319 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - | 1320 | `		sxu32 i;` |
|      27 | 1321 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 | 1322 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 | 1323 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - | 1324 | `			ph7_value *pIdx;` |
|       - | 1325 | `			sxu32 nConstIdx;` |
|       - | 1326 | `			/* DUP the source array (it's on stack top) */` |
|      15 | 1327 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - | 1328 | `			/* Push the integer index for this nested entry */` |
|      15 | 1329 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 | 1330 | `			if( pIdx == 0 ){` |
|     ! 0 | 1331 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1332 | `				SySetRelease(&sNested);` |
|     ! 0 | 1333 | `				return SXERR_ABORT;` |
|       - | 1334 | `			}` |
|      15 | 1335 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 | 1336 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - | 1337 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - | 1338 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - | 1339 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - | 1340 | `			 */` |
|      15 | 1341 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - | 1342 | `			/* Recursively compile the inner list */` |
|      15 | 1343 | `			pGen->pIn = apNested[i].pStart;` |
|      15 | 1344 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 | 1345 | `			if( apNested[i].isShort ){` |
|      13 | 1346 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 | 1347 | `			}else{` |
|       3 | 1348 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - | 1349 | `			}` |
|      15 | 1350 | `			pGen->pIn = pSavedIn;` |
|      15 | 1351 | `			pGen->pEnd = pSavedEnd;` |
|      15 | 1352 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1353 | `				SySetRelease(&sNested);` |
|     ! 0 | 1354 | `				return SXERR_ABORT;` |
|       - | 1355 | `			}` |
|       - | 1356 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 | 1357 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 | 1358 | `		}` |
|       6 | 1359 | `	}` |
|      76 | 1360 | `	SySetRelease(&sNested);` |
|       - | 1361 | `	/* Node successfully compiled */` |
|      76 | 1362 | `	return SXRET_OK;` |
|      39 | 1363 |  |
|      32 | 1364 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1365 |  |
|       - | 1366 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      34 | 1367 | `	pGen->pIn += 2;` |
|      34 | 1368 | `	pGen->pEnd--;` |
|      16 | 1369 | `	SXUNUSED(iCompileFlag);` |
|      34 | 1370 | `	return GenStateCompileListBody(pGen);` |
|       2 | 1371 |  |
|      42 | 1372 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1373 |  |
|       - | 1374 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      44 | 1375 | `	pGen->pIn++;` |
|      44 | 1376 | `	pGen->pEnd--;` |
|      21 | 1377 | `	SXUNUSED(iCompileFlag);` |
|      44 | 1378 | `	return GenStateCompileListBody(pGen);` |
|       2 | 1379 |  |
|       - | 1380 | `/* Forward declarations */` |
|       - | 1381 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - | 1382 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - | 1383 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - | 1384 | `/*` |
|       - | 1385 | ` * Compile an annoynmous function or a closure.` |
|       - | 1386 | ` * According to the PHP language reference` |
|       - | 1387 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - | 1388 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - | 1389 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - | 1390 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - | 1391 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - | 1392 | ` *  Example Anonymous function variable assignment example` |
|       - | 1393 | ` * <?php` |
|       - | 1394 | ` * $greet = function($name)` |
|       - | 1395 | ` * {` |
|       - | 1396 | ` *    printf("Hello %s\r\n", $name);` |
|       - | 1397 | ` * };` |
|       - | 1398 | ` * $greet('World');` |
|       - | 1399 | ` * $greet('PHP');` |
|       - | 1400 | ` * ?>` |
|       - | 1401 | ` * Note that the implementation of annoynmous function and closure under` |
|       - | 1402 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - | 1403 | ` */` |
|     166 | 1404 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1405 |  |
|       - | 1406 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - | 1407 | `	char zName[512];         /* Unique lambda name */` |
|       - | 1408 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - | 1409 | `							  * one thread is allowed to compile the script.` |
|       - | 1410 | `						      */` |
|       - | 1411 | `	ph7_value *pObj;` |
|       - | 1412 | `	SyString sName;` |
|       - | 1413 | `	sxu32 nIdx;` |
|       - | 1414 | `	sxu32 nLen;` |
|       - | 1415 | `	sxi32 rc;` |
|       - | 1416 |  |
|     168 | 1417 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     168 | 1418 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 | 1419 | `		pGen->pIn++;` |
|     ! 0 | 1420 | `	}` |
|       - | 1421 | `	/* Reserve a constant for the lambda */` |
|     168 | 1422 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     168 | 1423 | `	if( pObj == 0 ){` |
|     ! 0 | 1424 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1425 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1426 | `		return SXERR_ABORT;` |
|       - | 1427 | `	}` |
|       - | 1428 | `	/* Generate a unique name */` |
|     168 | 1429 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - | 1430 | `	/* Make sure the generated name is unique */` |
|     168 | 1431 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 | 1432 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 | 1433 | `	}` |
|     168 | 1434 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     168 | 1435 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - | 1436 | `	/* Compile the lambda body */` |
|     168 | 1437 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     168 | 1438 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1439 | `		return SXERR_ABORT;` |
|       - | 1440 | `	}` |
|     168 | 1441 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 1442 | `		/* Emit the load closure instruction */` |
|      14 | 1443 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       8 | 1444 | `	}else{` |
|       - | 1445 | `		/* Emit the load constant instruction */` |
|     156 | 1446 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1447 | `	}` |
|       - | 1448 | `	/* Node successfully compiled */` |
|     168 | 1449 | `	return SXRET_OK;` |
|      85 | 1450 |  |
|       - | 1451 | `/*` |
|       - | 1452 | ` * Compile a backtick quoted string.` |
|       - | 1453 | ` */` |
|       4 | 1454 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 1455 |  |
|       - | 1456 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - | 1457 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - | 1458 | `	 */` |
|       7 | 1459 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - | 1460 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 | 1461 | `		ph7_lib_version()` |
|       - | 1462 | `		);` |
|       - | 1463 | `	/* Load NULL */` |
|       5 | 1464 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1465 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 1466 | `	/* Node successfully compiled */` |
|       5 | 1467 | `	return SXRET_OK;` |
|       1 | 1468 |  |
|       - | 1469 | `/*` |
|       - | 1470 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - | 1471 | ` * construct.` |
|       - | 1472 | ` */` |
|      72 | 1473 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1474 |  |
|       - | 1475 | `	SyString *pName;` |
|       - | 1476 | `	sxu32 nKeyID;` |
|       - | 1477 | `	sxi32 rc;` |
|       - | 1478 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      74 | 1479 | `	pName = &pGen->pIn->sData;` |
|      74 | 1480 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      74 | 1481 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      74 | 1482 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 | 1483 | `		SyToken *pTmp,*pNext = 0;` |
|       - | 1484 | `		/* Compile arguments one after one */` |
|       9 | 1485 | `		pTmp = pGen->pEnd;` |
|       - | 1486 | `		/* Symisc eXtension to the PHP programming language:` |
|       - | 1487 | `		 * 'echo' can be used in the context of a function which` |
|       - | 1488 | `		 *  mean that the following expression is valid:` |
|       - | 1489 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - | 1490 | `		 */` |
|       9 | 1491 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 | 1492 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 | 1493 | `			if( pGen->pIn < pNext ){` |
|       9 | 1494 | `				pGen->pEnd = pNext;` |
|       9 | 1495 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 | 1496 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1497 | `					return SXERR_ABORT;` |
|       - | 1498 | `				}` |
|       9 | 1499 | `				if( rc != SXERR_EMPTY ){` |
|       - | 1500 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - | 1501 | `					 * without the overhead of a function call.` |
|       - | 1502 | `					 * This is a very powerful optimization that improve` |
|       - | 1503 | `					 * performance greatly.` |
|       - | 1504 | `					 */` |
|       9 | 1505 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 | 1506 | `				}` |
|       4 | 1507 | `			}` |
|       - | 1508 | `			/* Jump trailing commas */` |
|       9 | 1509 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 | 1510 | `				pNext++;` |
|     ! 0 | 1511 | `			}` |
|       9 | 1512 | `			pGen->pIn = pNext;` |
|       1 | 1513 | `		}` |
|       - | 1514 | `		/* Restore token stream */` |
|       9 | 1515 | `		pGen->pEnd = pTmp;` |
|       5 | 1516 | `	}else{` |
|      66 | 1517 | `		sxi32 nArg = 0;` |
|      66 | 1518 | `		sxu32 nIdx = 0;` |
|      66 | 1519 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      66 | 1520 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1521 | `			return SXERR_ABORT;` |
|      66 | 1522 | `		}else if(rc != SXERR_EMPTY ){` |
|      66 | 1523 | `			nArg = 1;` |
|      32 | 1524 | `		}` |
|      66 | 1525 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - | 1526 | `			ph7_value *pObj;` |
|       - | 1527 | `			/* Emit the call instruction */` |
|      20 | 1528 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      20 | 1529 | `			if( pObj == 0 ){` |
|     ! 0 | 1530 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1531 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1532 | `				return SXERR_ABORT;` |
|       - | 1533 | `			}` |
|      20 | 1534 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - | 1535 | `			/* Install in the literal table */` |
|      20 | 1536 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       9 | 1537 | `		}` |
|       - | 1538 | `		/* Emit the call instruction */` |
|      66 | 1539 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      66 | 1540 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,0,0);` |
|       - | 1541 | `	}` |
|       - | 1542 | `	/* Node successfully compiled */` |
|      74 | 1543 | `	return SXRET_OK;` |
|      38 | 1544 |  |
|       - | 1545 | `/*` |
|       - | 1546 | ` * Compile a node holding a variable declaration.` |
|       - | 1547 | ` * According to the PHP language reference` |
|       - | 1548 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - | 1549 | ` *  The variable name is case-sensitive.` |
|       - | 1550 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - | 1551 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1552 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - | 1553 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - | 1554 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - | 1555 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - | 1556 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - | 1557 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - | 1558 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - | 1559 | ` *  the chapter on Expressions.` |
|       - | 1560 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - | 1561 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - | 1562 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - | 1563 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - | 1564 | ` *  is being assigned (the source variable).` |
|       - | 1565 | ` */` |
|  766348 | 1566 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1567 |  |
|  766350 | 1568 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1569 | `	sxi32 iVv;` |
|       - | 1570 | `	sxi32 iP1;` |
|       - | 1571 | `	void *p3;` |
|       - | 1572 | `	sxi32 rc;` |
|  766350 | 1573 | `	iVv = -1; /* Variable variable counter */` |
| 1532710 | 1574 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  766362 | 1575 | `		pGen->pIn++;` |
|  766362 | 1576 | `		iVv++;` |
|       2 | 1577 | `	}` |
|  766350 | 1578 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 1579 | `		/* Invalid variable name */` |
|     ! 0 | 1580 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 | 1581 | `		if( rc == SXERR_ABORT ){` |
|       - | 1582 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1583 | `			return SXERR_ABORT;` |
|       - | 1584 | `		}` |
|     ! 0 | 1585 | `		return SXRET_OK;` |
|       - | 1586 | `	}` |
|  766350 | 1587 | `	p3  = 0;` |
|  766350 | 1588 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - | 1589 | `		/* Dynamic variable creation */` |
|      18 | 1590 | `		pGen->pIn++;  /* Jump the open curly */` |
|      18 | 1591 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      18 | 1592 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 1593 | `			/* Empty expression */` |
|       3 | 1594 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 | 1595 | `			return SXRET_OK;` |
|       - | 1596 | `		}` |
|       - | 1597 | `		/* Compile the expression holding the variable name */` |
|      16 | 1598 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 | 1599 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1600 | `			return SXERR_ABORT;` |
|      16 | 1601 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 | 1602 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 | 1603 | `			return SXRET_OK;` |
|       - | 1604 | `		}` |
|       7 | 1605 | `	}else{` |
|       - | 1606 | `		SyHashEntry *pEntry;` |
|       - | 1607 | `		SyString *pName;` |
|  766334 | 1608 | `		char *zName = 0;` |
|       - | 1609 | `		/* Extract variable name */` |
|  766334 | 1610 | `		pName = &pGen->pIn->sData;` |
|       - | 1611 | `		/* Advance the stream cursor */` |
|  766334 | 1612 | `		pGen->pIn++;` |
|  766334 | 1613 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  766334 | 1614 | `		if( pEntry == 0 ){` |
|       - | 1615 | `			/* Duplicate name */` |
|  110170 | 1616 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  110170 | 1617 | `			if( zName == 0 ){` |
|     ! 0 | 1618 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1619 | `				return SXERR_ABORT;` |
|       - | 1620 | `			}` |
|       - | 1621 | `			/* Install in the hashtable */` |
|  110170 | 1622 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   55086 | 1623 | `		}else{` |
|       - | 1624 | `			/* Name already available */` |
|  656166 | 1625 | `			zName = (char *)pEntry->pUserData;` |
|       - | 1626 | `		}` |
|  766334 | 1627 | `		p3 = (void *)zName;` |
|       - | 1628 | `	}` |
|  766346 | 1629 | `	iP1 = 0;` |
|  766346 | 1630 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  294716 | 1631 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 1632 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  288704 | 1633 | `			iP1 = 1;` |
|  144351 | 1634 | `		}` |
|  147357 | 1635 | `	}` |
|       - | 1636 | `	/* Emit the load instruction */` |
|  766346 | 1637 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  766358 | 1638 | `	while( iVv > 0 ){` |
|      13 | 1639 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 | 1640 | `		iVv--;` |
|       1 | 1641 | `	}` |
|       - | 1642 | `	/* Node successfully compiled */` |
|  766346 | 1643 | `	return SXRET_OK;` |
|  383176 | 1644 |  |
|       - | 1645 | `/*` |
|       - | 1646 | ` * Load a literal.` |
|       - | 1647 | ` */` |
|  514004 | 1648 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 | 1649 |  |
|  514006 | 1650 | `	SyToken *pToken = pGen->pIn;` |
|       - | 1651 | `	ph7_value *pObj;` |
|       - | 1652 | `	SyString *pStr;` |
|       - | 1653 | `	sxu32 nIdx;` |
|       - | 1654 | `	/* Extract token value */` |
|  514006 | 1655 | `	pStr = &pToken->sData;` |
|       - | 1656 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  514006 | 1657 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   93342 | 1658 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 1659 | `			/* NULL constant are always indexed at 0 */` |
|   39674 | 1660 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   39674 | 1661 | `			return SXRET_OK;` |
|   53670 | 1662 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 1663 | `			/* TRUE constant are always indexed at 1 */` |
|     488 | 1664 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     488 | 1665 | `			return SXRET_OK;` |
|       2 | 1666 | `		}` |
|  487808 | 1667 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   81102 | 1668 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 1669 | `			/* FALSE constant are always indexed at 2 */` |
|   34642 | 1670 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   34642 | 1671 | `			return SXRET_OK;` |
|  421839 | 1672 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   71626 | 1673 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 1674 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    5248 | 1675 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    5248 | 1676 | `			if( pObj == 0 ){` |
|     ! 0 | 1677 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1678 | `				return SXERR_ABORT;` |
|       - | 1679 | `			}` |
|    5248 | 1680 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 1681 | `			/* Emit the load constant instruction */` |
|    5248 | 1682 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5248 | 1683 | `			return SXRET_OK;` |
|  394008 | 1684 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   26456 | 1685 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - | 1686 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 | 1687 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 | 1688 | `			if( pObj == 0 ){` |
|     ! 0 | 1689 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1690 | `				return SXERR_ABORT;` |
|       - | 1691 | `			}` |
|       7 | 1692 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - | 1693 | `				SyString sNs;` |
|       7 | 1694 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 | 1695 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 | 1696 | `			}else{` |
|     ! 0 | 1697 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - | 1698 | `			}` |
|       7 | 1699 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 | 1700 | `			return SXRET_OK;` |
|  393203 | 1701 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   11072 | 1702 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  387661 | 1703 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   13792 | 1704 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 | 1705 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - | 1706 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 | 1707 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - | 1708 | `				/* Point to the upper block */` |
|      11 | 1709 | `				pBlock = pBlock->pParent;` |
|       1 | 1710 | `			}` |
|      11 | 1711 | `			if( pBlock == 0 ){` |
|       - | 1712 | `				/* Called in the global scope,load NULL */` |
|       5 | 1713 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 | 1714 | `			}else{` |
|       - | 1715 | `				/* Extract the target function/method */` |
|       7 | 1716 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 | 1717 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - | 1718 | `					/* Not a class method,Load null */` |
|       3 | 1719 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1720 | `				}else{` |
|       5 | 1721 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 | 1722 | `					if( pObj == 0 ){` |
|     ! 0 | 1723 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1724 | `						return SXERR_ABORT;` |
|       - | 1725 | `					}` |
|       5 | 1726 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - | 1727 | `					/* Emit the load constant instruction */` |
|       5 | 1728 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1729 | `				}` |
|       - | 1730 | `			}` |
|      11 | 1731 | `			return SXRET_OK;` |
|       - | 1732 | `	}` |
|       - | 1733 | `	/* Query literal table */` |
|  433946 | 1734 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - | 1735 | `		ph7_value *pLitObj;` |
|       - | 1736 | `		/* Unknown literal,install it in the literal table */` |
|  203014 | 1737 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  203014 | 1738 | `		if( pLitObj == 0 ){` |
|     ! 0 | 1739 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1740 | `			return SXERR_ABORT;` |
|       - | 1741 | `		}` |
|  203014 | 1742 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  203014 | 1743 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  101506 | 1744 | `	}` |
|       - | 1745 | `	/* Emit the load constant instruction */` |
|  433946 | 1746 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  433946 | 1747 | `	return SXRET_OK;` |
|  257004 | 1748 |  |
|       - | 1749 | `/*` |
|       - | 1750 | ` * Resolve a namespace path or simply load a literal.` |
|       - | 1751 | ` * If the token stream contains namespace separators (backslashes),` |
|       - | 1752 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - | 1753 | ` * Otherwise, load the simple literal directly.` |
|       - | 1754 | ` */` |
|  514028 | 1755 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 | 1756 |  |
|       - | 1757 | `	sxi32 rc;` |
|  514030 | 1758 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 1759 | `		return SXRET_OK;` |
|       - | 1760 | `	}` |
|       - | 1761 | `	/* Check if this is a multi-token namespace path */` |
|  514030 | 1762 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - | 1763 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      26 | 1764 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      26 | 1765 | `		int isAbsolute = 0;` |
|      26 | 1766 | `		SyBlobReset(pWorker);` |
|       - | 1767 | `		/* Check for leading backslash (absolute path) */` |
|      26 | 1768 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      24 | 1769 | `			isAbsolute = 1;` |
|      24 | 1770 | `			pGen->pIn++; /* Skip leading backslash */` |
|      11 | 1771 | `		}` |
|       - | 1772 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      26 | 1773 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 | 1774 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 | 1775 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 | 1776 | `		}` |
|       - | 1777 | `		/* Collect all path components */` |
|     102 | 1778 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     102 | 1779 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      40 | 1780 | `				SyBlobAppend(pWorker,"\\",1);` |
|      21 | 1781 | `			}else{` |
|      64 | 1782 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 1783 | `			}` |
|     102 | 1784 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      26 | 1785 | `				pGen->pIn++;` |
|      26 | 1786 | `				break;` |
|       - | 1787 | `			}` |
|      78 | 1788 | `			pGen->pIn++;` |
|       2 | 1789 | `		}` |
|      26 | 1790 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - | 1791 | `			ph7_value *pObj;` |
|       - | 1792 | `			SyString sPath;` |
|       - | 1793 | `			sxu32 nIdx;` |
|      26 | 1794 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - | 1795 | `			/* Install in the literal table */` |
|      26 | 1796 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      13 | 1797 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      13 | 1798 | `				if( pObj == 0 ){` |
|     ! 0 | 1799 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1800 | `					return SXERR_ABORT;` |
|       - | 1801 | `				}` |
|      13 | 1802 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      13 | 1803 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       6 | 1804 | `			}` |
|       - | 1805 | `			/* Emit the load constant instruction.` |
|       - | 1806 | `			 * P1=1 means candidate for constant/function/class expansion. */` |
|      26 | 1807 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|      26 | 1808 | `			return SXRET_OK;` |
|       - | 1809 | `		}` |
|     ! 0 | 1810 | `	}` |
|       - | 1811 | `	/* Single-token literal: load directly */` |
|  514006 | 1812 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  514006 | 1813 | `	return rc;` |
|  257016 | 1814 |  |
|       - | 1815 | `/*` |
|       - | 1816 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 1817 | ` */` |
|  514028 | 1818 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1819 |  |
|       - | 1820 | `	sxi32 rc;` |
|  514030 | 1821 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  514030 | 1822 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1823 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1824 | `		return rc;` |
|       - | 1825 | `	}` |
|       - | 1826 | `	/* Node successfully compiled */` |
|  514030 | 1827 | `	return SXRET_OK;` |
|  257016 | 1828 |  |
|       - | 1829 | `/*` |
|       - | 1830 | ` * Recover from a compile-time error. In other words synchronize` |
|       - | 1831 | ` * the token stream cursor with the first semi-colon seen.` |
|       - | 1832 | ` */` |
|       8 | 1833 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 | 1834 |  |
|       - | 1835 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 | 1836 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 | 1837 | `		pGen->pIn++;` |
|       1 | 1838 | `	}` |
|       9 | 1839 | `	return SXRET_OK;` |
|       1 | 1840 |  |
|       - | 1841 | `/*` |
|       - | 1842 | ` * Check if the given identifier name is reserved or not.` |
|       - | 1843 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - | 1844 | ` */` |
|      56 | 1845 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       2 | 1846 |  |
|      58 | 1847 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      26 | 1848 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 | 1849 | `			return TRUE;` |
|      24 | 1850 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 | 1851 | `			return TRUE;` |
|       2 | 1852 | `		}` |
|      43 | 1853 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 | 1854 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 | 1855 | `			return TRUE;` |
|       - | 1856 | `		}` |
|     ! 0 | 1857 | `	}` |
|       - | 1858 | `	/* Not a reserved constant */` |
|      50 | 1859 | `	return FALSE;` |
|      30 | 1860 |  |
|       - | 1861 | `/*` |
|       - | 1862 | ` * Compile the 'const' statement.` |
|       - | 1863 | ` * According to the PHP language reference` |
|       - | 1864 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - | 1865 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - | 1866 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - | 1867 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - | 1868 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1869 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - | 1870 | ` *  Syntax` |
|       - | 1871 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - | 1872 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - | 1873 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - | 1874 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - | 1875 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - | 1876 | ` *  to get a list of all defined constants.` |
|       - | 1877 | ` *` |
|       - | 1878 | ` * Symisc eXtension.` |
|       - | 1879 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - | 1880 | ` *  would allow only simple scalar value.` |
|       - | 1881 | ` *  Example` |
|       - | 1882 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 1883 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 1884 | ` */` |
|      32 | 1885 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       2 | 1886 |  |
|       - | 1887 | `	SySet *pConsCode,*pInstrContainer;` |
|      34 | 1888 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1889 | `	SyString *pName;` |
|       - | 1890 | `	sxi32 rc;` |
|      34 | 1891 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      34 | 1892 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 1893 | `		/* Invalid constant name */` |
|       7 | 1894 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       7 | 1895 | `		if( rc == SXERR_ABORT ){` |
|       - | 1896 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1897 | `			return SXERR_ABORT;` |
|       - | 1898 | `		}` |
|       7 | 1899 | `		goto Synchronize;` |
|       - | 1900 | `	}` |
|       - | 1901 | `	/* Peek constant name */` |
|      28 | 1902 | `	pName = &pGen->pIn->sData;` |
|       - | 1903 | `	/* Make sure the constant name isn't reserved */` |
|      28 | 1904 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 1905 | `		/* Reserved constant */` |
|       9 | 1906 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 | 1907 | `		if( rc == SXERR_ABORT ){` |
|       - | 1908 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1909 | `			return SXERR_ABORT;` |
|       - | 1910 | `		}` |
|       9 | 1911 | `		goto Synchronize;` |
|       - | 1912 | `	}` |
|      20 | 1913 | `	pGen->pIn++;` |
|      20 | 1914 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 1915 | `		/* Invalid statement*/` |
|       5 | 1916 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       5 | 1917 | `		if( rc == SXERR_ABORT ){` |
|       - | 1918 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1919 | `			return SXERR_ABORT;` |
|       - | 1920 | `		}` |
|       5 | 1921 | `		goto Synchronize;` |
|       - | 1922 | `	}` |
|      15 | 1923 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - | 1924 | `	/* Allocate a new constant value container */` |
|      15 | 1925 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 | 1926 | `	if( pConsCode == 0 ){` |
|     ! 0 | 1927 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1928 | `		return SXERR_ABORT;` |
|       - | 1929 | `	}` |
|      15 | 1930 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 1931 | `	/* Swap bytecode container */` |
|      15 | 1932 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 | 1933 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - | 1934 | `	/* Compile constant value */` |
|      15 | 1935 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 1936 | `	/* Emit the done instruction */` |
|      15 | 1937 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 | 1938 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 | 1939 | `	if( rc == SXERR_ABORT ){` |
|       - | 1940 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1941 | `		return SXERR_ABORT;` |
|       - | 1942 | `	}` |
|      15 | 1943 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - | 1944 | `	/* Register the constant with namespace-qualified name */` |
|       - | 1945 | `	{` |
|       - | 1946 | `		SyBlob sFQN;` |
|       - | 1947 | `		SyString sFQNStr;` |
|      15 | 1948 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 | 1949 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 | 1950 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 | 1951 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 | 1952 | `		SyBlobRelease(&sFQN);` |
|       - | 1953 | `	}` |
|      15 | 1954 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1955 | `		SySetRelease(pConsCode);` |
|     ! 0 | 1956 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 | 1957 | `	}` |
|      15 | 1958 | `	return SXRET_OK;` |
|       9 | 1959 | `Synchronize:` |
|       - | 1960 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      57 | 1961 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      39 | 1962 | `		pGen->pIn++;` |
|       1 | 1963 | `	}` |
|      19 | 1964 | `	return SXRET_OK;` |
|      18 | 1965 |  |
|       - | 1966 | `/*` |
|       - | 1967 | ` * Compile the 'continue' statement.` |
|       - | 1968 | ` * According to the PHP language reference` |
|       - | 1969 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - | 1970 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - | 1971 | ` *  iteration.` |
|       - | 1972 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - | 1973 | ` *  the purposes of continue.` |
|       - | 1974 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - | 1975 | ` *  of enclosing loops it should skip to the end of.` |
|       - | 1976 | ` *  Note:` |
|       - | 1977 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - | 1978 | ` */` |
|       - | 1979 | `/*` |
|       - | 1980 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - | 1981 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - | 1982 | ` * break/continue crosses a try boundary.` |
|       - | 1983 | ` *` |
|       - | 1984 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - | 1985 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - | 1986 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - | 1987 | ` */` |
|    2742 | 1988 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       2 | 1989 |  |
|    2744 | 1990 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   16014 | 1991 | `	while( pBlock && pBlock != pTarget ){` |
|   13272 | 1992 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 | 1993 | `			if( pBlock->pUserData ){` |
|       - | 1994 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 | 1995 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 | 1996 | `			}else{` |
|       - | 1997 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - | 1998 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - | 1999 | `				 * exception context from a sub-execution.` |
|       - | 2000 | `				 */` |
|     ! 0 | 2001 | `				break;` |
|       - | 2002 | `			}` |
|       1 | 2003 | `		}` |
|   13272 | 2004 | `		pBlock = pBlock->pParent;` |
|       2 | 2005 | `	}` |
|    2744 | 2006 |  |
|    2662 | 2007 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 | 2008 |  |
|       - | 2009 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 2010 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 2011 | `	sxu32 nLineLocal;` |
|       - | 2012 | `	sxi32 rc;` |
|    2664 | 2013 | `	nLineLocal = pGen->pIn->nLine;` |
|    2664 | 2014 | `	iLevel = 0;` |
|       - | 2015 | `	/* Jump the 'continue' keyword */` |
|    2664 | 2016 | `	pGen->pIn++;` |
|    2664 | 2017 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 2018 | `		/* optional numeric argument which tells us how many levels` |
|       - | 2019 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 2020 | `		 */` |
|      12 | 2021 | `		iLevel = (sxi32)PH7_TokenValueToInt64(&pGen->pIn->sData);` |
|      12 | 2022 | `		if( iLevel < 2 ){` |
|     ! 0 | 2023 | `			iLevel = 0;` |
|     ! 0 | 2024 | `		}` |
|      12 | 2025 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       5 | 2026 | `	}` |
|       - | 2027 | `	/* Point to the target loop */` |
|    2664 | 2028 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2664 | 2029 | `	if( pLoop == 0 ){` |
|       - | 2030 | `		/* Illegal continue */` |
|      11 | 2031 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 | 2032 | `		if( rc == SXERR_ABORT ){` |
|       - | 2033 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2034 | `			return SXERR_ABORT;` |
|       - | 2035 | `		}` |
|       6 | 2036 | `	}else{` |
|    2654 | 2037 | `		sxu32 nInstrIdx = 0;` |
|       - | 2038 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    2654 | 2039 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    2654 | 2040 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - | 2041 | `			/* According to the PHP language reference manual` |
|       - | 2042 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - | 2043 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - | 2044 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - | 2045 | `			 */` |
|       5 | 2046 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 | 2047 | `			if( rc == SXRET_OK ){` |
|       5 | 2048 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 | 2049 | `			}` |
|       3 | 2050 | `		}else{` |
|       - | 2051 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    2650 | 2052 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2650 | 2053 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - | 2054 | `				JumpFixup sJumpFix;` |
|       - | 2055 | `				/* Post-continue */` |
|       9 | 2056 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|       9 | 2057 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|       9 | 2058 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       4 | 2059 | `			}` |
|       - | 2060 | `		}` |
|       - | 2061 | `	}` |
|    2664 | 2062 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2063 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 2064 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 | 2065 | `	}` |
|       - | 2066 | `	/* Statement successfully compiled */` |
|    2664 | 2067 | `	return SXRET_OK;` |
|    1333 | 2068 |  |
|       - | 2069 | `/*` |
|       - | 2070 | ` * Compile the 'break' statement.` |
|       - | 2071 | ` * According to the PHP language reference` |
|       - | 2072 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - | 2073 | ` *  structure.` |
|       - | 2074 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - | 2075 | ` *  enclosing structures are to be broken out of.` |
|       - | 2076 | ` */` |
|     106 | 2077 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       2 | 2078 |  |
|       - | 2079 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 2080 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 2081 | `	sxi32 rc;` |
|     108 | 2082 | `	iLevel = 0;` |
|       - | 2083 | `	/* Jump the 'break' keyword */` |
|     108 | 2084 | `	pGen->pIn++;` |
|     108 | 2085 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 2086 | `		/* optional numeric argument which tells us how many levels` |
|       - | 2087 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 2088 | `		 */` |
|      12 | 2089 | `		iLevel = (sxi32)PH7_TokenValueToInt64(&pGen->pIn->sData);` |
|      12 | 2090 | `		if( iLevel < 2 ){` |
|     ! 0 | 2091 | `			iLevel = 0;` |
|     ! 0 | 2092 | `		}` |
|      12 | 2093 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       5 | 2094 | `	}` |
|       - | 2095 | `	/* Extract the target loop */` |
|     108 | 2096 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     108 | 2097 | `	if( pLoop == 0 ){` |
|       - | 2098 | `		/* Illegal break */` |
|      17 | 2099 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      17 | 2100 | `		if( rc == SXERR_ABORT ){` |
|       - | 2101 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2102 | `			return SXERR_ABORT;` |
|       - | 2103 | `		}` |
|       9 | 2104 | `	}else{` |
|       - | 2105 | `		sxu32 nInstrIdx;` |
|       - | 2106 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|      92 | 2107 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|      92 | 2108 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|      92 | 2109 | `		if( rc == SXRET_OK ){` |
|       - | 2110 | `			/* Fix the jump later when the jump destination is resolved */` |
|      92 | 2111 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      45 | 2112 | `		}` |
|       - | 2113 | `	}` |
|     108 | 2114 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2115 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 2116 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 | 2117 | `	}` |
|       - | 2118 | `	/* Statement successfully compiled */` |
|     108 | 2119 | `	return SXRET_OK;` |
|      55 | 2120 |  |
|       - | 2121 | `/*` |
|       - | 2122 | ` * Compile or record a label.` |
|       - | 2123 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - | 2124 | ` * Example` |
|       - | 2125 | ` *  goto LABEL;` |
|       - | 2126 | ` *   echo 'Foo';` |
|       - | 2127 | ` *  LABEL:` |
|       - | 2128 | ` *   echo 'Bar';` |
|       - | 2129 | ` */` |
|     112 | 2130 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       2 | 2131 |  |
|       - | 2132 | `	GenBlock *pBlock;` |
|       - | 2133 | `	Label sLabel;` |
|       - | 2134 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     114 | 2135 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     114 | 2136 | `	if( pBlock ){` |
|       - | 2137 | `		sxi32 rc;` |
|       7 | 2138 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 | 2139 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       5 | 2140 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2141 | `			return SXERR_ABORT;` |
|       - | 2142 | `		}` |
|       3 | 2143 | `	}else{` |
|     110 | 2144 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 2145 | `		char *zDup;` |
|       - | 2146 | `		/* Initialize label fields */` |
|     110 | 2147 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - | 2148 | `		/* Duplicate label name */` |
|     110 | 2149 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     110 | 2150 | `		if( zDup == 0 ){` |
|     ! 0 | 2151 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2152 | `			return SXERR_ABORT;` |
|       - | 2153 | `		}` |
|     110 | 2154 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     110 | 2155 | `		sLabel.bRef  = FALSE;` |
|     110 | 2156 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     110 | 2157 | `		pBlock = pGen->pCurrent;` |
|     218 | 2158 | `		while( pBlock ){` |
|     130 | 2159 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      22 | 2160 | `				break;` |
|       - | 2161 | `			}` |
|       - | 2162 | `			/* Point to the upper block */` |
|     110 | 2163 | `			pBlock = pBlock->pParent;` |
|       2 | 2164 | `		}` |
|     110 | 2165 | `		if( pBlock ){` |
|      22 | 2166 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      12 | 2167 | `		}else{` |
|      90 | 2168 | `			sLabel.pFunc = 0;` |
|       - | 2169 | `		}` |
|       - | 2170 | `		/* Insert in label set */` |
|     110 | 2171 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - | 2172 | `	}` |
|     114 | 2173 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     114 | 2174 | `	return SXRET_OK;` |
|      58 | 2175 |  |
|       - | 2176 | `/*` |
|       - | 2177 | ` * Compile the so hated 'goto' statement.` |
|       - | 2178 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - | 2179 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - | 2180 | ` * a compiler it has to do this.` |
|       - | 2181 | ` * According to the PHP language reference manual` |
|       - | 2182 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - | 2183 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - | 2184 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - | 2185 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - | 2186 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - | 2187 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - | 2188 | ` *   of a multi-level break` |
|       - | 2189 | ` */` |
|     152 | 2190 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       2 | 2191 |  |
|       - | 2192 | `	JumpFixup sJump;` |
|       - | 2193 | `	sxi32 rc;` |
|     154 | 2194 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     154 | 2195 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 2196 | `		/* Missing label */` |
|     ! 0 | 2197 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 | 2198 | `		if( rc == SXERR_ABORT ){` |
|       - | 2199 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2200 | `			return SXERR_ABORT;` |
|       - | 2201 | `		}` |
|     ! 0 | 2202 | `		return SXRET_OK;` |
|       - | 2203 | `	}` |
|     154 | 2204 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 | 2205 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 | 2206 | `		if( rc == SXERR_ABORT ){` |
|       - | 2207 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2208 | `			return SXERR_ABORT;` |
|       - | 2209 | `		}` |
|       3 | 2210 | `	}else{` |
|     150 | 2211 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 2212 | `		GenBlock *pBlock;` |
|       - | 2213 | `		char *zDup;` |
|       - | 2214 | `		/* Prepare the jump destination */` |
|     150 | 2215 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     150 | 2216 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - | 2217 | `		/* Duplicate label name */` |
|     150 | 2218 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     150 | 2219 | `		if( zDup == 0 ){` |
|     ! 0 | 2220 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2221 | `			return SXERR_ABORT;` |
|       - | 2222 | `		}` |
|     150 | 2223 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     150 | 2224 | `		pBlock = pGen->pCurrent;` |
|     312 | 2225 | `		while( pBlock ){` |
|     196 | 2226 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      34 | 2227 | `				break;` |
|       - | 2228 | `			}` |
|       - | 2229 | `			/* Point to the upper block */` |
|     164 | 2230 | `			pBlock = pBlock->pParent;` |
|       2 | 2231 | `		}` |
|     150 | 2232 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       7 | 2233 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       7 | 2234 | `			if( rc == SXERR_ABORT ){` |
|       - | 2235 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2236 | `				return SXERR_ABORT;` |
|       - | 2237 | `			}` |
|       3 | 2238 | `		}` |
|     150 | 2239 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      28 | 2240 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      15 | 2241 | `		}else{` |
|     124 | 2242 | `			sJump.pFunc = 0;` |
|       - | 2243 | `		}` |
|       - | 2244 | `		/* Emit the unconditional jump */` |
|     150 | 2245 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     150 | 2246 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 | 2247 | `		}` |
|       - | 2248 | `	}` |
|     154 | 2249 | `	pGen->pIn++; /* Jump the label name */` |
|     154 | 2250 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 | 2251 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 | 2252 | `	}` |
|       - | 2253 | `	/* Statement successfully compiled */` |
|     154 | 2254 | `	return SXRET_OK;` |
|      78 | 2255 |  |
|       - | 2256 | `/*` |
|       - | 2257 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - | 2258 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - | 2259 | ` * failure.` |
|       - | 2260 | ` */` |
|      20 | 2261 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 | 2262 |  |
|       - | 2263 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - | 2264 | `	sxu32 nRawObj;` |
|      10 | 2265 | `	sxu32 nObjIdx;` |
|       - | 2266 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - | 2267 | `	 * a PHP block.` |
|       - | 2268 | `	 */` |
|      10 | 2269 | `Consume:` |
|      21 | 2270 | `	nRawObj = nObjIdx = 0;` |
|      21 | 2271 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 | 2272 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 | 2273 | `		if( pRawObj == 0 ){` |
|     ! 0 | 2274 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2275 | `			return SXERR_ABORT;` |
|       - | 2276 | `		}` |
|       - | 2277 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 | 2278 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 | 2279 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 | 2280 | `		++nRawObj;` |
|     ! 0 | 2281 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 | 2282 | `	}` |
|      21 | 2283 | `	if( nRawObj > 0 ){` |
|       - | 2284 | `		/* Emit the consume instruction */` |
|     ! 0 | 2285 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 | 2286 | `	}` |
|      21 | 2287 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 | 2288 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - | 2289 | `		/* Reset the token set */` |
|     ! 0 | 2290 | `		SySetReset(pTokenSet);` |
|       - | 2291 | `		/* Tokenize input */` |
|     ! 0 | 2292 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 | 2293 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - | 2294 | `		/* Point to the fresh token stream */` |
|     ! 0 | 2295 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 | 2296 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - | 2297 | `		/* Advance the stream cursor */` |
|     ! 0 | 2298 | `		pGen->pRawIn++;` |
|       - | 2299 | `		/* TICKET 1433-011 */` |
|     ! 0 | 2300 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 2301 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 2302 | `			sxi32 rc;` |
|       - | 2303 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 | 2304 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 | 2305 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 | 2306 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 | 2307 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 2308 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2309 | `				return SXERR_ABORT;` |
|     ! 0 | 2310 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 | 2311 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 2312 | `			}` |
|     ! 0 | 2313 | `			goto Consume;` |
|       - | 2314 | `		}` |
|     ! 0 | 2315 | `	}else{` |
|       - | 2316 | `		/* No more chunks to process */` |
|      21 | 2317 | `		pGen->pIn = pGen->pEnd;` |
|      21 | 2318 | `		return SXERR_EOF;` |
|       - | 2319 | `	}` |
|     ! 0 | 2320 | `	return SXRET_OK;` |
|      11 | 2321 |  |
|       - | 2322 | `/*` |
|       - | 2323 | ` * Compile a PHP block.` |
|       - | 2324 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - | 2325 | ` * optionally delimited by braces {}.` |
|       - | 2326 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 2327 | ` * and this function takes care of generating the appropriate error` |
|       - | 2328 | ` * message.` |
|       - | 2329 | ` */` |
|  288938 | 2330 | `static sxi32 PH7_CompileBlock(` |
|       - | 2331 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 2332 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - | 2333 | `	)` |
|       2 | 2334 |  |
|       - | 2335 | `	sxi32 rc;` |
|       - | 2336 | `	sxu32 nLine;` |
|  288940 | 2337 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  287544 | 2338 | `		nLine = pGen->pIn->nLine;` |
|  287544 | 2339 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  287544 | 2340 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2341 | `			return SXERR_ABORT;` |
|       - | 2342 | `		}` |
|  287544 | 2343 | `		pGen->pIn++;` |
|       - | 2344 | `		/* Compile until we hit the closing braces '}' */` |
|  396954 | 2345 | `		for(;;){` |
|  793910 | 2346 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 | 2347 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 | 2348 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 2349 | `			 	   return SXERR_ABORT;` |
|       - | 2350 | `				}` |
|      21 | 2351 | `				if( rc == SXERR_EOF ){` |
|       - | 2352 | `					/* No more token to process. Missing closing braces */` |
|      21 | 2353 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 | 2354 | `					break;` |
|       - | 2355 | `				}` |
|     ! 0 | 2356 | `			}` |
|  793890 | 2357 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - | 2358 | `				/* Closing braces found,break immediately*/` |
|  287524 | 2359 | `				pGen->pIn++;` |
|  287524 | 2360 | `				break;` |
|       - | 2361 | `			}` |
|       - | 2362 | `			/* Compile a single statement */` |
|  506368 | 2363 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  506368 | 2364 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2365 | `				return SXERR_ABORT;` |
|       - | 2366 | `			}` |
|       2 | 2367 | `		}` |
|  287544 | 2368 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  145169 | 2369 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 | 2370 | `		pGen->pIn++;` |
|     ! 0 | 2371 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 | 2372 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2373 | `			return SXERR_ABORT;` |
|       - | 2374 | `		}` |
|       - | 2375 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 | 2376 | `		for(;;){` |
|     ! 0 | 2377 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 2378 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 | 2379 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 2380 | `			 	   return SXERR_ABORT;` |
|       - | 2381 | `				}` |
|     ! 0 | 2382 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - | 2383 | `					/* No more token to process */` |
|     ! 0 | 2384 | `					if( rc == SXERR_EOF ){` |
|     ! 0 | 2385 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - | 2386 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 | 2387 | `					}` |
|     ! 0 | 2388 | `					break;` |
|       - | 2389 | `				}` |
|     ! 0 | 2390 | `			}` |
|     ! 0 | 2391 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 2392 | `				sxi32 nKwrd;` |
|       - | 2393 | `				/* Keyword found */` |
|     ! 0 | 2394 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 | 2395 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 | 2396 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - | 2397 | `						/* Delimiter keyword found,break */` |
|     ! 0 | 2398 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 | 2399 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 | 2400 | `						}` |
|     ! 0 | 2401 | `						break;` |
|       - | 2402 | `				}` |
|     ! 0 | 2403 | `			}` |
|       - | 2404 | `			/* Compile a single statement */` |
|     ! 0 | 2405 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 | 2406 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2407 | `				return SXERR_ABORT;` |
|       - | 2408 | `			}` |
|     ! 0 | 2409 | `		}` |
|     ! 0 | 2410 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 | 2411 | `	}else{` |
|       - | 2412 | `		/* Compile a single statement */` |
|    1398 | 2413 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1398 | 2414 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2415 | `			return SXERR_ABORT;` |
|       - | 2416 | `		}` |
|       - | 2417 | `	}` |
|       - | 2418 | `	/* Jump trailing semi-colons ';' */` |
|  288940 | 2419 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2420 | `		pGen->pIn++;` |
|     ! 0 | 2421 | `	}` |
|  288940 | 2422 | `	return SXRET_OK;` |
|  144471 | 2423 |  |
|       - | 2424 | `/*` |
|       - | 2425 | ` * Compile the gentle 'while' statement.` |
|       - | 2426 | ` * According to the PHP language reference` |
|       - | 2427 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - | 2428 | ` *  The basic form of a while statement is:` |
|       - | 2429 | ` *  while (expr)` |
|       - | 2430 | ` *   statement` |
|       - | 2431 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - | 2432 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - | 2433 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - | 2434 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - | 2435 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - | 2436 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - | 2437 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - | 2438 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - | 2439 | ` *  while (expr):` |
|       - | 2440 | ` *    statement` |
|       - | 2441 | ` *   endwhile;` |
|       - | 2442 | ` */` |
|   10590 | 2443 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 | 2444 |  |
|   10592 | 2445 | `	GenBlock *pWhileBlock = 0;` |
|   10592 | 2446 | `	SyToken *pTmp,*pEnd = 0;` |
|       - | 2447 | `	sxu32 nFalseJump;` |
|       - | 2448 | `	sxu32 nLine;` |
|       - | 2449 | `	sxi32 rc;` |
|   10592 | 2450 | `	nLine = pGen->pIn->nLine;` |
|       - | 2451 | `	/* Jump the 'while' keyword */` |
|   10592 | 2452 | `	pGen->pIn++;` |
|   10592 | 2453 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2454 | `		/* Syntax error */` |
|     ! 0 | 2455 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2456 | `		if( rc == SXERR_ABORT ){` |
|       - | 2457 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2458 | `			return SXERR_ABORT;` |
|       - | 2459 | `		}` |
|     ! 0 | 2460 | `		goto Synchronize;` |
|       - | 2461 | `	}` |
|       - | 2462 | `	/* Jump the left parenthesis '(' */` |
|   10592 | 2463 | `	pGen->pIn++;` |
|       - | 2464 | `	/* Create the loop block */` |
|   10592 | 2465 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   10592 | 2466 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2467 | `		return SXERR_ABORT;` |
|       - | 2468 | `	}` |
|       - | 2469 | `	/* Delimit the condition */` |
|   10592 | 2470 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   10592 | 2471 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2472 | `		/* Empty expression */` |
|       3 | 2473 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 | 2474 | `		if( rc == SXERR_ABORT ){` |
|       - | 2475 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2476 | `			return SXERR_ABORT;` |
|       - | 2477 | `		}` |
|       1 | 2478 | `	}` |
|       - | 2479 | `	/* Swap token streams */` |
|   10592 | 2480 | `	pTmp = pGen->pEnd;` |
|   10592 | 2481 | `	pGen->pEnd = pEnd;` |
|       - | 2482 | `	/* Compile the expression */` |
|   10592 | 2483 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10592 | 2484 | `	if( rc == SXERR_ABORT ){` |
|       - | 2485 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2486 | `		return SXERR_ABORT;` |
|       - | 2487 | `	}` |
|       - | 2488 | `	/* Update token stream */` |
|   10592 | 2489 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2490 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2491 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2492 | `			return SXERR_ABORT;` |
|       - | 2493 | `		}` |
|     ! 0 | 2494 | `		pGen->pIn++;` |
|     ! 0 | 2495 | `	}` |
|       - | 2496 | `	/* Synchronize pointers */` |
|   10592 | 2497 | `	pGen->pIn  = &pEnd[1];` |
|   10592 | 2498 | `	pGen->pEnd = pTmp;` |
|       - | 2499 | `	/* Emit the false jump */` |
|   10592 | 2500 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2501 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   10592 | 2502 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - | 2503 | `	/* Compile the loop body */` |
|   10592 | 2504 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   10592 | 2505 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2506 | `		return SXERR_ABORT;` |
|       - | 2507 | `	}` |
|       - | 2508 | `	/* Emit the unconditional jump to the start of the loop */` |
|   10592 | 2509 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - | 2510 | `	/* Fix all jumps now the destination is resolved */` |
|   10592 | 2511 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2512 | `	/* Release the loop block */` |
|   10592 | 2513 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2514 | `	/* Statement successfully compiled */` |
|   10592 | 2515 | `	return SXRET_OK;` |
|     ! 0 | 2516 | `Synchronize:` |
|       - | 2517 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2518 | `	 * compiling this erroneous block.` |
|       - | 2519 | `	 */` |
|     ! 0 | 2520 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2521 | `		pGen->pIn++;` |
|     ! 0 | 2522 | `	}` |
|     ! 0 | 2523 | `	return SXRET_OK;` |
|    5297 | 2524 |  |
|       - | 2525 | `/*` |
|       - | 2526 | ` * Compile the ugly do..while() statement.` |
|       - | 2527 | ` * According to the PHP language reference` |
|       - | 2528 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - | 2529 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - | 2530 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - | 2531 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - | 2532 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - | 2533 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - | 2534 | ` *  would end immediately).` |
|       - | 2535 | ` *  There is just one syntax for do-while loops:` |
|       - | 2536 | ` *  <?php` |
|       - | 2537 | ` *  $i = 0;` |
|       - | 2538 | ` *  do {` |
|       - | 2539 | ` *   echo $i;` |
|       - | 2540 | ` *  } while ($i > 0);` |
|       - | 2541 | ` * ?>` |
|       - | 2542 | ` */` |
|       2 | 2543 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 | 2544 |  |
|       3 | 2545 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 | 2546 | `	GenBlock *pDoBlock = 0;` |
|       - | 2547 | `	sxu32 nLine;` |
|       - | 2548 | `	sxi32 rc;` |
|       3 | 2549 | `	nLine = pGen->pIn->nLine;` |
|       - | 2550 | `	/* Jump the 'do' keyword */` |
|       3 | 2551 | `	pGen->pIn++;` |
|       - | 2552 | `	/* Create the loop block */` |
|       3 | 2553 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 | 2554 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2555 | `		return SXERR_ABORT;` |
|       - | 2556 | `	}` |
|       - | 2557 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 | 2558 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 | 2559 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 | 2560 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2561 | `		return SXERR_ABORT;` |
|       - | 2562 | `	}` |
|       3 | 2563 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2564 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 | 2565 | `	}` |
|       3 | 2566 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 | 2567 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - | 2568 | `			/* Missing 'while' statement */` |
|       3 | 2569 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 | 2570 | `			if( rc == SXERR_ABORT ){` |
|       - | 2571 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2572 | `				return SXERR_ABORT;` |
|       - | 2573 | `			}` |
|       3 | 2574 | `			goto Synchronize;` |
|       - | 2575 | `	}` |
|       - | 2576 | `	/* Jump the 'while' keyword */` |
|     ! 0 | 2577 | `	pGen->pIn++;` |
|     ! 0 | 2578 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2579 | `		/* Syntax error */` |
|     ! 0 | 2580 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2581 | `		if( rc == SXERR_ABORT ){` |
|       - | 2582 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2583 | `			return SXERR_ABORT;` |
|       - | 2584 | `		}` |
|     ! 0 | 2585 | `		goto Synchronize;` |
|       - | 2586 | `	}` |
|       - | 2587 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 | 2588 | `	pGen->pIn++;` |
|       - | 2589 | `	/* Delimit the condition */` |
|     ! 0 | 2590 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 | 2591 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2592 | `		/* Empty expression */` |
|     ! 0 | 2593 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 | 2594 | `		if( rc == SXERR_ABORT ){` |
|       - | 2595 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2596 | `			return SXERR_ABORT;` |
|       - | 2597 | `		}` |
|     ! 0 | 2598 | `		goto Synchronize;` |
|       - | 2599 | `	}` |
|       - | 2600 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 | 2601 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - | 2602 | `		JumpFixup *aPost;` |
|       - | 2603 | `		VmInstr *pInstr;` |
|       - | 2604 | `		sxu32 nJumpDest;` |
|       - | 2605 | `		sxu32 n;` |
|     ! 0 | 2606 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 | 2607 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 | 2608 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 | 2609 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 | 2610 | `			if( pInstr ){` |
|       - | 2611 | `				/* Fix */` |
|     ! 0 | 2612 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 | 2613 | `			}` |
|     ! 0 | 2614 | `		}` |
|     ! 0 | 2615 | `	}` |
|       - | 2616 | `	/* Swap token streams */` |
|     ! 0 | 2617 | `	pTmp = pGen->pEnd;` |
|     ! 0 | 2618 | `	pGen->pEnd = pEnd;` |
|       - | 2619 | `	/* Compile the expression */` |
|     ! 0 | 2620 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 2621 | `	if( rc == SXERR_ABORT ){` |
|       - | 2622 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2623 | `		return SXERR_ABORT;` |
|       - | 2624 | `	}` |
|       - | 2625 | `	/* Update token stream */` |
|     ! 0 | 2626 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2627 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2628 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2629 | `			return SXERR_ABORT;` |
|       - | 2630 | `		}` |
|     ! 0 | 2631 | `		pGen->pIn++;` |
|     ! 0 | 2632 | `	}` |
|     ! 0 | 2633 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 | 2634 | `	pGen->pEnd = pTmp;` |
|       - | 2635 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 | 2636 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - | 2637 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 | 2638 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2639 | `	/* Release the loop block */` |
|     ! 0 | 2640 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2641 | `	/* Statement successfully compiled */` |
|     ! 0 | 2642 | `	return SXRET_OK;` |
|       1 | 2643 | `Synchronize:` |
|       - | 2644 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2645 | `	 * compiling this erroneous block.` |
|       - | 2646 | `	 */` |
|       3 | 2647 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2648 | `		pGen->pIn++;` |
|     ! 0 | 2649 | `	}` |
|       3 | 2650 | `	return SXRET_OK;` |
|       2 | 2651 |  |
|       - | 2652 | `/*` |
|       - | 2653 | ` * Compile the complex and powerful 'for' statement.` |
|       - | 2654 | ` * According to the PHP language reference` |
|       - | 2655 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - | 2656 | ` *  The syntax of a for loop is:` |
|       - | 2657 | ` *  for (expr1; expr2; expr3)` |
|       - | 2658 | ` *   statement` |
|       - | 2659 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - | 2660 | ` *  the beginning of the loop.` |
|       - | 2661 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - | 2662 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - | 2663 | ` *  to FALSE, the execution of the loop ends.` |
|       - | 2664 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - | 2665 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - | 2666 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - | 2667 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - | 2668 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - | 2669 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - | 2670 | ` *  of using the for truth expression.` |
|       - | 2671 | ` */` |
|   10574 | 2672 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 | 2673 |  |
|   10576 | 2674 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   10576 | 2675 | `	GenBlock *pForBlock = 0;` |
|       - | 2676 | `	sxu32 nFalseJump;` |
|       - | 2677 | `	sxu32 nLine;` |
|       - | 2678 | `	sxi32 rc;` |
|   10576 | 2679 | `	nLine = pGen->pIn->nLine;` |
|       - | 2680 | `	/* Jump the 'for' keyword */` |
|   10576 | 2681 | `	pGen->pIn++;` |
|   10576 | 2682 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2683 | `		/* Syntax error */` |
|     ! 0 | 2684 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 2685 | `		if( rc == SXERR_ABORT ){` |
|       - | 2686 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2687 | `			return SXERR_ABORT;` |
|       - | 2688 | `		}` |
|     ! 0 | 2689 | `		return SXRET_OK;` |
|       - | 2690 | `	}` |
|       - | 2691 | `	/* Jump the left parenthesis '(' */` |
|   10576 | 2692 | `	pGen->pIn++;` |
|       - | 2693 | `	/* Delimit the init-expr;condition;post-expr */` |
|   10576 | 2694 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   10576 | 2695 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2696 | `		/* Empty expression */` |
|     ! 0 | 2697 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 | 2698 | `		if( rc == SXERR_ABORT ){` |
|       - | 2699 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2700 | `			return SXERR_ABORT;` |
|       - | 2701 | `		}` |
|       - | 2702 | `		/* Synchronize */` |
|     ! 0 | 2703 | `		pGen->pIn = pEnd;` |
|     ! 0 | 2704 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2705 | `			pGen->pIn++;` |
|     ! 0 | 2706 | `		}` |
|     ! 0 | 2707 | `		return SXRET_OK;` |
|       - | 2708 | `	}` |
|       - | 2709 | `	/* Swap token streams */` |
|   10576 | 2710 | `	pTmp = pGen->pEnd;` |
|   10576 | 2711 | `	pGen->pEnd = pEnd;` |
|       - | 2712 | `	/* Compile initialization expressions if available */` |
|   10576 | 2713 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2714 | `	/* Pop operand lvalues */` |
|   10576 | 2715 | `	if( rc == SXERR_ABORT ){` |
|       - | 2716 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2717 | `		return SXERR_ABORT;` |
|   10576 | 2718 | `	}else if( rc != SXERR_EMPTY ){` |
|   10574 | 2719 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5286 | 2720 | `	}` |
|   10576 | 2721 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2722 | `		/* Syntax error */` |
|     ! 0 | 2723 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 2724 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 | 2725 | `		if( rc == SXERR_ABORT ){` |
|       - | 2726 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2727 | `			return SXERR_ABORT;` |
|       - | 2728 | `		}` |
|     ! 0 | 2729 | `		return SXRET_OK;` |
|       - | 2730 | `	}` |
|       - | 2731 | `	/* Jump the trailing ';' */` |
|   10576 | 2732 | `	pGen->pIn++;` |
|       - | 2733 | `	/* Create the loop block */` |
|   10576 | 2734 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   10576 | 2735 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2736 | `		return SXERR_ABORT;` |
|       - | 2737 | `	}` |
|       - | 2738 | `	/* Deffer continue jumps */` |
|   10576 | 2739 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 2740 | `	/* Compile the condition */` |
|   10576 | 2741 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10576 | 2742 | `	if( rc == SXERR_ABORT ){` |
|       - | 2743 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2744 | `		return SXERR_ABORT;` |
|   10576 | 2745 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 2746 | `		/* Emit the false jump */` |
|   10574 | 2747 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2748 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   10574 | 2749 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    5286 | 2750 | `	}` |
|   10576 | 2751 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2752 | `		/* Syntax error */` |
|       5 | 2753 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 2754 | `			"for: Expected ';' after conditionals expressions");` |
|       5 | 2755 | `		if( rc == SXERR_ABORT ){` |
|       - | 2756 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2757 | `			return SXERR_ABORT;` |
|       - | 2758 | `		}` |
|       5 | 2759 | `		return SXRET_OK;` |
|       - | 2760 | `	}` |
|       - | 2761 | `	/* Jump the trailing ';' */` |
|   10572 | 2762 | `	pGen->pIn++;` |
|       - | 2763 | `	/* Save the post condition stream */` |
|   10572 | 2764 | `	pPostStart = pGen->pIn;` |
|       - | 2765 | `	/* Compile the loop body */` |
|   10572 | 2766 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   10572 | 2767 | `	pGen->pEnd = pTmp;` |
|   10572 | 2768 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   10572 | 2769 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2770 | `		return SXERR_ABORT;` |
|       - | 2771 | `	}` |
|       - | 2772 | `	/* Fix post-continue jumps */` |
|   10572 | 2773 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - | 2774 | `		JumpFixup *aPost;` |
|       - | 2775 | `		VmInstr *pInstr;` |
|       - | 2776 | `		sxu32 nJumpDest;` |
|       - | 2777 | `		sxu32 n;` |
|       9 | 2778 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|       9 | 2779 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      17 | 2780 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|       9 | 2781 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       9 | 2782 | `			if( pInstr ){` |
|       - | 2783 | `				/* Fix jump */` |
|       9 | 2784 | `				pInstr->iP2 = nJumpDest;` |
|       4 | 2785 | `			}` |
|       5 | 2786 | `		}` |
|       4 | 2787 | `	}` |
|       - | 2788 | `	/* compile the post-expressions if available */` |
|   10572 | 2789 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2790 | `		pPostStart++;` |
|     ! 0 | 2791 | `	}` |
|   10572 | 2792 | `	if( pPostStart < pEnd ){` |
|       - | 2793 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   10572 | 2794 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   10572 | 2795 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10572 | 2796 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 2797 | `			/* Syntax error */` |
|     ! 0 | 2798 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 | 2799 | `			if( rc == SXERR_ABORT ){` |
|       - | 2800 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2801 | `				return SXERR_ABORT;` |
|       - | 2802 | `			}` |
|     ! 0 | 2803 | `			return SXRET_OK;` |
|       - | 2804 | `		}` |
|   10572 | 2805 | `		RE_SWAP_DELIMITER(pGen);` |
|   10572 | 2806 | `		if( rc == SXERR_ABORT ){` |
|       - | 2807 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2808 | `			return SXERR_ABORT;` |
|   10572 | 2809 | `		}else if( rc != SXERR_EMPTY){` |
|       - | 2810 | `			/* Pop operand lvalue */` |
|   10572 | 2811 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5285 | 2812 | `		}` |
|    5285 | 2813 | `	}` |
|       - | 2814 | `	/* Emit the unconditional jump to the start of the loop */` |
|   10572 | 2815 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 2816 | `	/* Fix all jumps now the destination is resolved */` |
|   10572 | 2817 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2818 | `	/* Release the loop block */` |
|   10572 | 2819 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2820 | `	/* Statement successfully compiled */` |
|   10572 | 2821 | `	return SXRET_OK;` |
|    5289 | 2822 |  |
|       - | 2823 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 2824 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - | 2825 | ` * are allowed.` |
|       - | 2826 | ` */` |
|    5640 | 2827 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 2828 |  |
|    5642 | 2829 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    5642 | 2830 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 2831 | `		/* Unexpected expression */` |
|     ! 0 | 2832 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 2833 | `			"foreach: Expecting a variable name");` |
|     ! 0 | 2834 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 2835 | `			rc = SXERR_INVALID;` |
|     ! 0 | 2836 | `		}` |
|     ! 0 | 2837 | `	}` |
|    5642 | 2838 | `	return rc;` |
|       2 | 2839 |  |
|       - | 2840 | `/*` |
|       - | 2841 | ` * Compile the 'foreach' statement.` |
|       - | 2842 | ` * According to the PHP language reference` |
|       - | 2843 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - | 2844 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - | 2845 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - | 2846 | ` *  is a minor but useful extension of the first:` |
|       - | 2847 | ` *  foreach (array_expression as $value)` |
|       - | 2848 | ` *    statement` |
|       - | 2849 | ` *  foreach (array_expression as $key => $value)` |
|       - | 2850 | ` *   statement` |
|       - | 2851 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - | 2852 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - | 2853 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - | 2854 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - | 2855 | ` *  to the variable $key on each loop.` |
|       - | 2856 | ` *  Note:` |
|       - | 2857 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - | 2858 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - | 2859 | ` *  Note:` |
|       - | 2860 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - | 2861 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - | 2862 | ` *  or after the foreach without resetting it.` |
|       - | 2863 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - | 2864 | ` *  of copying the value.` |
|       - | 2865 | ` */` |
|    2870 | 2866 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 | 2867 |  |
|    2872 | 2868 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    2872 | 2869 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    2872 | 2870 | `	GenBlock *pForeachBlock = 0;` |
|       - | 2871 | `	ph7_foreach_info *pInfo;` |
|       - | 2872 | `	sxu32 nFalseJump;` |
|       - | 2873 | `	VmInstr *pInstr;` |
|       - | 2874 | `	sxu32 nLine;` |
|       - | 2875 | `	sxi32 rc;` |
|    2872 | 2876 | `	nLine = pGen->pIn->nLine;` |
|       - | 2877 | `	/* Jump the 'foreach' keyword */` |
|    2872 | 2878 | `	pGen->pIn++;` |
|    2872 | 2879 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2880 | `		/* Syntax error */` |
|     ! 0 | 2881 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 2882 | `		if( rc == SXERR_ABORT ){` |
|       - | 2883 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2884 | `			return SXERR_ABORT;` |
|       - | 2885 | `		}` |
|     ! 0 | 2886 | `		goto Synchronize;` |
|       - | 2887 | `	}` |
|       - | 2888 | `	/* Jump the left parenthesis '(' */` |
|    2872 | 2889 | `	pGen->pIn++;` |
|       - | 2890 | `	/* Create the loop block */` |
|    2872 | 2891 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    2872 | 2892 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2893 | `		return SXERR_ABORT;` |
|       - | 2894 | `	}` |
|       - | 2895 | `	/* Delimit the expression */` |
|    2872 | 2896 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    2872 | 2897 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2898 | `		/* Empty expression */` |
|     ! 0 | 2899 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 | 2900 | `		if( rc == SXERR_ABORT ){` |
|       - | 2901 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2902 | `			return SXERR_ABORT;` |
|       - | 2903 | `		}` |
|       - | 2904 | `		/* Synchronize */` |
|     ! 0 | 2905 | `		pGen->pIn = pEnd;` |
|     ! 0 | 2906 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2907 | `			pGen->pIn++;` |
|     ! 0 | 2908 | `		}` |
|     ! 0 | 2909 | `		return SXRET_OK;` |
|       - | 2910 | `	}` |
|       - | 2911 | `	/* Compile the array expression */` |
|    2872 | 2912 | `	pCur = pGen->pIn;` |
|   19206 | 2913 | `	while( pCur < pEnd ){` |
|   19206 | 2914 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    2882 | 2915 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    2882 | 2916 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - | 2917 | `				/* Break with the first 'as' found */` |
|    2872 | 2918 | `				break;` |
|       - | 2919 | `			}` |
|       5 | 2920 | `		}` |
|       - | 2921 | `		/* Advance the stream cursor */` |
|   16336 | 2922 | `		pCur++;` |
|       2 | 2923 | `	}` |
|    2872 | 2924 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 2925 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2926 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 2927 | `		if( rc == SXERR_ABORT ){` |
|       - | 2928 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2929 | `			return SXERR_ABORT;` |
|       - | 2930 | `		}` |
|     ! 0 | 2931 | `		goto Synchronize;` |
|       - | 2932 | `	}` |
|       - | 2933 | `	/* Swap token streams */` |
|    2872 | 2934 | `	pTmp = pGen->pEnd;` |
|    2872 | 2935 | `	pGen->pEnd = pCur;` |
|    2872 | 2936 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    2872 | 2937 | `	if( rc == SXERR_ABORT ){` |
|       - | 2938 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2939 | `		return SXERR_ABORT;` |
|       - | 2940 | `	}` |
|       - | 2941 | `	/* Update token stream */` |
|    2872 | 2942 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 2943 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2944 | `		if( rc == SXERR_ABORT ){` |
|       - | 2945 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2946 | `			return SXERR_ABORT;` |
|       - | 2947 | `		}` |
|     ! 0 | 2948 | `		pGen->pIn++;` |
|     ! 0 | 2949 | `	}` |
|    2872 | 2950 | `	pCur++; /* Jump the 'as' keyword */` |
|    2872 | 2951 | `	pGen->pIn = pCur;` |
|    2872 | 2952 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2953 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 2954 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2955 | `			return SXERR_ABORT;` |
|       - | 2956 | `		}` |
|     ! 0 | 2957 | `	}` |
|       - | 2958 | `	/* Create the foreach context */` |
|    2872 | 2959 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    2872 | 2960 | `	if( pInfo == 0 ){` |
|     ! 0 | 2961 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 2962 | `		return SXERR_ABORT;` |
|       - | 2963 | `	}` |
|       - | 2964 | `	/* Zero the structure */` |
|    2872 | 2965 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 2966 | `	/* Initialize structure fields */` |
|    2872 | 2967 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 2968 | `	/* Check if we have a key field */` |
|    8662 | 2969 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    5792 | 2970 | `		pCur++;` |
|       2 | 2971 | `	}` |
|    2872 | 2972 | `	if( pCur < pEnd ){` |
|       - | 2973 | `		/* Compile the expression holding the key name */` |
|    2782 | 2974 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 2975 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 2976 | `			if( rc == SXERR_ABORT ){` |
|       - | 2977 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2978 | `				return SXERR_ABORT;` |
|       - | 2979 | `			}` |
|     ! 0 | 2980 | `		}else{` |
|    2782 | 2981 | `			pGen->pEnd = pCur;` |
|    2782 | 2982 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2782 | 2983 | `			if( rc == SXERR_ABORT ){` |
|       - | 2984 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2985 | `				return SXERR_ABORT;` |
|       - | 2986 | `			}` |
|    2782 | 2987 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2782 | 2988 | `			if( pInstr->p3 ){` |
|       - | 2989 | `				/* Record key name */` |
|    2782 | 2990 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1390 | 2991 | `			}` |
|    2782 | 2992 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 2993 | `		}` |
|    2782 | 2994 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1390 | 2995 | `	}` |
|    2872 | 2996 | `	pGen->pEnd = pEnd;` |
|    2872 | 2997 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2998 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 2999 | `		if( rc == SXERR_ABORT ){` |
|       - | 3000 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3001 | `			return SXERR_ABORT;` |
|       - | 3002 | `		}` |
|     ! 0 | 3003 | `		goto Synchronize;` |
|       - | 3004 | `	}` |
|    2872 | 3005 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 | 3006 | `		pGen->pIn++;` |
|       - | 3007 | `		/* Pass by reference  */` |
|      11 | 3008 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 | 3009 | `	}` |
|       - | 3010 | `	/* Check if the value target is list() */` |
|    2872 | 3011 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 3012 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 3013 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - | 3014 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - | 3015 | `		 */` |
|       - | 3016 | `		static int iForeachListCnt = 0;` |
|       - | 3017 | `		char zTmp[128];` |
|       - | 3018 | `		sxu32 nLen;` |
|       - | 3019 | `		char *zDup;` |
|      10 | 3020 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 | 3021 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 | 3022 | `		if( zDup == 0 ){` |
|     ! 0 | 3023 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3024 | `			return SXERR_ABORT;` |
|       - | 3025 | `		}` |
|      10 | 3026 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 3027 | `		/* Save list() token boundaries */` |
|      10 | 3028 | `		pListStart = pGen->pIn;` |
|       - | 3029 | `		/* Advance past list(...) — validate parentheses */` |
|      10 | 3030 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 | 3031 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 | 3032 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - | 3033 | `				"foreach: Expected '(' after 'list'");` |
|       3 | 3034 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3035 | `				return SXERR_ABORT;` |
|       - | 3036 | `			}` |
|       3 | 3037 | `			goto Synchronize;` |
|       - | 3038 | `		}` |
|       7 | 3039 | `		pGen->pIn++; /* Jump '(' */` |
|       7 | 3040 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 | 3041 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 3042 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3043 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 | 3044 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3045 | `				return SXERR_ABORT;` |
|       - | 3046 | `			}` |
|     ! 0 | 3047 | `			goto Synchronize;` |
|       - | 3048 | `		}` |
|       7 | 3049 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 | 3050 | `		pListEnd = pGen->pIn;` |
|       7 | 3051 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    2867 | 3052 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - | 3053 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - | 3054 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - | 3055 | `		 */` |
|       - | 3056 | `		static int iForeachShortListCnt = 0;` |
|       - | 3057 | `		char zTmp[128];` |
|       - | 3058 | `		sxu32 nLen;` |
|       - | 3059 | `		char *zDup;` |
|       3 | 3060 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       3 | 3061 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       3 | 3062 | `		if( zDup == 0 ){` |
|     ! 0 | 3063 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3064 | `			return SXERR_ABORT;` |
|       - | 3065 | `		}` |
|       3 | 3066 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 3067 | `		/* Save [...] token boundaries */` |
|       3 | 3068 | `		pListStart = pGen->pIn;` |
|       - | 3069 | `		/* Advance past [...] */` |
|       3 | 3070 | `		pGen->pIn++; /* Jump '[' */` |
|       3 | 3071 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       3 | 3072 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 3073 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3074 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 | 3075 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3076 | `				return SXERR_ABORT;` |
|       - | 3077 | `			}` |
|     ! 0 | 3078 | `			goto Synchronize;` |
|       - | 3079 | `		}` |
|       3 | 3080 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       3 | 3081 | `		pListEnd = pGen->pIn;` |
|       3 | 3082 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       2 | 3083 | `	}else{` |
|       - | 3084 | `		/* Compile the expression holding the value name */` |
|    2862 | 3085 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2862 | 3086 | `		if( rc == SXERR_ABORT ){` |
|       - | 3087 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3088 | `			return SXERR_ABORT;` |
|       - | 3089 | `		}` |
|    2862 | 3090 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2862 | 3091 | `		if( pInstr->p3 ){` |
|       - | 3092 | `			/* Record value name */` |
|    2862 | 3093 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1430 | 3094 | `		}` |
|       - | 3095 | `	}` |
|       - | 3096 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    2870 | 3097 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 3098 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2870 | 3099 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 3100 | `	/* Record the first instruction to execute */` |
|    2870 | 3101 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3102 | `	/* Emit the FOREACH_STEP instruction */` |
|    2870 | 3103 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 3104 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2870 | 3105 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 3106 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    2870 | 3107 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - | 3108 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - | 3109 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - | 3110 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - | 3111 | `		 */` |
|       9 | 3112 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - | 3113 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - | 3114 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - | 3115 | `		 * picks up the delimiter and the variable names inside.` |
|       - | 3116 | `		 */` |
|       9 | 3117 | `		pSavedIn = pGen->pIn;` |
|       9 | 3118 | `		pSavedEnd = pGen->pEnd;` |
|       9 | 3119 | `		pGen->pIn = pListStart;` |
|       9 | 3120 | `		pGen->pEnd = pListEnd;` |
|       9 | 3121 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       3 | 3122 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       2 | 3123 | `		}else{` |
|       7 | 3124 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - | 3125 | `		}` |
|       9 | 3126 | `		pGen->pIn = pSavedIn;` |
|       9 | 3127 | `		pGen->pEnd = pSavedEnd;` |
|       9 | 3128 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3129 | `			return SXERR_ABORT;` |
|       - | 3130 | `		}` |
|       - | 3131 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       9 | 3132 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       4 | 3133 | `	}` |
|       - | 3134 | `	/* Compile the loop body */` |
|    2870 | 3135 | `	pGen->pIn = &pEnd[1];` |
|    2870 | 3136 | `	pGen->pEnd = pTmp;` |
|    2870 | 3137 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    2870 | 3138 | `	if( rc == SXERR_ABORT ){` |
|       - | 3139 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3140 | `		return SXERR_ABORT;` |
|       - | 3141 | `	}` |
|       - | 3142 | `	/* Emit the unconditional jump to the start of the loop */` |
|    2870 | 3143 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 3144 | `	/* Fix all jumps now the destination is resolved */` |
|    2870 | 3145 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3146 | `	/* Release the loop block */` |
|    2870 | 3147 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3148 | `	/* Statement successfully compiled */` |
|    2870 | 3149 | `	return SXRET_OK;` |
|       1 | 3150 | `Synchronize:` |
|       - | 3151 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 3152 | `	 * compiling this erroneous block.` |
|       - | 3153 | `	 */` |
|       3 | 3154 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3155 | `		pGen->pIn++;` |
|     ! 0 | 3156 | `	}` |
|       3 | 3157 | `	return SXRET_OK;` |
|    1437 | 3158 |  |
|       - | 3159 | `/*` |
|       - | 3160 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - | 3161 | ` * According to the PHP language reference` |
|       - | 3162 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - | 3163 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - | 3164 | ` *  that is similar to that of C:` |
|       - | 3165 | ` *  if (expr)` |
|       - | 3166 | ` *   statement` |
|       - | 3167 | ` *  else construct:` |
|       - | 3168 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - | 3169 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - | 3170 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - | 3171 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - | 3172 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - | 3173 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - | 3174 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - | 3175 | ` *  elseif` |
|       - | 3176 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - | 3177 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - | 3178 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - | 3179 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - | 3180 | ` *   than b, a equal to b or a is smaller than b:` |
|       - | 3181 | ` *   <?php` |
|       - | 3182 | ` *    if ($a > $b) {` |
|       - | 3183 | ` *     echo "a is bigger than b";` |
|       - | 3184 | ` *    } elseif ($a == $b) {` |
|       - | 3185 | ` *     echo "a is equal to b";` |
|       - | 3186 | ` *    } else {` |
|       - | 3187 | ` *     echo "a is smaller than b";` |
|       - | 3188 | ` *    }` |
|       - | 3189 | ` *    ?>` |
|       - | 3190 | ` */` |
|  105290 | 3191 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 | 3192 |  |
|  105292 | 3193 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  105292 | 3194 | `	GenBlock *pCondBlock = 0;` |
|       - | 3195 | `	sxu32 nJumpIdx;` |
|       - | 3196 | `	sxu32 nKeyID;` |
|       - | 3197 | `	sxi32 rc;` |
|       - | 3198 | `	/* Jump the 'if' keyword */` |
|  105292 | 3199 | `	pGen->pIn++;` |
|  105292 | 3200 | `	pToken = pGen->pIn;` |
|       - | 3201 | `	/* Create the conditional block */` |
|  105292 | 3202 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  105292 | 3203 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3204 | `		return SXERR_ABORT;` |
|       - | 3205 | `	}` |
|       - | 3206 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   57901 | 3207 | `	for(;;){` |
|  115804 | 3208 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 3209 | `			/* Syntax error */` |
|     ! 0 | 3210 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3211 | `				pToken--;` |
|     ! 0 | 3212 | `			}` |
|     ! 0 | 3213 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 | 3214 | `			if( rc == SXERR_ABORT ){` |
|       - | 3215 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3216 | `				return SXERR_ABORT;` |
|       - | 3217 | `			}` |
|     ! 0 | 3218 | `			goto Synchronize;` |
|       - | 3219 | `		}` |
|       - | 3220 | `		/* Jump the left parenthesis '(' */` |
|  115804 | 3221 | `		pToken++;` |
|       - | 3222 | `		/* Delimit the condition */` |
|  115804 | 3223 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  115804 | 3224 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - | 3225 | `			/* Syntax error */` |
|     ! 0 | 3226 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3227 | `				pToken--;` |
|     ! 0 | 3228 | `			}` |
|     ! 0 | 3229 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 | 3230 | `			if( rc == SXERR_ABORT ){` |
|       - | 3231 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3232 | `				return SXERR_ABORT;` |
|       - | 3233 | `			}` |
|     ! 0 | 3234 | `			goto Synchronize;` |
|       - | 3235 | `		}` |
|       - | 3236 | `		/* Swap token streams */` |
|  115804 | 3237 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 3238 | `		/* Compile the condition */` |
|  115804 | 3239 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3240 | `		/* Update token stream */` |
|  115804 | 3241 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 3242 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3243 | `			pGen->pIn++;` |
|     ! 0 | 3244 | `		}` |
|  115804 | 3245 | `		pGen->pIn  = &pEnd[1];` |
|  115804 | 3246 | `		pGen->pEnd = pTmp;` |
|  115804 | 3247 | `		if( rc == SXERR_ABORT ){` |
|       - | 3248 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3249 | `			return SXERR_ABORT;` |
|       - | 3250 | `		}` |
|       - | 3251 | `		/* Emit the false jump */` |
|  115804 | 3252 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 3253 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  115804 | 3254 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 3255 | `		/* Compile the body */` |
|  115804 | 3256 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  115804 | 3257 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3258 | `			return SXERR_ABORT;` |
|       - | 3259 | `		}` |
|  115804 | 3260 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   31174 | 3261 | `			break;` |
|       - | 3262 | `		}` |
|       - | 3263 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   53460 | 3264 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   53460 | 3265 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   34360 | 3266 | `			break;` |
|       - | 3267 | `		}` |
|       - | 3268 | `		/* Emit the unconditional jump */` |
|   19102 | 3269 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 3270 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   19102 | 3271 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   19102 | 3272 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   13834 | 3273 | `			pToken = &pGen->pIn[1];` |
|   13834 | 3274 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    5274 | 3275 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4296 | 3276 | `					break;` |
|       - | 3277 | `			}` |
|    5246 | 3278 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2622 | 3279 | `		}` |
|   10514 | 3280 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 3281 | `		/* Synchronize cursors */` |
|   10514 | 3282 | `		pToken = pGen->pIn;` |
|       - | 3283 | `		/* Fix the false jump */` |
|   10514 | 3284 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 | 3285 | `	} /* For(;;) */` |
|       - | 3286 | `	/* Fix the false jump */` |
|  105292 | 3287 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  105292 | 3288 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   42946 | 3289 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 3290 | `			/* Compile the else block */` |
|    8590 | 3291 | `			pGen->pIn++;` |
|    8590 | 3292 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    8590 | 3293 | `			if( rc == SXERR_ABORT ){` |
|       - | 3294 |  |
|     ! 0 | 3295 | `				return SXERR_ABORT;` |
|       - | 3296 | `			}` |
|    4294 | 3297 | `	}` |
|  105292 | 3298 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3299 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  105292 | 3300 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 3301 | `	/* Release the conditional block */` |
|  105292 | 3302 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3303 | `	/* Statement successfully compiled */` |
|  105292 | 3304 | `	return SXRET_OK;` |
|     ! 0 | 3305 | `Synchronize:` |
|       - | 3306 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 3307 | `	 */` |
|     ! 0 | 3308 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3309 | `		pGen->pIn++;` |
|     ! 0 | 3310 | `	}` |
|     ! 0 | 3311 | `	return SXRET_OK;` |
|   52647 | 3312 |  |
|       - | 3313 | `/*` |
|       - | 3314 | ` * Compile the global construct.` |
|       - | 3315 | ` * According to the PHP language reference` |
|       - | 3316 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - | 3317 | ` *  to be used in that function.` |
|       - | 3318 | ` *  Example #1 Using global` |
|       - | 3319 | ` *  <?php` |
|       - | 3320 | ` *   $a = 1;` |
|       - | 3321 | ` *   $b = 2;` |
|       - | 3322 | ` *   function Sum()` |
|       - | 3323 | ` *   {` |
|       - | 3324 | ` *    global $a, $b;` |
|       - | 3325 | ` *    $b = $a + $b;` |
|       - | 3326 | ` *   }` |
|       - | 3327 | ` *   Sum();` |
|       - | 3328 | ` *   echo $b;` |
|       - | 3329 | ` *  ?>` |
|       - | 3330 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - | 3331 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - | 3332 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - | 3333 | ` */` |
|      26 | 3334 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       2 | 3335 |  |
|      28 | 3336 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3337 | `	sxi32 nExpr;` |
|       - | 3338 | `	sxi32 rc;` |
|       - | 3339 | `	/* Jump the 'global' keyword */` |
|      28 | 3340 | `	pGen->pIn++;` |
|      28 | 3341 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - | 3342 | `		/* Nothing to process */` |
|     ! 0 | 3343 | `		return SXRET_OK;` |
|       - | 3344 | `	}` |
|      28 | 3345 | `	pTmp = pGen->pEnd;` |
|      28 | 3346 | `	nExpr = 0;` |
|      56 | 3347 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      30 | 3348 | `		if( pGen->pIn < pNext ){` |
|      30 | 3349 | `			pGen->pEnd = pNext;` |
|      30 | 3350 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3351 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 | 3352 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 3353 | `					return SXERR_ABORT;` |
|       - | 3354 | `				}` |
|     ! 0 | 3355 | `			}else{` |
|      30 | 3356 | `				pGen->pIn++;` |
|      30 | 3357 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3358 | `					/* Emit a warning */` |
|     ! 0 | 3359 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 | 3360 | `				}else{` |
|      30 | 3361 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 | 3362 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 3363 | `						return SXERR_ABORT;` |
|      30 | 3364 | `					}else if(rc != SXERR_EMPTY ){` |
|      30 | 3365 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      30 | 3366 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - | 3367 | `							/* Variable name, not a constant */` |
|      30 | 3368 | `							pLast->iP1 = 0;` |
|      14 | 3369 | `						}` |
|      30 | 3370 | `						nExpr++;` |
|      14 | 3371 | `					}` |
|       - | 3372 | `				}` |
|       - | 3373 | `			}` |
|      14 | 3374 | `		}` |
|       - | 3375 | `		/* Next expression in the stream */` |
|      30 | 3376 | `		pGen->pIn = pNext;` |
|       - | 3377 | `		/* Jump trailing commas */` |
|      32 | 3378 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3379 | `			pGen->pIn++;` |
|       1 | 3380 | `		}` |
|       2 | 3381 | `	}` |
|       - | 3382 | `	/* Restore token stream */` |
|      28 | 3383 | `	pGen->pEnd = pTmp;` |
|      28 | 3384 | `	if( nExpr > 0 ){` |
|       - | 3385 | `		/* Emit the uplink instruction */` |
|      28 | 3386 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      13 | 3387 | `	}` |
|      28 | 3388 | `	return SXRET_OK;` |
|      15 | 3389 |  |
|       - | 3390 | `/*` |
|       - | 3391 | ` * Compile the return statement.` |
|       - | 3392 | ` * According to the PHP language reference` |
|       - | 3393 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - | 3394 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - | 3395 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - | 3396 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - | 3397 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - | 3398 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - | 3399 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - | 3400 | ` *  from within the main script file, then script execution end.` |
|       - | 3401 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - | 3402 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - | 3403 | ` *  should do so as PHP has less work to do in this case.` |
|       - | 3404 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - | 3405 | ` */` |
|  152714 | 3406 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 | 3407 |  |
|  152716 | 3408 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 3409 | `	sxi32 rc;` |
|       - | 3410 | `	/* Jump the 'return' keyword */` |
|  152716 | 3411 | `	pGen->pIn++;` |
|  152716 | 3412 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3413 | `		/* Compile the expression */` |
|  152694 | 3414 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  152694 | 3415 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3416 | `			return SXERR_ABORT;` |
|  152694 | 3417 | `		}else if(rc != SXERR_EMPTY ){` |
|  152694 | 3418 | `			nRet = 1;` |
|   76346 | 3419 | `		}` |
|   76346 | 3420 | `	}` |
|       - | 3421 | `	/* Emit the done instruction */` |
|  152716 | 3422 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  152716 | 3423 | `	return SXRET_OK;` |
|   76359 | 3424 |  |
|       - | 3425 | `/*` |
|       - | 3426 | ` * Compile a yield expression.` |
|       - | 3427 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - | 3428 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - | 3429 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - | 3430 | ` */` |
|      32 | 3431 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 | 3432 |  |
|       - | 3433 | `	SyToken *pTmp, *pSplit;` |
|      34 | 3434 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      34 | 3435 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - | 3436 | `	sxi32 rc;` |
|      16 | 3437 | `	(void)iCompileFlag;` |
|       - | 3438 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      34 | 3439 | `	pGen->pIn++;` |
|       - | 3440 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - | 3441 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|      34 | 3442 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3443 | `		/* Bare yield — no value */` |
|     ! 0 | 3444 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 | 3445 | `		return SXRET_OK;` |
|       - | 3446 | `	}` |
|       - | 3447 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      34 | 3448 | `	pSplit = 0;` |
|       - | 3449 | `	{` |
|      34 | 3450 | `		SyToken *pCur = pGen->pIn;` |
|      34 | 3451 | `		sxi32 nNest = 0;` |
|      78 | 3452 | `		while( pCur < pGen->pEnd ){` |
|      52 | 3453 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 | 3454 | `				nNest++;` |
|      52 | 3455 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 | 3456 | `				nNest--;` |
|      52 | 3457 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       7 | 3458 | `				pSplit = pCur;` |
|       7 | 3459 | `				break;` |
|       - | 3460 | `			}` |
|      46 | 3461 | `			pCur++;` |
|       2 | 3462 | `		}` |
|       - | 3463 | `	}` |
|      34 | 3464 | `	pTmp = pGen->pEnd;` |
|      34 | 3465 | `	if( pSplit ){` |
|       - | 3466 | `		/* yield $key => $value */` |
|       7 | 3467 | `		pGen->pEnd = pSplit;` |
|       7 | 3468 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 | 3469 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 | 3470 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       7 | 3471 | `		pGen->pEnd = pTmp;` |
|       7 | 3472 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 | 3473 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 | 3474 | `		iP1 = 1;` |
|       7 | 3475 | `		iP2 = 1;` |
|       4 | 3476 | `	}else{` |
|       - | 3477 | `		/* yield $value */` |
|      28 | 3478 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      28 | 3479 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      28 | 3480 | `		if( rc != SXERR_EMPTY ){` |
|      28 | 3481 | `			iP1 = 1;` |
|      13 | 3482 | `		}` |
|       - | 3483 | `	}` |
|      34 | 3484 | `	pGen->pEnd = pTmp;` |
|      34 | 3485 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      34 | 3486 | `	return SXRET_OK;` |
|      18 | 3487 |  |
|       - | 3488 | `/*` |
|       - | 3489 | ` * Compile the die/exit language construct.` |
|       - | 3490 | ` * The role of these constructs is to terminate execution of the script.` |
|       - | 3491 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - | 3492 | ` */` |
|      88 | 3493 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       2 | 3494 |  |
|      90 | 3495 | `	sxi32 nExpr = 0;` |
|       - | 3496 | `	sxi32 rc;` |
|       - | 3497 | `	/* Jump the die/exit keyword */` |
|      90 | 3498 | `	pGen->pIn++;` |
|      90 | 3499 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3500 | `		/* Compile the expression */` |
|      90 | 3501 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      90 | 3502 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3503 | `			return SXERR_ABORT;` |
|      90 | 3504 | `		}else if(rc != SXERR_EMPTY ){` |
|      90 | 3505 | `			nExpr = 1;` |
|      44 | 3506 | `		}` |
|      44 | 3507 | `	}` |
|       - | 3508 | `	/* Emit the HALT instruction */` |
|      90 | 3509 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      90 | 3510 | `	return SXRET_OK;` |
|      46 | 3511 |  |
|       - | 3512 | `/*` |
|       - | 3513 | ` * Compile the 'echo' language construct.` |
|       - | 3514 | ` */` |
|   10748 | 3515 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 | 3516 |  |
|   10750 | 3517 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3518 | `	sxi32 rc;` |
|       - | 3519 | `	/* Jump the 'echo' keyword */` |
|   10750 | 3520 | `	pGen->pIn++;` |
|       - | 3521 | `	/* Compile arguments one after one */` |
|   10750 | 3522 | `	pTmp = pGen->pEnd;` |
|   21886 | 3523 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   11138 | 3524 | `		if( pGen->pIn < pNext ){` |
|   11138 | 3525 | `			pGen->pEnd = pNext;` |
|   11138 | 3526 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   11138 | 3527 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3528 | `				return SXERR_ABORT;` |
|   11138 | 3529 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 3530 | `				/* Emit the consume instruction */` |
|   11114 | 3531 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    5556 | 3532 | `			}` |
|    5568 | 3533 | `		}` |
|       - | 3534 | `		/* Jump trailing commas */` |
|   11526 | 3535 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     390 | 3536 | `			pNext++;` |
|       2 | 3537 | `		}` |
|   11138 | 3538 | `		pGen->pIn = pNext;` |
|       2 | 3539 | `	}` |
|       - | 3540 | `	/* Restore token stream */` |
|   10750 | 3541 | `	pGen->pEnd = pTmp;` |
|   10750 | 3542 | `	return SXRET_OK;` |
|    5376 | 3543 |  |
|       - | 3544 | `/*` |
|       - | 3545 | ` * Compile the static statement.` |
|       - | 3546 | ` * According to the PHP language reference` |
|       - | 3547 | ` *  Another important feature of variable scoping is the static variable.` |
|       - | 3548 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - | 3549 | ` *  when program execution leaves this scope.` |
|       - | 3550 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - | 3551 | ` * Symisc eXtension.` |
|       - | 3552 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - | 3553 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 3554 | ` *  Example` |
|       - | 3555 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 3556 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 3557 | ` */` |
|       2 | 3558 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       1 | 3559 |  |
|       - | 3560 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - | 3561 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - | 3562 | `	GenBlock *pBlock;` |
|       - | 3563 | `	SyString *pName;` |
|       - | 3564 | `	char *zDup;` |
|       - | 3565 | `	sxu32 nLine;` |
|       - | 3566 | `	sxi32 rc;` |
|       - | 3567 | `	/* Jump the static keyword */` |
|       3 | 3568 | `	nLine = pGen->pIn->nLine;` |
|       3 | 3569 | `	pGen->pIn++;` |
|       - | 3570 | `	/* Extract the enclosing function if any */` |
|       3 | 3571 | `	pBlock = pGen->pCurrent;` |
|       5 | 3572 | `	while( pBlock ){` |
|       5 | 3573 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       3 | 3574 | `			break;` |
|       - | 3575 | `		}` |
|       - | 3576 | `		/* Point to the upper block */` |
|       3 | 3577 | `		pBlock = pBlock->pParent;` |
|       1 | 3578 | `	}` |
|       3 | 3579 | `	if( pBlock == 0 ){` |
|       - | 3580 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 | 3581 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3582 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 | 3583 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3584 | `				return SXERR_ABORT;` |
|       - | 3585 | `			}` |
|     ! 0 | 3586 | `			goto Synchronize;` |
|       - | 3587 | `		}` |
|       - | 3588 | `		/* Compile the expression holding the variable */` |
|     ! 0 | 3589 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 3590 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3591 | `			return SXERR_ABORT;` |
|     ! 0 | 3592 | `		}else if( rc != SXERR_EMPTY ){` |
|       - | 3593 | `			/* Emit the POP instruction */` |
|     ! 0 | 3594 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 3595 | `		}` |
|     ! 0 | 3596 | `		return SXRET_OK;` |
|       - | 3597 | `	}` |
|       3 | 3598 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - | 3599 | `	/* Make sure we are dealing with a valid statement */` |
|       3 | 3600 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     ! 0 | 3601 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 | 3602 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 | 3603 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3604 | `				return SXERR_ABORT;` |
|       - | 3605 | `			}` |
|       3 | 3606 | `			goto Synchronize;` |
|       - | 3607 | `	}` |
|     ! 0 | 3608 | `	pGen->pIn++;` |
|       - | 3609 | `	/* Extract variable name */` |
|     ! 0 | 3610 | `	pName = &pGen->pIn->sData;` |
|     ! 0 | 3611 | `	pGen->pIn++; /* Jump the var name */` |
|     ! 0 | 3612 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 | 3613 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3614 | `		goto Synchronize;` |
|       - | 3615 | `	}` |
|       - | 3616 | `	/* Initialize the structure describing the static variable */` |
|     ! 0 | 3617 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     ! 0 | 3618 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - | 3619 | `	/* Duplicate variable name */` |
|     ! 0 | 3620 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     ! 0 | 3621 | `	if( zDup == 0 ){` |
|     ! 0 | 3622 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3623 | `		return SXERR_ABORT;` |
|       - | 3624 | `	}` |
|     ! 0 | 3625 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - | 3626 | `	/* Check if we have an expression to compile */` |
|     ! 0 | 3627 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - | 3628 | `		SySet *pInstrContainer;` |
|       - | 3629 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - | 3630 | `		 * Static variable can take any complex expression including function` |
|       - | 3631 | `		 * call as their initialization value.` |
|       - | 3632 | `		 * Example:` |
|       - | 3633 | `		 *		static $var = foo(1,4+5,bar());` |
|       - | 3634 | `		 */` |
|     ! 0 | 3635 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - | 3636 | `		/* Swap bytecode container */` |
|     ! 0 | 3637 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     ! 0 | 3638 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - | 3639 | `		/* Compile the expression */` |
|     ! 0 | 3640 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3641 | `		/* Emit the done instruction */` |
|     ! 0 | 3642 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - | 3643 | `		/* Restore default bytecode container */` |
|     ! 0 | 3644 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 3645 | `	}` |
|       - | 3646 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     ! 0 | 3647 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     ! 0 | 3648 | `	return SXRET_OK;` |
|       1 | 3649 | `Synchronize:` |
|       - | 3650 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - | 3651 | `	 * statement.` |
|       - | 3652 | `	 */` |
|       5 | 3653 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 | 3654 | `		pGen->pIn++;` |
|       1 | 3655 | `	}` |
|       3 | 3656 | `	return SXRET_OK;` |
|       2 | 3657 |  |
|       - | 3658 | `/*` |
|       - | 3659 | ` * Compile the var statement.` |
|       - | 3660 | ` * Symisc Extension:` |
|       - | 3661 | ` *      var statement can be used outside of a class definition.` |
|       - | 3662 | ` */` |
|       4 | 3663 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 | 3664 |  |
|       - | 3665 | `	sxu32 nLine;` |
|       - | 3666 | `	sxi32 rc;` |
|       5 | 3667 | `	nLine = pGen->pIn->nLine;` |
|       - | 3668 | `	/* Jump the 'var' keyword */` |
|       5 | 3669 | `	pGen->pIn++;` |
|       5 | 3670 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 3671 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - | 3672 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 | 3673 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 | 3674 | `			pGen->pIn++;` |
|     ! 0 | 3675 | `		}` |
|     ! 0 | 3676 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3677 | `			return SXERR_ABORT;` |
|       - | 3678 | `		}` |
|     ! 0 | 3679 | `	}else{` |
|       - | 3680 | `		/* Compile the expression */` |
|       5 | 3681 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 | 3682 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3683 | `			return SXERR_ABORT;` |
|       5 | 3684 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 | 3685 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 | 3686 | `		}` |
|       - | 3687 | `	}` |
|       5 | 3688 | `	return SXRET_OK;` |
|       3 | 3689 |  |
|       - | 3690 | `/*` |
|       - | 3691 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - | 3692 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - | 3693 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - | 3694 | ` */` |
|       - | 3695 | `/*` |
|       - | 3696 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - | 3697 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - | 3698 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - | 3699 | ` * qualified name and updates the instruction's operand index.` |
|       - | 3700 | ` *` |
|       - | 3701 | ` * Resolution order:` |
|       - | 3702 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - | 3703 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - | 3704 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - | 3705 | ` *` |
|       - | 3706 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - | 3707 | ` * came from an import (step 1) and 0 otherwise.` |
|       - | 3708 | ` * Returns the (possibly new) literal index.` |
|       - | 3709 | ` */` |
|  313632 | 3710 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       2 | 3711 |  |
|       - | 3712 | `	ph7_value *pLit;` |
|       - | 3713 | `	const char *zLit;` |
|       - | 3714 | `	SyString sQualified;` |
|       - | 3715 | `	sxu32 nLit;` |
|       - | 3716 | `	sxu32 k;` |
|       - | 3717 | `	sxu32 nNewIdx;` |
|       - | 3718 | `	int hasNsSep;` |
|       - | 3719 | `	SyHashEntry *pImport;` |
|       - | 3720 | `	ph7_value *pNew;` |
|  313634 | 3721 | `	if( pFromImport ){` |
|  299960 | 3722 | `		*pFromImport = 0;` |
|  149979 | 3723 | `	}` |
|  313634 | 3724 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  313634 | 3725 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 | 3726 | `		return nOrigIdx;` |
|       - | 3727 | `	}` |
|  313634 | 3728 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  313634 | 3729 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - | 3730 | `	/* Skip if already qualified (contains backslash) */` |
|  313634 | 3731 | `	hasNsSep = 0;` |
| 3373416 | 3732 | `	for( k = 0; k < nLit; k++ ){` |
| 3059816 | 3733 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 1529893 | 3734 | `	}` |
|  313634 | 3735 | `	if( hasNsSep ){` |
|      34 | 3736 | `		return nOrigIdx;` |
|       - | 3737 | `	}` |
|       - | 3738 | `	/* Check use imports first (works even outside namespaces) */` |
|  313602 | 3739 | `	SyBlobReset(&pGen->sWorker);` |
|  313602 | 3740 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  313602 | 3741 | `	if( pImport ){` |
|      38 | 3742 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      38 | 3743 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      38 | 3744 | `		if( pFromImport ){` |
|      18 | 3745 | `			*pFromImport = 1;` |
|       8 | 3746 | `		}` |
|      20 | 3747 | `	}else{` |
|  313566 | 3748 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  313484 | 3749 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - | 3750 | `		}` |
|       - | 3751 | `		/* Prepend current namespace */` |
|      84 | 3752 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      84 | 3753 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      84 | 3754 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - | 3755 | `	}` |
|       - | 3756 | `	/* Look up or create a new literal for the qualified name */` |
|     120 | 3757 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     120 | 3758 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      52 | 3759 | `		return nNewIdx; /* Already interned */` |
|       - | 3760 | `	}` |
|      70 | 3761 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      70 | 3762 | `	if( pNew == 0 ){` |
|     ! 0 | 3763 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - | 3764 | `	}` |
|      70 | 3765 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      70 | 3766 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      70 | 3767 | `	return nNewIdx;` |
|  156818 | 3768 |  |
|       - | 3769 | `/*` |
|       - | 3770 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - | 3771 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - | 3772 | ` */` |
|   26524 | 3773 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3774 |  |
|       - | 3775 | `	SyHashEntry *pImport;` |
|       - | 3776 | `	/* Check use imports first */` |
|   26526 | 3777 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   26526 | 3778 | `	if( pImport ){` |
|      12 | 3779 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      12 | 3780 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      12 | 3781 | `		return;` |
|       - | 3782 | `	}` |
|       - | 3783 | `	/* Prepend current namespace if active */` |
|   26516 | 3784 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 | 3785 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 | 3786 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 | 3787 | `	}` |
|   26516 | 3788 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   13264 | 3789 |  |
|       - | 3790 | `/*` |
|       - | 3791 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - | 3792 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - | 3793 | ` * The caller must release pOut when done.` |
|       - | 3794 | ` */` |
|   45148 | 3795 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3796 |  |
|   45150 | 3797 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      50 | 3798 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      50 | 3799 | `		SyBlobAppend(pOut,"\\",1);` |
|      24 | 3800 | `	}` |
|   45150 | 3801 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   45150 | 3802 |  |
|       - | 3803 | `/*` |
|       - | 3804 | ` * Compile a namespace statement` |
|       - | 3805 | ` * According to the PHP language reference manual` |
|       - | 3806 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - | 3807 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - | 3808 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - | 3809 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - | 3810 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - | 3811 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - | 3812 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - | 3813 | ` *  programming world.` |
|       - | 3814 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - | 3815 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - | 3816 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - | 3817 | ` *  classes/functions/constants.` |
|       - | 3818 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - | 3819 | ` *  readability of source code.` |
|       - | 3820 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - | 3821 | ` *  Here is an example of namespace syntax in PHP:` |
|       - | 3822 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - | 3823 | ` *       class MyClass {}` |
|       - | 3824 | ` *       function myfunction() {}` |
|       - | 3825 | ` *       const MYCONST = 1;` |
|       - | 3826 | ` *       $a = new MyClass;` |
|       - | 3827 | ` *       $c = new \my\name\MyClass;` |
|       - | 3828 | ` *       $a = strlen('hi');` |
|       - | 3829 | ` *       $d = namespace\MYCONST;` |
|       - | 3830 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - | 3831 | ` *       echo constant($d);` |
|       - | 3832 | ` * NOTE` |
|       - | 3833 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3834 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3835 | ` */` |
|       - | 3836 | `/*` |
|       - | 3837 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - | 3838 | ` */` |
|      10 | 3839 | `static const char * TokenTypeName(sxu32 nType)` |
|       1 | 3840 |  |
|      11 | 3841 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       5 | 3842 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       5 | 3843 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       5 | 3844 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       5 | 3845 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       5 | 3846 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 | 3847 | `	return "token";` |
|       6 | 3848 |  |
|      96 | 3849 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       2 | 3850 |  |
|       - | 3851 | `	sxu32 nLine;` |
|       - | 3852 | `	sxi32 rc;` |
|      98 | 3853 | `	nLine = pGen->pIn->nLine;` |
|      98 | 3854 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - | 3855 | `	/* Reset namespace and clear previous use imports */` |
|      98 | 3856 | `	SyBlobReset(&pGen->sNamespace);` |
|      98 | 3857 | `	SyHashRelease(&pGen->hUseImports);` |
|      98 | 3858 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      98 | 3859 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|      98 | 3860 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|      98 | 3861 | `	SyHashRelease(&pGen->hUseConstImports);` |
|      98 | 3862 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|      98 | 3863 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3864 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 | 3865 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3866 | `		return SXRET_OK;` |
|       - | 3867 | `	}` |
|      98 | 3868 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - | 3869 | `		/* namespace; — switch to global namespace */` |
|     ! 0 | 3870 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3871 | `		return SXRET_OK;` |
|       - | 3872 | `	}` |
|      98 | 3873 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - | 3874 | `		/* namespace { } — global namespace block */` |
|     ! 0 | 3875 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3876 | `		return SXRET_OK;` |
|       - | 3877 | `	}` |
|       - | 3878 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     232 | 3879 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     136 | 3880 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - | 3881 | `			/* Append backslash separator */` |
|      21 | 3882 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      21 | 3883 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      10 | 3884 | `			}` |
|      11 | 3885 | `		}else{` |
|       - | 3886 | `			/* Append identifier */` |
|     116 | 3887 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 3888 | `		}` |
|     136 | 3889 | `		pGen->pIn++;` |
|       2 | 3890 | `	}` |
|       - | 3891 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - | 3892 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - | 3893 | `	{` |
|      98 | 3894 | `		char *zNsDup = 0;` |
|      98 | 3895 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     143 | 3896 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      94 | 3897 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      47 | 3898 | `		}` |
|      98 | 3899 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - | 3900 | `	}` |
|      98 | 3901 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 | 3902 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 3903 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 | 3904 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 | 3905 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3906 | `			return SXERR_ABORT;` |
|       - | 3907 | `		}` |
|       2 | 3908 | `	}` |
|      98 | 3909 | `	return SXRET_OK;` |
|      50 | 3910 |  |
|       - | 3911 | `/*` |
|       - | 3912 | ` * Compile the 'use' statement` |
|       - | 3913 | ` * According to the PHP language reference manual` |
|       - | 3914 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - | 3915 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - | 3916 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - | 3917 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - | 3918 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - | 3919 | ` *  a function or constant is not supported.` |
|       - | 3920 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - | 3921 | ` * NOTE` |
|       - | 3922 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3923 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3924 | ` */` |
|      66 | 3925 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       2 | 3926 |  |
|       - | 3927 | `	sxu32 nLine;` |
|       - | 3928 | `	sxi32 rc;` |
|       - | 3929 | `	SyBlob sPath;` |
|       - | 3930 | `	SyString sAlias;` |
|       - | 3931 | `	SyToken *pLast;` |
|       - | 3932 | `	char *zDup;` |
|       - | 3933 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - | 3934 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - | 3935 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      68 | 3936 | `	nLine = pGen->pIn->nLine;` |
|      68 | 3937 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - | 3938 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      68 | 3939 | `	iUseType = 0;` |
|      68 | 3940 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 | 3941 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 | 3942 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 | 3943 | `			iUseType = 1;` |
|      16 | 3944 | `			pGen->pIn++;` |
|      23 | 3945 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 | 3946 | `			iUseType = 2;` |
|      16 | 3947 | `			pGen->pIn++;` |
|       7 | 3948 | `		}` |
|      14 | 3949 | `	}` |
|       - | 3950 | `	/* Select target hash tables based on import type */` |
|      68 | 3951 | `	switch( iUseType ){` |
|       7 | 3952 | `		case 1:` |
|      16 | 3953 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 | 3954 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 | 3955 | `			break;` |
|       7 | 3956 | `		case 2:` |
|      16 | 3957 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 | 3958 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 | 3959 | `			break;` |
|      19 | 3960 | `		default:` |
|      40 | 3961 | `			pGenHash = &pGen->hUseImports;` |
|      40 | 3962 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      38 | 3963 | `			break;` |
|       - | 3964 | `	}` |
|      68 | 3965 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - | 3966 | `	/* Process one or more use declarations separated by commas */` |
|      34 | 3967 | `	for(;;){` |
|      70 | 3968 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3969 | `			break;` |
|       - | 3970 | `		}` |
|      70 | 3971 | `		SyBlobReset(&sPath);` |
|      70 | 3972 | `		pLast = 0;` |
|       - | 3973 | `		/* Collect the full namespace path */` |
|     254 | 3974 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     186 | 3975 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     126 | 3976 | `				pLast = pGen->pIn;` |
|     126 | 3977 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      62 | 3978 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 | 3979 | `				}` |
|     126 | 3980 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      62 | 3981 | `			}` |
|     186 | 3982 | `			pGen->pIn++;` |
|       2 | 3983 | `		}` |
|      70 | 3984 | `		if( pLast == 0 ){` |
|       - | 3985 | `			/* Empty path */` |
|       5 | 3986 | `			break;` |
|       - | 3987 | `		}` |
|       - | 3988 | `		/* Default alias is the last component of the path */` |
|      66 | 3989 | `		sAlias = pLast->sData;` |
|       - | 3990 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      64 | 3991 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      42 | 3992 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      18 | 3993 | `			pGen->pIn++; /* Jump 'as' */` |
|      18 | 3994 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      18 | 3995 | `				sAlias = pGen->pIn->sData;` |
|      18 | 3996 | `				pGen->pIn++;` |
|       8 | 3997 | `			}` |
|       8 | 3998 | `		}` |
|       - | 3999 | `		/* Check for duplicate import alias (per-type) */` |
|      66 | 4000 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       7 | 4001 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 4002 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 | 4003 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       5 | 4004 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4005 | `				SyBlobRelease(&sPath);` |
|     ! 0 | 4006 | `				return SXERR_ABORT;` |
|       - | 4007 | `			}` |
|       2 | 4008 | `		}` |
|       - | 4009 | `		/* Register the import: alias -> FQN.` |
|       - | 4010 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - | 4011 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - | 4012 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      98 | 4013 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      64 | 4014 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      66 | 4015 | `		if( zDup ){` |
|      66 | 4016 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      66 | 4017 | `			if( pVmHash ){` |
|       - | 4018 | `				/* Class imports: populate VM table directly (class resolution` |
|       - | 4019 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      38 | 4020 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      38 | 4021 | `				if( zAliasDup ){` |
|      38 | 4022 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      18 | 4023 | `				}` |
|      18 | 4024 | `			}` |
|      66 | 4025 | `			if( iUseType == 2 ){` |
|       - | 4026 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - | 4027 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 | 4028 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 | 4029 | `				if( zAliasDup ){` |
|       - | 4030 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - | 4031 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - | 4032 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 | 4033 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 | 4034 | `					if( azPair ){` |
|      16 | 4035 | `						azPair[0] = zAliasDup;` |
|      16 | 4036 | `						azPair[1] = zDup;` |
|      16 | 4037 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 | 4038 | `					}` |
|       7 | 4039 | `				}` |
|       7 | 4040 | `			}` |
|      32 | 4041 | `		}` |
|       - | 4042 | `		/* Check for comma (multiple use declarations) */` |
|      66 | 4043 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 4044 | `			pGen->pIn++;` |
|       2 | 4045 | `		}else{` |
|      33 | 4046 | `			break;` |
|       - | 4047 | `		}` |
|       1 | 4048 | `	}` |
|      68 | 4049 | `	SyBlobRelease(&sPath);` |
|      68 | 4050 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 | 4051 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 | 4052 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 | 4053 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4054 | `			return SXERR_ABORT;` |
|       - | 4055 | `		}` |
|       1 | 4056 | `	}` |
|      68 | 4057 | `	return SXRET_OK;` |
|      35 | 4058 |  |
|       - | 4059 | `/*` |
|       - | 4060 | ` * Compile the stupid 'declare' language construct.` |
|       - | 4061 | ` *` |
|       - | 4062 | ` * According to the PHP language reference manual.` |
|       - | 4063 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - | 4064 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - | 4065 | ` *  declare (directive)` |
|       - | 4066 | ` *   statement` |
|       - | 4067 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - | 4068 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - | 4069 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - | 4070 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - | 4071 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - | 4072 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - | 4073 | ` * <?php` |
|       - | 4074 | ` * // these are the same:` |
|       - | 4075 | ` * // you can use this:` |
|       - | 4076 | ` * declare(ticks=1) {` |
|       - | 4077 | ` *   // entire script here` |
|       - | 4078 | ` * }` |
|       - | 4079 | ` * // or you can use this:` |
|       - | 4080 | ` * declare(ticks=1);` |
|       - | 4081 | ` * // entire script here` |
|       - | 4082 | ` * ?>` |
|       - | 4083 | ` *` |
|       - | 4084 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - | 4085 | ` */` |
|       8 | 4086 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       1 | 4087 |  |
|       9 | 4088 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       9 | 4089 | `	SyToken *pEnd = 0; /* cc warning */` |
|       - | 4090 | `	sxi32 rc;` |
|       9 | 4091 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       9 | 4092 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 | 4093 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 | 4094 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4095 | `			return SXERR_ABORT;` |
|       - | 4096 | `		}` |
|       5 | 4097 | `		goto Synchro;` |
|       - | 4098 | `	}` |
|       5 | 4099 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       - | 4100 | `	/* Delimit the directive */` |
|       5 | 4101 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);` |
|       5 | 4102 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 4103 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 | 4104 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4105 | `			return SXERR_ABORT;` |
|       - | 4106 | `		}` |
|     ! 0 | 4107 | `		return SXRET_OK;` |
|       - | 4108 | `	}` |
|       - | 4109 | `	/* Update the cursor */` |
|       5 | 4110 | `	pGen->pIn = &pEnd[1];` |
|       5 | 4111 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0  ){` |
|     ! 0 | 4112 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 | 4113 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4114 | `			return SXERR_ABORT;` |
|       - | 4115 | `		}` |
|     ! 0 | 4116 | `	}` |
|       - | 4117 | `	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */` |
|       7 | 4118 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */` |
|       - | 4119 | `		"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 | 4120 | `		ph7_lib_version()` |
|       - | 4121 | `		);` |
|       - | 4122 | `	/*All done */` |
|       5 | 4123 | `	return SXRET_OK;` |
|       2 | 4124 | `Synchro:` |
|       - | 4125 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 | 4126 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 | 4127 | `		pGen->pIn++;` |
|       1 | 4128 | `	}` |
|       5 | 4129 | `	return SXRET_OK;` |
|       5 | 4130 |  |
|       - | 4131 | `/*` |
|       - | 4132 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - | 4133 | ` * as follows:` |
|       - | 4134 | ` * function makecoffee($type = "cappuccino")` |
|       - | 4135 | ` * {` |
|       - | 4136 | ` *   return "Making a cup of $type.\n";` |
|       - | 4137 | ` * }` |
|       - | 4138 | ` * Symisc eXtension.` |
|       - | 4139 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - | 4140 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - | 4141 | ` *      Example: Work only with PH7,generate error under zend` |
|       - | 4142 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - | 4143 | ` *      {` |
|       - | 4144 | ` *       var_dump($a);` |
|       - | 4145 | ` *      }` |
|       - | 4146 | ` *     //call test without args` |
|       - | 4147 | ` *      test();` |
|       - | 4148 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - | 4149 | ` *      Example:` |
|       - | 4150 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - | 4151 | ` * 3 -) Function overloading!!` |
|       - | 4152 | ` *      Example:` |
|       - | 4153 | ` *      function foo($a) {` |
|       - | 4154 | ` *   	  return $a.PHP_EOL;` |
|       - | 4155 | ` *	    }` |
|       - | 4156 | ` *	    function foo($a, $b) {` |
|       - | 4157 | ` *   	  return $a + $b;` |
|       - | 4158 | ` *	    }` |
|       - | 4159 | ` *	    echo foo(5); // Prints "5"` |
|       - | 4160 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - | 4161 | ` *      // Same arg` |
|       - | 4162 | ` *	   function foo(string $a)` |
|       - | 4163 | ` *	   {` |
|       - | 4164 | ` *	     echo "a is a string\n";` |
|       - | 4165 | ` *	     var_dump($a);` |
|       - | 4166 | ` *	   }` |
|       - | 4167 | ` *	  function foo(int $a)` |
|       - | 4168 | ` *	  {` |
|       - | 4169 | ` *	    echo "a is integer\n";` |
|       - | 4170 | ` *	    var_dump($a);` |
|       - | 4171 | ` *	  }` |
|       - | 4172 | ` *	  function foo(array $a)` |
|       - | 4173 | ` *	  {` |
|       - | 4174 | ` * 	    echo "a is an array\n";` |
|       - | 4175 | ` * 	    var_dump($a);` |
|       - | 4176 | ` *	  }` |
|       - | 4177 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - | 4178 | ` *	  foo(52); // a is integer [second foo]` |
|       - | 4179 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - | 4180 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - | 4181 | ` * introduced by the PH7 engine.` |
|       - | 4182 | ` */` |
|   41962 | 4183 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 | 4184 |  |
|       - | 4185 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 4186 | `	SySet *pInstrContainer;` |
|       - | 4187 | `	sxi32 rc;` |
|       - | 4188 | `	/* Swap token stream */` |
|   41964 | 4189 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   41964 | 4190 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   41964 | 4191 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - | 4192 | `	/* Compile the expression holding the argument value */` |
|   41964 | 4193 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 4194 | `	/* Emit the done instruction */` |
|   41964 | 4195 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   41964 | 4196 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   41964 | 4197 | `	RE_SWAP_DELIMITER(pGen);` |
|   41964 | 4198 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4199 | `		return SXERR_ABORT;` |
|       - | 4200 | `	}` |
|   41964 | 4201 | `	return SXRET_OK;` |
|   20983 | 4202 |  |
|       - | 4203 | `/*` |
|       - | 4204 | ` * Collect function arguments one after one.` |
|       - | 4205 | ` * According to the PHP language reference manual.` |
|       - | 4206 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - | 4207 | ` * list of expressions.` |
|       - | 4208 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - | 4209 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - | 4210 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - | 4211 | ` * for more information.` |
|       - | 4212 | ` * Example #1 Passing arrays to functions` |
|       - | 4213 | ` * <?php` |
|       - | 4214 | ` * function takes_array($input)` |
|       - | 4215 | ` * {` |
|       - | 4216 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - | 4217 | ` * }` |
|       - | 4218 | ` * ?>` |
|       - | 4219 | ` * Making arguments be passed by reference` |
|       - | 4220 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - | 4221 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - | 4222 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - | 4223 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - | 4224 | ` * to the argument name in the function definition:` |
|       - | 4225 | ` * Example #2 Passing function parameters by reference` |
|       - | 4226 | ` * <?php` |
|       - | 4227 | ` * function add_some_extra(&$string)` |
|       - | 4228 | ` * {` |
|       - | 4229 | ` *   $string .= 'and something extra.';` |
|       - | 4230 | ` * }` |
|       - | 4231 | ` * $str = 'This is a string, ';` |
|       - | 4232 | ` * add_some_extra($str);` |
|       - | 4233 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - | 4234 | ` * ?>` |
|       - | 4235 | ` *` |
|       - | 4236 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - | 4237 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - | 4238 | ` * on these extension.` |
|       - | 4239 | ` */` |
|   50378 | 4240 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)` |
|       2 | 4241 |  |
|       - | 4242 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - | 4243 | `	SyToken *pIn;  /* Token stream */` |
|       - | 4244 | `	SyBlob sSig;         /* Function signature */` |
|       - | 4245 | `	char *zDup;          /* Copy of argument name */` |
|       - | 4246 | `	sxi32 rc;` |
|       - | 4247 |  |
|   50380 | 4248 | `	pIn = pGen->pIn;` |
|   50380 | 4249 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - | 4250 | `	/* Process arguments one after one */` |
|   63738 | 4251 | `	for(;;){` |
|  127478 | 4252 | `		if( pIn >= pEnd ){` |
|       - | 4253 | `			/* No more arguments to process */` |
|   50378 | 4254 | `			break;` |
|       - | 4255 | `		}` |
|   77102 | 4256 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   77102 | 4257 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 4258 | `		/* Detect nullable prefix '?' on type hints */` |
|   77102 | 4259 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|      11 | 4260 | `			sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|      11 | 4261 | `			pIn++;` |
|       5 | 4262 | `		}` |
|       - | 4263 | `		/* Skip leading namespace separator '\' on FQN type hints like \Throwable */` |
|   77102 | 4264 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       5 | 4265 | `			pIn++;` |
|       2 | 4266 | `		}` |
|   77102 | 4267 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|   52474 | 4268 | `			if( pIn->nType & PH7_TK_KEYWORD ){` |
|   47224 | 4269 | `				sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   47224 | 4270 | `				if( nKey & PH7_TKWRD_ARRAY ){` |
|     ! 0 | 4271 | `					sArg.nType = MEMOBJ_HASHMAP;` |
|   47224 | 4272 | `				}else if( nKey & PH7_TKWRD_BOOL ){` |
|     ! 0 | 4273 | `					sArg.nType = MEMOBJ_BOOL;` |
|   47224 | 4274 | `				}else if( nKey & PH7_TKWRD_INT ){` |
|   13120 | 4275 | `					sArg.nType = MEMOBJ_INT;` |
|   40665 | 4276 | `				}else if( nKey & PH7_TKWRD_STRING ){` |
|   34104 | 4277 | `					sArg.nType = MEMOBJ_STRING;` |
|   17054 | 4278 | `				}else if( nKey & PH7_TKWRD_FLOAT ){` |
|     ! 0 | 4279 | `					sArg.nType = MEMOBJ_REAL;` |
|     ! 0 | 4280 | `				}else{` |
|       4 | 4281 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,` |
|       - | 4282 | `						"Invalid argument type '%z',Automatic cast will not be performed",` |
|       1 | 4283 | `						&pIn->sData);` |
|       - | 4284 | `				}` |
|   23613 | 4285 | `			}else{` |
|    5252 | 4286 | `				SyString *pName = &pIn->sData; /* Class name */` |
|       - | 4287 | `				char *zDupLocal;` |
|       - | 4288 | `				/* Argument must be a class instance,record that*/` |
|    5252 | 4289 | `				zDupLocal = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    5252 | 4290 | `				if( zDupLocal ){` |
|    5252 | 4291 | `					sArg.nType = SXU32_HIGH; /* 0xFFFFFFFF as sentinel */` |
|    5252 | 4292 | `					SyStringInitFromBuf(&sArg.sClass,zDupLocal,pName->nByte);` |
|    2625 | 4293 | `				}` |
|       - | 4294 | `			}` |
|   52474 | 4295 | `			pIn++;` |
|   26236 | 4296 | `		}` |
|   77102 | 4297 | `		if( pIn >= pEnd ){` |
|     ! 0 | 4298 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 | 4299 | `			return rc;` |
|       - | 4300 | `		}` |
|   77102 | 4301 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - | 4302 | `			/* Pass by reference,record that */` |
|    2648 | 4303 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    2648 | 4304 | `			pIn++;` |
|    1323 | 4305 | `		}` |
|   77102 | 4306 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - | 4307 | `			/* Variadic parameter: ...$args */` |
|      23 | 4308 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      23 | 4309 | `			pIn++;` |
|      11 | 4310 | `		}` |
|   77102 | 4311 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4312 | `			/* Invalid argument */` |
|     ! 0 | 4313 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 | 4314 | `			return rc;` |
|       - | 4315 | `		}` |
|   77102 | 4316 | `		pIn++; /* Jump the dollar sign */` |
|       - | 4317 | `		/* Copy argument name */` |
|   77102 | 4318 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   77102 | 4319 | `		if( zDup == 0 ){` |
|     ! 0 | 4320 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 | 4321 | `			return SXERR_ABORT;` |
|       - | 4322 | `		}` |
|   77102 | 4323 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   77102 | 4324 | `		pIn++;` |
|   77102 | 4325 | `		if( pIn < pEnd ){` |
|   47714 | 4326 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - | 4327 | `				SyToken *pDefend;` |
|   41966 | 4328 | `				sxi32 iNest = 0;` |
|   41966 | 4329 | `				pIn++; /* Jump the equal sign */` |
|   41966 | 4330 | `				pDefend = pIn;` |
|       - | 4331 | `				/* Process the default value associated with this argument */` |
|   89172 | 4332 | `				while( pDefend < pEnd ){` |
|   68184 | 4333 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   20978 | 4334 | `						break;` |
|       - | 4335 | `					}` |
|   47208 | 4336 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - | 4337 | `						/* Increment nesting level */` |
|    2624 | 4338 | `						iNest++;` |
|   45897 | 4339 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - | 4340 | `						/* Decrement nesting level */` |
|    2624 | 4341 | `						iNest--;` |
|    1311 | 4342 | `					}` |
|   47208 | 4343 | `					pDefend++;` |
|       2 | 4344 | `				}` |
|   41966 | 4345 | `				if( pIn >= pDefend ){` |
|       3 | 4346 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 | 4347 | `					return rc;` |
|       - | 4348 | `				}` |
|       - | 4349 | `				/* Process default value */` |
|   41964 | 4350 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   41964 | 4351 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 4352 | `					return rc;` |
|       - | 4353 | `				}` |
|       - | 4354 | `				/* Point beyond the default value */` |
|   41964 | 4355 | `				pIn = pDefend;` |
|   20981 | 4356 | `			}` |
|   47712 | 4357 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 4358 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 | 4359 | `				return rc;` |
|       - | 4360 | `			}` |
|   47712 | 4361 | `			pIn++; /* Jump the trailing comma */` |
|   23855 | 4362 | `		}` |
|       - | 4363 | `		/* Append argument signature */` |
|   77100 | 4364 | `		if( sArg.nType > 0 ){` |
|   52472 | 4365 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - | 4366 | `				/* Class name */` |
|    5252 | 4367 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    2627 | 4368 | `			}else{` |
|       - | 4369 | `				int c;` |
|   47222 | 4370 | `				c = 'n'; /* cc warning */` |
|       - | 4371 | `				/* Type leading character */` |
|   47222 | 4372 | `				switch(sArg.nType){` |
|     ! 0 | 4373 | `				case MEMOBJ_HASHMAP:` |
|       - | 4374 | `					/* Hashmap aka 'array' */` |
|     ! 0 | 4375 | `					c = 'h';` |
|     ! 0 | 4376 | `					break;` |
|    6559 | 4377 | `				case MEMOBJ_INT:` |
|       - | 4378 | `					/* Integer */` |
|   13120 | 4379 | `					c = 'i';` |
|   13120 | 4380 | `					break;` |
|     ! 0 | 4381 | `				case MEMOBJ_BOOL:` |
|       - | 4382 | `					/* Bool */` |
|     ! 0 | 4383 | `					c = 'b';` |
|     ! 0 | 4384 | `					break;` |
|     ! 0 | 4385 | `				case MEMOBJ_REAL:` |
|       - | 4386 | `					/* Float */` |
|     ! 0 | 4387 | `					c = 'f';` |
|     ! 0 | 4388 | `					break;` |
|   17051 | 4389 | `				case MEMOBJ_STRING:` |
|       - | 4390 | `					/* String */` |
|   34104 | 4391 | `					c = 's';` |
|   34102 | 4392 | `					break;` |
|     ! 0 | 4393 | `				default:` |
|     ! 0 | 4394 | `					break;` |
|       - | 4395 | `				}` |
|   47222 | 4396 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - | 4397 | `			}` |
|   26237 | 4398 | `		}else{` |
|       - | 4399 | `			/* No type is associated with this parameter which mean` |
|       - | 4400 | `			 * that this function is not condidate for overloading.` |
|       - | 4401 | `			 */` |
|   24630 | 4402 | `			SyBlobRelease(&sSig);` |
|       - | 4403 | `		}` |
|       - | 4404 | `		/* Save in the argument set */` |
|   77100 | 4405 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 | 4406 | `	}` |
|   50378 | 4407 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - | 4408 | `		/* Save function signature */` |
|   31490 | 4409 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   15744 | 4410 | `	}` |
|   50378 | 4411 | `	return SXRET_OK;` |
|   25191 | 4412 |  |
|       - | 4413 | `/*` |
|       - | 4414 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - | 4415 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 4416 | ` * and this routine takes care of generating the appropriate error message.` |
|       - | 4417 | ` */` |
|  140052 | 4418 | `static sxi32 GenStateCompileFuncBody(` |
|       - | 4419 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 4420 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - | 4421 | `	)` |
|       2 | 4422 |  |
|       - | 4423 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - | 4424 | `	GenBlock *pBlock;` |
|       - | 4425 | `	sxu32 nGotoOfft;` |
|       - | 4426 | `	sxi32 rc;` |
|       - | 4427 | `	/* Attach the new function */` |
|  140054 | 4428 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  140054 | 4429 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4430 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - | 4431 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4432 | `		return SXERR_ABORT;` |
|       - | 4433 | `	}` |
|  140054 | 4434 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - | 4435 | `	/* Swap bytecode containers */` |
|  140054 | 4436 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  140054 | 4437 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - | 4438 | `	/* Compile the body */` |
|  140054 | 4439 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 4440 | `	/* Fix exception jumps now the destination is resolved */` |
|  140054 | 4441 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4442 | `	/* Emit the final return if not yet done */` |
|  140054 | 4443 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 4444 | `	/* Fix gotos jumps now the destination is resolved */` |
|  140054 | 4445 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 | 4446 | `		rc = SXERR_ABORT;` |
|     ! 0 | 4447 | `	}` |
|  140054 | 4448 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - | 4449 | `	/* Restore the default container */` |
|  140054 | 4450 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 4451 | `	/* Leave function block */` |
|  140054 | 4452 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  140054 | 4453 | `	if( rc == SXERR_ABORT ){` |
|       - | 4454 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4455 | `		return SXERR_ABORT;` |
|       - | 4456 | `	}` |
|       - | 4457 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - | 4458 | `	{` |
|  140054 | 4459 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - | 4460 | `		sxu32 i;` |
| 2907908 | 4461 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 2767872 | 4462 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      18 | 4463 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      18 | 4464 | `				break;` |
|       - | 4465 | `			}` |
| 1383929 | 4466 | `		}` |
|       - | 4467 | `	}` |
|       - | 4468 | `	/* All done, function body compiled */` |
|  140054 | 4469 | `	return SXRET_OK;` |
|   70028 | 4470 |  |
|       - | 4471 | `/*` |
|       - | 4472 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - | 4473 | ` * According to the PHP language reference manual.` |
|       - | 4474 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - | 4475 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - | 4476 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - | 4477 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 4478 | ` *  Functions need not be defined before they are referenced.` |
|       - | 4479 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - | 4480 | ` *  a function even if they were defined inside and vice versa.` |
|       - | 4481 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - | 4482 | ` *  calls with over 32-64 recursion levels.` |
|       - | 4483 | ` *` |
|       - | 4484 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - | 4485 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - | 4486 | ` * on these extension.` |
|       - | 4487 | ` */` |
|       - | 4488 | `/*` |
|       - | 4489 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - | 4490 | ` */` |
|       6 | 4491 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       1 | 4492 |  |
|       - | 4493 | `	sxu32 i;` |
|      31 | 4494 | `	for( i = 0; i < n; i++ ){` |
|      25 | 4495 | `		int a = zA[i], b = zB[i];` |
|      25 | 4496 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      25 | 4497 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      25 | 4498 | `		if( a != b ) return a - b;` |
|      13 | 4499 | `	}` |
|       7 | 4500 | `	return 0;` |
|       4 | 4501 |  |
|       - | 4502 | `/*` |
|       - | 4503 | ` * Helper: set the return type to a class/self/parent/static sentinel.` |
|       - | 4504 | ` */` |
|       2 | 4505 | `static void GenStateSetReturnClass(ph7_gen_state *pGen, ph7_vm_func *pFunc, const char *zName, sxu32 nByte)` |
|       1 | 4506 |  |
|       3 | 4507 | `	char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator, zName, nByte);` |
|       3 | 4508 | `	if( zDup ){` |
|       3 | 4509 | `		pFunc->nReturnType = SXU32_HIGH;` |
|       3 | 4510 | `		SyStringInitFromBuf(&pFunc->sReturnClass, zDup, nByte);` |
|       1 | 4511 | `	}` |
|       3 | 4512 |  |
|       - | 4513 | `/*` |
|       - | 4514 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - | 4515 | `` * pGen->pIn should point to the token after `)`.`` |
|       - | 4516 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - | 4517 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - | 4518 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, and nullable `: ?type`.`` |
|       - | 4519 | ` */` |
|  161076 | 4520 | `static void GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       2 | 4521 |  |
|  161078 | 4522 | `	SyToken *pCur = pGen->pIn;` |
|  161078 | 4523 | `	pFunc->nReturnType = 0;` |
|  161078 | 4524 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  161078 | 4525 | `	if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_COLON) == 0 ){` |
|  161024 | 4526 | `		return; /* No return type */` |
|       - | 4527 | `	}` |
|      55 | 4528 | `	pCur++; /* Skip ':' */` |
|      55 | 4529 | `	if( pCur >= pGen->pEnd ){` |
|     ! 0 | 4530 | `		pGen->pIn = pCur;` |
|     ! 0 | 4531 | `		return;` |
|       - | 4532 | `	}` |
|       - | 4533 | `	/* Handle nullable prefix '?' (tokenized as PH7_TK_OP with '?' operator) */` |
|      55 | 4534 | `	if( (pCur->nType & PH7_TK_OP) && pCur->sData.nByte == 1 && pCur->sData.zString[0] == '?' ){` |
|       7 | 4535 | `		pCur++;` |
|       7 | 4536 | `		if( pCur >= pGen->pEnd ){` |
|     ! 0 | 4537 | `			pGen->pIn = pCur;` |
|     ! 0 | 4538 | `			return;` |
|       - | 4539 | `		}` |
|       3 | 4540 | `	}` |
|      55 | 4541 | `	if( pCur->nType & PH7_TK_KEYWORD ){` |
|      49 | 4542 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pCur->pUserData));` |
|      49 | 4543 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|       3 | 4544 | `			pFunc->nReturnType = MEMOBJ_HASHMAP;` |
|      48 | 4545 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       3 | 4546 | `			pFunc->nReturnType = MEMOBJ_BOOL;` |
|      46 | 4547 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|      17 | 4548 | `			pFunc->nReturnType = MEMOBJ_INT;` |
|      37 | 4549 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|      25 | 4550 | `			pFunc->nReturnType = MEMOBJ_STRING;` |
|      17 | 4551 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|       3 | 4552 | `			pFunc->nReturnType = MEMOBJ_REAL;` |
|       4 | 4553 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT \|\| nKey == PH7_TKWRD_STATIC ){` |
|       - | 4554 | `			/* self/parent/static — store as class sentinel */` |
|       3 | 4555 | `			GenStateSetReturnClass(pGen, pFunc, pCur->sData.zString, pCur->sData.nByte);` |
|       1 | 4556 | `		}` |
|      49 | 4557 | `		pCur++;` |
|      31 | 4558 | `	}else if( pCur->nType & PH7_TK_ID ){` |
|       7 | 4559 | `		SyString *pType = &pCur->sData;` |
|       7 | 4560 | `		if( pType->nByte == 4 && SyMemcmpNoCase(pType->zString, "void", 4) == 0 ){` |
|       7 | 4561 | `			pFunc->nReturnType = MEMOBJ_VOID;` |
|       4 | 4562 | `		}else{` |
|       - | 4563 | `			/* Class/interface name */` |
|     ! 0 | 4564 | `			GenStateSetReturnClass(pGen, pFunc, pType->zString, pType->nByte);` |
|       - | 4565 | `		}` |
|       7 | 4566 | `		pCur++;` |
|       3 | 4567 | `	}` |
|      55 | 4568 | `	pGen->pIn = pCur;` |
|   80540 | 4569 |  |
|       - | 4570 |  |
|   34750 | 4571 | `static sxi32 GenStateCompileFunc(` |
|       - | 4572 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4573 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - | 4574 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 4575 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - | 4576 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - | 4577 | `	)` |
|       2 | 4578 |  |
|       - | 4579 | `	ph7_vm_func *pFunc;` |
|       - | 4580 | `	SyToken *pEnd;` |
|       - | 4581 | `	sxu32 nLine;` |
|       - | 4582 | `	char *zName;` |
|       - | 4583 | `	sxi32 rc;` |
|       - | 4584 | `	/* Extract line number */` |
|   34752 | 4585 | `	nLine = pGen->pIn->nLine;` |
|       - | 4586 | `	/* Jump the left parenthesis '(' */` |
|   34752 | 4587 | `	pGen->pIn++;` |
|       - | 4588 | `	/* Delimit the function signature */` |
|   34752 | 4589 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   34752 | 4590 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4591 | `		/* Syntax error */` |
|       7 | 4592 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 | 4593 | `		if( rc == SXERR_ABORT ){` |
|       - | 4594 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4595 | `			return SXERR_ABORT;` |
|       - | 4596 | `		}` |
|       7 | 4597 | `		pGen->pIn = pGen->pEnd;` |
|       7 | 4598 | `		return SXRET_OK;` |
|       - | 4599 | `	}` |
|       - | 4600 | `	/* Create the function state */` |
|   34746 | 4601 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   34746 | 4602 | `	if( pFunc == 0 ){` |
|     ! 0 | 4603 | `		goto OutOfMem;` |
|       - | 4604 | `	}` |
|       - | 4605 | `	/* Build the function name, prepending namespace if active */` |
|   34753 | 4606 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - | 4607 | `		SyBlob sFQN;` |
|       - | 4608 | `		sxu32 nLen;` |
|      16 | 4609 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 | 4610 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 | 4611 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 | 4612 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 | 4613 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 | 4614 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 | 4615 | `		SyBlobRelease(&sFQN);` |
|      16 | 4616 | `		if( zName == 0 ){` |
|     ! 0 | 4617 | `			goto OutOfMem;` |
|       - | 4618 | `		}` |
|      16 | 4619 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 | 4620 | `	}else{` |
|   34732 | 4621 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   34732 | 4622 | `		if( zName == 0 ){` |
|     ! 0 | 4623 | `			goto OutOfMem;` |
|       - | 4624 | `		}` |
|   34732 | 4625 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - | 4626 | `	}` |
|   34746 | 4627 | `	if( pGen->pIn < pEnd ){` |
|       - | 4628 | `		/* Collect function arguments */` |
|   24078 | 4629 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);` |
|   24078 | 4630 | `		if( rc == SXERR_ABORT ){` |
|       - | 4631 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4632 | `			return SXERR_ABORT;` |
|       - | 4633 | `		}` |
|   12038 | 4634 | `	}` |
|       - | 4635 | `	/* Point past ')' and parse optional return type ': type' */` |
|   34746 | 4636 | `	pGen->pIn = &pEnd[1];` |
|   34746 | 4637 | `	GenStateParseReturnType(pGen, pFunc);` |
|   34746 | 4638 | `	if( bHandleClosure ){` |
|       - | 4639 | `		ph7_vm_func_closure_env sEnv;` |
|     168 | 4640 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     166 | 4641 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      91 | 4642 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      14 | 4643 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 4644 | `				/* Closure,record environment variable */` |
|      14 | 4645 | `				pGen->pIn++;` |
|      14 | 4646 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 4647 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 | 4648 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4649 | `						return SXERR_ABORT;` |
|       - | 4650 | `					}` |
|     ! 0 | 4651 | `				}` |
|      14 | 4652 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - | 4653 | `				/* Compile until we hit the first closing parenthesis */` |
|      28 | 4654 | `				while( pGen->pIn < pGen->pEnd ){` |
|      28 | 4655 | `					int iFlagsLocal = 0;` |
|      28 | 4656 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      14 | 4657 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      14 | 4658 | `						break;` |
|       - | 4659 | `					}` |
|      16 | 4660 | `					nLineLocal = pGen->pIn->nLine;` |
|      16 | 4661 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - | 4662 | `						/* Pass by reference,record that */` |
|     ! 0 | 4663 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - | 4664 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - | 4665 | `							);` |
|     ! 0 | 4666 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 | 4667 | `						pGen->pIn++;` |
|     ! 0 | 4668 | `					}` |
|      14 | 4669 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      16 | 4670 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 4671 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - | 4672 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 | 4673 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 4674 | `								return SXERR_ABORT;` |
|       - | 4675 | `							}` |
|       - | 4676 | `							/* Find the closing parenthesis */` |
|     ! 0 | 4677 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 4678 | `								pGen->pIn++;` |
|     ! 0 | 4679 | `							}` |
|     ! 0 | 4680 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 | 4681 | `								pGen->pIn++;` |
|     ! 0 | 4682 | `							}` |
|     ! 0 | 4683 | `							break;` |
|       - | 4684 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 | 4685 | `					}else{` |
|       - | 4686 | `						SyString *pNameLocal;` |
|       - | 4687 | `						char *zDup;` |
|       - | 4688 | `						/* Duplicate variable name */` |
|      16 | 4689 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      16 | 4690 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      16 | 4691 | `						if( zDup ){` |
|       - | 4692 | `							/* Zero the structure */` |
|      16 | 4693 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      16 | 4694 | `							sEnv.iFlags = iFlagsLocal;` |
|      16 | 4695 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      16 | 4696 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      16 | 4697 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 | 4698 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 | 4699 | `									got_this = 1;` |
|     ! 0 | 4700 | `							}` |
|       - | 4701 | `							/* Save imported variable */` |
|      16 | 4702 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       9 | 4703 | `						}else{` |
|     ! 0 | 4704 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4705 | `							 return SXERR_ABORT;` |
|       - | 4706 | `						}` |
|       - | 4707 | `					}` |
|      16 | 4708 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      18 | 4709 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4710 | `						/* Ignore trailing commas */` |
|       3 | 4711 | `						pGen->pIn++;` |
|       1 | 4712 | `					}` |
|       2 | 4713 | `				}` |
|      14 | 4714 | `				if( !got_this ){` |
|       - | 4715 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - | 4716 | `					 * available to the closure environment.` |
|       - | 4717 | `					 */` |
|      14 | 4718 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      14 | 4719 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      14 | 4720 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      14 | 4721 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      14 | 4722 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       6 | 4723 | `				}` |
|      14 | 4724 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - | 4725 | `					/* Mark as closure */` |
|      14 | 4726 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       6 | 4727 | `				}` |
|       6 | 4728 | `		}` |
|      83 | 4729 | `	}` |
|       - | 4730 | `	/* Compile the body */` |
|   34746 | 4731 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   34746 | 4732 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4733 | `		return SXERR_ABORT;` |
|       - | 4734 | `	}` |
|   34746 | 4735 | `	if( ppFunc ){` |
|     168 | 4736 | `		*ppFunc = pFunc;` |
|      83 | 4737 | `	}` |
|   34746 | 4738 | `	rc = SXRET_OK;` |
|   34746 | 4739 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - | 4740 | `		/* Finally register the function */` |
|   34734 | 4741 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   17366 | 4742 | `	}` |
|   34746 | 4743 | `	if( rc == SXRET_OK ){` |
|   34746 | 4744 | `		return SXRET_OK;` |
|       - | 4745 | `	}` |
|       - | 4746 | `	/* Fall through if something goes wrong */` |
|     ! 0 | 4747 | `OutOfMem:` |
|       - | 4748 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - | 4749 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - | 4750 | `	 */` |
|     ! 0 | 4751 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 4752 | `	return SXERR_ABORT;` |
|   17377 | 4753 |  |
|       - | 4754 | `/*` |
|       - | 4755 | ` * Compile a standard PHP function.` |
|       - | 4756 | ` *  Refer to the block-comment above for more information.` |
|       - | 4757 | ` */` |
|   34590 | 4758 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 | 4759 |  |
|       - | 4760 | `	SyString *pName;` |
|       - | 4761 | `	sxi32 iFlags;` |
|       - | 4762 | `	sxu32 nLine;` |
|       - | 4763 | `	sxi32 rc;` |
|       - | 4764 |  |
|   34592 | 4765 | `	nLine = pGen->pIn->nLine;` |
|   34592 | 4766 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   34592 | 4767 | `	iFlags = 0;` |
|   34592 | 4768 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4769 | `		/* Return by reference,remember that */` |
|       7 | 4770 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4771 | `		/* Jump the '&' token */` |
|       7 | 4772 | `		pGen->pIn++;` |
|       3 | 4773 | `	}` |
|   34592 | 4774 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4775 | `		/* Invalid function name */` |
|       5 | 4776 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 | 4777 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4778 | `			return SXERR_ABORT;` |
|       - | 4779 | `		}` |
|       - | 4780 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 | 4781 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 | 4782 | `			pGen->pIn++;` |
|       1 | 4783 | `		}` |
|       5 | 4784 | `		return SXRET_OK;` |
|       - | 4785 | `	}` |
|   34588 | 4786 | `	pName = &pGen->pIn->sData;` |
|   34588 | 4787 | `	nLine = pGen->pIn->nLine;` |
|       - | 4788 | `	/* Jump the function name */` |
|   34588 | 4789 | `	pGen->pIn++;` |
|   34588 | 4790 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4791 | `		/* Syntax error */` |
|       3 | 4792 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 | 4793 | `		if( rc == SXERR_ABORT ){` |
|       - | 4794 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4795 | `			return SXERR_ABORT;` |
|       - | 4796 | `		}` |
|       - | 4797 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 | 4798 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 4799 | `			pGen->pIn++;` |
|     ! 0 | 4800 | `		}` |
|       3 | 4801 | `		return SXRET_OK;` |
|       - | 4802 | `	}` |
|       - | 4803 | `	/* Compile function body */` |
|   34586 | 4804 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   34586 | 4805 | `	return rc;` |
|   17297 | 4806 |  |
|       - | 4807 | `/*` |
|       - | 4808 | ` * Extract the visibility level associated with a given keyword.` |
|       - | 4809 | ` * According to the PHP language reference manual` |
|       - | 4810 | ` *  Visibility:` |
|       - | 4811 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - | 4812 | ` *  the declaration with the keywords public, protected or private.` |
|       - | 4813 | ` *  Class members declared public can be accessed everywhere.` |
|       - | 4814 | ` *  Members declared protected can be accessed only within the class` |
|       - | 4815 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - | 4816 | ` *  may only be accessed by the class that defines the member.` |
|       - | 4817 | ` */` |
|  160664 | 4818 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 | 4819 |  |
|  160666 | 4820 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    7932 | 4821 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  152736 | 4822 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   18398 | 4823 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - | 4824 | `	}` |
|       - | 4825 | `	/* Assume public by default */` |
|  134340 | 4826 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   80334 | 4827 |  |
|       - | 4828 | `/*` |
|       - | 4829 | ` * Compile a class constant.` |
|       - | 4830 | ` * According to the PHP language reference manual` |
|       - | 4831 | ` *  Class Constants` |
|       - | 4832 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 4833 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 4834 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 4835 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 4836 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 4837 | ` *   It's also possible for interfaces to have constants.` |
|       - | 4838 | ` * Symisc eXtension.` |
|       - | 4839 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 4840 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4841 | ` *  Example:` |
|       - | 4842 | ` *   class Test{` |
|       - | 4843 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4844 | ` *   };` |
|       - | 4845 | ` *   var_dump(TEST::MyConst);` |
|       - | 4846 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4847 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4848 | ` */` |
|      30 | 4849 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4850 |  |
|      32 | 4851 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4852 | `	SySet *pInstrContainer;` |
|       - | 4853 | `	ph7_class_attr *pCons;` |
|       - | 4854 | `	SyString *pName;` |
|       - | 4855 | `	sxi32 rc;` |
|       - | 4856 | `	/* Extract visibility level */` |
|      32 | 4857 | `	iProtection = GetProtectionLevel(iProtection);` |
|      32 | 4858 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      15 | 4859 | `loop:` |
|       - | 4860 | `	/* Mark as constant */` |
|      32 | 4861 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      32 | 4862 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4863 | `		/* Invalid constant name */` |
|     ! 0 | 4864 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 | 4865 | `		if( rc == SXERR_ABORT ){` |
|       - | 4866 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4867 | `			return SXERR_ABORT;` |
|       - | 4868 | `		}` |
|     ! 0 | 4869 | `		goto Synchronize;` |
|       - | 4870 | `	}` |
|       - | 4871 | `	/* Peek constant name */` |
|      32 | 4872 | `	pName = &pGen->pIn->sData;` |
|       - | 4873 | `	/* Make sure the constant name isn't reserved */` |
|      32 | 4874 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 4875 | `		/* Reserved constant name */` |
|     ! 0 | 4876 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 | 4877 | `		if( rc == SXERR_ABORT ){` |
|       - | 4878 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4879 | `			return SXERR_ABORT;` |
|       - | 4880 | `		}` |
|     ! 0 | 4881 | `		goto Synchronize;` |
|       - | 4882 | `	}` |
|       - | 4883 | `	/* Advance the stream cursor */` |
|      32 | 4884 | `	pGen->pIn++;` |
|      32 | 4885 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 4886 | `		/* Invalid declaration */` |
|     ! 0 | 4887 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 | 4888 | `		if( rc == SXERR_ABORT ){` |
|       - | 4889 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4890 | `			return SXERR_ABORT;` |
|       - | 4891 | `		}` |
|     ! 0 | 4892 | `		goto Synchronize;` |
|       - | 4893 | `	}` |
|      32 | 4894 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - | 4895 | `	/* Allocate a new class attribute */` |
|      32 | 4896 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      32 | 4897 | `	if( pCons == 0 ){` |
|     ! 0 | 4898 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4899 | `		return SXERR_ABORT;` |
|       - | 4900 | `	}` |
|       - | 4901 | `	/* Swap bytecode container */` |
|      32 | 4902 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 | 4903 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - | 4904 | `	/* Compile constant value.` |
|       - | 4905 | `	 */` |
|      32 | 4906 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      32 | 4907 | `	if( rc == SXERR_EMPTY ){` |
|       3 | 4908 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 | 4909 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4910 | `			return SXERR_ABORT;` |
|       - | 4911 | `		}` |
|       1 | 4912 | `	}` |
|       - | 4913 | `	/* Emit the done instruction */` |
|      32 | 4914 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      32 | 4915 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 | 4916 | `	if( rc == SXERR_ABORT ){` |
|       - | 4917 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4918 | `		return SXERR_ABORT;` |
|       - | 4919 | `	}` |
|       - | 4920 | `	/* All done,install the constant */` |
|      32 | 4921 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      32 | 4922 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4923 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4924 | `		return SXERR_ABORT;` |
|       - | 4925 | `	}` |
|      32 | 4926 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4927 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 | 4928 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4929 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 4930 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4931 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4932 | `				pTok--;` |
|     ! 0 | 4933 | `			}` |
|     ! 0 | 4934 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4935 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 | 4936 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4937 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4938 | `				return SXERR_ABORT;` |
|       - | 4939 | `			}` |
|     ! 0 | 4940 | `		}else{` |
|     ! 0 | 4941 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 | 4942 | `				goto loop;` |
|       - | 4943 | `			}` |
|       - | 4944 | `		}` |
|     ! 0 | 4945 | `	}` |
|      32 | 4946 | `	return SXRET_OK;` |
|     ! 0 | 4947 | `Synchronize:` |
|       - | 4948 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 4949 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 | 4950 | `		pGen->pIn++;` |
|     ! 0 | 4951 | `	}` |
|     ! 0 | 4952 | `	return SXERR_CORRUPT;` |
|      17 | 4953 |  |
|       - | 4954 | `/*` |
|       - | 4955 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - | 4956 | ` * According to the PHP language reference manual` |
|       - | 4957 | ` *  Properties` |
|       - | 4958 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - | 4959 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - | 4960 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - | 4961 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - | 4962 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - | 4963 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - | 4964 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - | 4965 | ` * Symisc eXtension.` |
|       - | 4966 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - | 4967 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4968 | ` *  Example:` |
|       - | 4969 | ` *   class Test{` |
|       - | 4970 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4971 | ` *   };` |
|       - | 4972 | ` *   var_dump(TEST::myVar);` |
|       - | 4973 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4974 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4975 | ` */` |
|   34300 | 4976 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4977 |  |
|   34302 | 4978 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4979 | `	ph7_class_attr *pAttr;` |
|       - | 4980 | `	SyString *pName;` |
|       - | 4981 | `	sxi32 rc;` |
|       - | 4982 | `	/* Extract visibility level */` |
|   34302 | 4983 | `	iProtection = GetProtectionLevel(iProtection);` |
|   17150 | 4984 | `loop:` |
|   34302 | 4985 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   34302 | 4986 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - | 4987 | `		/* Invalid attribute name */` |
|     ! 0 | 4988 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 | 4989 | `		if( rc == SXERR_ABORT ){` |
|       - | 4990 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4991 | `			return SXERR_ABORT;` |
|       - | 4992 | `		}` |
|     ! 0 | 4993 | `		goto Synchronize;` |
|       - | 4994 | `	}` |
|       - | 4995 | `	/* Peek attribute name */` |
|   34302 | 4996 | `	pName = &pGen->pIn->sData;` |
|       - | 4997 | `	/* Advance the stream cursor */` |
|   34302 | 4998 | `	pGen->pIn++;` |
|   34302 | 4999 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - | 5000 | `		/* Invalid declaration */` |
|       3 | 5001 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 | 5002 | `		if( rc == SXERR_ABORT ){` |
|       - | 5003 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5004 | `			return SXERR_ABORT;` |
|       - | 5005 | `		}` |
|       3 | 5006 | `		goto Synchronize;` |
|       - | 5007 | `	}` |
|       - | 5008 | `	/* Allocate a new class attribute */` |
|   34300 | 5009 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|   34300 | 5010 | `	if( pAttr == 0 ){` |
|     ! 0 | 5011 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 5012 | `		return SXERR_ABORT;` |
|       - | 5013 | `	}` |
|   34300 | 5014 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - | 5015 | `		SySet *pInstrContainer;` |
|   10652 | 5016 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - | 5017 | `		/* Swap bytecode container */` |
|   10652 | 5018 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   10652 | 5019 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - | 5020 | `		/* Compile attribute value.` |
|       - | 5021 | `		 */` |
|   10652 | 5022 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   10652 | 5023 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 5024 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 | 5025 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5026 | `				return SXERR_ABORT;` |
|       - | 5027 | `			}` |
|     ! 0 | 5028 | `		}` |
|       - | 5029 | `		/* Emit the done instruction */` |
|   10652 | 5030 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   10652 | 5031 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    5325 | 5032 | `	}` |
|       - | 5033 | `	/* All done,install the attribute */` |
|   34300 | 5034 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   34300 | 5035 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5036 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5037 | `		return SXERR_ABORT;` |
|       - | 5038 | `	}` |
|   34300 | 5039 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 5040 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|     ! 0 | 5041 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 5042 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 5043 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 5044 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 5045 | `				pTok--;` |
|     ! 0 | 5046 | `			}` |
|     ! 0 | 5047 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5048 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5049 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 5050 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5051 | `				return SXERR_ABORT;` |
|       - | 5052 | `			}` |
|     ! 0 | 5053 | `		}else{` |
|     ! 0 | 5054 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 | 5055 | `				goto loop;` |
|       - | 5056 | `			}` |
|       - | 5057 | `		}` |
|     ! 0 | 5058 | `	}` |
|   34300 | 5059 | `	return SXRET_OK;` |
|       1 | 5060 | `Synchronize:` |
|       - | 5061 | `	/* Synchronize with the first semi-colon */` |
|       5 | 5062 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       3 | 5063 | `		pGen->pIn++;` |
|       1 | 5064 | `	}` |
|       3 | 5065 | `	return SXERR_CORRUPT;` |
|   17152 | 5066 |  |
|       - | 5067 | `/*` |
|       - | 5068 | ` * Compile a class method.` |
|       - | 5069 | ` *` |
|       - | 5070 | ` * Refer to the official documentation for more information` |
|       - | 5071 | ` * on the powerful extension introduced by the PH7 engine` |
|       - | 5072 | ` * to the OO subsystem such as full type hinting,method` |
|       - | 5073 | ` * overloading and many more.` |
|       - | 5074 | ` */` |
|  126334 | 5075 | `static sxi32 GenStateCompileClassMethod(` |
|       - | 5076 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 5077 | `	sxi32 iProtection,   /* Visibility level */` |
|       - | 5078 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - | 5079 | `	int doBody,          /* TRUE to process method body */` |
|       - | 5080 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - | 5081 | `	)` |
|       2 | 5082 |  |
|  126336 | 5083 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5084 | `	ph7_class_method *pMeth;` |
|       - | 5085 | `	sxi32 iFuncFlags;` |
|       - | 5086 | `	SyString *pName;` |
|       - | 5087 | `	SyToken *pEnd;` |
|       - | 5088 | `	sxi32 rc;` |
|       - | 5089 | `	/* Extract visibility level */` |
|  126336 | 5090 | `	iProtection = GetProtectionLevel(iProtection);` |
|  126336 | 5091 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  126336 | 5092 | `	iFuncFlags = 0;` |
|  126336 | 5093 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5094 | `		/* Invalid method name */` |
|     ! 0 | 5095 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 5096 | `		if( rc == SXERR_ABORT ){` |
|       - | 5097 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5098 | `			return SXERR_ABORT;` |
|       - | 5099 | `		}` |
|     ! 0 | 5100 | `		goto Synchronize;` |
|       - | 5101 | `	}` |
|  126336 | 5102 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 5103 | `		/* Return by reference,remember that */` |
|     ! 0 | 5104 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 5105 | `		/* Jump the '&' token */` |
|     ! 0 | 5106 | `		pGen->pIn++;` |
|     ! 0 | 5107 | `	}` |
|  126336 | 5108 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 5109 | `		/* Invalid method name */` |
|     ! 0 | 5110 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 5111 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5112 | `			return SXERR_ABORT;` |
|       - | 5113 | `		}` |
|     ! 0 | 5114 | `		goto Synchronize;` |
|       - | 5115 | `	}` |
|       - | 5116 | `	/* Peek method name */` |
|  126336 | 5117 | `	pName = &pGen->pIn->sData;` |
|  126336 | 5118 | `	nLine = pGen->pIn->nLine;` |
|       - | 5119 | `	/* Jump the method name */` |
|  126336 | 5120 | `	pGen->pIn++;` |
|  126336 | 5121 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 5122 | `		/* Abstract method */` |
|   21026 | 5123 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 5124 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5125 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 | 5126 | `				&pClass->sName,pName);` |
|     ! 0 | 5127 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5128 | `				return SXERR_ABORT;` |
|       - | 5129 | `			}` |
|     ! 0 | 5130 | `		}` |
|       - | 5131 | `		/* Assemble method signature only */` |
|   21026 | 5132 | `		doBody = FALSE;` |
|   10512 | 5133 | `	}` |
|  126336 | 5134 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 5135 | `		/* Syntax error */` |
|     ! 0 | 5136 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 | 5137 | `		if( rc == SXERR_ABORT ){` |
|       - | 5138 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5139 | `			return SXERR_ABORT;` |
|       - | 5140 | `		}` |
|     ! 0 | 5141 | `		goto Synchronize;` |
|       - | 5142 | `	}` |
|       - | 5143 | `	/* Allocate a new class_method instance */` |
|  126336 | 5144 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  126336 | 5145 | `	if( pMeth == 0 ){` |
|     ! 0 | 5146 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5147 | `		return SXERR_ABORT;` |
|       - | 5148 | `	}` |
|       - | 5149 | `	/* Jump the left parenthesis '(' */` |
|  126336 | 5150 | `	pGen->pIn++;` |
|  126336 | 5151 | `	pEnd = 0; /* cc warning */` |
|       - | 5152 | `	/* Delimit the method signature */` |
|  126336 | 5153 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  126336 | 5154 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5155 | `		/* Syntax error */` |
|       3 | 5156 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 | 5157 | `		if( rc == SXERR_ABORT ){` |
|       - | 5158 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5159 | `			return SXERR_ABORT;` |
|       - | 5160 | `		}` |
|       3 | 5161 | `		goto Synchronize;` |
|       - | 5162 | `	}` |
|  126334 | 5163 | `	if( pGen->pIn < pEnd ){` |
|       - | 5164 | `		/* Collect method arguments */` |
|   26304 | 5165 | `		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);` |
|   26304 | 5166 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5167 | `			return SXERR_ABORT;` |
|       - | 5168 | `		}` |
|   13151 | 5169 | `	}` |
|       - | 5170 | `	/* Point past ')' and parse optional return type ': type' */` |
|  126334 | 5171 | `	pGen->pIn = &pEnd[1];` |
|  126334 | 5172 | `	GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  126334 | 5173 | `	if( doBody ){` |
|       - | 5174 | `		/* Compile method body */` |
|  105310 | 5175 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  105310 | 5176 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5177 | `			return SXERR_ABORT;` |
|       - | 5178 | `		}` |
|   52656 | 5179 | `	}else{` |
|       - | 5180 | `		/* Only method signature is allowed */` |
|   21026 | 5181 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 | 5182 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5183 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 | 5184 | `				if( rc == SXERR_ABORT ){` |
|       - | 5185 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5186 | `					return SXERR_ABORT;` |
|       - | 5187 | `				}` |
|     ! 0 | 5188 | `				return SXERR_CORRUPT;` |
|       - | 5189 | `			}` |
|       - | 5190 | `	}` |
|       - | 5191 | `	/* All done,install the method */` |
|  126334 | 5192 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  126334 | 5193 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5194 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5195 | `		return SXERR_ABORT;` |
|       - | 5196 | `	}` |
|  126334 | 5197 | `	return SXRET_OK;` |
|       1 | 5198 | `Synchronize:` |
|       - | 5199 | `	/* Synchronize with the first semi-colon */` |
|       7 | 5200 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       5 | 5201 | `		pGen->pIn++;` |
|       1 | 5202 | `	}` |
|       3 | 5203 | `	return SXERR_CORRUPT;` |
|   63169 | 5204 |  |
|       - | 5205 | `/*` |
|       - | 5206 | ` * Compile an object interface.` |
|       - | 5207 | ` *  According to the PHP language reference manual` |
|       - | 5208 | ` *   Object Interfaces:` |
|       - | 5209 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - | 5210 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - | 5211 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - | 5212 | ` *   class, but without any of the methods having their contents defined.` |
|       - | 5213 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - | 5214 | ` */` |
|    7904 | 5215 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 | 5216 |  |
|    7906 | 5217 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5218 | `	ph7_class *pClass,*pBase;` |
|       - | 5219 | `	SyToken *pEnd,*pTmp;` |
|       - | 5220 | `	SyString *pName;` |
|       - | 5221 | `	sxi32 nKwrd;` |
|       - | 5222 | `	sxi32 rc;` |
|       - | 5223 | `	/* Jump the 'interface' keyword */` |
|    7906 | 5224 | `	pGen->pIn++;` |
|       - | 5225 | `	/* Extract interface name */` |
|    7906 | 5226 | `	pName = &pGen->pIn->sData;` |
|       - | 5227 | `	/* Advance the stream cursor */` |
|    7906 | 5228 | `	pGen->pIn++;` |
|       - | 5229 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5230 | `		SyBlob sFQN;` |
|       - | 5231 | `		SyString sFQNStr;` |
|    7906 | 5232 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    7906 | 5233 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    7906 | 5234 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    7906 | 5235 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    7906 | 5236 | `		SyBlobRelease(&sFQN);` |
|       - | 5237 | `	}` |
|    7906 | 5238 | `	if( pClass == 0 ){` |
|     ! 0 | 5239 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5240 | `		return SXERR_ABORT;` |
|       - | 5241 | `	}` |
|       - | 5242 | `	/* Mark as an interface */` |
|    7906 | 5243 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - | 5244 | `	/* Assume no base class is given */` |
|    7906 | 5245 | `	pBase = 0;` |
|    7906 | 5246 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 5247 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 5248 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - | 5249 | `			SyString *pBaseName;` |
|       - | 5250 | `			/* Extract base interface */` |
|       3 | 5251 | `			pGen->pIn++;` |
|       3 | 5252 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5253 | `				/* Syntax error */` |
|     ! 0 | 5254 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5255 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 | 5256 | `					pName);` |
|     ! 0 | 5257 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5258 | `				if( rc == SXERR_ABORT ){` |
|       - | 5259 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5260 | `					return SXERR_ABORT;` |
|       - | 5261 | `				}` |
|     ! 0 | 5262 | `				return SXRET_OK;` |
|       - | 5263 | `			}` |
|       3 | 5264 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 5265 | `			{` |
|       - | 5266 | `				SyBlob sResolved;` |
|       3 | 5267 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       3 | 5268 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|       4 | 5269 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|       2 | 5270 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       3 | 5271 | `				SyBlobRelease(&sResolved);` |
|       - | 5272 | `			}` |
|       - | 5273 | `			/* Only interfaces is allowed */` |
|       3 | 5274 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5275 | `				pBase = pBase->pNextName;` |
|     ! 0 | 5276 | `			}` |
|       3 | 5277 | `			if( pBase == 0 ){` |
|       - | 5278 | `				/* Inexistant interface */` |
|     ! 0 | 5279 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 | 5280 | `				if( rc == SXERR_ABORT ){` |
|       - | 5281 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5282 | `					return SXERR_ABORT;` |
|       - | 5283 | `				}` |
|     ! 0 | 5284 | `			}` |
|       - | 5285 | `			/* Advance the stream cursor */` |
|       3 | 5286 | `			pGen->pIn++;` |
|       1 | 5287 | `		}` |
|       1 | 5288 | `	}` |
|    7906 | 5289 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5290 | `		/* Syntax error */` |
|     ! 0 | 5291 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 | 5292 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5293 | `		if( rc == SXERR_ABORT ){` |
|       - | 5294 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5295 | `			return SXERR_ABORT;` |
|       - | 5296 | `		}` |
|     ! 0 | 5297 | `		return SXRET_OK;` |
|       - | 5298 | `	}` |
|    7906 | 5299 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    7906 | 5300 | `	pEnd = 0; /* cc warning */` |
|       - | 5301 | `	/* Delimit the interface body */` |
|    7906 | 5302 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    7906 | 5303 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5304 | `		/* Syntax error */` |
|     ! 0 | 5305 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 | 5306 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5307 | `		if( rc == SXERR_ABORT ){` |
|       - | 5308 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5309 | `			return SXERR_ABORT;` |
|       - | 5310 | `		}` |
|     ! 0 | 5311 | `		return SXRET_OK;` |
|       - | 5312 | `	}` |
|       - | 5313 | `	/* Swap token stream */` |
|    7906 | 5314 | `	pTmp = pGen->pEnd;` |
|    7906 | 5315 | `	pGen->pEnd = pEnd;` |
|       - | 5316 | `	/* Start the parse process` |
|       - | 5317 | `	 * Note (According to the PHP reference manual):` |
|       - | 5318 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - | 5319 | `	 *  Only 'public' visibility is allowed.` |
|       - | 5320 | `	 */` |
|   14459 | 5321 | `	for(;;){` |
|       - | 5322 | `		/* Jump leading/trailing semi-colons */` |
|   49934 | 5323 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   21016 | 5324 | `			pGen->pIn++;` |
|       2 | 5325 | `		}` |
|   28920 | 5326 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5327 | `			/* End of interface body */` |
|    7904 | 5328 | `			break;` |
|       - | 5329 | `		}` |
|   21018 | 5330 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5331 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5332 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 | 5333 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5334 | `			if( rc == SXERR_ABORT ){` |
|       - | 5335 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5336 | `				return SXERR_ABORT;` |
|       - | 5337 | `			}` |
|     ! 0 | 5338 | `			goto done;` |
|       - | 5339 | `		}` |
|       - | 5340 | `		/* Extract the current keyword */` |
|   21018 | 5341 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   21018 | 5342 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 5343 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - | 5344 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 | 5345 | `			const char *zKind = "member";` |
|       3 | 5346 | `			SyString *pMemberName = 0;` |
|       3 | 5347 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 | 5348 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 | 5349 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 | 5350 | `					zKind = "constant";` |
|       3 | 5351 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 | 5352 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 | 5353 | `					}` |
|       1 | 5354 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5355 | `					zKind = "method";` |
|     ! 0 | 5356 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 | 5357 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 | 5358 | `					}` |
|     ! 0 | 5359 | `				}` |
|       1 | 5360 | `			}` |
|       3 | 5361 | `			if( pMemberName ){` |
|       4 | 5362 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 | 5363 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 | 5364 | `			}else{` |
|     ! 0 | 5365 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5366 | `					"Access type for interface %s must be public",zKind);` |
|       - | 5367 | `			}` |
|       3 | 5368 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5369 | `				return SXERR_ABORT;` |
|       - | 5370 | `			}` |
|       3 | 5371 | `			goto done;` |
|       - | 5372 | `		}` |
|   21016 | 5373 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 5374 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5375 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 5376 | `			if( rc == SXERR_ABORT ){` |
|       - | 5377 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5378 | `				return SXERR_ABORT;` |
|       - | 5379 | `			}` |
|     ! 0 | 5380 | `			goto done;` |
|       - | 5381 | `		}` |
|   21016 | 5382 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - | 5383 | `			/* Advance the stream cursor */` |
|   21012 | 5384 | `			pGen->pIn++;` |
|   21012 | 5385 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5386 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5387 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 5388 | `				if( rc == SXERR_ABORT ){` |
|       - | 5389 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5390 | `					return SXERR_ABORT;` |
|       - | 5391 | `				}` |
|     ! 0 | 5392 | `				goto done;` |
|       - | 5393 | `			}` |
|   21012 | 5394 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   21012 | 5395 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 5396 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5397 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 5398 | `				if( rc == SXERR_ABORT ){` |
|       - | 5399 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5400 | `					return SXERR_ABORT;` |
|       - | 5401 | `				}` |
|     ! 0 | 5402 | `				goto done;` |
|       - | 5403 | `			}` |
|   10505 | 5404 | `		}` |
|   21016 | 5405 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 5406 | `			/* Parse constant */` |
|       3 | 5407 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 | 5408 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5409 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5410 | `					return SXERR_ABORT;` |
|       - | 5411 | `				}` |
|     ! 0 | 5412 | `				goto done;` |
|       - | 5413 | `			}` |
|       2 | 5414 | `		}else{` |
|   21014 | 5415 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   21014 | 5416 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 5417 | `				/* Static method,record that */` |
|     ! 0 | 5418 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - | 5419 | `				/* Advance the stream cursor */` |
|     ! 0 | 5420 | `				pGen->pIn++;` |
|     ! 0 | 5421 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 | 5422 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5423 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5424 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 5425 | `						if( rc == SXERR_ABORT ){` |
|       - | 5426 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5427 | `							return SXERR_ABORT;` |
|       - | 5428 | `						}` |
|     ! 0 | 5429 | `						goto done;` |
|       - | 5430 | `				}` |
|     ! 0 | 5431 | `			}` |
|       - | 5432 | `			/* Process method signature (no body for interface methods) */` |
|   21014 | 5433 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   21014 | 5434 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5435 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5436 | `					return SXERR_ABORT;` |
|       - | 5437 | `				}` |
|     ! 0 | 5438 | `				goto done;` |
|       - | 5439 | `			}` |
|       - | 5440 | `		}` |
|       2 | 5441 | `	}` |
|       - | 5442 | `	/* Install the interface */` |
|    7904 | 5443 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    7904 | 5444 | `	if( rc == SXRET_OK && pBase ){` |
|       - | 5445 | `		/* Inherit from the base interface */` |
|       3 | 5446 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 | 5447 | `	}` |
|    7904 | 5448 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5449 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5450 | `		return SXERR_ABORT;` |
|       - | 5451 | `	}` |
|    3951 | 5452 | `done:` |
|       - | 5453 | `	/* Point beyond the interface body */` |
|    7906 | 5454 | `	pGen->pIn  = &pEnd[1];` |
|    7906 | 5455 | `	pGen->pEnd = pTmp;` |
|    7906 | 5456 | `	return PH7_OK;` |
|    3954 | 5457 |  |
|       - | 5458 | `/*` |
|       - | 5459 | ` * Compile a user-defined class.` |
|       - | 5460 | ` * According to the PHP language reference manual` |
|       - | 5461 | ` *  class` |
|       - | 5462 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - | 5463 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - | 5464 | ` *  of the properties and methods belonging to the class.` |
|       - | 5465 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - | 5466 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - | 5467 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - | 5468 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 5469 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - | 5470 | ` *  (called "methods").` |
|       - | 5471 | ` */` |
|       - | 5472 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - | 5473 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - | 5474 | `struct TraitUseEntry {` |
|       - | 5475 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - | 5476 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - | 5477 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - | 5478 | `};` |
|       - | 5479 | `/*` |
|       - | 5480 | ` * Validate that methods implementing interface contracts have compatible` |
|       - | 5481 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - | 5482 | ` */` |
|   37174 | 5483 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 5484 |  |
|       - | 5485 | `	ph7_class **apIface;` |
|       - | 5486 | `	sxu32 nIface,i;` |
|       - | 5487 | `	sxi32 rc;` |
|   37176 | 5488 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 | 5489 | `		return SXRET_OK;` |
|       - | 5490 | `	}` |
|   37176 | 5491 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   37176 | 5492 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   39836 | 5493 | `	for(i = 0; i < nIface; i++){` |
|    2662 | 5494 | `		ph7_class *pIface = apIface[i];` |
|       - | 5495 | `		SyHashEntry *pEntry;` |
|    2662 | 5496 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   15850 | 5497 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   13190 | 5498 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - | 5499 | `			ph7_class_method *pImplMeth;` |
|   13190 | 5500 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - | 5501 | `			/* Find the implementing method in the class */` |
|   13190 | 5502 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   13190 | 5503 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 | 5504 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - | 5505 | `			}` |
|       - | 5506 | `			/* Check visibility: interface methods must be implemented as public */` |
|   13176 | 5507 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 | 5508 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 5509 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 | 5510 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 | 5511 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5512 | `					return SXERR_ABORT;` |
|       - | 5513 | `				}` |
|       1 | 5514 | `			}` |
|       - | 5515 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - | 5516 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - | 5517 | `			 */` |
|       - | 5518 | `			{` |
|   13176 | 5519 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   13176 | 5520 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   13176 | 5521 | `				int sigError = 0;` |
|   13176 | 5522 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 | 5523 | `					sigError = 1;` |
|   13175 | 5524 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - | 5525 | `					/* Extra parameters must all have default values */` |
|       5 | 5526 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - | 5527 | `					sxu32 k;` |
|       7 | 5528 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       5 | 5529 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 | 5530 | `							sigError = 1;` |
|       3 | 5531 | `							break;` |
|       - | 5532 | `						}` |
|       2 | 5533 | `					}` |
|       2 | 5534 | `				}` |
|   13176 | 5535 | `				if( sigError ){` |
|       - | 5536 | `					SyBlob sImplSig, sIfaceSig;` |
|       - | 5537 | `					ph7_vm_func_arg *aArgs;` |
|       - | 5538 | `					sxu32 j;` |
|       5 | 5539 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       5 | 5540 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - | 5541 | `					/* Build implementing method signature */` |
|       5 | 5542 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      11 | 5543 | `					for(j = 0; j < nImplArgs; j++){` |
|       7 | 5544 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       7 | 5545 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       7 | 5546 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 5547 | `					}` |
|       - | 5548 | `					/* Build interface method signature */` |
|       5 | 5549 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      11 | 5550 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       7 | 5551 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       7 | 5552 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       7 | 5553 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 5554 | `					}` |
|       7 | 5555 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 5556 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 | 5557 | `						&pClass->sName,pMName,` |
|       4 | 5558 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 | 5559 | `						&pIface->sName,pMName,` |
|       4 | 5560 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       5 | 5561 | `					SyBlobRelease(&sImplSig);` |
|       5 | 5562 | `					SyBlobRelease(&sIfaceSig);` |
|       5 | 5563 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5564 | `						return SXERR_ABORT;` |
|       - | 5565 | `					}` |
|       2 | 5566 | `				}` |
|       - | 5567 | `			}` |
|       2 | 5568 | `		}` |
|    1332 | 5569 | `	}` |
|   37176 | 5570 | `	return SXRET_OK;` |
|   18589 | 5571 |  |
|       - | 5572 | `/*` |
|       - | 5573 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - | 5574 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - | 5575 | ` */` |
|   37174 | 5576 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 5577 |  |
|       - | 5578 | `	ph7_class_method *pMeth;` |
|       - | 5579 | `	SyHashEntry *pEntry;` |
|       - | 5580 | `	sxu32 nAbstract;` |
|       - | 5581 | `	SyBlob sMsg;` |
|       - | 5582 | `	sxi32 rc;` |
|       - | 5583 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   37176 | 5584 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      20 | 5585 | `		return SXRET_OK;` |
|       - | 5586 | `	}` |
|       - | 5587 | `	/* Count abstract methods */` |
|   37158 | 5588 | `	nAbstract = 0;` |
|   37158 | 5589 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  352654 | 5590 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  315498 | 5591 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  315498 | 5592 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      17 | 5593 | `			nAbstract++;` |
|       8 | 5594 | `		}` |
|       2 | 5595 | `	}` |
|   37158 | 5596 | `	if( nAbstract == 0 ){` |
|   37144 | 5597 | `		return SXRET_OK;` |
|       - | 5598 | `	}` |
|       - | 5599 | `	/* Build the error message listing all abstract methods with origins */` |
|      15 | 5600 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      15 | 5601 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - | 5602 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 | 5603 | `		&pClass->sName,nAbstract,` |
|       7 | 5604 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 | 5605 | `		(nAbstract > 1 ? "s" : ""));` |
|       - | 5606 | `	/* Second pass: list methods with origins */` |
|       - | 5607 | `	{` |
|      15 | 5608 | `		sxu32 nListed = 0;` |
|      15 | 5609 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      33 | 5610 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      19 | 5611 | `			ph7_class *pOrigin = 0;` |
|       - | 5612 | `			SyString *pMName;` |
|      19 | 5613 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      19 | 5614 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 | 5615 | `				continue;` |
|       - | 5616 | `			}` |
|      17 | 5617 | `			pMName = &pMeth->sFunc.sName;` |
|      17 | 5618 | `			if( nListed > 0 ){` |
|       3 | 5619 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 | 5620 | `			}` |
|       - | 5621 | `			/* Find the origin of this abstract method.` |
|       - | 5622 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - | 5623 | `			 * inheritance chains) take precedence for interface-declared` |
|       - | 5624 | `			 * methods. Abstract class methods only win when the class` |
|       - | 5625 | `			 * itself declared the abstract method (not inherited from` |
|       - | 5626 | `			 * an interface). Trait methods are adopted into the using` |
|       - | 5627 | `			 * class's namespace.` |
|       - | 5628 | `			 */` |
|       - | 5629 | `			{` |
|       - | 5630 | `				ph7_class **apIface;` |
|       - | 5631 | `				ph7_class **apTrait;` |
|       - | 5632 | `				ph7_class *pWalk;` |
|       - | 5633 | `				sxu32 i;` |
|       - | 5634 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - | 5635 | `				 * (one that was written in the class body, not inherited from an` |
|       - | 5636 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - | 5637 | `				 */` |
|      17 | 5638 | `				if( pClass->pBase ){` |
|       9 | 5639 | `					pWalk = pClass->pBase;` |
|      17 | 5640 | `					while( pWalk ){` |
|       - | 5641 | `						ph7_class_method *pParentMeth;` |
|      11 | 5642 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      11 | 5643 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - | 5644 | `							/* Exclude methods that came from an interface anywhere` |
|       - | 5645 | `							 * in this class's ancestor chain.` |
|       - | 5646 | `							 */` |
|      11 | 5647 | `							int fromIface = 0;` |
|      11 | 5648 | `							ph7_class *pAnc = pWalk;` |
|      15 | 5649 | `							while( pAnc ){` |
|       - | 5650 | `								ph7_class **apPI;` |
|       - | 5651 | `								sxu32 j;` |
|      13 | 5652 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      13 | 5653 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       9 | 5654 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       9 | 5655 | `										fromIface = 1;` |
|       9 | 5656 | `										break;` |
|       - | 5657 | `									}` |
|     ! 0 | 5658 | `								}` |
|      13 | 5659 | `								if( fromIface ) break;` |
|       5 | 5660 | `								pAnc = pAnc->pBase;` |
|       1 | 5661 | `							}` |
|      11 | 5662 | `							if( !fromIface ){` |
|       3 | 5663 | `								pOrigin = pWalk;` |
|       3 | 5664 | `								break;` |
|       - | 5665 | `							}` |
|       4 | 5666 | `						}` |
|       9 | 5667 | `						pWalk = pWalk->pBase;` |
|       1 | 5668 | `					}` |
|       4 | 5669 | `				}` |
|       - | 5670 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - | 5671 | `				 * each interface's own parent chain for the deepest origin.` |
|       - | 5672 | `				 */` |
|      17 | 5673 | `				if( !pOrigin ){` |
|      15 | 5674 | `					pWalk = pClass;` |
|      37 | 5675 | `					while( pWalk && !pOrigin ){` |
|      23 | 5676 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      23 | 5677 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      13 | 5678 | `							ph7_class *pIface = apIface[i];` |
|      13 | 5679 | `							ph7_class *pDeepest = 0;` |
|      25 | 5680 | `							while( pIface ){` |
|      13 | 5681 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      13 | 5682 | `									pDeepest = pIface;` |
|       6 | 5683 | `								}` |
|      13 | 5684 | `								pIface = pIface->pBase;` |
|       1 | 5685 | `							}` |
|      13 | 5686 | `							if( pDeepest ){` |
|      13 | 5687 | `								pOrigin = pDeepest;` |
|      13 | 5688 | `								break;` |
|       - | 5689 | `							}` |
|     ! 0 | 5690 | `						}` |
|      23 | 5691 | `						pWalk = pWalk->pBase;` |
|       1 | 5692 | `					}` |
|       7 | 5693 | `				}` |
|       - | 5694 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      17 | 5695 | `				if( !pOrigin ){` |
|       3 | 5696 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 | 5697 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 | 5698 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 | 5699 | `							pOrigin = pClass;` |
|       3 | 5700 | `							break;` |
|       - | 5701 | `						}` |
|     ! 0 | 5702 | `					}` |
|       1 | 5703 | `				}` |
|       - | 5704 | `			}` |
|      17 | 5705 | `			if( pOrigin ){` |
|      17 | 5706 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       9 | 5707 | `			}else{` |
|       - | 5708 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 | 5709 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - | 5710 | `			}` |
|      17 | 5711 | `			nListed++;` |
|       1 | 5712 | `		}` |
|       - | 5713 | `	}` |
|      15 | 5714 | `	SyBlobAppend(&sMsg,")",1);` |
|      22 | 5715 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 | 5716 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      15 | 5717 | `	SyBlobRelease(&sMsg);` |
|      15 | 5718 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5719 | `		return SXERR_ABORT;` |
|       - | 5720 | `	}` |
|      15 | 5721 | `	return SXRET_OK;` |
|   18589 | 5722 |  |
|   37178 | 5723 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 | 5724 |  |
|   37180 | 5725 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5726 | `	ph7_class *pClass,*pBase;` |
|       - | 5727 | `	SyToken *pEnd,*pTmp;` |
|       - | 5728 | `	sxi32 iProtection;` |
|       - | 5729 | `	SySet aInterfaces;` |
|       - | 5730 | `	SySet aUseEntries;` |
|       - | 5731 | `	sxi32 iAttrflags;` |
|       - | 5732 | `	SyString *pName;` |
|       - | 5733 | `	sxi32 nKwrd;` |
|       - | 5734 | `	sxi32 rc;` |
|       - | 5735 | `	/* Jump the 'class' keyword */` |
|   37180 | 5736 | `	pGen->pIn++;` |
|   37180 | 5737 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5738 | `		/* Syntax error */` |
|     ! 0 | 5739 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 | 5740 | `		if( rc == SXERR_ABORT ){` |
|       - | 5741 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5742 | `			return SXERR_ABORT;` |
|       - | 5743 | `		}` |
|       - | 5744 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 | 5745 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 | 5746 | `			pGen->pIn++;` |
|     ! 0 | 5747 | `		}` |
|     ! 0 | 5748 | `		return SXRET_OK;` |
|       - | 5749 | `	}` |
|       - | 5750 | `	/* Extract class name */` |
|   37180 | 5751 | `	pName = &pGen->pIn->sData;` |
|       - | 5752 | `	/* Advance the stream cursor */` |
|   37180 | 5753 | `	pGen->pIn++;` |
|       - | 5754 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5755 | `		SyBlob sFQN;` |
|       - | 5756 | `		SyString sFQNStr;` |
|   37180 | 5757 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   37180 | 5758 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   37180 | 5759 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   37180 | 5760 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   37180 | 5761 | `		SyBlobRelease(&sFQN);` |
|       - | 5762 | `	}` |
|   37180 | 5763 | `	if( pClass == 0 ){` |
|     ! 0 | 5764 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5765 | `		return SXERR_ABORT;` |
|       - | 5766 | `	}` |
|       - | 5767 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   37180 | 5768 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   37180 | 5769 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - | 5770 | `	/* Assume a standalone class */` |
|   37180 | 5771 | `	pBase = 0;` |
|   37180 | 5772 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5773 | `		SyString *pBaseName;` |
|   26368 | 5774 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   26368 | 5775 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|   23710 | 5776 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   23710 | 5777 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5778 | `				/* Syntax error */` |
|     ! 0 | 5779 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5780 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 | 5781 | `					pName);` |
|     ! 0 | 5782 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5783 | `				if( rc == SXERR_ABORT ){` |
|       - | 5784 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5785 | `					return SXERR_ABORT;` |
|       - | 5786 | `				}` |
|     ! 0 | 5787 | `				return SXRET_OK;` |
|       - | 5788 | `			}` |
|       - | 5789 | `			/* Extract base class name and resolve through namespace/imports */` |
|   23710 | 5790 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 5791 | `			{` |
|       - | 5792 | `				SyBlob sResolved;` |
|   23710 | 5793 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   23710 | 5794 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|   35564 | 5795 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|   23708 | 5796 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   23710 | 5797 | `				SyBlobRelease(&sResolved);` |
|       - | 5798 | `			}` |
|       - | 5799 | `			/* Interfaces are not allowed */` |
|   23710 | 5800 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 | 5801 | `				pBase = pBase->pNextName;` |
|     ! 0 | 5802 | `			}` |
|   23710 | 5803 | `			if( pBase == 0 ){` |
|       - | 5804 | `				/* Inexistant base class */` |
|     ! 0 | 5805 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 | 5806 | `				if( rc == SXERR_ABORT ){` |
|       - | 5807 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5808 | `					return SXERR_ABORT;` |
|       - | 5809 | `				}` |
|     ! 0 | 5810 | `			}else{` |
|   23710 | 5811 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 | 5812 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 5813 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 | 5814 | `					if( rc == SXERR_ABORT ){` |
|       - | 5815 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5816 | `						return SXERR_ABORT;` |
|       - | 5817 | `					}` |
|     ! 0 | 5818 | `				}` |
|       - | 5819 | `			}` |
|       - | 5820 | `			/* Advance the stream cursor */` |
|   23710 | 5821 | `			pGen->pIn++;` |
|   11854 | 5822 | `		}` |
|   26368 | 5823 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - | 5824 | `			ph7_class *pInterface;` |
|       - | 5825 | `			SyString *pIntName;` |
|       - | 5826 | `			/* Interface implementation */` |
|    2662 | 5827 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    1330 | 5828 | `			for(;;){` |
|    2662 | 5829 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5830 | `					/* Syntax error */` |
|     ! 0 | 5831 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5832 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 | 5833 | `						pName);` |
|     ! 0 | 5834 | `					if( rc == SXERR_ABORT ){` |
|       - | 5835 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5836 | `						return SXERR_ABORT;` |
|       - | 5837 | `					}` |
|     ! 0 | 5838 | `					break;` |
|       - | 5839 | `				}` |
|       - | 5840 | `				/* Extract interface name and resolve through namespace/imports */` |
|    2662 | 5841 | `				pIntName = &pGen->pIn->sData;` |
|       - | 5842 | `				{` |
|       - | 5843 | `					SyBlob sResolved;` |
|    2662 | 5844 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    2662 | 5845 | `					GenStateResolveName(pGen,pIntName,&sResolved);` |
|    5322 | 5846 | `					pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    2660 | 5847 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    2662 | 5848 | `					SyBlobRelease(&sResolved);` |
|       - | 5849 | `				}` |
|       - | 5850 | `				/* Only interfaces are allowed */` |
|    2662 | 5851 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5852 | `					pInterface = pInterface->pNextName;` |
|     ! 0 | 5853 | `				}` |
|    2662 | 5854 | `				if( pInterface == 0 ){` |
|       - | 5855 | `					/* Inexistant interface */` |
|     ! 0 | 5856 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 | 5857 | `					if( rc == SXERR_ABORT ){` |
|       - | 5858 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5859 | `						return SXERR_ABORT;` |
|       - | 5860 | `					}` |
|     ! 0 | 5861 | `				}else{` |
|       - | 5862 | `					/* Register interface */` |
|    2662 | 5863 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - | 5864 | `				}` |
|       - | 5865 | `				/* Advance the stream cursor */` |
|    2662 | 5866 | `				pGen->pIn++;` |
|    2662 | 5867 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    1332 | 5868 | `					break;` |
|       - | 5869 | `				}` |
|     ! 0 | 5870 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 | 5871 | `			}` |
|    1330 | 5872 | `		}` |
|   13183 | 5873 | `	}` |
|   37180 | 5874 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5875 | `		/* Syntax error */` |
|     ! 0 | 5876 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 | 5877 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5878 | `		if( rc == SXERR_ABORT ){` |
|       - | 5879 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5880 | `			return SXERR_ABORT;` |
|       - | 5881 | `		}` |
|     ! 0 | 5882 | `		return SXRET_OK;` |
|       - | 5883 | `	}` |
|   37180 | 5884 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   37180 | 5885 | `	pEnd = 0; /* cc warning */` |
|       - | 5886 | `	/* Delimit the class body */` |
|   37180 | 5887 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   37180 | 5888 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5889 | `		/* Syntax error */` |
|     ! 0 | 5890 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 | 5891 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5892 | `		if( rc == SXERR_ABORT ){` |
|       - | 5893 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5894 | `			return SXERR_ABORT;` |
|       - | 5895 | `		}` |
|     ! 0 | 5896 | `		return SXRET_OK;` |
|       - | 5897 | `	}` |
|       - | 5898 | `	/* Swap token stream */` |
|   37180 | 5899 | `	pTmp = pGen->pEnd;` |
|   37180 | 5900 | `	pGen->pEnd = pEnd;` |
|       - | 5901 | `	/* Set the inherited flags */` |
|   37180 | 5902 | `	pClass->iFlags = iFlags;` |
|       - | 5903 | `	/* Start the parse process */` |
|   71239 | 5904 | `	for(;;){` |
|       - | 5905 | `		/* Jump leading/trailing semi-colons */` |
|  211154 | 5906 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   34356 | 5907 | `			pGen->pIn++;` |
|       2 | 5908 | `		}` |
|  176800 | 5909 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5910 | `			/* End of class body */` |
|   37176 | 5911 | `			break;` |
|       - | 5912 | `		}` |
|  139626 | 5913 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5914 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5915 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5916 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5917 | `			if( rc == SXERR_ABORT ){` |
|       - | 5918 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5919 | `				return SXERR_ABORT;` |
|       - | 5920 | `			}` |
|     ! 0 | 5921 | `			goto done;` |
|       - | 5922 | `		}` |
|       - | 5923 | `		/* Assume public visibility */` |
|  139626 | 5924 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  139626 | 5925 | `		iAttrflags = 0;` |
|  139626 | 5926 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 5927 | `			/* Extract the current keyword */` |
|  139626 | 5928 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  139626 | 5929 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 5930 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - | 5931 | `				TraitUseEntry sUse;` |
|      41 | 5932 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      41 | 5933 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      41 | 5934 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      28 | 5935 | `				for(;;){` |
|       - | 5936 | `					ph7_class *pTrait;` |
|       - | 5937 | `					SyString *pTraitName;` |
|      49 | 5938 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 5939 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5940 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 | 5941 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5942 | `							return SXERR_ABORT;` |
|       - | 5943 | `						}` |
|     ! 0 | 5944 | `						break;` |
|       - | 5945 | `					}` |
|      49 | 5946 | `					pTraitName = &pGen->pIn->sData;` |
|       - | 5947 | `					/* Resolve trait name through namespace/imports */ {` |
|       - | 5948 | `						SyBlob sResolved;` |
|      49 | 5949 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      49 | 5950 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|      97 | 5951 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      48 | 5952 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      49 | 5953 | `						SyBlobRelease(&sResolved);` |
|       - | 5954 | `					}` |
|       - | 5955 | `					/* Only traits are allowed */` |
|      49 | 5956 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 5957 | `						pTrait = pTrait->pNextName;` |
|     ! 0 | 5958 | `					}` |
|      49 | 5959 | `					if( pTrait == 0 ){` |
|     ! 0 | 5960 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5961 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 | 5962 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5963 | `							return SXERR_ABORT;` |
|       - | 5964 | `						}` |
|     ! 0 | 5965 | `					}else{` |
|      49 | 5966 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - | 5967 | `					}` |
|      49 | 5968 | `					pGen->pIn++; /* Advance past trait name */` |
|      49 | 5969 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      21 | 5970 | `						break;` |
|       - | 5971 | `					}` |
|       9 | 5972 | `					pGen->pIn++; /* Jump the comma */` |
|       1 | 5973 | `				}` |
|       - | 5974 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      41 | 5975 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 5976 | `					SyToken *pBlock;` |
|       9 | 5977 | `					pGen->pIn++; /* Jump '{' */` |
|       9 | 5978 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 | 5979 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 | 5980 | `					sUse.pResolvEnd = pBlock;` |
|       9 | 5981 | `					if( pBlock < pGen->pEnd ){` |
|       9 | 5982 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 | 5983 | `					}else{` |
|     ! 0 | 5984 | `						pGen->pIn = pGen->pEnd;` |
|       - | 5985 | `					}` |
|       4 | 5986 | `				}` |
|      41 | 5987 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - | 5988 | `				/* The semicolon will be consumed by the outer loop */` |
|      41 | 5989 | `				continue;` |
|       - | 5990 | `			}` |
|  139586 | 5991 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  136856 | 5992 | `				iProtection = nKwrd;` |
|  136856 | 5993 | `				pGen->pIn++; /* Jump the visibility token */` |
|  136856 | 5994 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5995 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5996 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5997 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 5998 | `					if( rc == SXERR_ABORT ){` |
|       - | 5999 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 6000 | `						return SXERR_ABORT;` |
|       - | 6001 | `					}` |
|     ! 0 | 6002 | `					goto done;` |
|       - | 6003 | `				}` |
|  136856 | 6004 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 6005 | `					/* Attribute declaration */` |
|   34280 | 6006 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   34280 | 6007 | `					if( rc != SXRET_OK ){` |
|       3 | 6008 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6009 | `							return SXERR_ABORT;` |
|       - | 6010 | `						}` |
|       3 | 6011 | `						goto done;` |
|       - | 6012 | `					}` |
|   34278 | 6013 | `					continue;` |
|       - | 6014 | `				}` |
|       - | 6015 | `				/* Extract the keyword */` |
|  102578 | 6016 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   51288 | 6017 | `			}` |
|  105308 | 6018 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 6019 | `				/* Process constant declaration */` |
|      30 | 6020 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      30 | 6021 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6022 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6023 | `						return SXERR_ABORT;` |
|       - | 6024 | `					}` |
|     ! 0 | 6025 | `					goto done;` |
|       - | 6026 | `				}` |
|      16 | 6027 | `			}else{` |
|  105280 | 6028 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 6029 | `					/* Static method or attribute,record that */` |
|    2648 | 6030 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    2648 | 6031 | `					pGen->pIn++; /* Jump the static keyword */` |
|    2648 | 6032 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 6033 | `						/* Extract the keyword */` |
|    2644 | 6034 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    2644 | 6035 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 6036 | `							iProtection = nKwrd;` |
|     ! 0 | 6037 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 | 6038 | `						}` |
|    1321 | 6039 | `					}` |
|    2648 | 6040 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6041 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6042 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 | 6043 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6044 | `						if( rc == SXERR_ABORT ){` |
|       - | 6045 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 6046 | `							return SXERR_ABORT;` |
|       - | 6047 | `						}` |
|     ! 0 | 6048 | `						goto done;` |
|       - | 6049 | `					}` |
|    2648 | 6050 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 6051 | `						/* Attribute declaration */` |
|       5 | 6052 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 6053 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 6054 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 6055 | `								return SXERR_ABORT;` |
|       - | 6056 | `							}` |
|     ! 0 | 6057 | `							goto done;` |
|       - | 6058 | `						}` |
|       5 | 6059 | `						continue;` |
|       - | 6060 | `					}` |
|       - | 6061 | `					/* Extract the keyword */` |
|    2644 | 6062 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  103955 | 6063 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - | 6064 | `					/* Abstract method,record that */` |
|      10 | 6065 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - | 6066 | `					/* Mark the whole class as abstract */` |
|      10 | 6067 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - | 6068 | `					/* Advance the stream cursor */` |
|      10 | 6069 | `					pGen->pIn++;` |
|      10 | 6070 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      10 | 6071 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      10 | 6072 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       8 | 6073 | `							iProtection = nKwrd;` |
|       8 | 6074 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 | 6075 | `						}` |
|       4 | 6076 | `					}` |
|      10 | 6077 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 6078 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 6079 | `							/* Static method */` |
|     ! 0 | 6080 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 6081 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 6082 | `					}` |
|      10 | 6083 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       8 | 6084 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6085 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6086 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 | 6087 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 6088 | `							if( rc == SXERR_ABORT ){` |
|       - | 6089 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 6090 | `								return SXERR_ABORT;` |
|       - | 6091 | `							}` |
|     ! 0 | 6092 | `							goto done;` |
|       - | 6093 | `					}` |
|      10 | 6094 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  102630 | 6095 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - | 6096 | `					/* final method ,record that */` |
|       5 | 6097 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 | 6098 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 | 6099 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 6100 | `						/* Extract the keyword */` |
|       5 | 6101 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 6102 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 6103 | `							iProtection = nKwrd;` |
|       5 | 6104 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 6105 | `						}` |
|       2 | 6106 | `					}` |
|       5 | 6107 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 | 6108 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 6109 | `							/* Static method */` |
|     ! 0 | 6110 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 6111 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 6112 | `					}` |
|       5 | 6113 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 6114 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6115 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6116 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 | 6117 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 6118 | `							if( rc == SXERR_ABORT ){` |
|       - | 6119 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 6120 | `								return SXERR_ABORT;` |
|       - | 6121 | `							}` |
|     ! 0 | 6122 | `							goto done;` |
|       - | 6123 | `					}` |
|       5 | 6124 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 6125 | `				}` |
|  105276 | 6126 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 6127 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6128 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 | 6129 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6130 | `						if( rc == SXERR_ABORT ){` |
|       - | 6131 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 6132 | `							return SXERR_ABORT;` |
|       - | 6133 | `						}` |
|     ! 0 | 6134 | `						goto done;` |
|       - | 6135 | `				}` |
|  105276 | 6136 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 | 6137 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 | 6138 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 | 6139 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6140 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 6141 | `						if( rc == SXERR_ABORT ){` |
|       - | 6142 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 6143 | `							return SXERR_ABORT;` |
|       - | 6144 | `						}` |
|     ! 0 | 6145 | `						goto done;` |
|       - | 6146 | `					}` |
|       - | 6147 | `					/* Attribute declaration */` |
|       7 | 6148 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 | 6149 | `				}else{` |
|       - | 6150 | `					/* Process method declaration */` |
|  105270 | 6151 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 6152 | `				}` |
|  105276 | 6153 | `				if( rc != SXRET_OK ){` |
|       3 | 6154 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6155 | `						return SXERR_ABORT;` |
|       - | 6156 | `					}` |
|       3 | 6157 | `					goto done;` |
|       - | 6158 | `				}` |
|       - | 6159 | `			}` |
|   52652 | 6160 | `		}else{` |
|       - | 6161 | `			/* Attribute declaration */` |
|     ! 0 | 6162 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6163 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6164 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6165 | `					return SXERR_ABORT;` |
|       - | 6166 | `				}` |
|     ! 0 | 6167 | `				goto done;` |
|       - | 6168 | `			}` |
|       - | 6169 | `		}` |
|       2 | 6170 | `	}` |
|       - | 6171 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - | 6172 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - | 6173 | `	 */` |
|       - | 6174 | `	{` |
|       - | 6175 | `		TraitUseEntry *apUse;` |
|       - | 6176 | `		sxu32 nU;` |
|   37176 | 6177 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   37216 | 6178 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      41 | 6179 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      41 | 6180 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      41 | 6181 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      41 | 6182 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - | 6183 | `			sxu32 nT;` |
|      41 | 6184 | `			if( !hasResolution ){` |
|       - | 6185 | `				/* No conflict resolution block: use standard trait application */` |
|      71 | 6186 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      39 | 6187 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      39 | 6188 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 6189 | `						break;` |
|       - | 6190 | `					}` |
|      20 | 6191 | `				}` |
|      17 | 6192 | `			}else{` |
|       - | 6193 | `				/* With resolution block: copy attributes, record traits,` |
|       - | 6194 | `				 * then use the block to resolve method conflicts.` |
|       - | 6195 | `				 */` |
|       - | 6196 | `				SyToken *pR;` |
|      19 | 6197 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 | 6198 | `					ph7_class *pTR = apTrait[nT];` |
|       - | 6199 | `					ph7_class_attr *pAR;` |
|       - | 6200 | `					SyHashEntry *pER;` |
|       - | 6201 | `					SyString *pNR;` |
|      11 | 6202 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 | 6203 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 | 6204 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 | 6205 | `						pNR = &pAR->sName;` |
|     ! 0 | 6206 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 | 6207 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 | 6208 | `						}` |
|     ! 0 | 6209 | `					}` |
|      11 | 6210 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 | 6211 | `				}` |
|       - | 6212 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 | 6213 | `				pR = pUse->pResolvStart;` |
|      21 | 6214 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 6215 | `					SyString sTrait,sMethod;` |
|       - | 6216 | `					ph7_class *pSrcTrait;` |
|       - | 6217 | `					ph7_class_method *pMeth;` |
|       - | 6218 | `					sxi32 nRKwrd;` |
|      33 | 6219 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 6220 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 6221 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 6222 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 6223 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 6224 | `					sMethod = pR->sData;` |
|      13 | 6225 | `					pR++;` |
|      13 | 6226 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 6227 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 6228 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 6229 | `							sTrait = sMethod;` |
|       7 | 6230 | `							pR++;` |
|       7 | 6231 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 6232 | `							sMethod = pR->sData;` |
|       7 | 6233 | `							pR++;` |
|       3 | 6234 | `						}` |
|       3 | 6235 | `					}` |
|      13 | 6236 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6237 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 6238 | `						continue;` |
|       - | 6239 | `					}` |
|      13 | 6240 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 6241 | `					pR++;` |
|      13 | 6242 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 | 6243 | `						pSrcTrait = 0;` |
|       7 | 6244 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 | 6245 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 | 6246 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 | 6247 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 | 6248 | `								pSrcTrait = apTrait[nT];` |
|       5 | 6249 | `								break;` |
|       - | 6250 | `							}` |
|       2 | 6251 | `						}` |
|       5 | 6252 | `						if( pSrcTrait ){` |
|       5 | 6253 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 | 6254 | `							if( pMeth ){` |
|       5 | 6255 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 | 6256 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 | 6257 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 | 6258 | `								}` |
|       2 | 6259 | `							}` |
|       2 | 6260 | `						}` |
|       2 | 6261 | `					}` |
|      29 | 6262 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 6263 | `				}` |
|       - | 6264 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 | 6265 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - | 6266 | `					ph7_class_method *pMR;` |
|       - | 6267 | `					SyHashEntry *pER;` |
|       - | 6268 | `					SyString *pNR;` |
|      11 | 6269 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 | 6270 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 | 6271 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 | 6272 | `						pNR = &pMR->sFunc.sName;` |
|      19 | 6273 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 | 6274 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 | 6275 | `						}` |
|       1 | 6276 | `					}` |
|       6 | 6277 | `				}` |
|       - | 6278 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 | 6279 | `				pR = pUse->pResolvStart;` |
|      21 | 6280 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 6281 | `					SyString sTrait,sMethod,sAlias;` |
|       - | 6282 | `					ph7_class *pSrcTrait;` |
|       - | 6283 | `					ph7_class_method *pMeth;` |
|      21 | 6284 | `					int hasQual = 0;` |
|       - | 6285 | `					sxi32 nRKwrd;` |
|      33 | 6286 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 6287 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 6288 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 6289 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 6290 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 | 6291 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 6292 | `					sMethod = pR->sData;` |
|      13 | 6293 | `					pR++;` |
|      13 | 6294 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 6295 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 6296 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 6297 | `							sTrait = sMethod;` |
|       7 | 6298 | `							hasQual = 1;` |
|       7 | 6299 | `							pR++;` |
|       7 | 6300 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 6301 | `							sMethod = pR->sData;` |
|       7 | 6302 | `							pR++;` |
|       3 | 6303 | `						}` |
|       3 | 6304 | `					}` |
|      13 | 6305 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6306 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 6307 | `						continue;` |
|       - | 6308 | `					}` |
|      13 | 6309 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 6310 | `					pR++;` |
|      13 | 6311 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 | 6312 | `						sxi32 iNewVis = -1;` |
|       9 | 6313 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 | 6314 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 | 6315 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 | 6316 | `								iNewVis = nAK;` |
|       7 | 6317 | `								pR++;` |
|       3 | 6318 | `							}` |
|       3 | 6319 | `						}` |
|       9 | 6320 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 | 6321 | `							sAlias = pR->sData;` |
|       7 | 6322 | `							pR++;` |
|       3 | 6323 | `						}` |
|       9 | 6324 | `						pMeth = 0;` |
|       9 | 6325 | `						if( hasQual ){` |
|       3 | 6326 | `							pSrcTrait = 0;` |
|       5 | 6327 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 | 6328 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 | 6329 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 | 6330 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 | 6331 | `									pSrcTrait = apTrait[nT];` |
|       3 | 6332 | `									break;` |
|       - | 6333 | `								}` |
|       2 | 6334 | `							}` |
|       3 | 6335 | `							if( pSrcTrait ){` |
|       3 | 6336 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 | 6337 | `							}` |
|       2 | 6338 | `						}else{` |
|       7 | 6339 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - | 6340 | `						}` |
|       9 | 6341 | `						if( pMeth ){` |
|       9 | 6342 | `							if( sAlias.nByte > 0 ){` |
|       - | 6343 | `								/* Create a shallow copy of the method struct for the alias` |
|       - | 6344 | `								 * so it can carry its own visibility without affecting the original.` |
|       - | 6345 | `								 */` |
|       - | 6346 | `								ph7_class_method *pAlias;` |
|       - | 6347 | `								char *zAliasDup;` |
|       7 | 6348 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 | 6349 | `								if( pAlias ){` |
|       7 | 6350 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 | 6351 | `									if( iNewVis >= 0 ){` |
|       5 | 6352 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 6353 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 6354 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 | 6355 | `									}` |
|       7 | 6356 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 | 6357 | `									if( zAliasDup ){` |
|       7 | 6358 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 | 6359 | `									}` |
|       4 | 6360 | `								}` |
|       6 | 6361 | `							}else if( iNewVis >= 0 ){` |
|       - | 6362 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - | 6363 | `								ph7_class_method *pCopy;` |
|       3 | 6364 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 | 6365 | `								if( pCopy ){` |
|       3 | 6366 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 | 6367 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 | 6368 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 6369 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 6370 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - | 6371 | `									/* Replace the method in the class hash */` |
|       3 | 6372 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 | 6373 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 | 6374 | `								}` |
|       1 | 6375 | `							}` |
|       4 | 6376 | `						}` |
|       4 | 6377 | `						SXUNUSED(hasQual);` |
|       4 | 6378 | `					}` |
|      17 | 6379 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 6380 | `				}` |
|       - | 6381 | `			}` |
|      41 | 6382 | `			SySetRelease(&pUse->aTraits);` |
|      21 | 6383 | `		}` |
|       - | 6384 | `	}` |
|       - | 6385 | `	/* Install the class */` |
|   37176 | 6386 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   37176 | 6387 | `	if( rc == SXRET_OK ){` |
|       - | 6388 | `		ph7_class **apInterface;` |
|       - | 6389 | `		sxu32 n;` |
|   37176 | 6390 | `		if( pBase ){` |
|       - | 6391 | `			/* Inherit from base class and mark as a subclass */` |
|   23710 | 6392 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   11854 | 6393 | `		}` |
|   37176 | 6394 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   39836 | 6395 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 6396 | `			/* Implements one or more interface */` |
|    2662 | 6397 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    2662 | 6398 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6399 | `				break;` |
|       - | 6400 | `			}` |
|    1332 | 6401 | `		}` |
|       - | 6402 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   37176 | 6403 | `		if( rc == SXRET_OK ){` |
|   37176 | 6404 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   37176 | 6405 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 6406 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 6407 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 6408 | `				return SXERR_ABORT;` |
|       - | 6409 | `			}` |
|   18587 | 6410 | `		}` |
|       - | 6411 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   37176 | 6412 | `		if( rc == SXRET_OK ){` |
|   37176 | 6413 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   37176 | 6414 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 6415 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 6416 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 6417 | `				return SXERR_ABORT;` |
|       - | 6418 | `			}` |
|   18587 | 6419 | `		}` |
|   18587 | 6420 | `	}` |
|   37176 | 6421 | `	SySetRelease(&aUseEntries);` |
|   37176 | 6422 | `	SySetRelease(&aInterfaces);` |
|   37176 | 6423 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6424 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6425 | `		return SXERR_ABORT;` |
|       - | 6426 | `	}` |
|   18587 | 6427 | `done:` |
|       - | 6428 | `	/* Point beyond the class body */` |
|   37180 | 6429 | `	pGen->pIn = &pEnd[1];` |
|   37180 | 6430 | `	pGen->pEnd = pTmp;` |
|   37180 | 6431 | `	return PH7_OK;` |
|   18591 | 6432 |  |
|       - | 6433 | `/*` |
|       - | 6434 | ` * Compile a user-defined abstract class.` |
|       - | 6435 | ` *  According to the PHP language reference manual` |
|       - | 6436 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 6437 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 6438 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 6439 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 6440 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 6441 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 6442 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 6443 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 6444 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 6445 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 6446 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 6447 | ` *   could differ.` |
|       - | 6448 | ` */` |
|      16 | 6449 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 | 6450 |  |
|       - | 6451 | `	sxi32 rc;` |
|      18 | 6452 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      18 | 6453 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      18 | 6454 | `	return rc;` |
|       2 | 6455 |  |
|       - | 6456 | `/*` |
|       - | 6457 | ` * Compile a user-defined final class.` |
|       - | 6458 | ` *  According to the PHP language reference manual` |
|       - | 6459 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - | 6460 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - | 6461 | ` *    final then it cannot be extended.` |
|       - | 6462 | ` */` |
|       2 | 6463 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 | 6464 |  |
|       - | 6465 | `	sxi32 rc;` |
|       3 | 6466 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 | 6467 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 | 6468 | `	return rc;` |
|       1 | 6469 |  |
|       - | 6470 | `/*` |
|       - | 6471 | ` * Compile a user-defined trait.` |
|       - | 6472 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - | 6473 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - | 6474 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - | 6475 | ` */` |
|      52 | 6476 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       2 | 6477 |  |
|      54 | 6478 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6479 | `	ph7_class *pClass;` |
|       - | 6480 | `	SyToken *pEnd,*pTmp;` |
|       - | 6481 | `	sxi32 iProtection;` |
|       - | 6482 | `	sxi32 iAttrflags;` |
|       - | 6483 | `	SyString *pName;` |
|       - | 6484 | `	sxi32 nKwrd;` |
|       - | 6485 | `	sxi32 rc;` |
|       - | 6486 | `	/* Jump the 'trait' keyword */` |
|      54 | 6487 | `	pGen->pIn++;` |
|      54 | 6488 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6489 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 | 6490 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6491 | `			return SXERR_ABORT;` |
|       - | 6492 | `		}` |
|     ! 0 | 6493 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 6494 | `			pGen->pIn++;` |
|     ! 0 | 6495 | `		}` |
|     ! 0 | 6496 | `		return SXRET_OK;` |
|       - | 6497 | `	}` |
|       - | 6498 | `	/* Extract trait name */` |
|      54 | 6499 | `	pName = &pGen->pIn->sData;` |
|      54 | 6500 | `	pGen->pIn++;` |
|       - | 6501 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 6502 | `		SyBlob sFQN;` |
|       - | 6503 | `		SyString sFQNStr;` |
|      54 | 6504 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      54 | 6505 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      54 | 6506 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      54 | 6507 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      54 | 6508 | `		SyBlobRelease(&sFQN);` |
|       - | 6509 | `	}` |
|      54 | 6510 | `	if( pClass == 0 ){` |
|     ! 0 | 6511 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6512 | `		return SXERR_ABORT;` |
|       - | 6513 | `	}` |
|       - | 6514 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      54 | 6515 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 6516 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 | 6517 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6518 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6519 | `			return SXERR_ABORT;` |
|       - | 6520 | `		}` |
|     ! 0 | 6521 | `		return SXRET_OK;` |
|       - | 6522 | `	}` |
|      54 | 6523 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      54 | 6524 | `	pEnd = 0;` |
|      54 | 6525 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      54 | 6526 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 6527 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 6528 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6529 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6530 | `			return SXERR_ABORT;` |
|       - | 6531 | `		}` |
|     ! 0 | 6532 | `		return SXRET_OK;` |
|       - | 6533 | `	}` |
|       - | 6534 | `	/* Swap token stream */` |
|      54 | 6535 | `	pTmp = pGen->pEnd;` |
|      54 | 6536 | `	pGen->pEnd = pEnd;` |
|       - | 6537 | `	/* Mark as trait */` |
|      54 | 6538 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - | 6539 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      53 | 6540 | `	for(;;){` |
|     144 | 6541 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      21 | 6542 | `			pGen->pIn++;` |
|       1 | 6543 | `		}` |
|     124 | 6544 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      54 | 6545 | `			break;` |
|       - | 6546 | `		}` |
|      71 | 6547 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6548 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6549 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 6550 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 6551 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6552 | `				return SXERR_ABORT;` |
|       - | 6553 | `			}` |
|     ! 0 | 6554 | `			goto done;` |
|       - | 6555 | `		}` |
|      71 | 6556 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      71 | 6557 | `		iAttrflags = 0;` |
|      71 | 6558 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      71 | 6559 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      71 | 6560 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 6561 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 | 6562 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 | 6563 | `				for(;;){` |
|       - | 6564 | `					ph7_class *pUsedTrait;` |
|       - | 6565 | `					SyString *pUsedName;` |
|       5 | 6566 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6567 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6568 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 6569 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6570 | `							return SXERR_ABORT;` |
|       - | 6571 | `						}` |
|     ! 0 | 6572 | `						break;` |
|       - | 6573 | `					}` |
|       5 | 6574 | `					pUsedName = &pGen->pIn->sData;` |
|       - | 6575 | `					{` |
|       - | 6576 | `						SyBlob sResolved;` |
|       5 | 6577 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 | 6578 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 | 6579 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 | 6580 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 | 6581 | `						SyBlobRelease(&sResolved);` |
|       - | 6582 | `					}` |
|       5 | 6583 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 6584 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 6585 | `					}` |
|       5 | 6586 | `					if( pUsedTrait == 0 ){` |
|       4 | 6587 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 | 6588 | `							"'%z' is not a trait",pUsedName);` |
|       3 | 6589 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6590 | `							return SXERR_ABORT;` |
|       - | 6591 | `						}` |
|       2 | 6592 | `					}else{` |
|       3 | 6593 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - | 6594 | `					}` |
|       5 | 6595 | `					pGen->pIn++;` |
|       5 | 6596 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 | 6597 | `						break;` |
|       - | 6598 | `					}` |
|     ! 0 | 6599 | `					pGen->pIn++;` |
|     ! 0 | 6600 | `				}` |
|       5 | 6601 | `				continue;` |
|       - | 6602 | `			}` |
|      67 | 6603 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      63 | 6604 | `				iProtection = nKwrd;` |
|      63 | 6605 | `				pGen->pIn++;` |
|      63 | 6606 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6607 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6608 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 6609 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6610 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6611 | `						return SXERR_ABORT;` |
|       - | 6612 | `					}` |
|     ! 0 | 6613 | `					goto done;` |
|       - | 6614 | `				}` |
|      63 | 6615 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 | 6616 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 | 6617 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 6618 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6619 | `							return SXERR_ABORT;` |
|       - | 6620 | `						}` |
|     ! 0 | 6621 | `						goto done;` |
|       - | 6622 | `					}` |
|      11 | 6623 | `					continue;` |
|       - | 6624 | `				}` |
|      53 | 6625 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      26 | 6626 | `			}` |
|      57 | 6627 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 6628 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6629 | `					"Traits cannot have constants");` |
|     ! 0 | 6630 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6631 | `					return SXERR_ABORT;` |
|       - | 6632 | `				}` |
|     ! 0 | 6633 | `				goto done;` |
|     ! 0 | 6634 | `			}else{` |
|      57 | 6635 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 | 6636 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 | 6637 | `					pGen->pIn++;` |
|       5 | 6638 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 6639 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 6640 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 6641 | `							iProtection = nKwrd;` |
|     ! 0 | 6642 | `							pGen->pIn++;` |
|     ! 0 | 6643 | `						}` |
|       1 | 6644 | `					}` |
|       5 | 6645 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6646 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6647 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 6648 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6649 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6650 | `							return SXERR_ABORT;` |
|       - | 6651 | `						}` |
|     ! 0 | 6652 | `						goto done;` |
|       - | 6653 | `					}` |
|       5 | 6654 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 6655 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 6656 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 6657 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 6658 | `								return SXERR_ABORT;` |
|       - | 6659 | `							}` |
|     ! 0 | 6660 | `							goto done;` |
|       - | 6661 | `						}` |
|       3 | 6662 | `						continue;` |
|       - | 6663 | `					}` |
|       3 | 6664 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      54 | 6665 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       5 | 6666 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       5 | 6667 | `					pGen->pIn++;` |
|       5 | 6668 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 | 6669 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 6670 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 6671 | `							iProtection = nKwrd;` |
|       5 | 6672 | `							pGen->pIn++;` |
|       2 | 6673 | `						}` |
|       2 | 6674 | `					}` |
|       5 | 6675 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 6676 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6677 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6678 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 6679 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6680 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6681 | `							return SXERR_ABORT;` |
|       - | 6682 | `						}` |
|     ! 0 | 6683 | `						goto done;` |
|       - | 6684 | `					}` |
|       5 | 6685 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 6686 | `				}` |
|      55 | 6687 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 6688 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6689 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 6690 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6691 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6692 | `						return SXERR_ABORT;` |
|       - | 6693 | `					}` |
|     ! 0 | 6694 | `					goto done;` |
|       - | 6695 | `				}` |
|      55 | 6696 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 6697 | `					pGen->pIn++;` |
|     ! 0 | 6698 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 6699 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6700 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 6701 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6702 | `							return SXERR_ABORT;` |
|       - | 6703 | `						}` |
|     ! 0 | 6704 | `						goto done;` |
|       - | 6705 | `					}` |
|     ! 0 | 6706 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6707 | `				}else{` |
|      55 | 6708 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 6709 | `				}` |
|      55 | 6710 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6711 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6712 | `						return SXERR_ABORT;` |
|       - | 6713 | `					}` |
|     ! 0 | 6714 | `					goto done;` |
|       - | 6715 | `				}` |
|       - | 6716 | `			}` |
|      28 | 6717 | `		}else{` |
|     ! 0 | 6718 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6719 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6720 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6721 | `					return SXERR_ABORT;` |
|       - | 6722 | `				}` |
|     ! 0 | 6723 | `				goto done;` |
|       - | 6724 | `			}` |
|       - | 6725 | `		}` |
|       1 | 6726 | `	}` |
|       - | 6727 | `	/* Install the trait */` |
|      54 | 6728 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      54 | 6729 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6730 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6731 | `		return SXERR_ABORT;` |
|       - | 6732 | `	}` |
|      26 | 6733 | `done:` |
|       - | 6734 | `	/* Point beyond the trait body */` |
|      54 | 6735 | `	pGen->pIn = &pEnd[1];` |
|      54 | 6736 | `	pGen->pEnd = pTmp;` |
|      54 | 6737 | `	return PH7_OK;` |
|      28 | 6738 |  |
|       - | 6739 | `/*` |
|       - | 6740 | ` * Compile a user-defined class.` |
|       - | 6741 | ` *  According to the PHP language reference manual` |
|       - | 6742 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 6743 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 6744 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 6745 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 6746 | ` *   and functions (called "methods").` |
|       - | 6747 | ` */` |
|   37160 | 6748 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 | 6749 |  |
|       - | 6750 | `	sxi32 rc;` |
|   37162 | 6751 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   37162 | 6752 | `	return rc;` |
|       2 | 6753 |  |
|       - | 6754 | `/*` |
|       - | 6755 | ` * Exception handling.` |
|       - | 6756 | ` *  According to the PHP language reference manual` |
|       - | 6757 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 6758 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 6759 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 6760 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 6761 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 6762 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 6763 | ` *    (or re-thrown) within a catch block.` |
|       - | 6764 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 6765 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 6766 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 6767 | ` *    been defined with set_exception_handler().` |
|       - | 6768 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 6769 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 6770 | ` */` |
|       - | 6771 | `/*` |
|       - | 6772 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 6773 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 6774 | ` * indicates failure.` |
|       - | 6775 | ` */` |
|    7926 | 6776 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 6777 |  |
|    7928 | 6778 | `	sxi32 rc = SXRET_OK;` |
|    7928 | 6779 | `	if( pRoot->pOp ){` |
|    7922 | 6780 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */` |
|    3963 | 6781 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 6782 | `			/* Unexpected expression */` |
|     ! 0 | 6783 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6784 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 6785 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 6786 | `				rc = SXERR_INVALID;` |
|     ! 0 | 6787 | `			}` |
|       2 | 6788 | `		}` |
|    3966 | 6789 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 6790 | `		/* Unexpected expression */` |
|     ! 0 | 6791 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6792 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 6793 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 6794 | `			rc = SXERR_INVALID;` |
|     ! 0 | 6795 | `		}` |
|     ! 0 | 6796 | `	}` |
|    7928 | 6797 | `	return rc;` |
|       2 | 6798 |  |
|       - | 6799 | `/*` |
|       - | 6800 | ` * Compile a 'throw' statement.` |
|       - | 6801 | ` * throw: This is how you trigger an exception.` |
|       - | 6802 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 6803 | ` */` |
|    7926 | 6804 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 | 6805 |  |
|    7928 | 6806 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6807 | `	GenBlock *pBlock;` |
|       - | 6808 | `	sxu32 nIdx;` |
|       - | 6809 | `	sxi32 rc;` |
|    7928 | 6810 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 6811 | `	/* Compile the expression */` |
|    7928 | 6812 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    7928 | 6813 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 6814 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 6815 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6816 | `			return SXERR_ABORT;` |
|       - | 6817 | `		}` |
|     ! 0 | 6818 | `		return SXRET_OK;` |
|       - | 6819 | `	}` |
|    7928 | 6820 | `	pBlock = pGen->pCurrent;` |
|       - | 6821 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   36830 | 6822 | `	while(pBlock->pParent){` |
|   36826 | 6823 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    7924 | 6824 | `			break;` |
|       - | 6825 | `		}` |
|       - | 6826 | `		/* Point to the parent block */` |
|   28904 | 6827 | `		pBlock = pBlock->pParent;` |
|       2 | 6828 | `	}` |
|       - | 6829 | `	/* Emit the throw instruction */` |
|    7928 | 6830 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 6831 | `	/* Emit the jump */` |
|    7928 | 6832 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    7928 | 6833 | `	return SXRET_OK;` |
|    3965 | 6834 |  |
|       - | 6835 | `/*` |
|       - | 6836 | ` * Compile a 'catch' block.` |
|       - | 6837 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 6838 | ` * an object containing the exception information.` |
|       - | 6839 | ` */` |
|      88 | 6840 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 | 6841 |  |
|      90 | 6842 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6843 | `	ph7_exception_block sCatch;` |
|       - | 6844 | `	SySet *pInstrContainer;` |
|       - | 6845 | `	SyString sClassName;` |
|       - | 6846 | `	GenBlock *pCatch;` |
|       - | 6847 | `	SyToken *pToken;` |
|       - | 6848 | `	SyString *pName;` |
|       - | 6849 | `	char *zDup;` |
|       - | 6850 | `	sxi32 rc;` |
|      90 | 6851 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 6852 | `	/* Zero the structure */` |
|      90 | 6853 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 6854 | `	/* Initialize fields */` |
|      90 | 6855 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|      90 | 6856 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|      90 | 6857 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 6858 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 6859 | `			pToken = pGen->pIn;` |
|     ! 0 | 6860 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6861 | `				pToken--;` |
|     ! 0 | 6862 | `			}` |
|     ! 0 | 6863 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 6864 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 6865 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 6866 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6867 | `				return SXERR_ABORT;` |
|       - | 6868 | `			}` |
|     ! 0 | 6869 | `			return SXERR_INVALID;` |
|       - | 6870 | `	}` |
|       - | 6871 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|      90 | 6872 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|      56 | 6873 | `	for(;;){` |
|     114 | 6874 | `		int isAbsolute = 0;` |
|       - | 6875 | `		SyBlob sName;` |
|     114 | 6876 | `		SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|       - | 6877 | `		/* Accept optional leading '\' for fully-qualified names */` |
|     114 | 6878 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|       7 | 6879 | `			isAbsolute = 1;` |
|       7 | 6880 | `			pGen->pIn++;` |
|       3 | 6881 | `		}` |
|     114 | 6882 | `		if( pGen->pIn >= pGen->pEnd \|\|` |
|     112 | 6883 | `			(pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       5 | 6884 | `			SyBlobRelease(&sName);` |
|       5 | 6885 | `			pToken = pGen->pIn;` |
|       5 | 6886 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6887 | `				pToken--;` |
|     ! 0 | 6888 | `			}` |
|       7 | 6889 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 6890 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 6891 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       5 | 6892 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6893 | `				return SXERR_ABORT;` |
|       - | 6894 | `			}` |
|       5 | 6895 | `			return SXERR_INVALID;` |
|       - | 6896 | `		}` |
|       - | 6897 | `		/* Collect namespace-qualified name: ID [\ ID]* */` |
|     110 | 6898 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|     110 | 6899 | `		pGen->pIn++;` |
|     168 | 6900 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|      62 | 6901 | `			&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       5 | 6902 | `			SyBlobAppend(&sName,"\\",1);` |
|       5 | 6903 | `			pGen->pIn++; /* Skip '\' separator */` |
|       5 | 6904 | `			SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       5 | 6905 | `			pGen->pIn++;` |
|       1 | 6906 | `		}` |
|       - | 6907 | `		/* Resolve through namespace/imports for non-absolute names */` |
|     110 | 6908 | `		if( !isAbsolute ){` |
|       - | 6909 | `			SyString sRaw;` |
|       - | 6910 | `			SyBlob sResolved;` |
|     104 | 6911 | `			SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     104 | 6912 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     104 | 6913 | `			GenStateResolveName(pGen,&sRaw,&sResolved);` |
|     155 | 6914 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     102 | 6915 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     104 | 6916 | `			SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     104 | 6917 | `			SyBlobRelease(&sResolved);` |
|      53 | 6918 | `		}else{` |
|       - | 6919 | `			/* Absolute name: use as-is without namespace prefix */` |
|      10 | 6920 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       6 | 6921 | `				(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|       7 | 6922 | `			SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sName));` |
|       - | 6923 | `		}` |
|     110 | 6924 | `		SyBlobRelease(&sName);` |
|     110 | 6925 | `		if( zDup == 0 ){` |
|     ! 0 | 6926 | `			goto Mem;` |
|       - | 6927 | `		}` |
|     110 | 6928 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     110 | 6929 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 6930 | `			goto Mem;` |
|       - | 6931 | `		}` |
|       - | 6932 | `		/* Check for '\|' (multi-catch separator) */` |
|     120 | 6933 | `		if( pGen->pIn < pGen->pEnd &&` |
|     108 | 6934 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      26 | 6935 | `			pGen->pIn->sData.nByte == 1 &&` |
|      24 | 6936 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      26 | 6937 | `			pGen->pIn++; /* Consume the '\|' */` |
|      26 | 6938 | `			continue;` |
|       - | 6939 | `		}` |
|      86 | 6940 | `		break;` |
|     ! 0 | 6941 | `	}` |
|     126 | 6942 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|      86 | 6943 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 6944 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 6945 | `			pToken = pGen->pIn;` |
|     ! 0 | 6946 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6947 | `				pToken--;` |
|     ! 0 | 6948 | `			}` |
|     ! 0 | 6949 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 6950 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 6951 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 6952 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6953 | `				return SXERR_ABORT;` |
|       - | 6954 | `			}` |
|     ! 0 | 6955 | `			return SXERR_INVALID;` |
|       - | 6956 | `	}` |
|      86 | 6957 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 6958 | `	/* Duplicate instance name */` |
|      86 | 6959 | `	pName = &pGen->pIn->sData;` |
|      86 | 6960 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      86 | 6961 | `	if( zDup == 0 ){` |
|     ! 0 | 6962 | `		goto Mem;` |
|       - | 6963 | `	}` |
|      86 | 6964 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|      86 | 6965 | `	pGen->pIn++;` |
|      86 | 6966 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 6967 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 6968 | `		pToken = pGen->pIn;` |
|     ! 0 | 6969 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6970 | `			pToken--;` |
|     ! 0 | 6971 | `		}` |
|     ! 0 | 6972 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 6973 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 6974 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 6975 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6976 | `			return SXERR_ABORT;` |
|       - | 6977 | `		}` |
|     ! 0 | 6978 | `		return SXERR_INVALID;` |
|       - | 6979 | `	}` |
|       - | 6980 | `	/* Compile the block */` |
|      86 | 6981 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 6982 | `	/* Create the catch block */` |
|      86 | 6983 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|      86 | 6984 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6985 | `		return SXERR_ABORT;` |
|       - | 6986 | `	}` |
|       - | 6987 | `	/* Swap bytecode container */` |
|      86 | 6988 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      86 | 6989 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 6990 | `	/* Compile the block */` |
|      86 | 6991 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 6992 | `	/* Fix forward jumps now the destination is resolved  */` |
|      86 | 6993 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6994 | `	/* Emit the DONE instruction */` |
|      86 | 6995 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 6996 | `	/* Leave the block */` |
|      86 | 6997 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6998 | `	/* Restore the default container */` |
|      86 | 6999 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 7000 | `	/* Install the catch block */` |
|      86 | 7001 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      86 | 7002 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7003 | `		goto Mem;` |
|       - | 7004 | `	}` |
|      86 | 7005 | `	return SXRET_OK;` |
|     ! 0 | 7006 | `Mem:` |
|     ! 0 | 7007 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 7008 | `	return SXERR_ABORT;` |
|      46 | 7009 |  |
|       - | 7010 | `/*` |
|       - | 7011 | ` * Compile a 'try' block.` |
|       - | 7012 | ` * A function using an exception should be in a "try" block.` |
|       - | 7013 | ` * If the exception does not trigger, the code will continue` |
|       - | 7014 | ` * as normal. However if the exception triggers, an exception` |
|       - | 7015 | ` * is "thrown".` |
|       - | 7016 | ` */` |
|      96 | 7017 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 | 7018 |  |
|       - | 7019 | `	ph7_exception *pException;` |
|      98 | 7020 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 7021 | `	GenBlock *pTry;` |
|       - | 7022 | `	sxu32 nJmpIdx;` |
|       - | 7023 | `	sxi32 rc;` |
|       - | 7024 | `	/* Create the exception container */` |
|      98 | 7025 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|      98 | 7026 | `	if( pException == 0 ){` |
|     ! 0 | 7027 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 7028 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 7029 | `		return SXERR_ABORT;` |
|       - | 7030 | `	}` |
|       - | 7031 | `	/* Zero the structure */` |
|      98 | 7032 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 7033 | `	/* Initialize fields */` |
|      98 | 7034 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|      98 | 7035 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      98 | 7036 | `	pException->iHasFinally = 0;` |
|      98 | 7037 | `	pException->iFinallyDone = 0;` |
|      98 | 7038 | `	pException->pVm = pGen->pVm;` |
|       - | 7039 | `	/* Create the try block */` |
|      98 | 7040 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      98 | 7041 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7042 | `		return SXERR_ABORT;` |
|       - | 7043 | `	}` |
|       - | 7044 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|      98 | 7045 | `	pTry->pUserData = pException;` |
|       - | 7046 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|      98 | 7047 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 7048 | `	/* Fix the jump later when the destination is resolved */` |
|      98 | 7049 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|      98 | 7050 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 7051 | `	/* Compile the block */` |
|      98 | 7052 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      98 | 7053 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 7054 | `		return SXERR_ABORT;` |
|       - | 7055 | `	}` |
|       - | 7056 | `	/* Fix forward jumps now the destination is resolved */` |
|      98 | 7057 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7058 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|      98 | 7059 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 7060 | `	/* Leave the block */` |
|      98 | 7061 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 7062 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|      98 | 7063 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      94 | 7064 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 7065 | `		/* Compile one or more catch blocks */` |
|      86 | 7066 | `		for(;;){` |
|     172 | 7067 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     134 | 7068 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      44 | 7069 | `					break;` |
|       - | 7070 | `			}` |
|      90 | 7071 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|      90 | 7072 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7073 | `				return SXERR_ABORT;` |
|       - | 7074 | `			}` |
|       2 | 7075 | `		}` |
|      42 | 7076 | `	}` |
|       - | 7077 | `	/* Compile optional finally block */` |
|      98 | 7078 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      46 | 7079 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 7080 | `		SySet *pInstrContainer;` |
|       - | 7081 | `		GenBlock *pFinBlock;` |
|      32 | 7082 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 7083 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      32 | 7084 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      32 | 7085 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7086 | `			return SXERR_ABORT;` |
|       - | 7087 | `		}` |
|       - | 7088 | `		/* Swap bytecode container */` |
|      32 | 7089 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 | 7090 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 7091 | `		/* Compile the finally body */` |
|      32 | 7092 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      32 | 7093 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7094 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 7095 | `			return SXERR_ABORT;` |
|       - | 7096 | `		}` |
|       - | 7097 | `		/* Fix forward jumps now the destination is resolved */` |
|      32 | 7098 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7099 | `		/* Emit DONE to terminate the finally block */` |
|      32 | 7100 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 7101 | `		/* Leave the block */` |
|      32 | 7102 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 7103 | `		/* Restore the default container */` |
|      32 | 7104 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 | 7105 | `		pException->iHasFinally = 1;` |
|      15 | 7106 | `	}` |
|       - | 7107 | `	/* Must have at least one catch or finally */` |
|      98 | 7108 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       7 | 7109 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 7110 | `			"Cannot use try without catch or finally");` |
|       7 | 7111 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7112 | `			return SXERR_ABORT;` |
|       - | 7113 | `		}` |
|       3 | 7114 | `	}` |
|      98 | 7115 | `	return SXRET_OK;` |
|      50 | 7116 |  |
|       - | 7117 | `/*` |
|       - | 7118 | ` * Compile a switch block.` |
|       - | 7119 | ` *  (See block-comment below for more information)` |
|       - | 7120 | ` */` |
|      98 | 7121 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 | 7122 |  |
|     100 | 7123 | `	sxi32 rc = SXRET_OK;` |
|     100 | 7124 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 7125 | `		/* Unexpected token */` |
|     ! 0 | 7126 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 7127 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7128 | `			return SXERR_ABORT;` |
|       - | 7129 | `		}` |
|     ! 0 | 7130 | `		pGen->pIn++;` |
|     ! 0 | 7131 | `	}` |
|     100 | 7132 | `	pGen->pIn++;` |
|       - | 7133 | `	/* First instruction to execute in this block. */` |
|     100 | 7134 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 7135 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 7136 | `	 * or the '}' token */` |
|     172 | 7137 | `	for(;;){` |
|     346 | 7138 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7139 | `			/* No more input to process */` |
|     ! 0 | 7140 | `			break;` |
|       - | 7141 | `		}` |
|     346 | 7142 | `		rc = SXRET_OK;` |
|     346 | 7143 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      68 | 7144 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      26 | 7145 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 7146 | `					/* Unexpected token */` |
|     ! 0 | 7147 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 7148 | `						&pGen->pIn->sData);` |
|     ! 0 | 7149 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7150 | `						return SXERR_ABORT;` |
|       - | 7151 | `					}` |
|       - | 7152 | `					/* FALL THROUGH */` |
|     ! 0 | 7153 | `				}` |
|      26 | 7154 | `				rc = SXERR_EOF;` |
|      26 | 7155 | `				break;` |
|       - | 7156 | `			}` |
|      23 | 7157 | `		}else{` |
|       - | 7158 | `			sxi32 nKwrd;` |
|       - | 7159 | `			/* Extract the keyword */` |
|     280 | 7160 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     280 | 7161 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      38 | 7162 | `				break;` |
|       - | 7163 | `			}` |
|     208 | 7164 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 7165 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 7166 | `					/* Unexpected token */` |
|     ! 0 | 7167 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 7168 | `						&pGen->pIn->sData);` |
|     ! 0 | 7169 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7170 | `						return SXERR_ABORT;` |
|       - | 7171 | `					}` |
|       - | 7172 | `					/* FALL THROUGH */` |
|     ! 0 | 7173 | `				}` |
|       - | 7174 | `				/* Block compiled */` |
|       3 | 7175 | `				break;` |
|       - | 7176 | `			}` |
|       - | 7177 | `		}` |
|       - | 7178 | `		/* Compile block */` |
|     248 | 7179 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     248 | 7180 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7181 | `			return SXERR_ABORT;` |
|       - | 7182 | `		}` |
|       2 | 7183 | `	}` |
|     100 | 7184 | `	return rc;` |
|      51 | 7185 |  |
|       - | 7186 | `/*` |
|       - | 7187 | ` * Compile a case eXpression.` |
|       - | 7188 | ` *  (See block-comment below for more information)` |
|       - | 7189 | ` */` |
|      80 | 7190 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 | 7191 |  |
|       - | 7192 | `	SySet *pInstrContainer;` |
|       - | 7193 | `	SyToken *pEnd,*pTmp;` |
|      82 | 7194 | `	sxi32 iNest = 0;` |
|       - | 7195 | `	sxi32 rc;` |
|       - | 7196 | `	/* Delimit the expression */` |
|      82 | 7197 | `	pEnd = pGen->pIn;` |
|     170 | 7198 | `	while( pEnd < pGen->pEnd ){` |
|     170 | 7199 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 7200 | `			/* Increment nesting level */` |
|       3 | 7201 | `			iNest++;` |
|     169 | 7202 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 7203 | `			/* Decrement nesting level */` |
|       3 | 7204 | `			iNest--;` |
|     167 | 7205 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      82 | 7206 | `			break;` |
|       - | 7207 | `		}` |
|      90 | 7208 | `		pEnd++;` |
|       2 | 7209 | `	}` |
|      82 | 7210 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 7211 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 7212 | `		if( rc == SXERR_ABORT ){` |
|       - | 7213 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7214 | `			return SXERR_ABORT;` |
|       - | 7215 | `		}` |
|     ! 0 | 7216 | `	}` |
|       - | 7217 | `	/* Swap token stream */` |
|      82 | 7218 | `	pTmp = pGen->pEnd;` |
|      82 | 7219 | `	pGen->pEnd = pEnd;` |
|      82 | 7220 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      82 | 7221 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      82 | 7222 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 7223 | `	/* Emit the done instruction */` |
|      82 | 7224 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      82 | 7225 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 7226 | `	/* Update token stream */` |
|      82 | 7227 | `	pGen->pIn  = pEnd;` |
|      82 | 7228 | `	pGen->pEnd = pTmp;` |
|      82 | 7229 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 7230 | `		return SXERR_ABORT;` |
|       - | 7231 | `	}` |
|      82 | 7232 | `	return SXRET_OK;` |
|      42 | 7233 |  |
|       - | 7234 | `/*` |
|       - | 7235 | ` * Compile the smart switch statement.` |
|       - | 7236 | ` * According to the PHP language reference manual` |
|       - | 7237 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 7238 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 7239 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 7240 | ` *  This is exactly what the switch statement is for.` |
|       - | 7241 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 7242 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 7243 | ` *  of the outer loop, use continue 2.` |
|       - | 7244 | ` *  Note that switch/case does loose comparision.` |
|       - | 7245 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 7246 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 7247 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 7248 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 7249 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 7250 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 7251 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 7252 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 7253 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 7254 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 7255 | ` *  list for the next case.` |
|       - | 7256 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 7257 | ` *  or floating-point numbers and strings.` |
|       - | 7258 | ` */` |
|      26 | 7259 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 | 7260 |  |
|       - | 7261 | `	GenBlock *pSwitchBlock;` |
|       - | 7262 | `	SyToken *pTmp,*pEnd;` |
|       - | 7263 | `	ph7_switch *pSwitch;` |
|       - | 7264 | `	sxu32 nToken;` |
|       - | 7265 | `	sxu32 nLine;` |
|       - | 7266 | `	sxi32 rc;` |
|      28 | 7267 | `	nLine = pGen->pIn->nLine;` |
|       - | 7268 | `	/* Jump the 'switch' keyword */` |
|      28 | 7269 | `	pGen->pIn++;` |
|      28 | 7270 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 7271 | `		/* Syntax error */` |
|     ! 0 | 7272 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 7273 | `		if( rc == SXERR_ABORT ){` |
|       - | 7274 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7275 | `			return SXERR_ABORT;` |
|       - | 7276 | `		}` |
|     ! 0 | 7277 | `		goto Synchronize;` |
|       - | 7278 | `	}` |
|       - | 7279 | `	/* Jump the left parenthesis '(' */` |
|      28 | 7280 | `	pGen->pIn++;` |
|      28 | 7281 | `	pEnd = 0; /* cc warning */` |
|       - | 7282 | `	/* Create the loop block */` |
|      41 | 7283 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      13 | 7284 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      28 | 7285 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7286 | `		return SXERR_ABORT;` |
|       - | 7287 | `	}` |
|       - | 7288 | `	/* Delimit the condition */` |
|      28 | 7289 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      28 | 7290 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 7291 | `		/* Empty expression */` |
|     ! 0 | 7292 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 7293 | `		if( rc == SXERR_ABORT ){` |
|       - | 7294 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7295 | `			return SXERR_ABORT;` |
|       - | 7296 | `		}` |
|     ! 0 | 7297 | `	}` |
|       - | 7298 | `	/* Swap token streams */` |
|      28 | 7299 | `	pTmp = pGen->pEnd;` |
|      28 | 7300 | `	pGen->pEnd = pEnd;` |
|       - | 7301 | `	/* Compile the expression */` |
|      28 | 7302 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      28 | 7303 | `	if( rc == SXERR_ABORT ){` |
|       - | 7304 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 7305 | `		return SXERR_ABORT;` |
|       - | 7306 | `	}` |
|       - | 7307 | `	/* Update token stream */` |
|      28 | 7308 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 7309 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 7310 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 7311 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7312 | `			return SXERR_ABORT;` |
|       - | 7313 | `		}` |
|     ! 0 | 7314 | `		pGen->pIn++;` |
|     ! 0 | 7315 | `	}` |
|      28 | 7316 | `	pGen->pIn  = &pEnd[1];` |
|      28 | 7317 | `	pGen->pEnd = pTmp;` |
|      28 | 7318 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      26 | 7319 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 7320 | `			pTmp = pGen->pIn;` |
|     ! 0 | 7321 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 7322 | `				pTmp--;` |
|     ! 0 | 7323 | `			}` |
|       - | 7324 | `			/* Unexpected token */` |
|     ! 0 | 7325 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 7326 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7327 | `				return SXERR_ABORT;` |
|       - | 7328 | `			}` |
|     ! 0 | 7329 | `			goto Synchronize;` |
|       - | 7330 | `	}` |
|       - | 7331 | `	/* Set the delimiter token */` |
|      28 | 7332 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 7333 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 7334 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 7335 | `	}else{` |
|      26 | 7336 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 7337 | `	}` |
|      28 | 7338 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 7339 | `	/* Create the switch blocks container */` |
|      28 | 7340 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      28 | 7341 | `	if( pSwitch == 0 ){` |
|       - | 7342 | `		/* Abort compilation */` |
|     ! 0 | 7343 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 7344 | `		return SXERR_ABORT;` |
|       - | 7345 | `	}` |
|       - | 7346 | `	/* Zero the structure */` |
|      28 | 7347 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 7348 | `	/* Initialize fields */` |
|      28 | 7349 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 7350 | `	/* Emit the switch instruction */` |
|      28 | 7351 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 7352 | `	/* Compile case blocks */` |
|      87 | 7353 | `	for(;;){` |
|       - | 7354 | `		sxu32 nKwrd;` |
|     102 | 7355 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7356 | `			/* No more input to process */` |
|     ! 0 | 7357 | `			break;` |
|       - | 7358 | `		}` |
|     102 | 7359 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 7360 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 7361 | `				/* Unexpected token */` |
|     ! 0 | 7362 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7363 | `					&pGen->pIn->sData);` |
|     ! 0 | 7364 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7365 | `					return SXERR_ABORT;` |
|       - | 7366 | `				}` |
|       - | 7367 | `				/* FALL THROUGH */` |
|     ! 0 | 7368 | `			}` |
|       - | 7369 | `			/* Block compiled */` |
|     ! 0 | 7370 | `			break;` |
|       - | 7371 | `		}` |
|       - | 7372 | `		/* Extract the keyword */` |
|     102 | 7373 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     102 | 7374 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 7375 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 7376 | `				/* Unexpected token */` |
|     ! 0 | 7377 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7378 | `					&pGen->pIn->sData);` |
|     ! 0 | 7379 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7380 | `					return SXERR_ABORT;` |
|       - | 7381 | `				}` |
|       - | 7382 | `				/* FALL THROUGH */` |
|     ! 0 | 7383 | `			}` |
|       - | 7384 | `			/* Block compiled */` |
|       3 | 7385 | `			break;` |
|       - | 7386 | `		}` |
|     100 | 7387 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 7388 | `			/*` |
|       - | 7389 | `			 * Accroding to the PHP language reference manual` |
|       - | 7390 | `			 *  A special case is the default case. This case matches anything` |
|       - | 7391 | `			 *  that wasn't matched by the other cases.` |
|       - | 7392 | `			 */` |
|      20 | 7393 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 7394 | `				/* Default case already compiled */` |
|     ! 0 | 7395 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 7396 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7397 | `					return SXERR_ABORT;` |
|       - | 7398 | `				}` |
|     ! 0 | 7399 | `			}` |
|      20 | 7400 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 7401 | `			/* Compile the default block */` |
|      20 | 7402 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      20 | 7403 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 7404 | `				return SXERR_ABORT;` |
|      20 | 7405 | `			}else if( rc == SXERR_EOF ){` |
|      18 | 7406 | `				break;` |
|       1 | 7407 | `			}` |
|      83 | 7408 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 7409 | `			ph7_case_expr sCase;` |
|       - | 7410 | `			/* Standard case block */` |
|      82 | 7411 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 7412 | `			/* initialize the structure */` |
|      82 | 7413 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 7414 | `			/* Compile the case expression */` |
|      82 | 7415 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      82 | 7416 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7417 | `				return SXERR_ABORT;` |
|       - | 7418 | `			}` |
|       - | 7419 | `			/* Compile the case block */` |
|      82 | 7420 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 7421 | `			/* Insert in the switch container */` |
|      82 | 7422 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      82 | 7423 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 7424 | `				return SXERR_ABORT;` |
|      82 | 7425 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 7426 | `				break;` |
|       - | 7427 | `			}` |
|      38 | 7428 | `		}else{` |
|       - | 7429 | `			/* Unexpected token */` |
|     ! 0 | 7430 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7431 | `				&pGen->pIn->sData);` |
|     ! 0 | 7432 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7433 | `				return SXERR_ABORT;` |
|       - | 7434 | `			}` |
|     ! 0 | 7435 | `			break;` |
|       - | 7436 | `		}` |
|       2 | 7437 | `	}` |
|       - | 7438 | `	/* Fix all jumps now the destination is resolved */` |
|      28 | 7439 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      28 | 7440 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7441 | `	/* Release the loop block */` |
|      28 | 7442 | `	GenStateLeaveBlock(pGen,0);` |
|      28 | 7443 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 7444 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      28 | 7445 | `		pGen->pIn++;` |
|      13 | 7446 | `	}` |
|       - | 7447 | `	/* Statement successfully compiled */` |
|      28 | 7448 | `	return SXRET_OK;` |
|     ! 0 | 7449 | `Synchronize:` |
|       - | 7450 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 7451 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 7452 | `		pGen->pIn++;` |
|     ! 0 | 7453 | `	}` |
|     ! 0 | 7454 | `	return SXRET_OK;` |
|      15 | 7455 |  |
|       - | 7456 | `/*` |
|       - | 7457 | ` * Generate bytecode for a given expression tree.` |
|       - | 7458 | ` * If something goes wrong while generating bytecode` |
|       - | 7459 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 7460 | ` * this function takes care of generating the appropriate` |
|       - | 7461 | ` * error message.` |
|       - | 7462 | ` */` |
| 2359038 | 7463 | `static sxi32 GenStateEmitExprCode(` |
|       - | 7464 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 7465 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 7466 | `	sxi32 iFlags /* Control flags */` |
|       - | 7467 | `	)` |
|       2 | 7468 |  |
|       - | 7469 | `	VmInstr *pInstr;` |
|       - | 7470 | `	sxu32 nJmpIdx;` |
| 2359040 | 7471 | `	sxi32 iP1 = 0;` |
| 2359040 | 7472 | `	sxu32 iP2 = 0;` |
| 2359040 | 7473 | `	void *p3  = 0;` |
|       - | 7474 | `	sxi32 iVmOp;` |
|       - | 7475 | `	sxi32 rc;` |
| 2359040 | 7476 | `	if( pNode->xCode ){` |
|       - | 7477 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 7478 | `		/* Compile node */` |
| 1462296 | 7479 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1462296 | 7480 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1462296 | 7481 | `		RE_SWAP_DELIMITER(pGen);` |
| 1462296 | 7482 | `		return rc;` |
|       - | 7483 | `	}` |
|  896746 | 7484 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 7485 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 7486 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 7487 | `		return SXERR_ABORT;` |
|       - | 7488 | `	}` |
|  896746 | 7489 | `	iVmOp = pNode->pOp->iVmOp;` |
|  896746 | 7490 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 7491 | `		sxu32 nJz,nJmp;` |
|       - | 7492 | `		/* Ternary operator require special handling */` |
|       - | 7493 | `		/* Phase#1: Compile the condition */` |
|    1882 | 7494 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    1882 | 7495 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7496 | `			return rc;` |
|       - | 7497 | `		}` |
|    1882 | 7498 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    1882 | 7499 | `		if( pNode->pLeft ){` |
|       - | 7500 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 7501 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1814 | 7502 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 7503 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1814 | 7504 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1814 | 7505 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7506 | `				return rc;` |
|       - | 7507 | `			}` |
|     908 | 7508 | `		}else{` |
|       - | 7509 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 7510 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 7511 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 7512 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 7513 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 7514 | `		}` |
|       - | 7515 | `		/* Phase#4: Emit the unconditional jump */` |
|    1882 | 7516 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 7517 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    1882 | 7518 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    1882 | 7519 | `		if( pInstr ){` |
|    1882 | 7520 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     940 | 7521 | `		}` |
|    1882 | 7522 | `		if( !pNode->pLeft ){` |
|       - | 7523 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 7524 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 7525 | `		}` |
|       - | 7526 | `		/* Phase#6: Compile the 'else' expression */` |
|    1882 | 7527 | `		if( pNode->pRight ){` |
|    1882 | 7528 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    1882 | 7529 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7530 | `				return rc;` |
|       - | 7531 | `			}` |
|     940 | 7532 | `		}` |
|    1882 | 7533 | `		if( nJmp > 0 ){` |
|       - | 7534 | `			/* Phase#7: Fix the unconditional jump */` |
|    1882 | 7535 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    1882 | 7536 | `			if( pInstr ){` |
|    1882 | 7537 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     940 | 7538 | `			}` |
|     940 | 7539 | `		}` |
|       - | 7540 | `		/* All done */` |
|    1882 | 7541 | `		return SXRET_OK;` |
|       - | 7542 | `	}` |
|       - | 7543 | `	/* Generate code for the left tree */` |
|  894866 | 7544 | `	if( pNode->pLeft ){` |
|  894830 | 7545 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 7546 | `			ph7_expr_node **apNode;` |
|  300526 | 7547 | `			int hasSpread = 0;` |
|       - | 7548 | `			sxi32 n;` |
|       - | 7549 | `			/* Recurse and generate bytecodes for function arguments */` |
|  300526 | 7550 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       - | 7551 | `			/* Read-only load */` |
|  300526 | 7552 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  600510 | 7553 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  299986 | 7554 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  299986 | 7555 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7556 | `					return rc;` |
|       - | 7557 | `				}` |
|  299986 | 7558 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 7559 | `					/* Emit spread opcode to unpack this array argument */` |
|      15 | 7560 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      15 | 7561 | `					hasSpread = 1;` |
|       7 | 7562 | `				}` |
|  149994 | 7563 | `			}` |
|       - | 7564 | `			/* Total number of given arguments */` |
|  300526 | 7565 | `			iP1 = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|  300526 | 7566 | `			iP2 = hasSpread;` |
|       - | 7567 | `			/* Remove stale flags now */` |
|  300526 | 7568 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  150262 | 7569 | `		}` |
|  894830 | 7570 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|  894830 | 7571 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7572 | `			return rc;` |
|       - | 7573 | `		}` |
|  894830 | 7574 | `		if( iVmOp == PH7_OP_CALL ){` |
|  300526 | 7575 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  300526 | 7576 | `			if( pInstr ){` |
|  300526 | 7577 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  299960 | 7578 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 7579 | `					sxu32 nQual;` |
|       - | 7580 | `					/* Prevent constant expansion */` |
|  299960 | 7581 | `					pInstr->iP1 = 0;` |
|       - | 7582 | `					/* Namespace-qualify the function name for CALL.` |
|       - | 7583 | `					 * Only check function imports — class imports must NOT` |
|       - | 7584 | ``					 * affect function resolution.  For `new Foo()`, the CALL`` |
|       - | 7585 | `					 * handler fires before NEW; we store the original literal` |
|       - | 7586 | `					 * index in the CALL instruction's iP2 so the NEW handler` |
|       - | 7587 | `					 * can recover the unqualified name and re-qualify with` |
|       - | 7588 | `					 * class imports. */ {` |
|  299960 | 7589 | `						int fromImport = 0;` |
|  299960 | 7590 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  299960 | 7591 | `						pInstr->iP2 = (sxi32)nQual;` |
|  299960 | 7592 | `						if( nQual != nOrig ){` |
|       - | 7593 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 7594 | `							 * NEW handler can recover the unqualified name. */` |
|      68 | 7595 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      68 | 7596 | `							if( !fromImport ){` |
|      58 | 7597 | `								p3 = (void *)1;` |
|      28 | 7598 | `							}` |
|      35 | 7599 | `						}` |
|       - | 7600 | `					}` |
|  150547 | 7601 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 7602 | `					/* Method call,flag that */` |
|     546 | 7603 | `					pInstr->iP2 = 1;` |
|     272 | 7604 | `				}` |
|  150264 | 7605 | `			}` |
|  744568 | 7606 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 7607 | `			ph7_expr_node **apNode;` |
|       - | 7608 | `			sxi32 n;` |
|       - | 7609 | `			/* Recurse and generate bytecodes for array index */` |
|   67310 | 7610 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  121450 | 7611 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   54142 | 7612 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   54142 | 7613 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7614 | `					return rc;` |
|       - | 7615 | `				}` |
|   27072 | 7616 | `			}` |
|   67310 | 7617 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   54142 | 7618 | `				iP1 = 1; /* Node have an index associated with it */` |
|   27070 | 7619 | `			}` |
|   67310 | 7620 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 7621 | `				/* Create an empty entry when the desired index is not found */` |
|   26580 | 7622 | `				iP2 = 1;` |
|   13291 | 7623 | `			}` |
|  560652 | 7624 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 7625 | `			/* POP the left node */` |
|      32 | 7626 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 7627 | `		}` |
|  447414 | 7628 | `	}` |
|  894866 | 7629 | `	rc = SXRET_OK;` |
|  894866 | 7630 | `	nJmpIdx = 0;` |
|       - | 7631 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 7632 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 7633 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  894866 | 7634 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     234 | 7635 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     234 | 7636 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     234 | 7637 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     234 | 7638 | `			int isSpecial = 0;` |
|     234 | 7639 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     150 | 7640 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     150 | 7641 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     161 | 7642 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     129 | 7643 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      66 | 7644 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      86 | 7645 | `					isSpecial = 1;` |
|      42 | 7646 | `				}` |
|      95 | 7647 | `			}` |
|     276 | 7648 | `			pInstr->iP1 = 0;` |
|     276 | 7649 | `			if( !isSpecial ){` |
|     108 | 7650 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      53 | 7651 | `			}` |
|       - | 7652 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 7653 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     192 | 7654 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     108 | 7655 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     108 | 7656 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 | 7657 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      44 | 7658 | `					return SXRET_OK;` |
|       - | 7659 | `				}` |
|      32 | 7660 | `			}` |
|      74 | 7661 | `		}` |
|     146 | 7662 | `	}` |
|       - | 7663 | `	/* Generate code for the right tree */` |
|  894790 | 7664 | `	if( pNode->pRight ){` |
|  467414 | 7665 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 7666 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    8286 | 7667 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  463272 | 7668 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 7669 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    2768 | 7670 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  457747 | 7671 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 7672 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      32 | 7673 | `			iVmOp = 0; /* No binary operator to emit */` |
|      32 | 7674 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  456349 | 7675 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  204008 | 7676 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  102003 | 7677 | `		}` |
|  467414 | 7678 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  467414 | 7679 | `		if( iVmOp == PH7_OP_STORE ){` |
|  201220 | 7680 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  201194 | 7681 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 7682 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 7683 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 7684 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 7685 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 7686 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 7687 | `				 */` |
|      54 | 7688 | `				iVmOp = 0;` |
|  201194 | 7689 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  201168 | 7690 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 7691 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   44662 | 7692 | `					iP2 = 1;` |
|   22332 | 7693 | `				}else{` |
|  156508 | 7694 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 7695 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   26542 | 7696 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   26542 | 7697 | `						iP1 = pInstr->iP1;` |
|   13272 | 7698 | `					}else{` |
|  129968 | 7699 | `						p3 = pInstr->p3;` |
|       - | 7700 | `					}` |
|       - | 7701 | `					/* POP the last dynamic load instruction */` |
|  156508 | 7702 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 7703 | `				}` |
|  100585 | 7704 | `			}` |
|  366805 | 7705 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      46 | 7706 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      46 | 7707 | `			if( pInstr ){` |
|      46 | 7708 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 7709 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 7710 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 7711 | `					 */` |
|      15 | 7712 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 7713 | `					iP1 = pInstr->iP1;` |
|      15 | 7714 | `					iP2 = pInstr->iP2;` |
|      15 | 7715 | `					p3  = pInstr->p3;` |
|       8 | 7716 | `				}else{` |
|      32 | 7717 | `					p3 = pInstr->p3;` |
|       - | 7718 | `				}` |
|      22 | 7719 | `			}` |
|      22 | 7720 | `		}` |
|  233706 | 7721 | `	}` |
|  894790 | 7722 | `	if( iVmOp > 0 ){` |
|  894678 | 7723 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   10722 | 7724 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 7725 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    7874 | 7726 | `				iP1 = 1;` |
|    3938 | 7727 | `			}` |
|  889318 | 7728 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 7729 | `			/* Namespace-qualify the class name for NEW */ {` |
|   13536 | 7730 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   13536 | 7731 | `				VmInstr *pCallInstr = 0;` |
|   13536 | 7732 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   13520 | 7733 | `					pCallInstr = pPeek;` |
|   13520 | 7734 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    6759 | 7735 | `				}` |
|   13536 | 7736 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|       - | 7737 | `					sxu32 nLitForClass;` |
|       - | 7738 | `					/* If the CALL handler already qualified the name using` |
|       - | 7739 | `					 * function imports, recover the original unqualified` |
|       - | 7740 | `					 * literal so we can re-qualify with class imports. */` |
|   13534 | 7741 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      32 | 7742 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      17 | 7743 | `					}else{` |
|   13504 | 7744 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 7745 | `					}` |
|   13534 | 7746 | `					pPeek->iP1 = 0;` |
|   13534 | 7747 | `					pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    6766 | 7748 | `				}` |
|       - | 7749 | `			}` |
|   13536 | 7750 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   13536 | 7751 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 7752 | `				VmInstr *pPrev;` |
|   13520 | 7753 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   13520 | 7754 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 7755 | `					/* Pop the call instruction */` |
|   13520 | 7756 | `					iP1 = pInstr->iP1;` |
|   13520 | 7757 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    6759 | 7758 | `				}` |
|    6761 | 7759 | `			}` |
|  877191 | 7760 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 7761 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 7762 | `			 * Namespace-qualify it, but skip self/static/parent. */` |
|      50 | 7763 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      50 | 7764 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      50 | 7765 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      50 | 7766 | `				int isSpecialIs = 0;` |
|      50 | 7767 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      46 | 7768 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      46 | 7769 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      49 | 7770 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      41 | 7771 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      23 | 7772 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 7773 | `						isSpecialIs = 1;` |
|       5 | 7774 | `					}` |
|      23 | 7775 | `				}` |
|      52 | 7776 | `				pInstr->iP1 = 0;` |
|      52 | 7777 | `				if( !isSpecialIs ){` |
|      38 | 7778 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      18 | 7779 | `				}` |
|      25 | 7780 | `			}` |
|  870403 | 7781 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 7782 | `			/* Prevent constant expansion for member/property names.` |
|       - | 7783 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 7784 | `			 * should not trigger constant lookup. */` |
|  100596 | 7785 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  100596 | 7786 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  100580 | 7787 | `				pInstr->iP1 = 0;` |
|   50289 | 7788 | `			}` |
|  100596 | 7789 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 7790 | `				/* Static member access,remember that */` |
|     158 | 7791 | `				iP1 = 1;` |
|     158 | 7792 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     158 | 7793 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      10 | 7794 | `					p3 = pInstr->p3;` |
|      10 | 7795 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       4 | 7796 | `				}` |
|      78 | 7797 | `			}` |
|   50297 | 7798 | `		}` |
|       - | 7799 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  894676 | 7800 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  447337 | 7801 | `	}` |
|  894788 | 7802 | `	if( nJmpIdx > 0 ){` |
|       - | 7803 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   11082 | 7804 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   11082 | 7805 | `		if( pInstr ){` |
|   11082 | 7806 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    5540 | 7807 | `		}` |
|    5540 | 7808 | `	}` |
|  894788 | 7809 | `	return rc;` |
| 1179503 | 7810 |  |
|       - | 7811 | `/*` |
|       - | 7812 | ` * Compile a PHP expression.` |
|       - | 7813 | ` * According to the PHP language reference manual:` |
|       - | 7814 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 7815 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 7816 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 7817 | ` *  is "anything that has a value".` |
|       - | 7818 | ` * If something goes wrong while compiling the expression,this` |
|       - | 7819 | ` * function takes care of generating the appropriate error` |
|       - | 7820 | ` * message.` |
|       - | 7821 | ` */` |
|  637236 | 7822 | `static sxi32 PH7_CompileExpr(` |
|       - | 7823 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 7824 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 7825 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 7826 | `	)` |
|       2 | 7827 |  |
|       - | 7828 | `	ph7_expr_node *pRoot;` |
|       - | 7829 | `	SySet sExprNode;` |
|       - | 7830 | `	SyToken *pEnd;` |
|       - | 7831 | `	sxi32 nExpr;` |
|       - | 7832 | `	sxi32 iNest;` |
|       - | 7833 | `	sxi32 rc;` |
|       - | 7834 | `	/* Initialize worker variables */` |
|  637238 | 7835 | `	nExpr = 0;` |
|  637238 | 7836 | `	pRoot = 0;` |
|  637238 | 7837 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  637238 | 7838 | `	SySetAlloc(&sExprNode,0x10);` |
|  637238 | 7839 | `	rc = SXRET_OK;` |
|       - | 7840 | `	/* Delimit the expression */` |
|  637238 | 7841 | `	pEnd = pGen->pIn;` |
|  637238 | 7842 | `	iNest = 0;` |
| 4295936 | 7843 | `	while( pEnd < pGen->pEnd ){` |
| 4073622 | 7844 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 7845 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     234 | 7846 | `			iNest++;` |
| 4073506 | 7847 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     242 | 7848 | `			iNest--;` |
| 4073270 | 7849 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  415128 | 7850 | `			if( iNest <= 0 ){` |
|  414924 | 7851 | `				break;` |
|       - | 7852 | `			}` |
|     102 | 7853 | `		}` |
| 3658700 | 7854 | `		pEnd++;` |
|       2 | 7855 | `	}` |
|  637238 | 7856 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   10682 | 7857 | `		SyToken *pEnd2 = pGen->pIn;` |
|   10682 | 7858 | `		iNest = 0;` |
|       - | 7859 | `		/* Stop at the first comma */` |
|   21386 | 7860 | `		while( pEnd2 < pEnd ){` |
|   10706 | 7861 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|       6 | 7862 | `				iNest++;` |
|   10704 | 7863 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|       6 | 7864 | `				iNest--;` |
|   10700 | 7865 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       5 | 7866 | `				if( iNest <= 0 ){` |
|     ! 0 | 7867 | `					break;` |
|       - | 7868 | `				}` |
|       2 | 7869 | `			}` |
|   10706 | 7870 | `			pEnd2++;` |
|       2 | 7871 | `		}` |
|   10682 | 7872 | `		if( pEnd2 <pEnd ){` |
|     ! 0 | 7873 | `			pEnd = pEnd2;` |
|     ! 0 | 7874 | `		}` |
|    5340 | 7875 | `	}` |
|  637238 | 7876 | `	if( pEnd > pGen->pIn ){` |
|  637228 | 7877 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 7878 | `		/* Swap delimiter */` |
|  637228 | 7879 | `		pGen->pEnd = pEnd;` |
|       - | 7880 | `		/* Try to get an expression tree */` |
|  637228 | 7881 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  637228 | 7882 | `		if( rc == SXRET_OK && pRoot ){` |
|  637068 | 7883 | `			rc = SXRET_OK;` |
|  637068 | 7884 | `			if( xTreeValidator ){` |
|       - | 7885 | `				/* Call the upper layer validator callback */` |
|   13726 | 7886 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    6862 | 7887 | `			}` |
|  637068 | 7888 | `			if( rc != SXERR_ABORT ){` |
|       - | 7889 | `				/* Generate code for the given tree */` |
|  637068 | 7890 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|  318533 | 7891 | `			}` |
|  637068 | 7892 | `			nExpr = 1;` |
|  318533 | 7893 | `		}` |
|       - | 7894 | `		/* Release the whole tree */` |
|  637228 | 7895 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 7896 | `		/* Synchronize token stream */` |
|  637228 | 7897 | `		pGen->pEnd = pTmp;` |
|  637228 | 7898 | `		pGen->pIn  = pEnd;` |
|  637228 | 7899 | `		if( rc == SXERR_ABORT ){` |
|       3 | 7900 | `			SySetRelease(&sExprNode);` |
|       3 | 7901 | `			return SXERR_ABORT;` |
|       - | 7902 | `		}` |
|  318612 | 7903 | `	}` |
|  637236 | 7904 | `	SySetRelease(&sExprNode);` |
|  637236 | 7905 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  318620 | 7906 |  |
|       - | 7907 | `/*` |
|       - | 7908 | ` * Return a pointer to the node construct handler associated` |
|       - | 7909 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 7910 | ` */` |
|  158706 | 7911 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 7912 |  |
|  158708 | 7913 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 7914 | `		/* Numeric literal: Either real or integer */` |
|   86722 | 7915 | `		return PH7_CompileNumLiteral;` |
|   71988 | 7916 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 7917 | `		/* Double quoted string */` |
|   15288 | 7918 | `		return PH7_CompileString;` |
|   56702 | 7919 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 7920 | `		/* Single quoted string */` |
|   56642 | 7921 | `		return PH7_CompileSimpleString;` |
|      62 | 7922 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 7923 | `		/* Heredoc */` |
|      28 | 7924 | `		return PH7_CompileHereDoc;` |
|      36 | 7925 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 7926 | `		/* Nowdoc */` |
|      29 | 7927 | `		return PH7_CompileNowDoc;` |
|       7 | 7928 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 7929 | `		/* Backtick quoted string */` |
|       5 | 7930 | `		return PH7_CompileBacktic;` |
|       - | 7931 | `	}` |
|       3 | 7932 | `	return 0;` |
|   79355 | 7933 |  |
|       - | 7934 | `/*` |
|       - | 7935 | ` * Compile an unset() statement.` |
|       - | 7936 | ` * unset($var, $arr[$key], ...);` |
|       - | 7937 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 7938 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 7939 | ` * parent array before extracting the element to unset.` |
|       - | 7940 | ` */` |
|    2572 | 7941 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       2 | 7942 |  |
|    2574 | 7943 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2574 | 7944 | `	sxu32 nIdx = 0;` |
|       - | 7945 | `	SyString sName;` |
|       - | 7946 | `	sxi32 rc;` |
|       - | 7947 | `	/* Jump the 'unset' keyword */` |
|    2574 | 7948 | `	pGen->pIn++;` |
|       - | 7949 | `	/* Save delimiter */` |
|    2574 | 7950 | `	pTmp = pGen->pEnd;` |
|       - | 7951 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2574 | 7952 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2574 | 7953 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 7954 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 7955 | `		SyToken *pClose;` |
|    2574 | 7956 | `		pGen->pIn++;   /* Skip '(' */` |
|    2574 | 7957 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2574 | 7958 | `		pEnd = pClose; /* Stop at ')' */` |
|    1286 | 7959 | `	}` |
|    2574 | 7960 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 7961 | `	/* Resolve the 'unset' builtin name once */` |
|    2574 | 7962 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     304 | 7963 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     304 | 7964 | `		if( pObj == 0 ){` |
|     ! 0 | 7965 | `			return SXERR_ABORT;` |
|       - | 7966 | `		}` |
|     304 | 7967 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     304 | 7968 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     151 | 7969 | `	}` |
|       - | 7970 | `	/* Compile each comma-separated argument */` |
|    8584 | 7971 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6012 | 7972 | `		if( pGen->pIn < pNext ){` |
|    6012 | 7973 | `			pGen->pEnd = pNext;` |
|    6012 | 7974 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 7975 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_STORE,0);` |
|    6012 | 7976 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7977 | `				return SXERR_ABORT;` |
|       - | 7978 | `			}` |
|    6012 | 7979 | `			if( rc != SXERR_EMPTY ){` |
|       - | 7980 | `				/* Emit call for this single argument */` |
|    6010 | 7981 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6010 | 7982 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,0,0);` |
|    6010 | 7983 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3004 | 7984 | `			}` |
|    3005 | 7985 | `		}` |
|       - | 7986 | `		/* Jump trailing commas */` |
|    9450 | 7987 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3440 | 7988 | `			pNext++;` |
|       2 | 7989 | `		}` |
|    6012 | 7990 | `		pGen->pIn = pNext;` |
|       2 | 7991 | `	}` |
|       - | 7992 | `	/* Skip past the closing ')' if present */` |
|    2574 | 7993 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2574 | 7994 | `		pGen->pIn++;` |
|    1286 | 7995 | `	}` |
|       - | 7996 | `	/* Restore token stream */` |
|    2574 | 7997 | `	pGen->pEnd = pTmp;` |
|    2574 | 7998 | `	return SXRET_OK;` |
|    1288 | 7999 |  |
|       - | 8000 | `/*` |
|       - | 8001 | ` * PHP Language construct table.` |
|       - | 8002 | ` */` |
|       - | 8003 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 8004 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 8005 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 8006 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 8007 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 8008 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 8009 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 8010 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 8011 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 8012 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 8013 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 8014 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 8015 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 8016 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 8017 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 8018 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 8019 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 8020 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 8021 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 8022 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 8023 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 8024 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 8025 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 8026 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 8027 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 8028 | `};` |
|       - | 8029 | `/*` |
|       - | 8030 | ` * Return a pointer to the statement handler routine associated` |
|       - | 8031 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 8032 | ` */` |
|  386508 | 8033 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 8034 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 8035 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 8036 | `	)` |
|       2 | 8037 |  |
|  386510 | 8038 | `	sxu32 n = 0;` |
| 1624954 | 8039 | `	for(;;){` |
| 3249910 | 8040 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   45270 | 8041 | `			break;` |
|       - | 8042 | `		}` |
| 3204642 | 8043 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  341242 | 8044 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 8045 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 8046 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 8047 | `					/* 'static' (class context),return null */` |
|     ! 0 | 8048 | `					return 0;` |
|       - | 8049 | `				}` |
|     ! 0 | 8050 | `			}` |
|       - | 8051 | `			/* Return a pointer to the handler.` |
|       - | 8052 | `			*/` |
|  341242 | 8053 | `			return aLangConstruct[n].xConstruct;` |
|       - | 8054 | `		}` |
| 2863402 | 8055 | `		n++;` |
|       2 | 8056 | `	}` |
|   45270 | 8057 | `	if( pLookahed ){` |
|   45270 | 8058 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    7906 | 8059 | `			return PH7_CompileClassInterface;` |
|   37366 | 8060 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   37162 | 8061 | `			return PH7_CompileClass;` |
|     206 | 8062 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      54 | 8063 | `			return PH7_CompileTrait;` |
|     152 | 8064 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      19 | 8065 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      18 | 8066 | `				return PH7_CompileAbstractClass;` |
|     136 | 8067 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 8068 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 8069 | `				return PH7_CompileFinalClass;` |
|       - | 8070 | `		}` |
|      67 | 8071 | `	}` |
|       - | 8072 | `	/* Not a language construct */` |
|     136 | 8073 | `	return 0;` |
|  193256 | 8074 |  |
|       - | 8075 | `/*` |
|       - | 8076 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 8077 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 8078 | ` */` |
|     134 | 8079 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 8080 |  |
|       - | 8081 | `	int rc;` |
|     136 | 8082 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     136 | 8083 | `	if( rc == FALSE ){` |
|      40 | 8084 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      38 | 8085 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 8086 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 8087 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 8088 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 8089 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 8090 | `			*/` |
|       - | 8091 | `			){` |
|      34 | 8092 | `				rc = TRUE;` |
|      16 | 8093 | `		}` |
|      20 | 8094 | `	}` |
|     136 | 8095 | `	return rc;` |
|       2 | 8096 |  |
|       - | 8097 | `/*` |
|       - | 8098 | ` * Compile a PHP chunk.` |
|       - | 8099 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 8100 | ` * takes care of generating the appropriate error message.` |
|       - | 8101 | ` */` |
|  519156 | 8102 | `static sxi32 GenStateCompileChunk(` |
|       - | 8103 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 8104 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 8105 | `	)` |
|       2 | 8106 |  |
|       - | 8107 | `	ProcLangConstruct xCons;` |
|       - | 8108 | `	sxi32 rc;` |
|  519158 | 8109 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  310143 | 8110 | `	for(;;){` |
|  620288 | 8111 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 8112 | `			/* No more input to process */` |
|   11394 | 8113 | `			break;` |
|       - | 8114 | `		}` |
|  608896 | 8115 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 8116 | `			/* Compile block */` |
|      12 | 8117 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      12 | 8118 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 8119 | `				break;` |
|       - | 8120 | `			}` |
|       7 | 8121 | `		}else{` |
|  608886 | 8122 | `			xCons = 0;` |
|  608886 | 8123 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  386510 | 8124 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 8125 | `				/* Try to extract a language construct handler */` |
|  386510 | 8126 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  386510 | 8127 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 8128 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 8129 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 8130 | `						&pGen->pIn->sData);` |
|       9 | 8131 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 8132 | `						break;` |
|       - | 8133 | `					}` |
|       - | 8134 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 8135 | `					 * this erroneous statement.` |
|       - | 8136 | `					 */` |
|       9 | 8137 | `					xCons = PH7_ErrorRecover;` |
|       4 | 8138 | `				}` |
|  415632 | 8139 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   38946 | 8140 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 8141 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 8142 | `				xCons = PH7_CompileLabel;` |
|      56 | 8143 | `			}` |
|  608886 | 8144 | `			if( xCons == 0 ){` |
|       - | 8145 | `				/* Assume an expression an try to compile it */` |
|  222392 | 8146 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  222392 | 8147 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 8148 | `					/* Pop l-value */` |
|  222264 | 8149 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  111131 | 8150 | `				}` |
|  111197 | 8151 | `			}else{` |
|       - | 8152 | `				/* Go compile the sucker */` |
|  386496 | 8153 | `				rc = xCons(&(*pGen));` |
|       - | 8154 | `			}` |
|  608886 | 8155 | `			if( rc == SXERR_ABORT ){` |
|       - | 8156 | `				/* Request to abort compilation */` |
|       3 | 8157 | `				break;` |
|       - | 8158 | `			}` |
|       - | 8159 | `		}` |
|       - | 8160 | `		/* Ignore trailing semi-colons ';' */` |
| 1008450 | 8161 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  399558 | 8162 | `			pGen->pIn++;` |
|       2 | 8163 | `		}` |
|  608894 | 8164 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 8165 | `			/* Compile a single statement and return */` |
|  507764 | 8166 | `			break;` |
|       - | 8167 | `		}` |
|       - | 8168 | `		/* LOOP ONE */` |
|       - | 8169 | `		/* LOOP TWO */` |
|       - | 8170 | `		/* LOOP THREE */` |
|       - | 8171 | `		/* LOOP FOUR */` |
|       2 | 8172 | `	}` |
|       - | 8173 | `	/* Return compilation status */` |
|  519158 | 8174 | `	return rc;` |
|       2 | 8175 |  |
|       - | 8176 | `/*` |
|       - | 8177 | ` * Compile a Raw PHP chunk.` |
|       - | 8178 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 8179 | ` * takes care of generating the appropriate error message.` |
|       - | 8180 | ` */` |
|   11396 | 8181 | `static sxi32 PH7_CompilePHP(` |
|       - | 8182 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 8183 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 8184 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 8185 | `	)` |
|       2 | 8186 |  |
|   11398 | 8187 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 8188 | `	sxi32 rc;` |
|       - | 8189 | `	/* Reset the token set */` |
|   11398 | 8190 | `	SySetReset(&(*pTokenSet));` |
|       - | 8191 | `	/* Mark as the default token set */` |
|   11398 | 8192 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 8193 | `	/* Advance the stream cursor */` |
|   11398 | 8194 | `	pGen->pRawIn++;` |
|       - | 8195 | `	/* Tokenize the PHP chunk first */` |
|   11398 | 8196 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 8197 | `	/* Point to the head and tail of the token stream. */` |
|   11398 | 8198 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   11398 | 8199 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   11398 | 8200 | `	if( is_expr ){` |
|     ! 0 | 8201 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 8202 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 8203 | `			/* A simple expression,compile it */` |
|     ! 0 | 8204 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 8205 | `		}` |
|       - | 8206 | `		/* Emit the DONE instruction */` |
|     ! 0 | 8207 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 8208 | `		return SXRET_OK;` |
|       - | 8209 | `	}` |
|   11398 | 8210 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 8211 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 8212 | `		/*` |
|       - | 8213 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 8214 | `		 * According to the PHP reference manual:` |
|       - | 8215 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 8216 | `		 *  immediately follow` |
|       - | 8217 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 8218 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 8219 | `		 * Symisc extension:` |
|       - | 8220 | `		 *   This short syntax works with all PHP opening` |
|       - | 8221 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 8222 | `		 *   only short tag.` |
|       - | 8223 | `		 */` |
|       - | 8224 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 8225 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 8226 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 8227 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 8228 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 8229 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 8230 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 8231 | `		}` |
|       3 | 8232 | `		return SXRET_OK;` |
|       - | 8233 | `	}` |
|       - | 8234 | `	/* Compile the PHP chunk */` |
|   11396 | 8235 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 8236 | `	/* Fix exceptions jumps */` |
|   11396 | 8237 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 8238 | `	/* Fix gotos now, the jump destination is resolved */` |
|   11396 | 8239 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 8240 | `		rc = SXERR_ABORT;` |
|       1 | 8241 | `	}` |
|       - | 8242 | `	/* Reset container */` |
|   11396 | 8243 | `	SySetReset(&pGen->aGoto);` |
|   11396 | 8244 | `	SySetReset(&pGen->aLabel);` |
|       - | 8245 | `	/* Compilation result */` |
|   11396 | 8246 | `	return rc;` |
|    5700 | 8247 |  |
|       - | 8248 | `/*` |
|       - | 8249 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 8250 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 8251 | ` * This is the only compile interface exported from this file.` |
|       - | 8252 | ` */` |
|   13444 | 8253 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 8254 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 8255 | `	SyString *pScript,  /* Script to compile */` |
|       - | 8256 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 8257 | `	)` |
|       2 | 8258 |  |
|       - | 8259 | `	SySet aPhpToken,aRawToken;` |
|       - | 8260 | `	ph7_gen_state *pCodeGen;` |
|       - | 8261 | `	ph7_value *pRawObj;` |
|       - | 8262 | `	sxu32 nObjIdx;` |
|       - | 8263 | `	sxi32 nRawObj;` |
|       - | 8264 | `	int is_expr;` |
|       - | 8265 | `	sxi32 rc;` |
|   13446 | 8266 | `	if( pScript->nByte < 1 ){` |
|       - | 8267 | `		/* Nothing to compile */` |
|     ! 0 | 8268 | `		return PH7_OK;` |
|       - | 8269 | `	}` |
|       - | 8270 | `	/* Initialize the tokens containers */` |
|   13446 | 8271 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   13446 | 8272 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   13446 | 8273 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   13446 | 8274 | `	is_expr = 0;` |
|   13446 | 8275 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 8276 | `		SyToken sTmp;` |
|       - | 8277 | `		/* PHP only: -*/` |
|    2652 | 8278 | `		sTmp.nLine = 1;` |
|    2652 | 8279 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2652 | 8280 | `		sTmp.pUserData = 0;` |
|    2652 | 8281 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2652 | 8282 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2652 | 8283 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 8284 | `			/* A simple PHP expression */` |
|     ! 0 | 8285 | `			is_expr = 1;` |
|     ! 0 | 8286 | `		}` |
|    1327 | 8287 | `	}else{` |
|       - | 8288 | `		/* Tokenize raw text */` |
|   10796 | 8289 | `		SySetAlloc(&aRawToken,32);` |
|   10796 | 8290 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 8291 | `	}` |
|   13446 | 8292 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 8293 | `	/* Process high-level tokens */` |
|   13446 | 8294 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   13446 | 8295 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   13446 | 8296 | `	rc = PH7_OK;` |
|   13446 | 8297 | `	if( is_expr ){` |
|       - | 8298 | `		/* Compile the expression */` |
|     ! 0 | 8299 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 8300 | `		goto cleanup;` |
|       - | 8301 | `	}` |
|   13446 | 8302 | `	nObjIdx = 0;` |
|       - | 8303 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 8304 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 8305 | `	 * preventing namespace bleeding across include()d files. */` |
|   13446 | 8306 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 8307 | `	/* Start the compilation process */` |
|   12123 | 8308 | `	for(;;){` |
|   35640 | 8309 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   13442 | 8310 | `			break; /* No more tokens to process */` |
|       - | 8311 | `		}` |
|   22200 | 8312 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 8313 | `			/* Compile the PHP chunk */` |
|   11398 | 8314 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   11398 | 8315 | `			if( rc == SXERR_ABORT ){` |
|       5 | 8316 | `				break;` |
|       - | 8317 | `			}` |
|   11394 | 8318 | `			continue;` |
|       - | 8319 | `		}` |
|       - | 8320 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   10804 | 8321 | `		nRawObj = 0;` |
|   21606 | 8322 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 8323 | `			/* Consume the raw chunk without any processing */` |
|   10804 | 8324 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   10804 | 8325 | `			if( pRawObj == 0 ){` |
|     ! 0 | 8326 | `				rc = SXERR_MEM;` |
|     ! 0 | 8327 | `				break;` |
|       - | 8328 | `			}` |
|       - | 8329 | `			/* Mark as constant and emit the load constant instruction */` |
|   10804 | 8330 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   10804 | 8331 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   10804 | 8332 | `			++nRawObj;` |
|   10804 | 8333 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 8334 | `		}` |
|   10804 | 8335 | `		if( nRawObj > 0 ){` |
|       - | 8336 | `			/* Emit the consume instruction */` |
|   10804 | 8337 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    5401 | 8338 | `		}` |
|    6724 | 8339 | `	}` |
|    6722 | 8340 | `cleanup:` |
|   13446 | 8341 | `	SySetRelease(&aRawToken);` |
|   13446 | 8342 | `	SySetRelease(&aPhpToken);` |
|   13446 | 8343 | `	return rc;` |
|    6724 | 8344 |  |
|       - | 8345 | `/*` |
|       - | 8346 | ` * Utility routines.Initialize the code generator.` |
|       - | 8347 | ` */` |
|    2622 | 8348 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 8349 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 8350 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 8351 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 8352 | `	)` |
|       2 | 8353 |  |
|    2624 | 8354 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 8355 | `	/* Zero the structure */` |
|    2624 | 8356 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 8357 | `	/* Initial state */` |
|    2624 | 8358 | `	pGen->pVm  = &(*pVm);` |
|    2624 | 8359 | `	pGen->xErr = xErr;` |
|    2624 | 8360 | `	pGen->pErrData = pErrData;` |
|    2624 | 8361 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2624 | 8362 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2624 | 8363 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2624 | 8364 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 8365 | `	/* Error log buffer */` |
|    2624 | 8366 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 8367 | `	/* General purpose working buffer */` |
|    2624 | 8368 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 8369 | `	/* Namespace state */` |
|    2624 | 8370 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2624 | 8371 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    2624 | 8372 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    2624 | 8373 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 8374 | `	/* Create the global scope */` |
|    2624 | 8375 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 8376 | `	/* Point to the global scope */` |
|    2624 | 8377 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2624 | 8378 | `	return SXRET_OK;` |
|       2 | 8379 |  |
|       - | 8380 | `/*` |
|       - | 8381 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 8382 | ` */` |
|   15798 | 8383 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 8384 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 8385 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 8386 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 8387 | `	)` |
|       2 | 8388 |  |
|   15800 | 8389 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 8390 | `	GenBlock *pBlock,*pParent;` |
|       - | 8391 | `	/* Reset state */` |
|   15800 | 8392 | `	SySetReset(&pGen->aLabel);` |
|   15800 | 8393 | `	SySetReset(&pGen->aGoto);` |
|   15800 | 8394 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   15800 | 8395 | `	SyBlobRelease(&pGen->sWorker);` |
|   15800 | 8396 | `	SyBlobRelease(&pGen->sNamespace);` |
|   15800 | 8397 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   15800 | 8398 | `	SyHashRelease(&pGen->hUseImports);` |
|   15800 | 8399 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   15800 | 8400 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   15800 | 8401 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   15800 | 8402 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   15800 | 8403 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 8404 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 8405 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 8406 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 8407 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 8408 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 8409 | `	 * number of unique names, which is acceptable. */` |
|       - | 8410 | `	/* Point to the global scope */` |
|   15800 | 8411 | `	pBlock = pGen->pCurrent;` |
|   15800 | 8412 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 8413 | `		pParent = pBlock->pParent;` |
|     ! 0 | 8414 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 8415 | `		pBlock = pParent;` |
|     ! 0 | 8416 | `	}` |
|   15800 | 8417 | `	pGen->xErr = xErr;` |
|   15800 | 8418 | `	pGen->pErrData = pErrData;` |
|   15800 | 8419 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   15800 | 8420 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   15800 | 8421 | `	pGen->pIn = pGen->pEnd = 0;` |
|   15800 | 8422 | `	pGen->nErr = 0;` |
|   15800 | 8423 | `	return SXRET_OK;` |
|       2 | 8424 |  |
|       - | 8425 | `/*` |
|       - | 8426 | ` * Generate a compile-time error message.` |
|       - | 8427 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 8428 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 8429 | ` * abort compilation immediately.` |
|       - | 8430 | ` */` |
|     468 | 8431 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 8432 |  |
|     470 | 8433 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     470 | 8434 | `	const char *zErr = "Error";` |
|       - | 8435 | `	SyString *pFile;` |
|       - | 8436 | `	va_list ap;` |
|       - | 8437 | `	sxi32 rc;` |
|       - | 8438 | `	/* Reset the working buffer */` |
|     470 | 8439 | `	SyBlobReset(pWorker);` |
|       - | 8440 | `	/* Peek the processed file path if available */` |
|     470 | 8441 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     470 | 8442 | `	if( nErrType == E_ERROR ){` |
|       - | 8443 | `		/* Increment the error counter */` |
|     424 | 8444 | `		pGen->nErr++;` |
|     424 | 8445 | `		if( pGen->nErr > 15 ){` |
|       - | 8446 | `			/* Error count limit reached */` |
|       5 | 8447 | `			if( pGen->xErr ){` |
|       5 | 8448 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 8449 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 8450 | `				if( pFile ){` |
|       5 | 8451 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 8452 | `				}` |
|       5 | 8453 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 8454 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 8455 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 8456 | `				}` |
|       2 | 8457 | `			}` |
|       - | 8458 | `			/* Abort immediately */` |
|       5 | 8459 | `			return SXERR_ABORT;` |
|       - | 8460 | `		}` |
|     209 | 8461 | `	}` |
|     466 | 8462 | `	if( pGen->xErr == 0 ){` |
|       - | 8463 | `		/* No available error consumer,return immediately */` |
|       3 | 8464 | `		return SXRET_OK;` |
|       - | 8465 | `	}` |
|     463 | 8466 | `	switch(nErrType){` |
|     417 | 8467 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      29 | 8468 | `	case E_WARNING: zErr = "Warning";     break;` |
|      11 | 8469 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 8470 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 8471 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 8472 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 8473 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 8474 | `	default:` |
|     ! 0 | 8475 | `		break;` |
|       - | 8476 | `	}` |
|     463 | 8477 | `	rc = SXRET_OK;` |
|       - | 8478 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     463 | 8479 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     463 | 8480 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     463 | 8481 | `	va_start(ap,zFormat);` |
|     463 | 8482 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     463 | 8483 | `	va_end(ap);` |
|     463 | 8484 | `	if( pFile ){` |
|     463 | 8485 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     231 | 8486 | `	}` |
|       - | 8487 | `	/* Append a new line */` |
|     463 | 8488 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     463 | 8489 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 8490 | `		/* Consume the generated error message */` |
|     463 | 8491 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     231 | 8492 | `	}` |
|     463 | 8493 | `	return rc;` |
|     236 | 8494 |  |
|       - | 8495 |  |
