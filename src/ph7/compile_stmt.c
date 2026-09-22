/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include "compile_int.h"
/*
 * Section:
 *    Statement compilation: const, continue/break, labels and goto, blocks,
 *    loops (while/do/for/foreach), if, global, return, yield, halt, echo,
 *    static, var, namespace/use/declare, throw, try/catch/finally, switch.
 * Status:
 *    Stable.
 */
/*
 * Compile the 'const' statement.
 * According to the PHP language reference
 *  A constant is an identifier (name) for a simple value. As the name suggests, that value
 *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).
 *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.
 *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts
 *  with a letter or underscore, followed by any number of letters, numbers, or underscores.
 *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*
 *  Syntax
 *  You can define a constant by using the define()-function or by using the const keyword outside
 *  a class definition. Once a constant is defined, it can never be changed or undefined.
 *  You can get the value of a constant by simply specifying its name. Unlike with variables
 *  you should not prepend a constant with a $. You can also use the function constant() to read
 *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()
 *  to get a list of all defined constants.
 *
 * Symisc eXtension.
 *  PH7 allow any complex expression to be associated with the constant while the zend engine
 *  would allow only simple scalar value.
 *  Example
 *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine
 *    Refer to the official documentation for more information on this feature.
 */
PH7_PRIVATE sxi32 PH7_CompileConstant(ph7_gen_state *pGen)
{
	SySet *pConsCode,*pInstrContainer;
	sxu32 nLineLocal;
	SyString *pName;
	sxi32 rc;
	/* php forbids attributes on a comma-separated const list. Snapshot whether the
	 * statement carries any now, before the first constant consumes them. */
	int bHadAttrs = SySetUsed(&pGen->aPendingAttrs) > 0;
	/* php attributes every const-statement compile error to the `const` keyword's
	 * line, not the offending list element's own line (`const A=1,\nB=strlen()` blames
	 * line 1). Capture it once here, before jumping the keyword. */
	nLineLocal = pGen->pIn->nLine;
	pGen->pIn++; /* Jump the 'const' keyword */
	/* php allows a single `const` statement to declare several constants at once
	 * (`const A = 1, B = 2;`). Loop over the comma-separated name = value pairs;
	 * PH7_CompileExpr(EXPR_FLAG_COMMA_STATEMENT) stops each value at the first
	 * top-level comma so the next pair starts cleanly. */
Loop:
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_SSTR|PH7_TK_DSTR|PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
		/* Invalid constant name */
		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"identifier");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Peek constant name */
	pName = &pGen->pIn->sData;
	/* Make sure the constant name isn't reserved */
	if( GenStateIsReservedConstant(pName) ){
		/* Reserved constant */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Cannot redeclare constant '%z'",pName);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	pGen->pIn++;
	if(pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){
		/* Invalid statement*/
		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"=\"");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	pGen->pIn++; /*Jump the equal sign */
	/* php: a closure in a constant expression must be `static function`; a
	 * non-static closure and any arrow fn are compile-time fatals with distinct
	 * messages. Checked ahead of the call scan (it skips closure bodies). */
	{
		int iClo = PH7_GenStateInitClosureError(pGen);
		if( iClo ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,
				iClo == 2 ? "Closures in constant expressions must be static"
				          : "Constant expression contains invalid operations");
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
	}
	/* php: a constant expression may not CALL anything --
	 * "Constant expression contains invalid operations". PHL used to evaluate the
	 * call happily, so `const X = strlen("ab");` defined X as 2. */
	if( PH7_GenStateInitHasCallExpr(pGen) ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,
			"Constant expression contains invalid operations");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Allocate a new constant value container */
	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));
	if( pConsCode == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");
		return SXERR_ABORT;
	}
	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));
	/* Swap bytecode container */
	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);
	/* Compile constant value. php: a stray token after `const X = EXPR` is
	 * `... expecting "," or ";"` (const supports a comma-separated list).
	 * EXPR_FLAG_COMMA_STATEMENT stops this value at the first top-level comma so a
	 * following declaration is left for the loop below. */
	{
		const char *zSaveConst = pGen->zClauseCloser;
		pGen->zClauseCloser = "\",\" or \";\"";
		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);
		pGen->zClauseCloser = zSaveConst;
	}
	/* Emit the done instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);
	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	if( rc == SXERR_ABORT ){
		/* Don't worry about freeing memory, everything will be released shortly */
		return SXERR_ABORT;
	}
	/* php parse-errors an empty initializer (`const A = ;`, or a `,B=` list hole);
	 * the class-const path rejects it too. Reject loudly rather than silently
	 * defining a NULL constant. (php's error KIND -- a parse error -- differs from
	 * PHL's compile fatal, the recursive-descent-vs-bison family; both refuse.) */
	if( rc == SXERR_EMPTY && PH7_GenCompileError(pGen,E_ERROR,nLineLocal,
			"Empty constant '%z' value",pName) == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	SySetSetUserData(pConsCode,pGen->pVm);
	/* Register the constant with namespace-qualified name */
	{
		SyBlob sFQN;
		SyString sFQNStr;
		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);
		GenStateBuildFQN(pGen,pName,&sFQN);
		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));
		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,
			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);
		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){
			/* php 8.5: attributes on `const` statements — attach the pending
			 * groups to the registered constant record for Reflection. */
			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,
				SyBlobData(&sFQN),SyBlobLength(&sFQN));
			if( pCEntry ){
				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;
				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){
					SyBlobRelease(&sFQN);
					return SXERR_ABORT;
				}
			}
		}
		SyBlobRelease(&sFQN);
	}
	if( rc != SXRET_OK ){
		SySetRelease(pConsCode);
		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);
	}
	/* Another declaration in the same statement: `const A = 1, B = 2;`. */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /* ',' */) ){
		if( bHadAttrs ){
			/* php compile-fatals `#[Attr] const A = 1, B = 2;` outright. */
			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,
				"Cannot apply attributes to multiple constants at once");
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
		pGen->pIn++; /* Jump the comma */
		goto Loop;
	}
	return SXRET_OK;
Synchronize:
	/* Synchronize with the next top-level semicolon and avoid compiling this
	 * erroneous statement. BRACE-aware (only `{`...`}`): a rejected closure
	 * initializer's body holds inner `;` that are not statement terminators, so
	 * without this its `;` and `}` dangle into a spurious second error where php
	 * halts at the first fatal. Parentheses and brackets are deliberately NOT
	 * tracked -- a lone unbalanced `(`/`[` in erroneous input must not swallow the
	 * following statements (which would drop later error reports); a `{` never
	 * appears in a valid global-const initializer except as a closure body. */
	{
		int iBrace = 0;
		while( pGen->pIn < pGen->pEnd ){
			if( iBrace == 0 && (pGen->pIn->nType & PH7_TK_SEMI) ){
				break;
			}
			if( pGen->pIn->nType & PH7_TK_OCB ){
				iBrace++;
			}else if( pGen->pIn->nType & PH7_TK_CCB ){
				if( iBrace > 0 ){ iBrace--; }
			}
			pGen->pIn++;
		}
	}
	return SXRET_OK;
}
/*
 * Compile the 'continue' statement.
 * According to the PHP language reference
 *  continue is used within looping structures to skip the rest of the current loop iteration
 *  and continue execution at the condition evaluation and then the beginning of the next
 *  iteration.
 *  Note: Note that in PHP the switch statement is considered a looping structure for
 *  the purposes of continue.
 *  continue accepts an optional numeric argument which tells it how many levels
 *  of enclosing loops it should skip to the end of.
 *  Note:
 *   continue 0; and continue 1; is the same as running continue;.
 */
/*
 * Emit PH7_OP_POP_EXCEPTION for each exception block between the current
 * block and the target loop block. This ensures finally blocks run when
 * break/continue crosses a try boundary.
 *
 * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):
 * those are compiled into separate bytecode containers executed via VmLocalExec,
 * so we must not emit POP_EXCEPTION for the parent try from inside them.
 */
static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)
{
	GenBlock *pBlock = pGen->pCurrent;
	int nInlineTry = 0;
	while( pBlock && pBlock != pTarget ){
		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){
			if( pBlock->pUserData ){
				/* A try block with an exception context. In a generator its catch/finally
				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that
				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.
				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */
				if( pGen->bInGenerator ){
					nInlineTry++;
				}else{
					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);
				}
			}else{
				/* A catch/finally block compiled into a separate bytecode container
				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */
				break;
			}
		}
		pBlock = pBlock->pParent;
	}
	return nInlineTry;
}
PH7_PRIVATE sxi32 PH7_CompileContinue(ph7_gen_state *pGen)
{
	GenBlock *pLoop; /* Target loop */
	sxi32 iLevel;    /* How many nesting loop to skip */
	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */
	sxu32 nLineLocal;
	sxi32 rc;
	iRawLevel = 1;
	nLineLocal = pGen->pIn->nLine;
	iLevel = 0;
	/* Jump the 'continue' keyword */
	pGen->pIn++;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){
		/* optional numeric argument which tells us how many levels
		 * of enclosing loops we should skip to the end of.
		 */
		char zScratch[GEN_NUM_SCRATCH];
		char *zAlloc = 0;
		SyString sNum;
		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		if( rc == SXRET_OK ){
			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,
				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);
			if( rc != SXRET_OK ){
				return SXERR_ABORT;
			}
			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);
			iRawLevel = iLevel;
			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }
		}
		if( iLevel < 2 ){
			iLevel = 0;
		}
		pGen->pIn++; /* Jump the optional numeric argument */
	}
	/* php rejects a non-positive level outright, before asking where it lands. */
	if( iRawLevel < 1 ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,
			"'continue' operator accepts only positive integers");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* Point to the target loop */
	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);
	if( pLoop == 0 ){
		/* Same split php makes for `break`: no loop at all keeps the not-in-context
		 * wording, too FEW loops is `Cannot 'continue' N levels`. */
		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,
				"Cannot 'continue' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");
		}else{
			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");
		}
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
	}else{
		sxu32 nInstrIdx = 0;
		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */
		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);
		/* ROOT C: in a generator, a break/continue crossing inline trys must run their
		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */
		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;
		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){
			/* `continue` inside a switch acts like `break` — which is almost never what
			 * the author meant, so php 7.3+ says so at compile time. The generated jump
			 * is unchanged; only the diagnostic was missing. An explicit level
			 * (`continue 2`) targets the enclosing loop and stays silent. */
			if( iLevel < 1 ){
				PH7_GenCompileError(&(*pGen),E_WARNING,nLineLocal,
					"\"continue\" targeting switch is equivalent to \"break\"."
					" Did you mean to use \"continue 2\"?");
			}
			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);
			if( rc == SXRET_OK ){
				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);
			}
		}else{
			/* Emit the unconditional jump to the beginning of the target loop */
			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);
			if( pLoop->bPostContinue == TRUE ){
				JumpFixup sJumpFix;
				/* Post-continue */
				sJumpFix.nJumpType = PH7_OP_JMP;
				sJumpFix.nInstrIdx = nInstrIdx;
				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);
			}
		}
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		/* Not so fatal,emit a warning only */
		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");
	}
	/* Statement successfully compiled */
	return SXRET_OK;
}
/*
 * Compile the 'break' statement.
 * According to the PHP language reference
 *  break ends execution of the current for, foreach, while, do-while or switch
 *  structure.
 *  break accepts an optional numeric argument which tells it how many nested
 *  enclosing structures are to be broken out of.
 */
PH7_PRIVATE sxi32 PH7_CompileBreak(ph7_gen_state *pGen)
{
	GenBlock *pLoop; /* Target loop */
	sxi32 iLevel;    /* How many nesting loop to skip */
	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */
	sxi32 rc;
	iLevel = 0;
	iRawLevel = 1;
	/* Jump the 'break' keyword */
	pGen->pIn++;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){
		/* optional numeric argument which tells us how many levels
		 * of enclosing loops we should skip to the end of.
		 */
		char zScratch[GEN_NUM_SCRATCH];
		char *zAlloc = 0;
		SyString sNum;
		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		if( rc == SXRET_OK ){
			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,
				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);
			if( rc != SXRET_OK ){
				return SXERR_ABORT;
			}
			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);
			iRawLevel = iLevel;
			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }
		}
		if( iLevel < 2 ){
			iLevel = 0;
		}
		pGen->pIn++; /* Jump the optional numeric argument */
	}
	/* php rejects a non-positive level outright, before asking where it lands. */
	if( iRawLevel < 1 ){
		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
			"'break' operator accepts only positive integers");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto BreakLevelDone;
	}
	/* Extract the target loop */
	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);
	if( pLoop == 0 ){
		/* php distinguishes "there is no loop at all here" from "there is one, but
		 * not N of them": the first keeps the not-in-context wording, the second is
		 * `Cannot 'break' N levels`. */
		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){
			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
				"Cannot 'break' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");
		}else{
			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");
		}
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
	}else{
		sxu32 nInstrIdx;
		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */
		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);
		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */
		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);
		if( rc == SXRET_OK ){
			/* Fix the jump later when the jump destination is resolved */
			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);
		}
	}
BreakLevelDone:
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		/* Not so fatal,emit a warning only */
		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");
	}
	/* Statement successfully compiled */
	return SXRET_OK;
}
/*
 * Compile or record a label.
 *  A label is a target point that is specified by an identifier followed by a colon.
 * Example
 *  goto LABEL;
 *   echo 'Foo';
 *  LABEL:
 *   echo 'Bar';
 */
