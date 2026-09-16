/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __COMPILE_INT_H__
#define __COMPILE_INT_H__
/*
 * Internal contract shared by the compile*.c translation units (and no one
 * else): the code-generator block/jump/label structures, compilation flags
 * and the cross-unit GenState/PH7_Compile* prototypes. Include after
 * ph7int.h.
 */
/* Forward declaration */
typedef struct LangConstruct LangConstruct;
typedef struct JumpFixup     JumpFixup;
typedef struct Label         Label;
/* Block [i.e: set of statements] control flags */
#define GEN_BLOCK_LOOP        0x001    /* Loop block [i.e: for,while,...] */
#define GEN_BLOCK_PROTECTED   0x002    /* Protected block */
#define GEN_BLOCK_COND        0x004    /* Conditional block [i.e: if(condition){} ]*/
#define GEN_BLOCK_FUNC        0x008    /* Function body */
#define GEN_BLOCK_GLOBAL      0x010    /* Global block (always set)*/
#define GEN_BLOC_NESTED_FUNC  0x020    /* Nested function body */
#define GEN_BLOCK_EXPR        0x040    /* Expression */
#define GEN_BLOCK_STD         0x080    /* Standard block */
#define GEN_BLOCK_EXCEPTION   0x100    /* Exception block [i.e: try{ } }*/
#define GEN_BLOCK_SWITCH      0x200    /* Switch statement */
/*
 * Each label seen in the input is recorded in an instance
 * of the following structure.
 * A label is a target point [i.e: a jump destination] that is specified
 * by an identifier followed by a colon.
 * Example
 *  LABEL:
 *		echo "hello\n";
 */
struct Label
{
	ph7_vm_func *pFunc;  /* Compiled function where the label was declared.NULL otherwise */
	sxu32 nJumpDest;     /* Jump destination */
	SyString sName;      /* Label name */
	sxu32 nLine;         /* Line number this label occurs */
	sxu32 nLoopId;       /* Innermost loop/switch enclosing this label (0 = none) */
	sxu8 bRef;           /* True if the label was referenced */
};
/*
 * Compilation of some PHP constructs such as if, for, while, the logical or
 * (||) and logical and (&&) operators in expressions requires the
 * generation of forward jumps.
 * Since the destination PC target of these jumps isn't known when the jumps
 * are emitted, we record each forward jump in an instance of the following
 * structure. Those jumps are fixed later when the jump destination is resolved.
 */
struct JumpFixup
{
	sxi32 nJumpType;     /* Jump type. Either TRUE jump, FALSE jump or Unconditional jump */
	sxu32 nInstrIdx;     /* Instruction index to fix later when the jump destination is resolved. */
	/* The following fields are only used by the goto statement */
	SyString sLabel;    /* Label name */
	ph7_vm_func *pFunc; /* Compiled function inside which the goto was emitted. NULL otherwise */
	sxu32 nLine;        /* Track line number */
	sxu32 nLoopId;      /* Innermost loop/switch enclosing this goto (0 = none) */
};
/*
 * Each language construct is represented by an instance
 * of the following structure.
 */
struct LangConstruct
{
	sxu32 nID;                     /* Language construct ID [i.e: PH7_TKWRD_WHILE,PH7_TKWRD_FOR,PH7_TKWRD_IF...] */
	ProcLangConstruct xConstruct;  /* C function implementing the language construct */
};
/* Compilation flags */
#define PH7_COMPILE_SINGLE_STMT 0x001 /* Compile a single statement */
/* Token stream synchronization macros */
#define SWAP_TOKEN_STREAM(GEN,START,END)\
	pTmp  = GEN->pEnd;\
	pGen->pIn  = START;\
	pGen->pEnd = END
#define UPDATE_TOKEN_STREAM(GEN)\
	if( GEN->pIn < pTmp ){\
	    GEN->pIn++;\
	}\
	GEN->pEnd = pTmp
#define SWAP_DELIMITER(GEN,START,END)\
	pTmpIn  = GEN->pIn;\
	pTmpEnd = GEN->pEnd;\
	GEN->pIn = START;\
	GEN->pEnd = END
#define RE_SWAP_DELIMITER(GEN)\
	GEN->pIn  = pTmpIn;\
	GEN->pEnd = pTmpEnd
/* Flags related to expression compilation */
#define EXPR_FLAG_LOAD_IDX_STORE    0x001 /* Set the iP2 flag when dealing with the LOAD_IDX instruction */
#define EXPR_FLAG_RDONLY_LOAD       0x002 /* Read-only load, refer to the 'PH7_OP_LOAD' VM instruction for more information */
#define EXPR_FLAG_COMMA_STATEMENT   0x004 /* Treat comma expression as a single statement (used by class attributes) */
#define EXPR_FLAG_LOAD_IDX_ISSET    0x008 /* LOAD_IDX argument is the LHS of isset() — emit iP2=4 (offsetExists) */
#define EXPR_FLAG_LOAD_IDX_UNSET    0x010 /* LOAD_IDX argument is the LHS of unset() — emit iP2=5 (offsetUnset) */
#define EXPR_FLAG_LOAD_IDX_EMPTY    0x020 /* LOAD_IDX argument is the LHS of empty() — emit iP2=6 (offsetExists+offsetGet) */
#define EXPR_FLAG_MEMBER_WRITE      0x040 /* Sub-tree is the write lvalue of an assignment: tag a target
                                           * OP_MEMBER iP2=PH7_MEMBER_WRITE so the VM auto-creates a missing
                                           * property (e.g. `$o->arr[$k] = v`, `$o->p ??= v`). Propagated
                                           * from the precedence-18 lvalue through SUBSCRIPT to the base
                                           * member; stripped when descending into an intermediate `->`
                                           * container (the container is read, not the write target). */