PH7_PRIVATE sxi32 PH7_CompileLabel(ph7_gen_state *pGen)
{
	GenBlock *pBlock;
	Label sLabel;
	/* php places NO restriction on where a label may be DEFINED — inside a loop, a switch
	 * or a try{} is all fine. The only rule is on the jump: you may not goto INTO a loop
	 * or switch from outside it, which is checked once the labels are all known (see
	 * GenStateFixJumps). Record the loop this label sits in so that check can run. */
	{
		SyString *pTarget = &pGen->pIn->sData;
		char *zDup;
		/* Initialize label fields */
		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);
		/* Duplicate label name */
		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);
		if( zDup == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");
			return SXERR_ABORT;
		}
		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);
		sLabel.bRef  = FALSE;
		sLabel.nLine = pGen->pIn->nLine;
		sLabel.nLoopId = pGen->nCurLoopId;
		pBlock = pGen->pCurrent;
		while( pBlock ){
			if( pBlock->iFlags & (GEN_BLOCK_FUNC|GEN_BLOCK_EXCEPTION) ){
				break;
			}
			/* Point to the upper block */
			pBlock = pBlock->pParent;
		}
		if( pBlock ){
			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;
		}else{
			sLabel.pFunc = 0;
		}
		/* Insert in label set */
		SySetPut(&pGen->aLabel,(const void *)&sLabel);
	}
	pGen->pIn += 2; /* Jump the label name and the semi-colon*/
	return SXRET_OK;
}
/*
 * Compile the so hated 'goto' statement.
 * You've probably been taught that gotos are bad, but this sort
 * of rewriting  happens all the time, in fact every time you run
 * a compiler it has to do this.
 * According to the PHP language reference manual
 *   The goto operator can be used to jump to another section in the program.
 *   The target point is specified by a label followed by a colon, and the instruction
 *   is given as goto followed by the desired target label. This is not a full unrestricted goto.
 *   The target label must be within the same file and context, meaning that you cannot jump out
 *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop
 *   or switch structure. You may jump out of these, and a common use is to use a goto in place
 *   of a multi-level break
 */
PH7_PRIVATE sxi32 PH7_CompileGoto(ph7_gen_state *pGen)
{
	JumpFixup sJump;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'goto' keyword */
	if( pGen->pIn >= pGen->pEnd ){
		/* Missing label */
		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	if( (pGen->pIn->nType & (PH7_TK_KEYWORD|PH7_TK_ID)) == 0 ){
		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
	}else{
		SyString *pTarget = &pGen->pIn->sData;
		GenBlock *pBlock;
		char *zDup;
		/* Prepare the jump destination */
		sJump.nJumpType = PH7_OP_JMP;
		sJump.nLine = pGen->pIn->nLine;
		/* Duplicate label name */
		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);
		if( zDup == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");
			return SXERR_ABORT;
		}
		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);
		/* The loop/switch this goto sits in, for the "goto into a loop" check later. */
		sJump.nLoopId = pGen->nCurLoopId;
		/* A goto inside a try{}/catch{} is legal php (jumping OUT of the block is fine);
		 * only the owning function matters here, since a goto may not cross functions. */
		pBlock = pGen->pCurrent;
		while( pBlock ){
			if( pBlock->iFlags & GEN_BLOCK_FUNC ){
				break;
			}
			/* Point to the upper block */
			pBlock = pBlock->pParent;
		}
		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){
			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;
		}else{
			sJump.pFunc = 0;
		}
		/* Emit the unconditional jump */
		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){
			SySetPut(&pGen->aGoto,(const void *)&sJump);
		}
	}
	pGen->pIn++; /* Jump the label name */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");
	}
	/* Statement successfully compiled */
	return SXRET_OK;
}
/*
 * Point to the next PHP chunk that will be processed shortly.
 * Return SXRET_OK on success. Any other return value indicates
 * failure.
 */
static sxi32 GenStateNextChunk(ph7_gen_state *pGen)
{
	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */
	sxu32 nRawObj;
	sxu32 nObjIdx;
	/* Consume raw chunks verbatim without any processing until we get
	 * a PHP block.
	 */
Consume:
	nRawObj = nObjIdx = 0;
	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){
		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);
		if( pRawObj == 0 ){
			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");
			return SXERR_ABORT;
		}
		/* Mark as constant and emit the load constant instruction */
		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);
		++nRawObj;
		pGen->pRawIn++; /* Next chunk */
	}
	if( nRawObj > 0 ){
		/* Emit the consume instruction */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);
	}
	if( pGen->pRawIn < pGen->pRawEnd ){
		SySet *pTokenSet = pGen->pTokenSet;
		/* Reset the token set (and its trivia sidecar) */
		SySetReset(pTokenSet);
		SySetReset(&pGen->aTrivia);
		/* Tokenize input */
		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),
			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);
		/* Point to the fresh token stream */
		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);
		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];
		/* Advance the stream cursor */
		pGen->pRawIn++;
		/* TICKET 1433-011 */
		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){
			static const sxu32 nKeyID = PH7_TKWRD_ECHO;
			sxi32 rc;
			/* Refer to TICKET 1433-009  */
			pGen->pIn->nType = PH7_TK_KEYWORD;
			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);
			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);
			/* Synthesized short-tag echo: legitimately an expression here. */
			pGen->nExprEchoOk++;
			rc = PH7_CompileExpr(pGen,0,0);
			pGen->nExprEchoOk--;
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}else if( rc != SXERR_EMPTY ){
				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
			}
			goto Consume;
		}
	}else{
		/* No more chunks to process */
		pGen->pIn = pGen->pEnd;
		return SXERR_EOF;
	}
	return SXRET_OK;
}
/*
 * Compile a PHP block.
 * A block is simply one or more PHP statements and expressions to compile
 * optionally delimited by braces {}.
 * Return SXRET_OK on success. Any other return value indicates failure
 * and this function takes care of generating the appropriate error
 * message.
 */
PH7_PRIVATE sxi32 PH7_CompileBlock(
	ph7_gen_state *pGen, /* Code generator state */
	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */
	)
{
	sxi32 rc;
	sxu32 nLine;
	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){
		nLine = pGen->pIn->nLine;
		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);
		if( rc != SXRET_OK ){
			return SXERR_ABORT;
		}
		pGen->pIn++;
		/* Compile until we hit the closing braces '}' */
		for(;;){
			if( pGen->pIn >= pGen->pEnd ){
				rc = GenStateNextChunk(&(*pGen));
				if (rc == SXERR_ABORT ){
			 	   return SXERR_ABORT;
				}
				if( rc == SXERR_EOF ){
					/* No more token to process: the block was never closed. php reports
					 * the line the '{' was opened on, not where the input ran out. */
					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);
					break;
				}
			}
			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){
				/* Closing braces found,break immediately*/
				pGen->pIn++;
				break;
			}
			/* Compile a single statement */
			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		GenStateLeaveBlock(&(*pGen),0);
	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){
		pGen->pIn++;
		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);
		if( rc != SXRET_OK ){
			return SXERR_ABORT;
		}
		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */
		for(;;){
			if( pGen->pIn >= pGen->pEnd ){
				rc = GenStateNextChunk(&(*pGen));
				if (rc == SXERR_ABORT ){
			 	   return SXERR_ABORT;
				}
				if( rc == SXERR_EOF || pGen->pIn >= pGen->pEnd ){
					/* No more token to process */
					if( rc == SXERR_EOF ){
						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,
							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");
					}
					break;
				}
			}
			if( pGen->pIn->nType & PH7_TK_KEYWORD ){
				sxi32 nKwrd;
				/* Keyword found */
				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
				if( nKwrd == nKeywordEnd ||
					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE || nKwrd == PH7_TKWRD_ELIF)) ){
						/* Delimiter keyword found,break */
						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){
							pGen->pIn++; /*  endif;endswitch... */
						}
						break;
				}
			}
			/* Compile a single statement */
			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		GenStateLeaveBlock(&(*pGen),0);
	}else{
		/* Compile a single statement */
		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Jump trailing semi-colons ';' */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Compile the gentle 'while' statement.
 * According to the PHP language reference
 *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.
 *  The basic form of a while statement is:
 *  while (expr)
 *   statement
 *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)
 *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression
 *  is checked each time at the beginning of the loop, so even if this value changes during
 *  the execution of the nested statement(s), execution will not stop until the end of the iteration
 *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while
 *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.
 *  Like with the if statement, you can group multiple statements within the same while loop by surrounding
 *  a group of statements with curly braces, or by using the alternate syntax:
 *  while (expr):
 *    statement
 *   endwhile;
 */
PH7_PRIVATE sxi32 PH7_CompileWhile(ph7_gen_state *pGen)
{
	GenBlock *pWhileBlock = 0;
	SyToken *pTmp,*pEnd = 0;
	sxu32 nFalseJump;
	sxu32 nLine;
	sxi32 rc;
	nLine = pGen->pIn->nLine;
	/* Jump the 'while' keyword */
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Jump the left parenthesis '(' */
	pGen->pIn++;
	/* Create the loop block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Delimit the condition */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
	if( pGen->pIn == pEnd || pEnd >= pGen->pEnd ){
		/* Empty condition. php reports the token that actually stopped it -- for
		 * `while ()` that is the ')' -- not a hand-written "expected expression". */
		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
	}
	/* Swap token streams */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	/* Compile the expression */
	rc = PH7_CompileExpr(&(*pGen),0,0);
	if( rc == SXERR_ABORT ){
		/* Expression handler request an operation abort [i.e: Out-of-memory] */
		return SXERR_ABORT;
	}
	/* Update token stream */
	while(pGen->pIn < pEnd ){
		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		pGen->pIn++;
	}
	/* Synchronize pointers */
	pGen->pIn  = &pEnd[1];
	pGen->pEnd = pTmp;
	/* Emit the false jump */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);
	/* Save the instruction index so we can fix it later when the jump destination is resolved */
	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);
	/* Compile the loop body */
	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* Emit the unconditional jump to the start of the loop */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);
	/* Fix all jumps now the destination is resolved */
	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));
	/* Release the loop block */
	GenStateLeaveBlock(pGen,0);
	/* Statement successfully compiled */
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon ';' so we can avoid
	 * compiling this erroneous block.
	 */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI|PH7_TK_OCB)) == 0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Compile the ugly do..while() statement.
 * According to the PHP language reference
 *  do-while loops are very similar to while loops, except the truth expression is checked
 *  at the end of each iteration instead of in the beginning. The main difference from regular
 *  while loops is that the first iteration of a do-while loop is guaranteed to run
 *  (the truth expression is only checked at the end of the iteration), whereas it may not
 *  necessarily run with a regular while loop (the truth expression is checked at the beginning
 *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution
 *  would end immediately).
 *  There is just one syntax for do-while loops:
 *  <?php
 *  $i = 0;
 *  do {
 *   echo $i;
 *  } while ($i > 0);
 * ?>
 */
PH7_PRIVATE sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)
{
	SyToken *pTmp,*pEnd = 0;
	GenBlock *pDoBlock = 0;
	sxu32 nLine;
	sxi32 rc;
	nLine = pGen->pIn->nLine;
	/* Jump the 'do' keyword */
	pGen->pIn++;
	/* Create the loop block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Deffer 'continue;' jumps until we compile the block */
	pDoBlock->bPostContinue = TRUE;
	rc = PH7_CompileBlock(&(*pGen),0);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	if( pGen->pIn < pGen->pEnd ){
		nLine = pGen->pIn->nLine;
	}
	if( pGen->pIn >= pGen->pEnd || pGen->pIn->nType != PH7_TK_KEYWORD ||
		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){
			/* Missing 'while' statement */
			/* php: `do {} ;` names the ';', while `do {}` at EOF is "unexpected end of
			 * file". The do-block's own slice stops before its terminator, so look at
			 * the whole CHUNK stream: a token still there is the one php names; nothing
			 * left means end of file (NULL). */
			{
				SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;
				if( pBad == 0 && pGen->pTokenSet ){
					/* The do-block consumed its terminator, so the token php names is
					 * the one just behind the cursor -- but only when it is a real
					 * terminator (`do {} ;` -> the ';'). If the block simply ended on
					 * its '}' with nothing after it, php reports end of file. */
					SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);
					if( pGen->pIn > pBase && (pGen->pIn[-1].nType & PH7_TK_SEMI) ){
						pBad = &pGen->pIn[-1];
					}
				}
				rc = PH7_GenSyntaxError(pGen,pBad,"\"while\"");
			}
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
			goto Synchronize;
	}
	/* Jump the 'while' keyword */
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Jump the left parenthesis '(' */
	pGen->pIn++;
	/* Delimit the condition */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
	if( pGen->pIn == pEnd || pEnd >= pGen->pEnd ){
		/* Empty condition. php reports the token that actually stopped it -- for
		 * `while ()` that is the ')' -- not a hand-written "expected expression". */
		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Fix post-continue jumps now the jump destination is resolved */
	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){
		JumpFixup *aPost;
		VmInstr *pInstr;
		sxu32 nJumpDest;
		sxu32 n;
		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);
		nJumpDest = PH7_VmInstrLength(pGen->pVm);
		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){
			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);
			if( pInstr ){
				/* Fix */
				pInstr->iP2 = nJumpDest;
			}
		}
	}
	/* Swap token streams */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	/* Compile the expression */
	rc = PH7_CompileExpr(&(*pGen),0,0);
	if( rc == SXERR_ABORT ){
		/* Expression handler request an operation abort [i.e: Out-of-memory] */
		return SXERR_ABORT;
	}
	/* Update token stream */
	while(pGen->pIn < pEnd ){
		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		pGen->pIn++;
	}
	pGen->pIn  = &pEnd[1];
	pGen->pEnd = pTmp;
	/* Emit the true jump to the beginning of the loop */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);
	/* Fix all jumps now the destination is resolved */
	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));
	/* Release the loop block */
	GenStateLeaveBlock(pGen,0);
	/* Statement successfully compiled */
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon ';' so we can avoid
	 * compiling this erroneous block.
	 */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI|PH7_TK_OCB)) == 0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Compile the complex and powerful 'for' statement.
 * According to the PHP language reference
 *  for loops are the most complex loops in PHP. They behave like their C counterparts.
 *  The syntax of a for loop is:
 *  for (expr1; expr2; expr3)
 *   statement
 *  The first expression (expr1) is evaluated (executed) once unconditionally at
 *  the beginning of the loop.
 *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to
 *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates
 *  to FALSE, the execution of the loop ends.
 *  At the end of each iteration, expr3 is evaluated (executed).
 *  Each of the expressions can be empty or contain multiple expressions separated by commas.
 *  In expr2, all expressions separated by a comma are evaluated but the result is taken
 *  from the last part. expr2 being empty means the loop should be run indefinitely
 *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might
 *  think, since often you'd want to end the loop using a conditional break statement instead
 *  of using the for truth expression.
 */
PH7_PRIVATE sxi32 PH7_CompileFor(ph7_gen_state *pGen)
{
	SyToken *pTmp,*pPostStart,*pEnd = 0;
	GenBlock *pForBlock = 0;
	sxu32 nFalseJump;
	sxu32 nLine;
	sxi32 rc;
	nLine = pGen->pIn->nLine;
	/* Jump the 'for' keyword */
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* Jump the left parenthesis '(' */
	pGen->pIn++;
	/* Delimit the init-expr;condition;post-expr */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
	if( pGen->pIn == pEnd || pEnd >= pGen->pEnd ){
		/* Empty expression */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		/* Synchronize */
		pGen->pIn = pEnd;
		if( pGen->pIn < pGen->pEnd ){
			pGen->pIn++;
		}
		return SXRET_OK;
	}
	/* Swap token streams */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	/* for() clauses are the ONLY place php's grammar allows a comma-separated
	 * expression list, so the comma operator is permitted for their duration
	 * (see GenStateTreeHasComma). A closure body nested inside a clause is
	 * compiled through this same window — recorded as a known leniency. */
	pGen->nCommaExprOk++;
	pGen->zClauseCloser = "\";\""; /* init/condition clauses close on ';' */
	/* Compile initialization expressions if available */
	rc = PH7_CompileExpr(&(*pGen),0,0);
	/* Pop operand lvalues */
	if( rc == SXERR_ABORT ){
		/* Expression handler request an operation abort [i.e: Out-of-memory] */
		return SXERR_ABORT;
	}else if( rc != SXERR_EMPTY ){
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
	}
	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		/* Syntax error */
		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* Jump the trailing ';' */
	pGen->pIn++;
	/* Create the loop block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Deffer continue jumps */
	pForBlock->bPostContinue = TRUE;
	/* Compile the condition */
	rc = PH7_CompileExpr(&(*pGen),0,0);
	if( rc == SXERR_ABORT ){
		/* Expression handler request an operation abort [i.e: Out-of-memory] */
		return SXERR_ABORT;
	}else if( rc != SXERR_EMPTY ){
		/* Emit the false jump */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);
		/* Save the instruction index so we can fix it later when the jump destination is resolved */
		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);
	}
	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		/* Syntax error */
		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* Jump the trailing ';' */
	pGen->pIn++;
	/* Save the post condition stream */
	pPostStart = pGen->pIn;
	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary
	 * php, so `(1, 2)` inside it is the parse error it should be). */
	pGen->nCommaExprOk--;
	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */
	pGen->pEnd = pTmp;
	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* Fix post-continue jumps */
	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){
		JumpFixup *aPost;
		VmInstr *pInstr;
		sxu32 nJumpDest;
		sxu32 n;
		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);
		nJumpDest = PH7_VmInstrLength(pGen->pVm);
		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){
			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);
			if( pInstr ){
				/* Fix jump */
				pInstr->iP2 = nJumpDest;
			}
		}
	}
	/* compile the post-expressions if available */
	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){
		pPostStart++;
	}
	if( pPostStart < pEnd ){
		SyToken *pTmpIn,*pTmpEnd;
		SWAP_DELIMITER(pGen,pPostStart,pEnd);
		pGen->nCommaExprOk++; /* post-expressions are a clause list again */
		pGen->zClauseCloser = "\")\""; /* the post clause closes on ')' */
		rc = PH7_CompileExpr(&(*pGen),0,0);
		pGen->nCommaExprOk--;
		pGen->zClauseCloser = 0;
		if( pGen->pIn < pGen->pEnd ){
			/* php names the token and expects ')'; the post-clause runs to the ')'. */
			rc = PH7_GenSyntaxError(pGen,pGen->pIn,"\")\"");
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			return SXRET_OK;
		}
		RE_SWAP_DELIMITER(pGen);
		if( rc == SXERR_ABORT ){
			/* Expression handler request an operation abort [i.e: Out-of-memory] */
			return SXERR_ABORT;
		}else if( rc != SXERR_EMPTY){
			/* Pop operand lvalue */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}
	}
	/* Emit the unconditional jump to the start of the loop */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);
	/* Fix all jumps now the destination is resolved */
	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));
	/* Release the loop block */
	GenStateLeaveBlock(pGen,0);
	/* Statement successfully compiled */
	return SXRET_OK;
}
/* Expression tree validator callback used by the 'foreach' statement.
 * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]
 * are allowed.
 */
static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)
{
	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */
	if( pRoot->xCode != PH7_CompileVariable ){
		/* Unexpected expression */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,
			"foreach: Expecting a variable name");
		if( rc != SXERR_ABORT ){
			rc = SXERR_INVALID;
		}
	}
	return rc;
}
/*
 * Compile the 'foreach' statement.
 * According to the PHP language reference
 *  The foreach construct simply gives an easy way to iterate over arrays. foreach works
 *  only on arrays (and objects), and will issue an error when you try to use it on a variable
 *  with a different data type or an uninitialized variable. There are two syntaxes; the second
 *  is a minor but useful extension of the first:
 *  foreach (array_expression as $value)
 *    statement
 *  foreach (array_expression as $key => $value)
 *   statement
 *  The first form loops over the array given by array_expression. On each loop, the value
 *  of the current element is assigned to $value and the internal array pointer is advanced
 *  by one (so on the next loop, you'll be looking at the next element).
 *  The second form does the same thing, except that the current element's key will be assigned
 *  to the variable $key on each loop.
 *  Note:
 *  When foreach first starts executing, the internal array pointer is automatically reset to the
 *  first element of the array. This means that you do not need to call reset() before a foreach loop.
 *  Note:
 *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array
 *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during
 *  or after the foreach without resetting it.
 *  You can easily modify array's elements by preceding $value with &. This will assign reference instead
 *  of copying the value.
 */
PH7_PRIVATE sxi32 PH7_CompileForeach(ph7_gen_state *pGen)
{
	SyToken *pCur,*pTmp,*pEnd = 0;
	SyToken *pListStart = 0,*pListEnd = 0;
	GenBlock *pForeachBlock = 0;
	ph7_foreach_info *pInfo;
	sxu32 nFalseJump;
	VmInstr *pInstr;
	sxu32 nLine;
	sxi32 rc;
	nLine = pGen->pIn->nLine;
	/* Jump the 'foreach' keyword */
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Jump the left parenthesis '(' */
	pGen->pIn++;
	/* Create the loop block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Delimit the expression */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
	if( pGen->pIn == pEnd || pEnd >= pGen->pEnd ){
		/* Empty expression */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		/* Synchronize */
		pGen->pIn = pEnd;
		if( pGen->pIn < pGen->pEnd ){
			pGen->pIn++;
		}
		return SXRET_OK;
	}
	/* Compile the array expression */
	pCur = pGen->pIn;
	while( pCur < pEnd ){
		if( pCur->nType & PH7_TK_KEYWORD ){
			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);
			if( nKeywrd == PH7_TKWRD_AS ){
				/* Break with the first 'as' found */
				break;
			}
		}
		/* Advance the stream cursor */
		pCur++;
	}
	if( pCur <= pGen->pIn ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
			"foreach: Missing array/object expression");
		if( rc == SXERR_ABORT ){
			/* Don't worry about freeing memory, everything will be released shortly */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Swap token streams */
	pTmp = pGen->pEnd;
	pGen->pEnd = pCur;
	rc = PH7_CompileExpr(&(*pGen),0,0);
	if( rc == SXERR_ABORT ){
		/* Expression handler request an operation abort [i.e: Out-of-memory] */
		return SXERR_ABORT;
	}
	/* Update token stream */
	while(pGen->pIn < pCur ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			/* Don't worry about freeing memory, everything will be released shortly */
			return SXERR_ABORT;
		}
		pGen->pIn++;
	}
	pCur++; /* Jump the 'as' keyword */
	pGen->pIn = pCur;
	if( pGen->pIn >= pEnd ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Create the foreach context */
	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));
	if( pInfo == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");
		return SXERR_ABORT;
	}
	/* Zero the structure */
	SyZero(pInfo,sizeof(ph7_foreach_info));
	/* Initialize structure fields */
	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));
	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed
	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner
	 * '=>'. */
	pCur = GenStateFindTopLevelArrow(pCur,pEnd);
	if( pCur < pEnd ){
		/* Compile the expression holding the key name */
		if( pGen->pIn >= pCur ){
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");
			if( rc == SXERR_ABORT ){
				/* Don't worry about freeing memory, everything will be released shortly */
				return SXERR_ABORT;
			}
		}else{
			pGen->pEnd = pCur;
			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);
			if( rc == SXERR_ABORT ){
				/* Don't worry about freeing memory, everything will be released shortly */
				return SXERR_ABORT;
			}
			pInstr = PH7_VmPopInstr(pGen->pVm);
			if( pInstr->p3 ){
				/* Record key name */
				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));
			}
			pInfo->iFlags |= PH7_4EACH_STEP_KEY;
		}
		pGen->pIn = &pCur[1]; /* Jump the arrow */
	}
	pGen->pEnd = pEnd;
	if( pGen->pIn >= pEnd ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");
		if( rc == SXERR_ABORT ){
			/* Don't worry about freeing memory, everything will be released shortly */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){
		pGen->pIn++;
		/* Pass by reference  */
		pInfo->iFlags |= PH7_4EACH_STEP_REF;
	}
	/* Check if the value target is list() */
	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&
		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){
		/* foreach ($arr as list($a, $b)) — list unpacking.
		 * Save the list() token range; we'll compile it after FOREACH_STEP.
		 */
		static int iForeachListCnt = 0;
		char zTmp[128];
		sxu32 nLen;
		char *zDup;
		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);
		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);
		if( zDup == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");
			return SXERR_ABORT;
		}
		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);
		/* Save list() token boundaries */
		pListStart = pGen->pIn;
		/* Advance past list(...) — validate parentheses */
		pGen->pIn++; /* Jump 'list' keyword */
		if( pGen->pIn >= pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
			/* php: syntax error, unexpected variable "$x", expecting "(" */
			rc = PH7_GenSyntaxError(pGen,pGen->pIn < pEnd ? pGen->pIn : 0,"\"(\"");
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
		pGen->pIn++; /* Jump '(' */
		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);
		if( pListEnd >= pEnd ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"foreach: Missing closing ')' after list");
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
		pGen->pIn = &pListEnd[1]; /* Past ')' */
		pListEnd = pGen->pIn;
		pInfo->iFlags |= PH7_4EACH_STEP_LIST;
	}else if( pGen->pIn->nType & PH7_TK_OSB ){
		/* foreach ($arr as [$a, $b]) — short list unpacking.
		 * Save the [...] token range; we'll compile it after FOREACH_STEP.
		 */
		static int iForeachShortListCnt = 0;
		char zTmp[128];
		sxu32 nLen;
		char *zDup;
		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);
		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);
		if( zDup == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");
			return SXERR_ABORT;
		}
		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);
		/* Save [...] token boundaries */
		pListStart = pGen->pIn;
		/* Advance past [...] */
		pGen->pIn++; /* Jump '[' */
		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);
		if( pListEnd >= pEnd ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"foreach: Missing closing ']' after short list");
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
		pGen->pIn = &pListEnd[1]; /* Past ']' */
		pListEnd = pGen->pIn;
		pInfo->iFlags |= PH7_4EACH_STEP_LIST;
	}else{
		/* Compile the expression holding the value name */
		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);
		if( rc == SXERR_ABORT ){
			/* Don't worry about freeing memory, everything will be released shortly */
			return SXERR_ABORT;
		}
		pInstr = PH7_VmPopInstr(pGen->pVm);
		if( pInstr->p3 ){
			/* Record value name */
			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));
		}
	}
	/* Emit the 'FOREACH_INIT' instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);
	/* Save the instruction index so we can fix it later when the jump destination is resolved */
	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);
	/* Record the first instruction to execute */
	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);
	/* Emit the FOREACH_STEP instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);
	/* Save the instruction index so we can fix it later when the jump destination is resolved */
	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);
	/* If list() unpacking, emit bytecode to destructure the temp variable */
	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){
		SyToken *pSavedIn,*pSavedEnd;
		/* Load the temporary variable holding the current value onto the stack.
		 * The LOAD_LIST handler expects the array below the variable entries.
		 */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);
		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.
		 * We position the tokens at the construct start so the appropriate compiler
		 * picks up the delimiter and the variable names inside.
		 */
		pSavedIn = pGen->pIn;
		pSavedEnd = pGen->pEnd;
		pGen->pIn = pListStart;
		pGen->pEnd = pListEnd;
		if( pListStart->nType & PH7_TK_OSB ){
			rc = PH7_CompileShortList(&(*pGen),0);
		}else{
			rc = PH7_CompileList(&(*pGen),0);
		}
		pGen->pIn = pSavedIn;
		pGen->pEnd = pSavedEnd;
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
	}
	/* Compile the loop body */
	pGen->pIn = &pEnd[1];
	pGen->pEnd = pTmp;
	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);
	if( rc == SXERR_ABORT ){
		/* Don't worry about freeing memory, everything will be released shortly */
		return SXERR_ABORT;
	}
	/* Emit the unconditional jump to the start of the loop */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);
	/* Fix all jumps now the destination is resolved */
	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));
	/* Release the loop block */
	GenStateLeaveBlock(pGen,0);
	/* Statement successfully compiled */
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon ';' so we can avoid
	 * compiling this erroneous block.
	 */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI|PH7_TK_OCB)) == 0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Compile the infamous if/elseif/else if/else statements.
 * According to the PHP language reference
 *  The if construct is one of the most important features of many languages PHP included.
 *  It allows for conditional execution of code fragments. PHP features an if structure
 *  that is similar to that of C:
 *  if (expr)
 *   statement
 *  else construct:
 *   Often you'd want to execute a statement if a certain condition is met, and a different
 *   statement if the condition is not met. This is what else is for. else extends an if statement
 *   to execute a statement in case the expression in the if statement evaluates to FALSE.
 *   For example, the following code would display a is greater than b if $a is greater than
 *   $b, and a is NOT greater than b otherwise.
 *   The else statement is only executed if the if expression evaluated to FALSE, and if there
 *   were any elseif expressions - only if they evaluated to FALSE as well
 *  elseif
 *   elseif, as its name suggests, is a combination of if and else. Like else, it extends
 *   an if statement to execute a different statement in case the original if expression evaluates
 *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif
 *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger
 *   than b, a equal to b or a is smaller than b:
 *   <?php
 *    if ($a > $b) {
 *     echo "a is bigger than b";
 *    } elseif ($a == $b) {
 *     echo "a is equal to b";
 *    } else {
 *     echo "a is smaller than b";
 *    }
 *    ?>
 */