#define EXPR_FLAG_RMW_LOAD          0x080 /* Operand is a read-modify-write target (`$x++`, `$x .= 'a'`,
                                           * `$x += 1`): php WARNS that the variable is undefined and then
                                           * creates it, unlike a plain `=` which is silent. Emits
                                           * OP_LOAD iP2=2 — warn-then-create. */
#define EXPR_FLAG_QUIET_VAR         0x100 /* A VARIABLE read in this sub-tree must not warn when the
                                           * variable is undefined (`isset`/`empty`, and the whole left
                                           * operand of `??` — php treats that chain as isset-context,
                                           * including the BASE of a subscript). Distinct from
                                           * EXPR_FLAG_LOAD_IDX_ISSET, which also switches LOAD_IDX to
                                           * offsetExists — wrong for `$o[$k] ?? d`, which needs the
                                           * offsetGet value. Emits OP_LOAD iP2=1. */
/* compile.c GenState substrate — shared with the other compile*.c units */
PH7_PRIVATE sxi32 GenStateEnterBlock(ph7_gen_state *pGen,sxi32 iType,sxu32 nFirstInstr,void *pUserData,GenBlock **ppBlock);
PH7_PRIVATE sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock);
PH7_PRIVATE sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest);
PH7_PRIVATE sxi32 PH7_CompileExpr(ph7_gen_state *pGen,sxi32 iFlags,sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *));
PH7_PRIVATE sxi32 GenStateCompileArrayEntry(ph7_gen_state *pGen,SyToken *pIn,SyToken *pEnd,sxi32 iFlags,sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *));
PH7_PRIVATE sxi32 GenStateCompileFuncBody(ph7_gen_state *pGen,ph7_vm_func *pFunc);
PH7_PRIVATE sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx);
PH7_PRIVATE sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut);
PH7_PRIVATE sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut);
PH7_PRIVATE void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut);
PH7_PRIVATE void GenStateSetPendingDoc(ph7_gen_state *pGen);
PH7_PRIVATE void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);
PH7_PRIVATE void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);
PH7_PRIVATE int GenStateUnconditionalTopLevel(ph7_gen_state *pGen);
PH7_PRIVATE int GenStateIsReservedConstant(SyString *pName);
PH7_PRIVATE void *GenStateAttachStrictFlag(ph7_gen_state *pGen,void *p3);
PH7_PRIVATE sxi32 GenStateParseReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc);
PH7_PRIVATE sxi32 GenStateParseUnionTypeDecl(ph7_gen_state *pGen,sxu32 *pnType,SyString *pClass,SySet *pAlts,
	sxi32 *piTypeFlags,SyString *pTypeText,int iNullableFlag,int iUnionFlag,int bAllowVoid,sxu32 nLine);
PH7_PRIVATE int SyMemcmpNoCase(const char *zA,const char *zB,sxu32 n);
/*
 * Stack-scratch size for stripping PHP 7.4 numeric separators. A typical
 * literal (INT64_MAX decimal is 19 digits, binary 64-bit with per-nibble
 * separators is ~80 chars) fits comfortably, so the fast path never touches
 * the heap. The language itself imposes no upper bound on the length of a
 * well-formed literal — the stripper falls back to a VM-allocator buffer
 * for anything larger, so correctness is preserved even for pathological
 * inputs like a thousand-digit number.
 */
#define GEN_NUM_SCRATCH 128
PH7_PRIVATE sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx);
PH7_PRIVATE sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx);
PH7_PRIVATE sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen,SyToken *pToken);
PH7_PRIVATE sxi32 GenStateStripNumericSeparators(SyMemBackend *pAlloc,const SyString *pToken,
	char *zScratch,sxu32 nScratch,SyString *pOut,char **pzAlloc);
PH7_PRIVATE SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd);
PH7_PRIVATE sxi32 GenStateCompileChunk(ph7_gen_state *pGen,sxi32 iFlags);
PH7_PRIVATE ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx);
PH7_PRIVATE sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag);
/* compile_class.c — cross-unit prototypes */
PH7_PRIVATE sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileClass(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileTrait(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileEnum(ph7_gen_state *pGen);
PH7_PRIVATE int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd);
PH7_PRIVATE int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd);
PH7_PRIVATE int GenStateIsReadonly(SyToken *pTok);
PH7_PRIVATE sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok);
PH7_PRIVATE sxi32 GenStateSetVisFlag(sxi32 nKw);
PH7_PRIVATE sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr);
PH7_PRIVATE sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,
	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);
PH7_PRIVATE sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn);
PH7_PRIVATE sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft);
PH7_PRIVATE sxi32 PH7_CompileBlock(ph7_gen_state *pGen,sxi32 nKeywordEnd);
/* compile_func.c — cross-unit prototypes */
PH7_PRIVATE sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);
PH7_PRIVATE sxi32 PH7_CompileFunction(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 GenStateGuardFuncRedeclaration(ph7_gen_state *pGen,ph7_vm_func *pFunc);
/* compile_stmt.c — cross-unit prototypes */
PH7_PRIVATE GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount);
PH7_PRIVATE sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx);
PH7_PRIVATE const char * TokenTypeName(sxu32 nType);
PH7_PRIVATE sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport);
PH7_PRIVATE int PH7_GenStateInitHasCallExpr(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileConstant(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileContinue(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileBreak(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileLabel(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileGoto(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileWhile(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileFor(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileForeach(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileIf(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileGlobal(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileReturn(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileHalt(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileEcho(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileStatic(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileVar(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileNamespace(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileUse(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileDeclare(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileThrow(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileTry(ph7_gen_state *pGen);
PH7_PRIVATE sxi32 PH7_CompileSwitch(ph7_gen_state *pGen);
#endif /* __COMPILE_INT_H__ */