PH7_PRIVATE sxi32 PH7_CompileIf(ph7_gen_state *pGen)
{
	SyToken *pToken,*pTmp,*pEnd = 0;
	GenBlock *pCondBlock = 0;
	sxu32 nJumpIdx;
	sxu32 nKeyID;
	sxi32 rc;
	/* Jump the 'if' keyword */
	pGen->pIn++;
	pToken = pGen->pIn;
	/* Create the conditional block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Process as many [if/else if/elseif/else] blocks as we can */
	for(;;){
		if( pToken >= pGen->pEnd || (pToken->nType & PH7_TK_LPAREN) == 0 ){
			/* Syntax error */
			if( pToken >= pGen->pEnd ){
				pToken--;
			}
			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
		/* Jump the left parenthesis '(' */
		pToken++;
		/* Delimit the condition */
		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
		if( pToken >= pEnd || (pEnd->nType & PH7_TK_RPAREN) == 0 ){
			/* Syntax error */
			if( pToken >= pGen->pEnd ){
				pToken--;
			}
			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
		/* Swap token streams */
		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);
		/* Compile the condition */
		rc = PH7_CompileExpr(&(*pGen),0,0);
		/* Update token stream */
		while(pGen->pIn < pEnd ){
			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);
			pGen->pIn++;
		}
		pGen->pIn  = &pEnd[1];
		pGen->pEnd = pTmp;
		if( rc == SXERR_ABORT ){
			/* Expression handler request an operation abort [i.e: Out-of-memory] */
			return SXERR_ABORT;
		}
		/* Emit the false jump */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);
		/* Save the instruction index so we can fix it later when the jump destination is resolved */
		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);
		/* Compile the body */
		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){
			break;
		}
		/* Ensure that the keyword ID is 'else if' or 'else' */
		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);
		if( (nKeyID & (PH7_TKWRD_ELSE|PH7_TKWRD_ELIF)) == 0 ){
			break;
		}
		/* Emit the unconditional jump */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);
		/* Save the instruction index so we can fix it later when the jump destination is resolved */
		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);
		if( nKeyID & PH7_TKWRD_ELSE ){
			pToken = &pGen->pIn[1];
			if( pToken >= pGen->pEnd || (pToken->nType & PH7_TK_KEYWORD) == 0 ||
				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){
					break;
			}
			pGen->pIn++; /* Jump the 'else' keyword */
		}
		pGen->pIn++; /* Jump the 'elseif/if' keyword */
		/* Synchronize cursors */
		pToken = pGen->pIn;
		/* Fix the false jump */
		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));
	} /* For(;;) */
	/* Fix the false jump */
	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&
		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){
			/* Compile the else block */
			pGen->pIn++;
			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);
			if( rc == SXERR_ABORT ){

				return SXERR_ABORT;
			}
	}
	nJumpIdx = PH7_VmInstrLength(pGen->pVm);
	/* Fix all unconditional jumps now the destination is resolved */
	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);
	/* Release the conditional block */
	GenStateLeaveBlock(pGen,0);
	/* Statement successfully compiled */
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.
	 */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI|PH7_TK_OCB)) == 0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Compile the global construct.
 * According to the PHP language reference
 *  In PHP global variables must be declared global inside a function if they are going
 *  to be used in that function.
 *  Example #1 Using global
 *  <?php
 *   $a = 1;
 *   $b = 2;
 *   function Sum()
 *   {
 *    global $a, $b;
 *    $b = $a + $b;
 *   }
 *   Sum();
 *   echo $b;
 *  ?>
 *  The above script will output 3. By declaring $a and $b global within the function
 *  all references to either variable will refer to the global version. There is no limit
 *  to the number of global variables that can be manipulated by a function.
 */
PH7_PRIVATE sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)
{
	SyToken *pTmp,*pNext = 0;
	sxi32 nExpr;
	sxi32 rc;
	/* Jump the 'global' keyword */
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_SEMI) ){
		/* Nothing to process */
		return SXRET_OK;
	}
	pTmp = pGen->pEnd;
	nExpr = 0;
	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){
		if( pGen->pIn < pNext ){
			pGen->pEnd = pNext;
			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}else{
				pGen->pIn++;
				if( pGen->pIn >= pGen->pEnd ){
					/* Emit a warning */
					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");
				}else{
					rc = PH7_CompileExpr(&(*pGen),0,0);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}else if(rc != SXERR_EMPTY ){
						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);
						if( pLast && pLast->iOp == PH7_OP_LOADC ){
							/* Variable name, not a constant */
							pLast->iP1 = 0;
						}
						nExpr++;
					}
				}
			}
		}
		/* Next expression in the stream */
		pGen->pIn = pNext;
		/* Jump trailing commas */
		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){
			pGen->pIn++;
		}
	}
	/* Restore token stream */
	pGen->pEnd = pTmp;
	if( nExpr > 0 ){
		/* Emit the uplink instruction */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);
	}
	return SXRET_OK;
}
/*
 * Compile the return statement.
 * According to the PHP language reference
 *  If called from within a function, the return() statement immediately ends execution
 *  of the current function, and returns its argument as the value of the function call.
 *  return() will also end the execution of an eval() statement or script file.
 *  If called from the global scope, then execution of the current script file is ended.
 *  If the current script file was include()ed or require()ed, then control is passed back
 *  to the calling file. Furthermore, if the current script file was include()ed, then the value
 *  given to return() will be returned as the value of the include() call. If return() is called
 *  from within the main script file, then script execution end.
 *  Note that since return() is a language construct and not a function, the parentheses
 *  surrounding its arguments are not required. It is common to leave them out, and you actually
 *  should do so as PHP has less work to do in this case.
 *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.
 */
PH7_PRIVATE sxi32 PH7_CompileReturn(ph7_gen_state *pGen)
{
	sxi32 nRet = 0; /* TRUE if there is a return value */
	sxi32 rc;
	sxu32 nLine = pGen->pIn->nLine;
	GenBlock *pFuncBlock = pGen->pCurrent;
	/* A `never`-returning function must not contain a `return` statement at all
	 * (PHP compile error), with or without a value. Find the enclosing function
	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is
	 * recorded (nErr>0 fails the whole compile); the statement is still consumed
	 * normally below so token processing stays consistent. */
	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){
		pFuncBlock = pFuncBlock->pParent;
	}
	if( pFuncBlock && pFuncBlock->pUserData
	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){
		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,
			"A never-returning function must not return");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Jump the 'return' keyword */
	pGen->pIn++;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		/* php: a stray token after `return EXPR` is `... expecting ";"`. */
		const char *zSave = pGen->zClauseCloser;
		pGen->zClauseCloser = "\";\"";
		/* A `return` READS its operand (the value is consumed), so compile it
		 * read-only: a lone undefined variable `return $z` must warn at the read
		 * exactly like echo/interpolation, not be loaded quietly (the same quiet
		 * load that correctly keeps a bare `$z;` statement silent). Matches the
		 * arrow-fn implicit-return body fix. */
		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);
		pGen->zClauseCloser = zSave;
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if(rc != SXERR_EMPTY ){
			nRet = 1;
		}
	}
	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every
	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the
	 * live aException stack). With no enclosing try the action materializes immediately, so
	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */
	if( pGen->bInGenerator ){
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);
		return SXRET_OK;
	}
	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this
	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with
	 * bReturnPropagates), the VM must return from the enclosing function rather
	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),
	 * so the VM can tell a real `return` from the body simply ending. */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);
	return SXRET_OK;
}
/*
 * Compile a yield expression.
 * Called from the expression code generator when a yield node is encountered.
 * Handles: yield, yield $value, yield $key => $value
 * The yield expression evaluates to the value passed via Generator::send().
 */
PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)
{
	SyToken *pTmp, *pSplit;
	sxi32 iP1 = 0; /* 1 if value present */
	sxi32 iP2 = 0; /* 1 if key => value */
	sxi32 rc;
	(void)iCompileFlag;
	/* pGen->pIn points to 'yield' keyword, skip it */
	pGen->pIn++;
	/* Now pGen->pIn points to the first token after 'yield'
	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */
	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a
	 * contextual identifier, not a keyword; a variable named $from lexes as
	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)
		&& pGen->pIn->sData.nByte == 4
		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){
		pGen->pIn++; /* Skip 'from' */
		rc = PH7_CompileExpr(pGen, 0, 0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		if( rc == SXERR_EMPTY ){
			rc = PH7_GenCompileError(pGen, E_ERROR,
				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,
				"Missing expression after 'yield from'");
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);
		return SXRET_OK;
	}
	if( pGen->pIn >= pGen->pEnd ){
		/* Bare yield — no value */
		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);
		return SXRET_OK;
	}
	/* Scan for '=>' at nesting level 0 to detect key => value syntax */
	pSplit = 0;
	{
		SyToken *pCur = pGen->pIn;
		sxi32 nNest = 0;
		while( pCur < pGen->pEnd ){
			if( pCur->nType & (PH7_TK_LPAREN|PH7_TK_OSB|PH7_TK_OCB) ){
				nNest++;
			}else if( pCur->nType & (PH7_TK_RPAREN|PH7_TK_CSB|PH7_TK_CCB) ){
				nNest--;
			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){
				pSplit = pCur;
				break;
			}
			pCur++;
		}
	}
	pTmp = pGen->pEnd;
	if( pSplit ){
		/* yield $key => $value */
		pGen->pEnd = pSplit;
		rc = PH7_CompileExpr(pGen, 0, 0);
		if( rc == SXERR_ABORT ) return SXERR_ABORT;
		pGen->pIn = pSplit + 1; /* Skip '=>' */
		pGen->pEnd = pTmp;
		rc = PH7_CompileExpr(pGen, 0, 0);
		if( rc == SXERR_ABORT ) return SXERR_ABORT;
		iP1 = 1;
		iP2 = 1;
	}else{
		/* yield $value */
		rc = PH7_CompileExpr(pGen, 0, 0);
		if( rc == SXERR_ABORT ) return SXERR_ABORT;
		if( rc != SXERR_EMPTY ){
			iP1 = 1;
		}
	}
	pGen->pEnd = pTmp;
	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);
	return SXRET_OK;
}
/*
 * Compile the die/exit language construct.
 * The role of these constructs is to terminate execution of the script.
 * Shutdown functions will always be executed even if exit() is called.
 */
PH7_PRIVATE sxi32 PH7_CompileHalt(ph7_gen_state *pGen)
{
	sxi32 nExpr = 0;
	sxi32 rc;
	/* Jump the die/exit keyword */
	pGen->pIn++;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		/* Compile the expression */
		rc = PH7_CompileExpr(&(*pGen),0,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if(rc != SXERR_EMPTY ){
			nExpr = 1;
		}
	}
	/* Emit the HALT instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);
	return SXRET_OK;
}
/*
 * Compile the 'echo' language construct.
 */
PH7_PRIVATE sxi32 PH7_CompileEcho(ph7_gen_state *pGen)
{
	SyToken *pTmp,*pNext = 0;
	sxu32 nLine = pGen->pIn->nLine;
	int nExpr = 0;      /* expressions actually compiled */
	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */
	sxi32 rc;
	/* Jump the 'echo' keyword */
	pGen->pIn++;
	/* Compile arguments one after one. php: a stray token in an echo list is
	 * `... expecting "," or ";"` — set the closer for each argument's compile. */
	pTmp = pGen->pEnd;
	{
	const char *zSaveEcho = pGen->zClauseCloser;
	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){
		if( pGen->pIn < pNext ){
			pGen->pEnd = pNext;
			pGen->zClauseCloser = "\",\" or \";\"";
			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);
			pGen->zClauseCloser = zSaveEcho;
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}else if( rc != SXERR_EMPTY ){
				/* Emit the consume instruction */
				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);
				nExpr++;
				bExpectMore = 0;
			}
		}
		/* Jump trailing commas (php: exactly one between expressions; a
		 * dangling or doubled comma is a parse error, enforced below) */
		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){
			if( bExpectMore ){
				/* two commas in a row */
				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,
					"syntax error, unexpected token \",\"");
				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;
			}
			bExpectMore = 1;
			pNext++;
		}
		pGen->pIn = pNext;
	}
	}
	/* Restore token stream */
	pGen->pEnd = pTmp;
	if( nExpr == 0 || bExpectMore ){
		/* `echo ;` or `echo expr, ;` — php rejects both */
		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,
			"syntax error, unexpected token \";\"");
		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;
	}
	return SXRET_OK;
}
/*
 * Compile the static statement.
 * According to the PHP language reference
 *  Another important feature of variable scoping is the static variable.
 *  A static variable exists only in a local function scope, but it does not lose its value
 *  when program execution leaves this scope.
 *  Static variables also provide one way to deal with recursive functions.
 * Symisc eXtension.
 *  PH7 allow any complex expression to be associated with the static variable while
 *  the zend engine would allow only simple scalar value.
 *  Example
 *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine
 *    Refer to the official documentation for more information on this feature.
 */
PH7_PRIVATE sxi32 PH7_CompileStatic(ph7_gen_state *pGen)
{
	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */
	ph7_vm_func *pFunc;             /* Enclosing function */
	GenBlock *pBlock;
	SyString *pName;
	char *zDup;
	sxu32 nLine;
	sxi32 rc;
	/* `static function () {}` / `static fn () =>` at statement position is an
	 * EXPRESSION statement (a bare static closure), not a static-variable
	 * declaration — hand it to the expression compiler (php accepts it). */
	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)
	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION
	  || SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){
		rc = PH7_CompileExpr(&(*pGen),0,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if( rc != SXERR_EMPTY ){
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}
		return SXRET_OK;
	}
	/* Jump the static keyword */
	nLine = pGen->pIn->nLine;
	pGen->pIn++;
	/* Extract the enclosing function if any */
	pBlock = pGen->pCurrent;
	while( pBlock ){
		if( pBlock->iFlags & GEN_BLOCK_FUNC){
			break;
		}
		/* Point to the upper block */
		pBlock = pBlock->pParent;
	}
	if( pBlock == 0 ){
		/* Static statement,called outside of a function body,treat it as a simple variable. */
		if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){
			/* php: `static FOO;` is a syntax error naming FOO and expecting "::"
			 * (the parser is still open to `static::` at that point). */
			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
		/* Compile the expression holding the variable */
		rc = PH7_CompileExpr(&(*pGen),0,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if( rc != SXERR_EMPTY ){
			/* Emit the POP instruction */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}
		return SXRET_OK;
	}
	pFunc = (ph7_vm_func *)pBlock->pUserData;
	/* Make sure we are dealing with a valid statement */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 || &pGen->pIn[1] >= pGen->pEnd ||
		(pGen->pIn[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
			/* php: `static FOO;` is a syntax error naming FOO and expecting "::"
			 * (the parser is still open to `static::` at that point). */
			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
	}
	pGen->pIn++;
	/* Extract variable name */
	pName = &pGen->pIn->sData;
	pGen->pIn++; /* Jump the var name */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/|PH7_TK_EQUAL/*'='*/)) == 0 ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);
		goto Synchronize;
	}
	/* Initialize the structure describing the static variable */
	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));
	sStatic.nIdx = SXU32_HIGH; /* Not yet created */
	/* Duplicate variable name */
	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);
	if( zDup == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");
		return SXERR_ABORT;
	}
	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);
	/* Check if we have an expression to compile */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){
		SySet *pInstrContainer;
		/* TICKET 1433-014: Symisc extension to the PHP programming language
		 * Static variable can take any complex expression including function
		 * call as their initialization value.
		 * Example:
		 *		static $var = foo(1,4+5,bar());
		 */
		pGen->pIn++; /* Jump the equal '=' sign */
		/* Swap bytecode container */
		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);
		/* Compile the expression */
		rc = PH7_CompileExpr(&(*pGen),0,0);
		/* Emit the done instruction */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);
		/* Restore default bytecode container */
		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	}
	/* Finally save the compiled static variable in the appropriate container */
	SySetPut(&pFunc->aStatic,(const void *)&sStatic);
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous
	 * statement.
	 */
	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Compile the var statement.
 * Symisc Extension:
 *      var statement can be used outside of a class definition.
 */
PH7_PRIVATE sxi32 PH7_CompileVar(ph7_gen_state *pGen)
{
	/* `var` is a PROPERTY modifier and nothing else. A class body's `var $x;`
	 * is compiled by compile_class.c, never here, so reaching this statement
	 * handler means `var` appeared outside a class — which php rejects as a
	 * parse error. PHL used to compile it as an ordinary expression statement,
	 * so top-level `var $x = 1;` silently ran (a Symisc extension). */
	PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);
	return SXERR_ABORT;
}
/*
 * Namespace-qualify a literal in-place for CALL/NEW instructions.
 * Resolution: use imports -> current NS prefix. The VM handles global fallback.
 * Only rewrites unqualified names (no backslash) when a namespace is active.
 */
/*
 * Namespace-qualify a name for CALL/NEW/instanceof instructions.
 * Instead of mutating the interned literal (which would corrupt the literal
 * hash and any shared references), this creates a new literal entry with the
 * qualified name and updates the instruction's operand index.
 *
 * Resolution order:
 *   1. Check the given import table (pImports) — matches even outside namespaces.
 *   2. If no import matches and a namespace is active, prepend the current NS.
 *   3. Otherwise return the original literal index unchanged.
 *
 * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution
 * came from an import (step 1) and 0 otherwise.
 * Returns the (possibly new) literal index.
 */
PH7_PRIVATE sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)
{
	ph7_value *pLit;
	const char *zLit;
	SyString sQualified;
	sxu32 nLit;
	sxu32 k;
	sxu32 nNewIdx;
	int hasNsSep;
	SyHashEntry *pImport;
	ph7_value *pNew;
	if( pFromImport ){
		*pFromImport = 0;
	}
	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);
	if( !pLit || !(pLit->iFlags & MEMOBJ_STRING) || SyBlobLength(&pLit->sBlob) == 0 ){
		return nOrigIdx;
	}
	zLit = (const char *)SyBlobData(&pLit->sBlob);
	nLit = (sxu32)SyBlobLength(&pLit->sBlob);
	/* Skip if already qualified (contains backslash) */
	hasNsSep = 0;
	for( k = 0; k < nLit; k++ ){
		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }
	}
	if( hasNsSep ){
		return nOrigIdx;
	}
	/* Check use imports first (works even outside namespaces) */
	SyBlobReset(&pGen->sWorker);
	pImport = SyHashGet(pImports,(const void *)zLit,nLit);
	if( pImport ){
		const char *zFQN = (const char *)pImport->pUserData;
		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));
		if( pFromImport ){
			*pFromImport = 1;
		}
	}else{
		if( SyBlobLength(&pGen->sNamespace) == 0 ){
			return nOrigIdx; /* Not in a namespace and no import match */
		}
		/* Prepend current namespace */
		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));
		SyBlobAppend(&pGen->sWorker,"\\",1);
		SyBlobAppend(&pGen->sWorker,zLit,nLit);
	}
	/* Look up or create a new literal for the qualified name */
	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));
	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){
		return nNewIdx; /* Already interned */
	}
	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);
	if( pNew == 0 ){
		return nOrigIdx; /* OOM, fall back to original */
	}
	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);
	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);
	return nNewIdx;
}
/*
 * Resolve a class/function name at compile time through use imports and current namespace.
 * Writes the resolved FQN into pOut. Caller must release pOut.
 */
PH7_PRIVATE void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)
{
	SyHashEntry *pImport;
	const char *zName = pName->zString;
	sxu32 nName = pName->nByte;
	sxu32 nFirst = 0;
	/* php resolves a name through use-imports on its LEADING segment: an
	 * unqualified `C` maps the whole name (`use X\C;` -> X\C), while a QUALIFIED
	 * `A\B\C` maps just `A` (`use X\A;` -> X\A\B\C) and keeps the `\B\C` tail.
	 * The old code looked up the whole qualified string (which never matches a
	 * single-segment import alias) and then blindly prefixed the current
	 * namespace, so `use PHPUnit\Framework; extends Framework\TestCase` resolved
	 * to Ns\Framework\TestCase. Mirror GenStateResolveNamespaceLiteral here. */
	while( nFirst < nName && zName[nFirst] != '\\' ){ nFirst++; }
	pImport = SyHashGet(&pGen->hUseImports,(const void *)zName,nFirst);
	if( pImport ){
		const char *zFQN = (const char *)pImport->pUserData;
		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));
		SyBlobAppend(pOut,zName + nFirst,nName - nFirst); /* the \B\C tail, if any */
		return;
	}
	/* Prepend current namespace if active */
	if( SyBlobLength(&pGen->sNamespace) > 0 ){
		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));
		SyBlobAppend(pOut,"\\",1);
	}
	SyBlobAppend(pOut,zName,nName);
}
/*
 * Build a fully-qualified name by prepending the current namespace to a short name.
 * If no namespace is active, pOut receives a copy of the short name.
 * The caller must release pOut when done.
 */
PH7_PRIVATE void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)
{
	if( SyBlobLength(&pGen->sNamespace) > 0 ){
		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));
		SyBlobAppend(pOut,"\\",1);
	}
	SyBlobAppend(pOut,pName->zString,pName->nByte);
}
/*
 * php's `namespace\X` NAME OPERATOR (5.3): a leading `namespace` keyword glued to
 * a `\` names the CURRENT namespace, and the whole name is then FULLY QUALIFIED —
 * `namespace\X` inside `namespace B;` is `\B\X`, and plain `\X` at global scope.
 * php's lexer matches it as one token (T_NAME_RELATIVE, `"namespace"("\\"{LABEL})+`,
 * case-insensitively), so the `\` must be GLUED to the keyword: `namespace \X` is a
 * php parse error, and this mirrors that by comparing source offsets.
 *
 * This predicate only RECOGNIZES the operator (it consumes nothing), which is what
 * the statement dispatcher needs to tell `namespace\X::m();` from a namespace
 * DECLARATION; GenStateNsRelPrefix below is what the name collectors call.
 */
PH7_PRIVATE int GenStateIsNsRelName(SyToken *pIn,SyToken *pEnd)
{
	if( pIn >= pEnd || (pIn->nType & PH7_TK_KEYWORD) == 0
	 || (sxu32)SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_NAMESPACE ){
		return 0;
	}
	if( &pIn[1] >= pEnd || (pIn[1].nType & PH7_TK_NSSEP) == 0 ){
		return 0;
	}
	if( &pIn[2] >= pEnd || (pIn[2].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
		return 0; /* php's T_NAME_RELATIVE needs at least one segment after the `\` */
	}
	/* Glued? The tokenizer drops whitespace, so adjacency is the source offsets. */
	return pIn->sData.zString + pIn->sData.nByte == pIn[1].sData.zString;
}
/*
 * Consume a leading `namespace\` (see GenStateIsNsRelName) at *ppIn and seed pOut
 * with the current namespace plus its separator — nothing at global scope, where
 * the bare name already IS the FQN. Returns TRUE when it fired, and the caller
 * must then treat the name it goes on to collect as ABSOLUTE: no `use` import may
 * apply to it, and the current namespace is already in place.
 */
PH7_PRIVATE int GenStateNsRelPrefix(ph7_gen_state *pGen,SyToken **ppIn,SyToken *pEnd,SyBlob *pOut)
{
	SyToken *pIn = *ppIn;
	if( !GenStateIsNsRelName(pIn,pEnd) ){
		return 0;
	}
	if( SyBlobLength(&pGen->sNamespace) > 0 ){
		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));
		SyBlobAppend(pOut,"\\",1);
	}
	*ppIn = &pIn[2];
	return 1;
}
/*
 * Compile a namespace statement
 * According to the PHP language reference manual
 *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.
 *  This can be seen as an abstract concept in many places. For example, in any operating system
 *  directories serve to group related files, and act as a namespace for the files within them.
 *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other
 *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt
 *  file outside of the /home/greg directory, we must prepend the directory name to the file name using
 *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the
 *  programming world.
 *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications
 *  encounter when creating re-usable code elements such as classes or functions:
 *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party
 *  classes/functions/constants.
 *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving
 *  readability of source code.
 *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.
 *  Here is an example of namespace syntax in PHP:
 *       namespace my\name; // see "Defining Namespaces" section
 *       class MyClass {}
 *       function myfunction() {}
 *       const MYCONST = 1;
 *       $a = new MyClass;
 *       $c = new \my\name\MyClass;
 *       $a = strlen('hi');
 *       $d = namespace\MYCONST;
 *       $d = __NAMESPACE__ . '\MYCONST';
 *       echo constant($d);
 * NOTE
 *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT
 *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.
 */
/*
 * Return a PHP-style type name for a token, used in parse error messages.
 */
PH7_PRIVATE const char * TokenTypeName(sxu32 nType)
{
	if( nType & PH7_TK_INTEGER ){ return "integer"; }
	if( nType & PH7_TK_REAL ){ return "float"; }
	if( nType & (PH7_TK_DSTR|PH7_TK_SSTR|PH7_TK_HEREDOC|PH7_TK_NOWDOC) ){ return "string"; }
	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }
	if( nType & PH7_TK_ID ){ return "identifier"; }
	if( nType & PH7_TK_DOLLAR ){ return "variable"; }
	return "token";
}
PH7_PRIVATE sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)
{
	sxu32 nLine;
	sxi32 rc;
	nLine = pGen->pIn->nLine;
	pGen->pIn++; /* Jump the 'namespace' keyword */
	/* Reset namespace and clear previous use imports */
	SyBlobReset(&pGen->sNamespace);
	GenStateResetUseImports(&(*pGen),pGen->pVm);
	if( pGen->pIn >= pGen->pEnd ){
		/* Global namespace (bare "namespace;") */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);
		return SXRET_OK;
	}
	if( pGen->pIn->nType & PH7_TK_SEMI ){
		/* namespace; — switch to global namespace */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);
		return SXRET_OK;
	}
	if( pGen->pIn->nType & PH7_TK_OCB ){
		/* namespace { } — global namespace block */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);
		return SXRET_OK;
	}
	/* Collect the namespace path: namespace Foo\Bar\Baz */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP|PH7_TK_ID|PH7_TK_KEYWORD)) ){
		if( pGen->pIn->nType & PH7_TK_NSSEP ){
			/* Append backslash separator */
			if( SyBlobLength(&pGen->sNamespace) > 0 ){
				SyBlobAppend(&pGen->sNamespace,"\\",1);
			}
		}else{
			/* Append identifier */
			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);
		}
		pGen->pIn++;
	}
	/* Emit a runtime namespace switch so the VM tracks the active namespace
	 * at the correct program counter, not just the last one compiled. */
	{
		char *zNsDup = 0;
		if( SyBlobLength(&pGen->sNamespace) > 0 ){
			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));
		}
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI|PH7_TK_OCB)) == 0 ){
		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,
			"syntax error, unexpected %s \"%z\", expecting \"{\"",
			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	return SXRET_OK;
}
/*
 * Initialize the three use-import tables of a code generator.
 *
 * php resolves CLASS and FUNCTION imports case-INSENSITIVELY, like every other
 * name in those two families: `use A\Cee;` then `CEE::K`, `use A\Cee as Alias;`
 * then `ALIAS::K`, `use function A\eff;` then `EFF()`, and a wrong-case leading
 * segment of an imported namespace (`use A\B;` then `b\Cee::K`) all resolve.
 * So both tables fold through SyStrHash/SyStrnmicmp, exactly like hClass /
 * hMethod / hFunction.
 *
 * The CONST table stays BYTE-EXACT: php keeps constant names case-sensitive,
 * so `use const A\KAY;` followed by `kay` must remain an undefined constant.
 * That asymmetry is why the three tables exist separately.
 */
PH7_PRIVATE void GenStateInitUseImports(ph7_gen_state *pGen,ph7_vm *pVm)
{
	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,SyStrHash,SyStrnmicmp);
	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,SyStrHash,SyStrnmicmp);
	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);
}
/*
 * Drop every import currently in scope and start a fresh set (a namespace
 * switch clears imports).  Keeps the case rules of GenStateInitUseImports.
 */
PH7_PRIVATE void GenStateResetUseImports(ph7_gen_state *pGen,ph7_vm *pVm)
{
	SyHashRelease(&pGen->hUseImports);
	SyHashRelease(&pGen->hUseFuncImports);
	SyHashRelease(&pGen->hUseConstImports);
	GenStateInitUseImports(&(*pGen),&(*pVm));
}
/*
 * Register one resolved `use` import: alias -> FQN, in the table its KIND owns
 * (iUseType: 0 = class, 1 = function, 2 = const).  Shared by the plain form
 * (`use A\Cee;`) and by each member of a group (`use A\{Cee, Dee};`).
 */
static sxi32 GenStateAddImport(
	ph7_gen_state *pGen,  /* Code generator state */
	int iUseType,         /* 0=class, 1=function, 2=const */
	SyBlob *pPath,        /* Fully qualified name being imported */
	SyString *pAlias,     /* Short name it is imported under */
	sxu32 nLine           /* Line of the 'use' keyword (for diagnostics) */
	)
{
	SyHash *pGenHash;   /* Compile-time import table */
	char *zDup;
	sxi32 rc;
	/* Select the target hash table based on import type.  Class and function
	 * imports are resolved entirely at compile time; only const imports need a
	 * runtime table, which PH7_OP_USECONST fills so imports stay namespace-scoped. */
	switch( iUseType ){
		case 1:  pGenHash = &pGen->hUseFuncImports; break;
		case 2:  pGenHash = &pGen->hUseConstImports; break;
		default: pGenHash = &pGen->hUseImports; break;
	}
	/* Check for duplicate import alias (per-type) */
	if( SyHashGet(pGenHash,pAlias->zString,pAlias->nByte) != 0 ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
			"Cannot use %.*s as %z because the name is already in use",
			(int)SyBlobLength(pPath),(const char *)SyBlobData(pPath),pAlias);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Register the import: alias -> FQN.
	 * Strings are allocated from the VM pool allocator and freed
	 * when the entire VM is released. SyHashRelease does not free
	 * user-data, but pool memory is reclaimed in bulk at shutdown. */
	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
		(const char *)SyBlobData(pPath),SyBlobLength(pPath));
	if( zDup ){
		SyHashInsert(pGenHash,pAlias->zString,pAlias->nByte,zDup);
		if( iUseType == 2 ){
			/* Const imports: emit a runtime instruction so imports are
			 * namespace-scoped (NSSWITCH clears the VM table). */
			char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pAlias->zString,pAlias->nByte);
			if( zAliasDup ){
				/* Encode alias length in iP1, alias string in p3 is not enough —
				 * we need both alias and FQN.  Pack them: iP1=alias length,
				 * iP2 unused, p3 points to a two-pointer struct. */
				char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);
				if( azPair ){
					azPair[0] = zAliasDup;
					azPair[1] = zDup;
					PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)pAlias->nByte,0,azPair,0);
				}
			}
		}
	}
	return SXRET_OK;
}
/*
 * Collect one `\`-separated name into pOut (appending to whatever it holds, with
 * a separator when needed) and return its LAST segment token, or 0 when the
 * cursor is not on a name at all.
 */
static SyToken * GenStateCollectNsPath(ph7_gen_state *pGen,SyBlob *pOut)
{
	SyToken *pLast = 0;
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP|PH7_TK_ID)) ){
		if( pGen->pIn->nType & PH7_TK_ID ){
			pLast = pGen->pIn;
			if( SyBlobLength(pOut) > 0 ){
				SyBlobAppend(pOut,"\\",1);
			}
			SyBlobAppend(pOut,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);
		}
		pGen->pIn++;
	}
	return pLast;
}
/*
 * Consume the optional `as Alias` clause, leaving *pAlias untouched when absent.
 */
static void GenStateCollectImportAlias(ph7_gen_state *pGen,SyString *pAlias)
{
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)
		&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){
		pGen->pIn++; /* Jump 'as' */
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){
			*pAlias = pGen->pIn->sData;
			pGen->pIn++;
		}
	}
}
/*
 * Compile the members of a GROUP use declaration (php 7.0):
 *
 *      use A\{Cee, Dee as D2, Sub\Eee};
 *      use function A\{f, g as h};
 *      use A\{function f, const K, Cee};   // per-member kind, untyped group only
 *
 * pPrefix holds the path before the brace; the cursor sits on `{`.  Each member
 * is the prefix, a `\`, and the member's own (possibly multi-segment) name.  A
 * trailing comma is allowed, an empty group is not.
 */
static sxi32 GenStateCompileGroupUse(ph7_gen_state *pGen,SyBlob *pPrefix,int iUseType,sxu32 nLine)
{
	SyBlob sPath;
	sxi32 rc = SXRET_OK;
	pGen->pIn++; /* Jump '{' */
	SyBlobInit(&sPath,&pGen->pVm->sAllocator);
	for(;;){
		int iMemberType = iUseType;
		SyString sAlias;
		SyToken *pLast;
		/* `function`/`const` may qualify a single member, but only inside a
		 * group that is not itself typed (php rejects `use function A\{const C}`). */
		if( iUseType == 0 && pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){
			sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));
			if( nKey == PH7_TKWRD_FUNCTION ){
				iMemberType = 1;
				pGen->pIn++;
			}else if( nKey == PH7_TKWRD_CONST ){
				iMemberType = 2;
				pGen->pIn++;
			}
		}
		SyBlobReset(&sPath);
		SyBlobAppend(&sPath,SyBlobData(pPrefix),SyBlobLength(pPrefix));
		pLast = GenStateCollectNsPath(pGen,&sPath);
		if( pLast == 0 ){
			/* No member name: `use A\{};` or a stray token.  Report once, then
			 * skip to the end of the group so the statement does not cascade. */
			rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,
				"syntax error, unexpected %s \"%z\", expecting identifier",
				TokenTypeName(pGen->pIn < pGen->pEnd ? pGen->pIn->nType : 0),
				pGen->pIn < pGen->pEnd ? &pGen->pIn->sData : 0);
			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_CCB|PH7_TK_SEMI)) == 0 ){
				pGen->pIn++;
			}
			break;
		}
		sAlias = pLast->sData; /* Default alias is the member's last component */
		GenStateCollectImportAlias(pGen,&sAlias);
		rc = GenStateAddImport(&(*pGen),iMemberType,&sPath,&sAlias,nLine);
		if( rc == SXERR_ABORT ){
			break;
		}
		rc = SXRET_OK;
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){
			pGen->pIn++;
			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) ){
				break; /* Trailing comma before the closing brace */
			}
			continue;
		}
		break;
	}
	SyBlobRelease(&sPath);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) ){
		pGen->pIn++; /* Jump '}' */
	}
	return SXRET_OK;
}
/*
 * Compile the 'use' statement
 * According to the PHP language reference manual
 *  The ability to refer to an external fully qualified name with an alias or importing
 *  is an important feature of namespaces. This is similar to the ability of unix-based
 *  filesystems to create symbolic links to a file or to a directory.
 *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name
 *  aliasing an interface name, and aliasing a namespace name. Note that importing
 *  a function or constant is not supported.
 *  In PHP, aliasing is accomplished with the 'use' operator.
 * NOTE
 *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT
 *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.
 */
PH7_PRIVATE sxi32 PH7_CompileUse(ph7_gen_state *pGen)
{
	sxu32 nLine;
	sxi32 rc;
	SyBlob sPath;
	SyString sAlias;
	SyToken *pLast;
	int iUseType; /* 0=class, 1=function, 2=const */
	nLine = pGen->pIn->nLine;
	pGen->pIn++; /* Jump the 'use' keyword */
	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */
	iUseType = 0;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){
		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));
		if( nKey == PH7_TKWRD_FUNCTION ){
			iUseType = 1;
			pGen->pIn++;
		}else if( nKey == PH7_TKWRD_CONST ){
			iUseType = 2;
			pGen->pIn++;
		}
	}
	SyBlobInit(&sPath,&pGen->pVm->sAllocator);
	/* Process one or more use declarations separated by commas */
	for(;;){
		if( pGen->pIn >= pGen->pEnd ){
			break;
		}
		SyBlobReset(&sPath);
		/* Collect the full namespace path */
		pLast = GenStateCollectNsPath(pGen,&sPath);
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) && SyBlobLength(&sPath) > 0 ){
			/* GROUP declaration: what was collected is the shared prefix.  php
			 * does not let a group be comma-combined with another declaration,
			 * so the members close the statement. */
			rc = GenStateCompileGroupUse(&(*pGen),&sPath,iUseType,nLine);
			if( rc == SXERR_ABORT ){
				SyBlobRelease(&sPath);
				return SXERR_ABORT;
			}
			break;
		}
		if( pLast == 0 ){
			/* Empty path */
			break;
		}
		/* Default alias is the last component of the path */
		sAlias = pLast->sData;
		/* Check for explicit alias: use Foo\Bar as Baz */
		GenStateCollectImportAlias(pGen,&sAlias);
		rc = GenStateAddImport(&(*pGen),iUseType,&sPath,&sAlias,nLine);
		if( rc == SXERR_ABORT ){
			SyBlobRelease(&sPath);
			return SXERR_ABORT;
		}
		/* Check for comma (multiple use declarations) */
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){
			pGen->pIn++;
		}else{
			break;
		}
	}
	SyBlobRelease(&sPath);
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",
			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	return SXRET_OK;
}
/*
 * Compile the stupid 'declare' language construct.
 *
 * According to the PHP language reference manual.
 *  The declare construct is used to set execution directives for a block of code.
 *  The syntax of declare is similar to the syntax of other flow control constructs:
 *  declare (directive)
 *   statement
 * The directive section allows the behavior of the declare block to be set.
 *  Currently only two directives are recognized: the ticks directive and the encoding directive.
 * The statement part of the declare block will be executed - how it is executed and what side
 * effects occur during execution may depend on the directive set in the directive block.
 * The declare construct can also be used in the global scope, affecting all code following
 * it (however if the file with declare was included then it does not affect the parent file).
 * <?php
 * // these are the same:
 * // you can use this:
 * declare(ticks=1) {
 *   // entire script here
 * }
 * // or you can use this:
 * declare(ticks=1);
 * // entire script here
 * ?>
 *
 * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.
 */
/*
 * Match a directive name against a known literal (case-insensitive).
 */
static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)
{
	return SyStringLength(pName) == nWant
	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;
}

PH7_PRIVATE sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)
{
	sxu32 nLine = pGen->pIn->nLine;
	SyToken *pBodyEnd = 0;
	SyToken *pBodyStart;
	SyToken *pCursor;
	int bHasStrictTypes;
	int bBlockForm;
	int bPlacementOk;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'declare' keyword */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){
		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchro;
	}
	pGen->pIn++; /* Jump the left parenthesis */
	pBodyStart = pGen->pIn;
	/* Delimit the directive body (between the outer '(' and its matching ')'). */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);
	if( pBodyEnd >= pGen->pEnd ){
		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\")\"");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)
	 * now delimits the comma-separated directive list. */
	pGen->pIn = &pBodyEnd[1];
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/|PH7_TK_OCB/*'{'*/)) == 0 ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;
	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );
	bHasStrictTypes = 0;
	/* First pass: scan directive names to detect any strict_types occurrence.
	 * PHP applies strict_types placement and block-form rules as long as the
	 * directive appears anywhere in the list, before validating values. */
	pCursor = pBodyStart;
	while( pCursor < pBodyEnd ){
		if( (pCursor->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) != 0 ){
			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){
				bHasStrictTypes = 1;
				break;
			}
		}
		pCursor++;
	}
	if( bHasStrictTypes && bBlockForm ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
			"strict_types declaration must not use block mode");
		if( rc == SXERR_ABORT ) return SXERR_ABORT;
		return SXRET_OK;
	}
	if( bHasStrictTypes && !bPlacementOk ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
			"strict_types declaration must be the very first statement in the script");
		if( rc == SXERR_ABORT ) return SXERR_ABORT;
		return SXRET_OK;
	}
	/* Second pass: iterate comma-separated directives and apply each. */
	pCursor = pBodyStart;
	while( pCursor < pBodyEnd ){
		SyToken *pNameTok;
		SyToken *pEqTok;
		SyToken *pValTok;
		SyString *pDirName;
		int bIsStrict;
		int iStrictValue;
		pNameTok = pCursor;
		if( (pNameTok->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"declare: Expecting a directive name");
			if( rc == SXERR_ABORT ) return SXERR_ABORT;
			return SXRET_OK;
		}
		pEqTok = pNameTok + 1;
		if( pEqTok >= pBodyEnd || (pEqTok->nType & PH7_TK_EQUAL) == 0 ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"declare: Expecting '=' after directive name");
			if( rc == SXERR_ABORT ) return SXERR_ABORT;
			return SXRET_OK;
		}
		pValTok = pEqTok + 1;
		if( pValTok >= pBodyEnd ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"declare: Expecting value after '='");
			if( rc == SXERR_ABORT ) return SXERR_ABORT;
			return SXRET_OK;
		}
		pDirName = &pNameTok->sData;
		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);
		if( bIsStrict ){
			/* strict_types value must be a literal 0 or 1 (integer). PHP
			 * distinguishes non-literal (bareword) from other bad values. */
			if( (pValTok->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) != 0 ){
				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
					"declare(strict_types) value must be a literal");
				if( rc == SXERR_ABORT ) return SXERR_ABORT;
				return SXRET_OK;
			}
			iStrictValue = -1;
			if( pValTok->nType & PH7_TK_INTEGER ){
				const char *zv = SyStringData(&pValTok->sData);
				sxu32 nv = SyStringLength(&pValTok->sData);
				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;
				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;
			}
			if( iStrictValue != 0 && iStrictValue != 1 ){
				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
					"strict_types declaration must have 0 or 1 as its value");
				if( rc == SXERR_ABORT ) return SXERR_ABORT;
				return SXRET_OK;
			}
			pGen->bStrictTypes = (sxi8)iStrictValue;
		}else if( DeclareNameIs(pDirName, "encoding", sizeof("encoding")-1) ){
			/* php always ignores declare(encoding=...) unless it was built with
			 * Zend multibyte, and says so in these exact words. */
			PH7_GenCompileError(&(*pGen),E_WARNING,nLine,
				"declare(encoding=...) ignored because Zend multibyte feature is turned off by settings");
		}else{
			/* Other directives (ticks and friends) are accepted as no-ops.
			 * This used to emit a NOTICE naming the upstream engine and its
			 * version ("the declare construct is a no-op in the current release
			 * of the PH7(2.1.4) engine") — php prints nothing at all for
			 * `declare(ticks=1)`, and leaking the old engine's branding into
			 * user-visible diagnostics was wrong on its own. */
		}
		pCursor = pValTok + 1;
		/* Consume separating comma (or end). */
		if( pCursor < pBodyEnd ){
			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){
				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
					"declare: Expecting ',' or ')' after directive value");
				if( rc == SXERR_ABORT ) return SXERR_ABORT;
				return SXRET_OK;
			}
			pCursor++;
		}
	}
	/* Declares never lock the first-statement rule: PHP allows another
	 * declare(strict_types) to follow immediately, or a declare(ticks)
	 * to precede strict_types. Only non-declare statements lock. */
	return SXRET_OK;
Synchro:
	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/|PH7_TK_OCB/*'{'*/)) == 0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Compile a class constant.
 * According to the PHP language reference manual
 *  Class Constants
 *   It is possible to define constant values on a per-class basis remaining
 *   the same and unchangeable. Constants differ from normal variables in that
 *   you don't use the $ symbol to declare or use them.
 *   The value must be a constant expression, not (for example) a variable,
 *   a property, a result of a mathematical operation, or a function call.
 *   It's also possible for interfaces to have constants.
 * Symisc eXtension.
 *  PH7 allow any complex expression to be associated with the constant while
 *  the zend engine would allow only simple scalar value.
 *  Example:
 *   class Test{
 *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call
 *   };
 *   var_dump(TEST::MyConst);
 *   Refer to the official documentation for more information on the powerful extension
 *   introduced by the PH7 engine to the OO subsystem.
 */
/*
 * Exception handling.
 *  According to the PHP language reference manual
 *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded
 *    in a try block, to facilitate the catching of potential exceptions. Each try must have
 *    at least one corresponding catch block. Multiple catch blocks can be used to catch
 *    different classes of exceptions. Normal execution (when no exception is thrown within
 *    the try block, or when a catch matching the thrown exception's class is not present)
 *    will continue after that last catch block defined in sequence. Exceptions can be thrown
 *    (or re-thrown) within a catch block.
 *    When an exception is thrown, code following the statement will not be executed, and PHP
 *    will attempt to find the first matching catch block. If an exception is not caught, a PHP
 *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has
 *    been defined with set_exception_handler().
 *    The thrown object must be an instance of the Exception class or a subclass of Exception.
 *    Trying to throw an object that is not will result in a PHP Fatal Error.
 */
/*
 * Expression tree validator callback associated with the 'throw' statement.
 * Return SXRET_OK if the tree form a valid expression.Any other error
 * indicates failure.
 */
static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)
{
	/* php decides throwability at RUNTIME: `throw 5` compiles and raises the
	 * catchable Error "Can only throw objects" when it executes, and a
	 * non-Throwable object raises "Cannot throw objects that do not implement
	 * Throwable". This used to whitelist a handful of expression shapes and
	 * reject the rest at compile time as a "friendlier" error, which meant the
	 * program never ran and no catch could ever see it — and it was inconsistent
	 * anyway, since `throw $v` (a variable holding an int) always compiled and
	 * fell through to the same runtime path. OP_THROW now owns the whole rule,
	 * so accept any expression here; `throw;` with no operand is still rejected
	 * by PH7_CompileThrow's SXERR_EMPTY branch. */
	SXUNUSED(pGen);
	SXUNUSED(pRoot);
	return SXRET_OK;
}
/*
 * Compile a 'throw' statement.
 * throw: This is how you trigger an exception.
 * Each "throw" block must have at least one "catch" block associated with it.
 */
PH7_PRIVATE sxi32 PH7_CompileThrow(ph7_gen_state *pGen)
{
	sxu32 nLine = pGen->pIn->nLine;
	GenBlock *pBlock;
	sxu32 nIdx;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'throw' keyword */
	/* Compile the expression */
	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);
	if( rc == SXERR_EMPTY ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	pBlock = pGen->pCurrent;
	/* Point to the top most function or try block and emit the forward jump */
	while(pBlock->pParent){
		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION|GEN_BLOCK_FUNC) ){
			break;
		}
		/* Point to the parent block */
		pBlock = pBlock->pParent;
	}
	/* Emit the throw instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);
	/* Emit the jump */
	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);
	return SXRET_OK;
}
/*
 * Compile a PHP 8.0 'throw' expression.
 * Called from the expression code generator when a 'throw' keyword is
 * encountered in an expression context (e.g. `$x ?? throw new E()`).
 * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;
 * the validator guarantees the operand is a valid exception target.
 */
PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)
{
	sxu32 nLine = pGen->pIn->nLine;
	GenBlock *pBlock;
	sxu32 nIdx;
	sxi32 rc;
	(void)iCompileFlag;
	pGen->pIn++; /* Skip 'throw' */
	if( pGen->pIn >= pGen->pEnd ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
			"throw: Expecting an exception class instance");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	if( rc == SXERR_EMPTY ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
			"throw: Expecting an exception class instance");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* Walk up to nearest exception/function block for the jump target */
	pBlock = pGen->pCurrent;
	while( pBlock->pParent ){
		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION|GEN_BLOCK_FUNC) ){
			break;
		}
		pBlock = pBlock->pParent;
	}
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);
	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);
	return SXRET_OK;
}
/*
 * ROOT C: parse a single `catch (A | B $e)` header (no body) into an
 * ph7_exception_block. On success pGen->pIn is positioned at the catch body's
 * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body
 * compilation to the caller (which emits it inline). Returns SXRET_OK, or a
 * compile error propagated from the parser.
 */
static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)
{
	SyString sClassName;
	SyToken *pToken;
	SyString *pName;
	char *zDup;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'catch' keyword */
	SyZero(pCatch,sizeof(ph7_exception_block));
	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));
	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }
		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",
			TokenTypeName(pToken->nType),&pToken->sData);
		return SXERR_INVALID;
	}
	pGen->pIn++; /* '(' */
	for(;;){
		SyBlob sResolved;
		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);
		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){
			SyBlobRelease(&sResolved);
			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }
			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",
				TokenTypeName(pToken->nType),&pToken->sData);
			return SXERR_INVALID;
		}
		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));
		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));
		SyBlobRelease(&sResolved);
		if( zDup == 0 ){ return SXERR_ABORT; }
		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);
		if( rc != SXRET_OK ){ return SXERR_ABORT; }
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&
			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '|' ){
			pGen->pIn++; continue;
		}
		break;
	}
	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A|B)` with no
	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding. */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) ){
		pGen->pIn++; /* ')' */
		return SXRET_OK;
	}
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ||
		&pGen->pIn[1] >= pGen->pEnd || (pGen->pIn[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }
		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",
			TokenTypeName(pToken->nType),&pToken->sData);
		return SXERR_INVALID;
	}
	pGen->pIn++; /* '$' */
	pName = &pGen->pIn->sData;
	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);
	if( zDup == 0 ){ return SXERR_ABORT; }
	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){
		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }
		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",
			TokenTypeName(pToken->nType),&pToken->sData);
		return SXERR_INVALID;
	}
	pGen->pIn++; /* ')' */
	return SXRET_OK;
}
/*
 * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode
 * container. Used only for generator bodies so a `yield` inside a catch/finally
 * suspends correctly (the legacy path runs them via a detached VmLocalExec whose
 * pc/stack a generator resume cannot restore). Layout (see the block comment on
 * VmThrowException):
 *
 *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame
 *    <try body>
 *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)
 *    JMP  -> finally|end
 *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e
 *    <catch body>
 *    JMP  -> finally|end
 *    ... more catches ...
 *  Lfin: <finally body>
 *    END_FINALLY p3=pExc               ; dispatch pending action
 *  Lend:
 */
static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)
{
	sxu32 nLine = pGen->pIn->nLine;
	GenBlock *pTry;
	VmInstr *pInstr;
	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;
	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */
	sxi32 rc;
	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));
	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);
	if( rc != SXRET_OK ){ return SXERR_ABORT; }
	pTry->pUserData = pException;
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);
	pGen->pIn++; /* Jump the 'try' keyword */
	rc = PH7_CompileBlock(&(*pGen),0);
	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }
	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));
	iLpop = PH7_VmInstrLength(pGen->pVm);
	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */
	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);
	if( pInstr ){ pInstr->iP2 = iLpop; }
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);
	GenStateLeaveBlock(&(*pGen),0);
	/* Normal-completion jump -> finally or end (target fixed after layout) */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);
	/* Catch clauses (inline) */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&
		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){
		sxu32 k = 0;
		for(;;){
			ph7_exception_block sCatch;
			GenBlock *pCatchBlk;
			sxu32 idxJmp = 0;
			if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_KEYWORD) == 0
				|| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){
				break;
			}
			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);
			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }
			if( rc != SXRET_OK ){ return SXERR_INVALID; }
			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);
			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);
			if( rc != SXRET_OK ){ return SXERR_ABORT; }
			/* Tag the catch block with its try so a break/continue leaving the catch counts
			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch
			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */
			pCatchBlk->pUserData = pException;
			rc = PH7_CompileBlock(&(*pGen),0);
			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }
			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));
			GenStateLeaveBlock(&(*pGen),0);
			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a
			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);
			SySetPut(&aCatchJmp,(const void *)&idxJmp);
			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);
			if( rc != SXRET_OK ){ return SXERR_ABORT; }
			k++;
		}
	}
	/* Finally (inline) */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&
		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){
		GenBlock *pFinBlk;
		pGen->pIn++; /* Jump 'finally' */
		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);
		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);
		if( rc != SXRET_OK ){ return SXERR_ABORT; }
		rc = PH7_CompileBlock(&(*pGen),0);
		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }
		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));
		GenStateLeaveBlock(&(*pGen),0);
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);
		pException->iHasFinally = 1;
	}
	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);
	pException->iInlined = 1;
	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */
	{
		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;
		sxu32 *aJ; sxu32 n;
		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);
		if( pInstr ){ pInstr->iP2 = iTarget; }
		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);
		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){
			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);
			if( pInstr ){ pInstr->iP2 = iTarget; }
		}
	}
	SySetRelease(&aCatchJmp);
	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){
		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");
	}
	return SXRET_OK;
}
/*
 * Compile a 'catch' block.
 * Catch: A "catch" block retrieves an exception and creates
 * an object containing the exception information.
 */
static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)
{
	sxu32 nLine = pGen->pIn->nLine;
	ph7_exception_block sCatch;
	SySet *pInstrContainer;
	SyString sClassName;
	GenBlock *pCatch;
	SyToken *pToken;
	SyString *pName;
	char *zDup;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'catch' keyword */
	/* Zero the structure */
	SyZero(&sCatch,sizeof(ph7_exception_block));
	/* Initialize fields */
	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));
	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){
			/* Unexpected token,break immediately */
			pToken = pGen->pIn;
			if( pToken >= pGen->pEnd ){
				pToken--;
			}
			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,
				"syntax error, unexpected %s \"%z\"",
				TokenTypeName(pToken->nType),&pToken->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			return SXERR_INVALID;
	}
	/* Extract the exception class(es) — supports multi-catch: catch (A | B $e) */
	pGen->pIn++; /* Jump the left parenthesis '(' */
	for(;;){
		SyBlob sResolved;
		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);
		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){
			SyBlobRelease(&sResolved);
			pToken = pGen->pIn;
			if( pToken >= pGen->pEnd ){
				pToken--;
			}
			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,
				"syntax error, unexpected %s \"%z\"",
				TokenTypeName(pToken->nType),&pToken->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			return SXERR_INVALID;
		}
		/* Persist the FQN beyond this function — aClasses outlives the
		 * transient SyBlob allocation. */
		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));
		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));
		SyBlobRelease(&sResolved);
		if( zDup == 0 ){
			goto Mem;
		}
		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);
		if( rc != SXRET_OK ){
			goto Mem;
		}
		/* Check for '|' (multi-catch separator) */
		if( pGen->pIn < pGen->pEnd &&
			(pGen->pIn->nType & PH7_TK_OP) &&
			pGen->pIn->sData.nByte == 1 &&
			pGen->pIn->sData.zString[0] == '|' ){
			pGen->pIn++; /* Consume the '|' */
			continue;
		}
		break;
	}
	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A|B)` with no
	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding;
	 * jump straight to compiling the block below. */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) /*)*/ ){
		goto CatchBody;
	}
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ ||
		&pGen->pIn[1] >= pGen->pEnd || (pGen->pIn[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
			/* Unexpected token,break immediately */
			pToken = pGen->pIn;
			if( pToken >= pGen->pEnd ){
				pToken--;
			}
			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,
				"syntax error, unexpected %s \"%z\"",
				TokenTypeName(pToken->nType),&pToken->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			return SXERR_INVALID;
	}
	pGen->pIn++; /* Jump the dollar sign */
	/* Duplicate instance name */
	pName = &pGen->pIn->sData;
	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);
	if( zDup == 0 ){
		goto Mem;
	}
	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);
	pGen->pIn++;
CatchBody:
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){
		/* Unexpected token,break immediately */
		pToken = pGen->pIn;
		if( pToken >= pGen->pEnd ){
			pToken--;
		}
		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,
			"syntax error, unexpected %s \"%z\"",
			TokenTypeName(pToken->nType),&pToken->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		return SXERR_INVALID;
	}
	/* Compile the block */
	pGen->pIn++; /* Jump the right parenthesis */
	/* Create the catch block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Swap bytecode container */
	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);
	/* Compile the block */
	PH7_CompileBlock(&(*pGen),0);
	/* Fix forward jumps now the destination is resolved  */
	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));
	/* Emit the DONE instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);
	/* Leave the block */
	GenStateLeaveBlock(&(*pGen),0);
	/* Restore the default container */
	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	/* Install the catch block */
	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);
	if( rc != SXRET_OK ){
		goto Mem;
	}
	return SXRET_OK;
Mem:
	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");
	return SXERR_ABORT;
}
/*
 * Compile a 'try' block.
 * A function using an exception should be in a "try" block.
 * If the exception does not trigger, the code will continue
 * as normal. However if the exception triggers, an exception
 * is "thrown".
 */
PH7_PRIVATE sxi32 PH7_CompileTry(ph7_gen_state *pGen)
{
	ph7_exception *pException;
	sxu32 nLine = pGen->pIn->nLine;
	GenBlock *pTry;
	sxu32 nJmpIdx;
	sxi32 rc;
	/* Create the exception container */
	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));
	if( pException == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,
			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");
		return SXERR_ABORT;
	}
	/* Zero the structure */
	SyZero(pException,sizeof(ph7_exception));
	/* Initialize fields */
	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));
	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));
	pException->iHasFinally = 0;
	pException->iFinallyDone = 0;
	pException->pVm = pGen->pVm;
	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a
	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.
	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,
	 * VmThrowException pc-redirect, return/break-through-finally threading, generator
	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet
	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */
	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){
		return PH7_CompileTryInline(&(*pGen),pException);
	}
	/* Create the try block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Store exception pointer so break/continue can emit POP_EXCEPTION */
	pTry->pUserData = pException;
	/* Emit the 'LOAD_EXCEPTION' instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);
	/* Fix the jump later when the destination is resolved */
	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);
	pGen->pIn++; /* Jump the 'try' keyword */
	/* Compile the block */
	rc = PH7_CompileBlock(&(*pGen),0);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* Fix forward jumps now the destination is resolved */
	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));
	/* Emit the 'POP_EXCEPTION' instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);
	/* Leave the block */
	GenStateLeaveBlock(&(*pGen),0);
	/* Compile catch block(s) — at least one catch or finally is required */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&
		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){
		/* Compile one or more catch blocks */
		for(;;){
			if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_KEYWORD) == 0
				|| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){
					break;
			}
			rc = PH7_CompileCatch(&(*pGen),pException);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
	}
	/* Compile optional finally block */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&
		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){
		SySet *pInstrContainer;
		GenBlock *pFinBlock;
		pGen->pIn++; /* Jump the 'finally' keyword */
		/* Create the finally block for jump fixup bookkeeping */
		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);
		if( rc != SXRET_OK ){
			return SXERR_ABORT;
		}
		/* Swap bytecode container */
		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);
		/* Compile the finally body */
		rc = PH7_CompileBlock(&(*pGen),0);
		if( rc == SXERR_ABORT ){
			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
			return SXERR_ABORT;
		}
		/* Fix forward jumps now the destination is resolved */
		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));
		/* Emit DONE to terminate the finally block */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);
		/* Leave the block */
		GenStateLeaveBlock(&(*pGen),0);
		/* Restore the default container */
		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
		pException->iHasFinally = 1;
	}
	/* Must have at least one catch or finally */
	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
			"Cannot use try without catch or finally");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	return SXRET_OK;
}
/*
 * Compile a switch block.
 *  (See block-comment below for more information)
 */
static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)
{
	sxi32 rc = SXRET_OK;
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/|PH7_TK_COLON/*':'*/)) == 0 ){
		/* Unexpected token */
		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		pGen->pIn++;
	}
	pGen->pIn++;
	/* First instruction to execute in this block. */
	*pBlockStart = PH7_VmInstrLength(pGen->pVm);
	/* Compile the block until we hit a case/default/endswitch keyword
	 * or the '}' token */
	for(;;){
		if( pGen->pIn >= pGen->pEnd ){
			/* No more input to process */
			break;
		}
		rc = SXRET_OK;
		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){
			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){
				if( iTokenDelim != PH7_TK_CCB ){
					/* Unexpected token */
					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",
						&pGen->pIn->sData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					/* FALL THROUGH */
				}
				rc = SXERR_EOF;
				break;
			}
		}else{
			sxi32 nKwrd;
			/* Extract the keyword */
			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
			if( nKwrd == PH7_TKWRD_CASE || nKwrd == PH7_TKWRD_DEFAULT ){
				break;
			}
			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){
				if( iTokenDelim != PH7_TK_KEYWORD ){
					/* Unexpected token */
					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",
						&pGen->pIn->sData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					/* FALL THROUGH */
				}
				/* Block compiled */
				break;
			}
		}
		/* Compile block */
		rc = PH7_CompileBlock(&(*pGen),0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	return rc;
}
/*
 * Compile a case eXpression.
 *  (See block-comment below for more information)
 */
static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)
{
	SySet *pInstrContainer;
	SyToken *pEnd,*pTmp;
	sxi32 iNest = 0;
	sxi32 rc;
	/* Delimit the expression */
	pEnd = pGen->pIn;
	while( pEnd < pGen->pEnd ){
		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){
			/* Increment nesting level */
			iNest++;
		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){
			/* Decrement nesting level */
			iNest--;
		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/|PH7_TK_COLON/*;'*/) && iNest < 1 ){
			break;
		}
		pEnd++;
	}
	if( pGen->pIn >= pEnd ){
		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
	}
	/* Swap token stream */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);
	rc = PH7_CompileExpr(&(*pGen),0,0);
	/* Emit the done instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);
	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	/* Update token stream */
	pGen->pIn  = pEnd;
	pGen->pEnd = pTmp;
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	return SXRET_OK;
}
/*
 * Compile the smart switch statement.
 * According to the PHP language reference manual
 *  The switch statement is similar to a series of IF statements on the same expression.
 *  In many occasions, you may want to compare the same variable (or expression) with many
 *  different values, and execute a different piece of code depending on which value it equals to.
 *  This is exactly what the switch statement is for.
 *  Note: Note that unlike some other languages, the continue statement applies to switch and acts
 *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration
 *  of the outer loop, use continue 2.
 *  Note that switch/case does loose comparision.
 *  It is important to understand how the switch statement is executed in order to avoid mistakes.
 *  The switch statement executes line by line (actually, statement by statement).
 *  In the beginning, no code is executed. Only when a case statement is found with a value that
 *  matches the value of the switch expression does PHP begin to execute the statements.
 *  PHP continues to execute the statements until the end of the switch block, or the first time
 *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.
 *  In a switch statement, the condition is evaluated only once and the result is compared to each
 *  case statement. In an elseif statement, the condition is evaluated again. If your condition
 *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.
 *  The statement list for a case can also be empty, which simply passes control into the statement
 *  list for the next case.
 *  The case expression may be any expression that evaluates to a simple type, that is, integer
 *  or floating-point numbers and strings.
 */
PH7_PRIVATE sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)
{
	GenBlock *pSwitchBlock;
	SyToken *pTmp,*pEnd;
	ph7_switch *pSwitch;
	sxu32 nToken;
	sxu32 nLine;
	sxi32 rc;
	nLine = pGen->pIn->nLine;
	/* Jump the 'switch' keyword */
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Jump the left parenthesis '(' */
	pGen->pIn++;
	pEnd = 0; /* cc warning */
	/* Create the loop block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP|GEN_BLOCK_SWITCH,
		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Delimit the condition */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
	if( pGen->pIn == pEnd || pEnd >= pGen->pEnd ){
		/* Empty expression */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
	}
	/* Swap token streams */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	/* Compile the expression */
	rc = PH7_CompileExpr(&(*pGen),0,0);
	if( rc == SXERR_ABORT ){
		/* Expression handler request an operation abort [i.e: Out-of-memory] */
		return SXERR_ABORT;
	}
	/* Update token stream */
	while(pGen->pIn < pEnd ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
			"Switch: Unexpected token '%z'",&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		pGen->pIn++;
	}
	pGen->pIn  = &pEnd[1];
	pGen->pEnd = pTmp;
	if( pGen->pIn >= pGen->pEnd || &pGen->pIn[1] >= pGen->pEnd ||
		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/|PH7_TK_COLON/*:*/)) == 0 ){
			pTmp = pGen->pIn;
			if( pTmp >= pGen->pEnd ){
				pTmp--;
			}
			/* Unexpected token */
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
	}
	/* Set the delimiter token */
	if( pGen->pIn->nType & PH7_TK_COLON ){
		nToken = PH7_TK_KEYWORD;
		/* Stop compilation when the 'endswitch;' keyword is seen */
	}else{
		nToken = PH7_TK_CCB; /* '}' */
	}
	pGen->pIn++; /* Jump the leading curly braces/colons */
	/* Create the switch blocks container */
	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));
	if( pSwitch == 0 ){
		/* Abort compilation */
		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	/* Zero the structure */
	SyZero(pSwitch,sizeof(ph7_switch));
	/* Initialize fields */
	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));
	/* Emit the switch instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);
	/* Compile case blocks */
	for(;;){
		sxu32 nKwrd;
		if( pGen->pIn >= pGen->pEnd ){
			/* No more input to process */
			break;
		}
		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){
			if( nToken != PH7_TK_CCB || (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){
				/* Unexpected token */
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",
					&pGen->pIn->sData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				/* FALL THROUGH */
			}
			/* Block compiled */
			break;
		}
		/* Extract the keyword */
		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){
			if( nToken != PH7_TK_KEYWORD ){
				/* Unexpected token */
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",
					&pGen->pIn->sData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				/* FALL THROUGH */
			}
			/* Block compiled */
			break;
		}
		if( nKwrd == PH7_TKWRD_DEFAULT ){
			/*
			 * Accroding to the PHP language reference manual
			 *  A special case is the default case. This case matches anything
			 *  that wasn't matched by the other cases.
			 */
			if( pSwitch->nDefault > 0 ){
				/* Default case already compiled */
				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			pGen->pIn++; /* Jump the 'default' keyword */
			/* Compile the default block */
			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);
			if( rc == SXERR_ABORT){
				return SXERR_ABORT;
			}else if( rc == SXERR_EOF ){
				break;
			}
		}else if( nKwrd == PH7_TKWRD_CASE ){
			ph7_case_expr sCase;
			/* Standard case block */
			pGen->pIn++; /* Jump the 'case' keyword */
			/* initialize the structure */
			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));
			/* Compile the case expression */
			rc = GenStateCompileCaseExpr(pGen,&sCase);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			/* Compile the case block */
			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);
			/* Insert in the switch container */
			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);
			if( rc == SXERR_ABORT){
				return SXERR_ABORT;
			}else if( rc == SXERR_EOF ){
				break;
			}
		}else{
			/* Unexpected token */
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",
				&pGen->pIn->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			break;
		}
	}
	/* Fix all jumps now the destination is resolved */
	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);
	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));
	/* Release the loop block */
	GenStateLeaveBlock(pGen,0);
	if( pGen->pIn < pGen->pEnd ){
		/* Jump the trailing curly braces or the endswitch keyword*/
		pGen->pIn++;
	}
	/* Statement successfully compiled */
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
