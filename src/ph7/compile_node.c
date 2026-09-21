/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include "compile_int.h"
/*
 * Section:
 *    Expression-node compilation: anonymous functions/closures, arrow
 *    functions and their capture scan, match expressions, backtick,
 *    language constructs, variables and literal/name resolution.
 * Status:
 *    Stable.
 */
/*
 * Compile an annoynmous function or a closure.
 * According to the PHP language reference
 *  Anonymous functions, also known as closures, allow the creation of functions
 *  which have no specified name. They are most useful as the value of callback
 *  parameters, but they have many other uses. Closures can also be used as
 *  the values of variables; Assigning a closure to a variable uses the same
 *  syntax as any other assignment, including the trailing semicolon:
 *  Example Anonymous function variable assignment example
 * <?php
 * $greet = function($name)
 * {
 *    printf("Hello %s\r\n", $name);
 * };
 * $greet('World');
 * $greet('PHP');
 * ?>
 * Note that the implementation of annoynmous function and closure under
 * PH7 is completely different from the one used by the zend engine.
 */
PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */
	char zName[512];         /* Unique lambda name */
	static int iCnt = 1;     /* There is no worry about thread-safety here,because only
							  * one thread is allowed to compile the script.
						      */
	SyString sName;
	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia
	                              * is keyed to this ['static'] 'function' token */
	sxu32 nKwLine;
	sxi32 iFlags = 0;
	sxu32 nLen;
	sxi32 rc;
	SXUNUSED(iCompileFlag); /* cc warning */

	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */
	if( (pGen->pIn->nType & PH7_TK_KEYWORD)
		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){
		/* Static closure: no $this auto-capture, bind refused */
		iFlags |= VM_FUNC_STATIC_CL;
		pGen->pIn++; /* Jump the 'static' keyword */
	}
	pGen->pIn++; /* Jump the 'function' keyword */
	if( pGen->pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD) ){
		pGen->pIn++;
	}
	/* Generate a unique name */
	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);
	/* Make sure the generated name is unique */
	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){
		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);
	}
	SyStringInitFromBuf(&sName,zName,nLen);
	/* Compile the lambda body */
	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	if( pAnnonFunc ){
		pAnnonFunc->nLine = nKwLine;
		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia
		 * sidecar keys them to the closure's first keyword token. */
		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for
	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);
	 * the handler wraps either in a Closure instance. */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Add a free variable to the arrow function's closure environment, unless
 * it is 'this' (handled separately), is shadowed by a parameter at any
 * enclosing arrow level, or has already been captured.
 */
static sxi32 GenStateArrowAddCapture(
	ph7_gen_state *pGen,
	ph7_vm_func *pFunc,
	const char *zName,
	sxu32 nByte,
	SyString *aShadow,
	sxu32 nShadow)
{
	ph7_vm_func_closure_env sEnv;
	ph7_vm_func_closure_env *aEnv;
	sxu32 n, nEnv;
	char *zDup;
	if( nByte == 0 ){
		return SXRET_OK;
	}
	if( nByte == sizeof("this")-1
		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){
		return SXRET_OK;
	}
	for( n = 0 ; n < nShadow ; n++ ){
		if( SyStringLength(&aShadow[n]) == nByte
			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){
			return SXRET_OK;
		}
	}
	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);
	nEnv = SySetUsed(&pFunc->aClosureEnv);
	for( n = 0 ; n < nEnv ; n++ ){
		if( SyStringLength(&aEnv[n].sName) == nByte
			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){
			return SXRET_OK;
		}
	}
	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);
	if( zDup == 0 ){
		return SXERR_ABORT;
	}
	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));
	sEnv.iFlags = 0;
	sEnv.nIdx = SXU32_HIGH;
	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);
	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);
	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);
	return SXRET_OK;
}
/*
 * Walk the raw body of a double-quoted string or heredoc, extracting every
 * unescaped $<identifier> reference. The semantics mirror the "simple
 * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,
 * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.
 */
static sxi32 GenStateArrowScanInterpolatedString(
	ph7_gen_state *pGen,
	ph7_vm_func *pFunc,
	const char *zIn,
	const char *zEnd,
	SyString *aShadow,
	sxu32 nShadow)
{
	sxi32 rc;
	while( zIn < zEnd ){
		if( zIn[0] == '\\' ){
			zIn++;
			if( zIn < zEnd ){
				zIn++;
			}
			continue;
		}
		if( zIn[0] == '$' && &zIn[1] < zEnd
			&& ((unsigned char)zIn[1] >= 0xc0
				|| SyisAlpha(zIn[1]) || zIn[1] == '_') ){
			const char *zName;
			zIn++; /* skip '$' */
			zName = zIn;
			while( zIn < zEnd ){
				unsigned char c = (unsigned char)zIn[0];
				if( c >= 0xc0 ){
					zIn++;
					while( zIn < zEnd
						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){
						zIn++;
					}
					continue;
				}
				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){
					break;
				}
				zIn++;
			}
			if( zIn > zName ){
				rc = GenStateArrowAddCapture(pGen,pFunc,zName,
					(sxu32)(zIn - zName),aShadow,nShadow);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			continue;
		}
		zIn++;
	}
	return SXRET_OK;
}
/*
 * Scan the body token range of an arrow function for free-variable
 * references and record them in pFunc's closure environment. Handles:
 *   - plain $<id> pairs
 *   - variables inside "..." and heredocs (via interpolation scan)
 *   - nested arrow functions: descends into the inner body with the inner
 *     parameters added to the shadow list, so a variable referenced by a
 *     nested arrow that is not the inner's parameter is captured by the
 *     OUTER (enabling transitive capture), while the inner's own params
 *     are never mistakenly captured.
 */
static sxi32 GenStateArrowCaptureScan(
	ph7_gen_state *pGen,
	ph7_vm_func *pFunc,
	SyToken *pStart,
	SyToken *pEnd,
	SyString *aShadow,
	sxu32 nShadow)
{
	SyToken *pScan = pStart;
	sxi32 rc;
	while( pScan < pEnd ){
		if( pScan->nType & (PH7_TK_DSTR|PH7_TK_HEREDOC) ){
			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,
				pScan->sData.zString,
				pScan->sData.zString + pScan->sData.nByte,
				aShadow,nShadow);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			pScan++;
			continue;
		}
		if( pScan->nType & PH7_TK_KEYWORD ){
			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);
			SyToken *pFnKw = pScan;
			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd
				&& (pScan[1].nType & PH7_TK_KEYWORD)
				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){
				pFnKw = &pScan[1];
				nKw = PH7_TKWRD_FN;
			}
			if( nKw == PH7_TKWRD_FN ){
				SyToken *pInnerSigStart;
				SyToken *pInnerSigEnd;
				SyToken *pInnerBodyEnd;
				SyString *aInnerShadow;
				sxu32 nInnerShadow;
				sxu32 nInnerParamMax;
				SyToken *p;
				int iNestInner;
				pScan = pFnKw + 1; /* past 'fn' */
				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){
					pScan++;
				}
				if( pScan >= pEnd || (pScan->nType & PH7_TK_LPAREN) == 0 ){
					pScan++;
					continue;
				}
				pInnerSigStart = ++pScan; /* past '(' */
				PH7_DelimitNestedTokens(pScan,pEnd,
					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);
				if( pInnerSigEnd >= pEnd ){
					pScan = pEnd;
					continue;
				}
				/* Build an augmented shadow list: inherited + inner params */
				nInnerParamMax = 0;
				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){
					if( p->nType & PH7_TK_DOLLAR ){
						nInnerParamMax++;
					}
				}
				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(
					&pGen->pVm->sAllocator,
					sizeof(SyString) * (nShadow + nInnerParamMax + 1));
				if( aInnerShadow == 0 ){
					return SXERR_ABORT;
				}
				nInnerShadow = 0;
				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){
					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];
				}
				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){
					if( (p->nType & PH7_TK_DOLLAR) == 0 ){
						continue;
					}
					if( &p[1] >= pInnerSigEnd ){
						break;
					}
					if( (p[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
						continue;
					}
					aInnerShadow[nInnerShadow++] = p[1].sData;
				}
				pScan = &pInnerSigEnd[1]; /* past ')' */
				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){
					pScan++;
					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)
						&& pScan->sData.nByte == 1
						&& pScan->sData.zString[0] == '?' ){
						pScan++;
					}
					if( pScan < pEnd
						&& (pScan->nType & (PH7_TK_KEYWORD|PH7_TK_ID)) ){
						pScan++;
					}
				}
				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){
					pScan++; /* past '=>' */
				}
				pInnerBodyEnd = pScan;
				iNestInner = 0;
				while( pInnerBodyEnd < pEnd ){
					if( iNestInner == 0 && (pInnerBodyEnd->nType &
						(PH7_TK_COMMA|PH7_TK_SEMI|PH7_TK_RPAREN
						 |PH7_TK_CSB|PH7_TK_CCB)) ){
						break;
					}
					if( pInnerBodyEnd->nType &
						(PH7_TK_LPAREN|PH7_TK_OSB|PH7_TK_OCB) ){
						iNestInner++;
					}else if( pInnerBodyEnd->nType &
						(PH7_TK_RPAREN|PH7_TK_CSB|PH7_TK_CCB) ){
						iNestInner--;
					}
					pInnerBodyEnd++;
				}
				/* Scan the inner arrow's default-parameter VALUES as part of
				 * the outer's body: a default value is evaluated at call time
				 * in the outer frame, so any free variable it references is
				 * an outer capture. We must NOT scan the parameter-name
				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)
				 * or those names leak into the outer's closure environment.
				 *
				 * Walk the signature argument-by-argument, splitting on
				 * top-level commas, and for each argument scan only the token
				 * range after the '=' sign. */
				{
					SyToken *pArgStart = pInnerSigStart;
					while( pArgStart < pInnerSigEnd ){
						SyToken *pArgEnd = pArgStart;
						SyToken *pEq = 0;
						int iNestArg = 0;
						while( pArgEnd < pInnerSigEnd ){
							if( iNestArg == 0
								&& (pArgEnd->nType & PH7_TK_COMMA) ){
								break;
							}
							if( pArgEnd->nType &
								(PH7_TK_LPAREN|PH7_TK_OSB|PH7_TK_OCB) ){
								iNestArg++;
							}else if( pArgEnd->nType &
								(PH7_TK_RPAREN|PH7_TK_CSB|PH7_TK_CCB) ){
								iNestArg--;
							}
							if( pEq == 0 && iNestArg == 0
								&& (pArgEnd->nType & PH7_TK_EQUAL) ){
								pEq = pArgEnd;
							}
							pArgEnd++;
						}
						if( pEq && (pEq + 1) < pArgEnd ){
							rc = GenStateArrowCaptureScan(pGen,pFunc,
								pEq + 1,pArgEnd,aShadow,nShadow);
							if( rc == SXERR_ABORT ){
								return SXERR_ABORT;
							}
						}
						pArgStart = pArgEnd;
						if( pArgStart < pInnerSigEnd
							&& (pArgStart->nType & PH7_TK_COMMA) ){
							pArgStart++;
						}
					}
				}
				rc = GenStateArrowCaptureScan(pGen,pFunc,
					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				pScan = pInnerBodyEnd;
				continue;
			}
		}
		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){
			pScan++;
			continue;
		}
		{
			/* Walk past variable-variable chains ($$x) to the base name. */
			SyToken *pDollar = pScan;
			while( &pDollar[1] < pEnd
				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){
				pDollar++;
			}
			if( &pDollar[1] >= pEnd ){
				break;
			}
			if( (pDollar[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
				pScan = pDollar + 1;
				continue;
			}
			rc = GenStateArrowAddCapture(pGen,pFunc,
				pDollar[1].sData.zString,pDollar[1].sData.nByte,
				aShadow,nShadow);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			pScan = pDollar + 2;
		}
	}
	return SXRET_OK;
}
/*
 * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr
 * Arrow functions are always closures that auto-capture enclosing-scope
 * variables by value. The body is a single expression that acts as an
 * implicit return. Unless prefixed with 'static', the enclosing object's
 * $this is also made available.
 */
PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	ph7_vm_func *pFunc;
	ph7_vm_func_closure_env sEnv;
	GenBlock *pBlock;
	SySet *pInstrContainer;
	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */
	SyToken *pBodyStart;   /* First token after '=>' */
	SyToken *pBodyEnd;     /* Token just past the last body token */
	SyToken *pSavedEnd;
	ph7_vm_func_arg *aArgs;
	char zName[512];
	static int iCnt = 1;
	char *zDup;
	SyToken *pTokKw;
	sxu32 nLen;
	sxu32 nLine;
	sxi32 iFlags = 0;
	int bStatic = 0;
	sxi32 rc;
	sxu32 n;
	SXUNUSED(iCompileFlag); /* cc warning */

	nLine = pGen->pIn->nLine;
	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */
	pTokKw = pGen->pIn;
	/* Optional 'static' prefix */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)
		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){
		bStatic = 1;
		iFlags |= VM_FUNC_STATIC_CL;
		pGen->pIn++;
	}
	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_KEYWORD) == 0
		|| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){
		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
			"Arrow function: expected 'fn' keyword");
		return SXERR_SYNTAX;
	}
	pGen->pIn++; /* Jump 'fn' */
	/* Optional '&' — return by reference */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){
		iFlags |= VM_FUNC_REF_RETURN;
		pGen->pIn++;
	}
	/* Expect '(' */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		if( pGen->pIn < pGen->pEnd ){
			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,
				"syntax error, unexpected %s \"%z\", expecting \"(\"",
				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);
		}else{
			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,
				"syntax error, unexpected end of file, expecting \"(\"");
		}
		return SXERR_SYNTAX;
	}
	pGen->pIn++; /* Jump '(' */
	/* Delimit the parameter list */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);
	if( pSigEnd >= pGen->pEnd ){
		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,
			"syntax error, unexpected end of file, expecting \")\"");
		return SXERR_SYNTAX;
	}
	/* Allocate the function state */
	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));
	if( pFunc == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
			"Fatal, PH7 engine is running out of memory");
		return SXERR_ABORT;
	}
	/* Generate a unique lambda name */
	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);
	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){
		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);
	}
	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);
	if( zDup == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
			"Fatal, PH7 engine is running out of memory");
		return SXERR_ABORT;
	}
	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);
	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */
	pFunc->nLine = nLine;
	/* Expression-position attributes (`$f = #[A] fn () => …`) */
	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* Collect function arguments */
	if( pGen->pIn < pSigEnd ){
		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Point past ')' and parse optional return type */
	pGen->pIn = &pSigEnd[1];
	rc = GenStateParseReturnType(pGen,pFunc);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}else if( rc == SXERR_SYNTAX ){
		return SXERR_SYNTAX;
	}
	/* Expect '=>' */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){
		if( pGen->pIn < pGen->pEnd ){
			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,
				"syntax error, unexpected %s \"%z\", expecting \"=>\"",
				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);
		}else{
			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,
				"syntax error, unexpected end of file, expecting \"=>\"");
		}
		return SXERR_SYNTAX;
	}
	pGen->pIn++; /* Jump '=>' */
	pBodyStart = pGen->pIn;
	pBodyEnd = pGen->pEnd;
	/* Build the initial shadow list from the arrow's own parameters, then
	 * recursively collect free-variable references from the body. The scan
	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow
	 * functions with proper parameter shadowing for transitive capture. */
	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);
	{
		SyString *aShadow = 0;
		sxu32 nShadow = SySetUsed(&pFunc->aArgs);
		if( nShadow > 0 ){
			aShadow = (SyString *)SyMemBackendPoolAlloc(
				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);
			if( aShadow == 0 ){
				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
					"Fatal, PH7 engine is running out of memory");
				return SXERR_ABORT;
			}
			for( n = 0 ; n < nShadow ; n++ ){
				aShadow[n] = aArgs[n].sName;
			}
		}
		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,
			aShadow,nShadow);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Unless declared static, auto-capture $this so arrow functions used
	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the
	 * captured value is silently dropped when the enclosing scope has no
	 * $this. */
	if( !bStatic ){
		char *zThisDup;
		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);
		if( zThisDup == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
				"Fatal, PH7 engine is running out of memory");
			return SXERR_ABORT;
		}
		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));
		sEnv.iFlags = VM_FUNC_ARG_IGNORE;
		sEnv.nIdx = SXU32_HIGH;
		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);
		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);
		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);
	}
	/* Arrow functions are always closures; the ARROW mark tells OP_LOAD_CLOSURE
	 * these captures are implicit (auto-scanned) so an undefined one stays silent
	 * at creation — php only warns when the body reads it. */
	pFunc->iFlags |= VM_FUNC_CLOSURE | VM_FUNC_ARROW;
	/* Compile the body expression as an implicit return */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED|GEN_BLOCK_FUNC,
		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
			"PH7 engine is running out-of-memory");
		return SXERR_ABORT;
	}
	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);
	pSavedEnd = pGen->pEnd;
	pGen->pIn = pBodyStart;
	pGen->pEnd = pBodyEnd;
	/* The body is an implicit `return <expr>`, which READS its operands. Compile
	 * it read-only (like echo / string interpolation) so a lone undefined variable
	 * — e.g. `fn()=>$z` for an auto-capture that was undefined at creation and so
	 * never captured (see VmExecOpLoadClosure) — raises php's "Undefined variable"
	 * warning at the read instead of being loaded quietly as a plain expression
	 * statement (`$z;`, silent in both engines) would be. */
	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* The cursor stopped just past the body expression */
	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;
	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.
	 * Any throw-expression inside the body needs a valid jump target and a
	 * stack-balanced exit path — point its fixup at a separate OP_DONE with
	 * p1=0 emitted below, which does not pop the (absent) return value. */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);
	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);
	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	GenStateLeaveBlock(&(*pGen),0);
	/* Restore cursors; caller will re-synchronize via the node's pEnd */
	pGen->pIn = pBodyEnd;
	pGen->pEnd = pSavedEnd;
	/* Emit the load-closure instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);
	return SXRET_OK;
}
/*
 * Compile a single arm's expression range into a freshly-allocated
 * sub-bytecode container. The caller supplies the token range [pStart, pEnd).
 * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the
 * expression's value.
 */
static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,
	SyToken *pStart,SyToken *pStop,SySet *pOut)
{
	SySet *pInstrContainer;
	SyToken *pTmpIn,*pTmpEnd;
	GenBlock *pArmBlock;
	sxi32 rc;
	pTmpIn  = pGen->pIn;
	pTmpEnd = pGen->pEnd;
	pGen->pIn  = pStart;
	pGen->pEnd = pStop;
	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);
	/* Enter a local FUNC block so any throw-expression fixups register on it
	 * (and not on an outer try/catch whose instruction indices live in a
	 * different bytecode container). We resolve those fixups to a trailing
	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates
	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED|GEN_BLOCK_FUNC,
		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);
	if( rc != SXRET_OK ){
		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
		pGen->pIn  = pTmpIn;
		pGen->pEnd = pTmpEnd;
		return SXERR_ABORT;
	}
	rc = PH7_CompileExpr(&(*pGen),0,0);
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);
	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);
	GenStateLeaveBlock(&(*pGen),0);
	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	pGen->pIn  = pTmpIn;
	pGen->pEnd = pTmpEnd;
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	if( rc == SXERR_EMPTY ){
		return SXERR_EMPTY;
	}
	return SXRET_OK;
}
/*
 * Compile a PHP 8.0 match expression:
 *     match(subject){ cond_list => result, ..., default => result }
 * Match is an expression — on exit the match result is on top of the stack.
 * Strict comparison (===) is used between the subject and each condition.
 * No fallthrough. If no arm matches and no default is present, a fatal
 * Uncaught UnhandledMatchError is raised at runtime.
 */
/*
 * Emit a parse error for match and propagate SXERR_ABORT if the error
 * count limit has been reached. Otherwise returns SXERR_SYNTAX so the
 * caller can bail out of the current expression.
 */
static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)
{
	va_list ap;
	sxi32 rc;
	SyBlob sMsg;
	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);
	va_start(ap,zFmt);
	SyBlobFormatAp(&sMsg,zFmt,ap);
	va_end(ap);
	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */
	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));
	SyBlobRelease(&sMsg);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	return SXERR_SYNTAX;
}
/*
 * Scan a top-level token range inside a match body, stopping at the first
 * token whose type is in stopMask (not counting nested parens/brackets/braces).
 * Returns the stop token pointer (or pEnd if none found).
 */
static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)
{
	SyToken *pCur = pStart;
	int iNest = 0;
	while( pCur < pEnd ){
		if( pCur->nType & (PH7_TK_LPAREN|PH7_TK_OSB|PH7_TK_OCB) ){
			iNest++;
		}else if( pCur->nType & (PH7_TK_RPAREN|PH7_TK_CSB|PH7_TK_CCB) ){
			iNest--;
		}else if( iNest == 0 && (pCur->nType & stopMask) ){
			return pCur;
		}
		pCur++;
	}
	return pEnd;
}
PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	ph7_match *pMatch;
	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;
	int bHasDefault = 0;
	sxu32 nLine;
	sxi32 rc;
	SXUNUSED(iCompileFlag);
	nLine = pGen->pIn->nLine;
	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */
	/* Expect '(' */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		return GenStateMatchError(pGen,nLine,
			"syntax error, unexpected %s, expecting \"(\"",
			pGen->pIn < pGen->pEnd ? "token" : "end of file");
	}
	pGen->pIn++; /* Jump '(' */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);
	if( pSubjEnd >= pGen->pEnd ){
		return GenStateMatchError(pGen,nLine,
			"syntax error, unexpected end of file, expecting \")\"");
	}
	if( pGen->pIn >= pSubjEnd ){
		return GenStateMatchError(pGen,nLine,
			"syntax error, unexpected \")\", expecting match subject");
	}
	/* Compile subject inline — result stays on the caller's operand stack */
	pSavedEnd = pGen->pEnd;
	pGen->pEnd = pSubjEnd;
	rc = PH7_CompileExpr(&(*pGen),0,0);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	pGen->pEnd = pSavedEnd;
	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */
	/* Expect '{' */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_OCB) == 0 ){
		return GenStateMatchError(pGen,
			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,
			"syntax error, expecting \"{\" after match subject");
	}
	pGen->pIn++; /* Jump '{' */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);
	if( pBodyEnd >= pGen->pEnd ){
		return GenStateMatchError(pGen,nLine,
			"syntax error, unexpected end of file, expecting \"}\"");
	}
	/* Allocate ph7_match container */
	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));
	if( pMatch == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
			"Fatal, PH7 engine is running out of memory");
		return SXERR_ABORT;
	}
	SyZero(pMatch,sizeof(ph7_match));
	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));
	/* Iterate arms */
	while( pGen->pIn < pBodyEnd ){
		ph7_match_arm sArm;
		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;
		sxu32 nArmLine = pGen->pIn->nLine;
		SyZero(&sArm,sizeof(ph7_match_arm));
		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));
		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));
		/* 'default' arm? */
		if( (pGen->pIn->nType & PH7_TK_KEYWORD)
			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){
			if( bHasDefault ){
				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,
					"Match expressions may only contain one default arm");
				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;
			}
			sArm.bDefault = 1;
			bHasDefault = 1;
			pGen->pIn++;
			if( pGen->pIn >= pBodyEnd || (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){
				return GenStateMatchError(pGen,nArmLine,
					"syntax error, expecting \"=>\" after 'default'");
			}
			pGen->pIn++; /* Jump '=>' */
		}else{
			/* Condition list: cond (',' cond)* '=>' */
			pCondStart = pGen->pIn;
			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,
				PH7_TK_ARRAY_OP|PH7_TK_COMMA);
			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){
				SySet sCondBc;
				if( pCondStart >= pArrow ){
					return GenStateMatchError(pGen,nArmLine,
						"syntax error, empty match condition expression");
				}
				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));
				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				SySetPut(&sArm.aConds,(const void *)&sCondBc);
				pCondStart = &pArrow[1]; /* Skip ',' */
				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,
					PH7_TK_ARRAY_OP|PH7_TK_COMMA);
			}
			if( pArrow >= pBodyEnd || (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){
				return GenStateMatchError(pGen,nArmLine,
					"syntax error, expecting \"=>\" in match arm");
			}
			if( pCondStart >= pArrow ){
				return GenStateMatchError(pGen,nArmLine,
					"syntax error, empty match condition expression");
			}
			{
				SySet sCondBc;
				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));
				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				SySetPut(&sArm.aConds,(const void *)&sCondBc);
			}
			pGen->pIn = &pArrow[1]; /* Jump '=>' */
		}
		/* Compile result expression: up to top-level ',' or body end */
		pResStart = pGen->pIn;
		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);
		if( pResStart >= pResEnd ){
			return GenStateMatchError(pGen,nArmLine,
				"syntax error, expected expression after \"=>\"");
		}
		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		pGen->pIn = pResEnd;
		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){
			pGen->pIn++; /* Skip trailing ',' */
		}
		SySetPut(&pMatch->aArms,(const void *)&sArm);
	}
	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);
	return SXRET_OK;
}
/*
 * Compile a backtick quoted string.
 */
PH7_PRIVATE sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SXUNUSED(iCompileFlag);
	/*
	 * The backtick (`) operator was DEPRECATED by php (shell_exec() is the replacement).
	 * PHL targets php's *non-deprecated* surface, so it is a hard parse error — never
	 * compiled to a shell_exec() call.
	 */
	PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,
		"syntax error, the backtick (`) operator was removed, use shell_exec() instead");
	return SXERR_ABORT;
}
/*
 * Compile a function [i.e: die(),exit(),include(),...] which is a langauge
 * construct.
 */
PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SyString *pName;
	sxu32 nKeyID;
	sxi32 rc;
	/* Name of the language construct [i.e: echo,die...]*/
	pName = &pGen->pIn->sData;
	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);
	pGen->pIn++; /* Jump the language construct keyword */
	if( nKeyID == PH7_TKWRD_ECHO ){
		SyToken *pTmp,*pNext = 0;
		/* A STATEMENT `echo` never reaches here — it dispatches through the statement
		 * table. Arriving in expression position means source like
		 * `fopen('f','r') or echo "IO error";`, which was a Symisc extension and is a
		 * php parse error (§10: a PH7-ism that changes the meaning of valid source is a
		 * bug). The one legitimate expression-echo is the token a `<?= ... ?>` short tag
		 * synthesizes, which raises nExprEchoOk around its own compile. */
		if( pGen->nExprEchoOk < 1 ){
			PH7_GenSyntaxError(&(*pGen),pGen->pIn - 1,0);
			return SXERR_ABORT;
		}
		/* Compile arguments one after one */
		pTmp = pGen->pEnd;
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);
		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){
			if( pGen->pIn < pNext ){
				pGen->pEnd = pNext;
				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				if( rc != SXERR_EMPTY ){
					/* Ticket 1433-008: Optimization #1: Consume input directly
					 * without the overhead of a function call.
					 * This is a very powerful optimization that improve
					 * performance greatly.
					 */
					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);
				}
			}
			/* Jump trailing commas */
			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){
				pNext++;
			}
			pGen->pIn = pNext;
		}
		/* Restore token stream */
		pGen->pEnd = pTmp;
	}else{
		sxi32 nArg = 0;
		sxu32 nIdx = 0;
		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if(rc != SXERR_EMPTY ){
			nArg = 1;
		}
		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){
			ph7_value *pObj;
			/* Emit the call instruction */
			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
			if( pObj == 0 ){
				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");
				SXUNUSED(iCompileFlag); /* cc warning */
				return SXERR_ABORT;
			}
			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);
			/* Install in the literal table */
			GenStateInstallLiteral(&(*pGen),pObj,nIdx);
		}
		/* Emit the call instruction */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);
	}
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Compile a node holding a variable declaration.
 * According to the PHP language reference
 *  Variables in PHP are represented by a dollar sign followed by the name of the variable.
 *  The variable name is case-sensitive.
 *  Variable names follow the same rules as other labels in PHP. A valid variable name starts
 *  with a letter or underscore, followed by any number of letters, numbers, or underscores.
 *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'
 *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).
 *  Note: $this is a special variable that can't be assigned.
 *  By default, variables are always assigned by value. That is to say, when you assign an expression
 *  to a variable, the entire value of the original expression is copied into the destination variable.
 *  This means, for instance, that after assigning one variable's value to another, changing one of those
 *  variables will have no effect on the other. For more information on this kind of assignment, see
 *  the chapter on Expressions.
 *  PHP also offers another way to assign values to variables: assign by reference. This means that
 *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original
 *  variable. Changes to the new variable affect the original, and vice versa.
 *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which
 *  is being assigned (the source variable).
 */
PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	sxu32 nLineLocal = pGen->pIn->nLine;
	sxi32 iVv;
	sxi32 iP1;
	sxi32 iP2 = 0; /* 1 = quiet read (isset/empty): a missing variable must not warn */
	void *p3;
	sxi32 rc;
	iVv = -1; /* Variable variable counter */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){
		pGen->pIn++;
		iVv++;
	}
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD|PH7_TK_OCB/*'{'*/)) == 0 ){
		/* Invalid variable name */
		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	p3  = 0;
	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){
		/* Dynamic variable creation */
		pGen->pIn++;  /* Jump the open curly */
		pGen->pEnd--; /* Ignore the trailing curly */
		if( pGen->pIn >= pGen->pEnd ){
			/* Empty expression */
			{
			/* php names the offending token and, for an empty "${}", stops there:
			 * the "expecting" tail only appears when something could still follow. */
			/* `${}`: pEnd was stepped back past the trailing '}', so the token php
			 * names sits AT pEnd. Reach for it before deciding the tail -- php stops
			 * at "unexpected token \"}\"" with no "expecting" clause, which the
			 * NULL-token path could not express because it never saw the '}'. */
			SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;
			if( pBad == 0 && pGen->pTokenSet ){
				SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);
				SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];
				if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){
					pBad = pGen->pEnd;
				}
			}
			PH7_GenSyntaxError(&(*pGen),pBad,
				(pBad && (pBad->nType & PH7_TK_CCB)) ? 0 : "variable or \"{\" or \"$\"");
			}
			return SXRET_OK;
		}
		/* Compile the expression holding the variable name. It is a pure READ, so
		 * compile it read-only: `${$u}` warns on an undefined $u (php) instead of
		 * silently creating it, matching the `$$u` name-read path below. A quiet
		 * outer (isset()/empty()) suppresses that name warning too, so carry the
		 * quiet flag into the name expression. */
		sxi32 iNameFlags = EXPR_FLAG_RDONLY_LOAD;
		if( iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET|EXPR_FLAG_LOAD_IDX_EMPTY) ){
			/* isset()/empty() suppress the name warning; `??` (EXPR_FLAG_QUIET_VAR)
			 * does NOT — php warns `Undefined variable $u` for `${$u} ?? x` and only
			 * quiets the TARGET read, so QUIET_VAR is deliberately excluded here. */
			iNameFlags |= EXPR_FLAG_QUIET_VAR;
		}
		rc = PH7_CompileExpr(&(*pGen),iNameFlags,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if( rc == SXERR_EMPTY ){
			PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);
			return SXRET_OK;
		}
	}else{
		SyHashEntry *pEntry;
		SyString *pName;
		char *zName = 0;
		/* Extract variable name */
		pName = &pGen->pIn->sData;
		/* Advance the stream cursor */
		pGen->pIn++;
		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);
		if( pEntry == 0 ){
			/* Duplicate name */
			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);
			if( zName == 0 ){
				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");
				return SXERR_ABORT;
			}
			/* Install in the hashtable */
			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);
		}else{
			/* Name already available */
			zName = (char *)pEntry->pUserData;
		}
		p3 = (void *)zName;
	}
	iP1 = 0;
	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){
		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){
			/* Read-only load.In other words do not create the variable if inexistant */
			iP1 = 1;
		}
	}
	/* iP2 marks a QUIET read: `isset($x)` / `empty($x)` inspect a variable
	 * without reading it, so an undefined one must not warn (php stays silent
	 * for both). Every other read of a missing variable warns — see OP_LOAD.
	 * The two flags are cleared before recursing into a subscript's index
	 * expression, so `isset($a[$i])` still warns for an undefined $i, as php
	 * does. Contexts that VIVIFY (assignment targets, `??`, appends) already
	 * emit iP1 = 0 and never reach the warning. */
	if( iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET|EXPR_FLAG_LOAD_IDX_EMPTY|EXPR_FLAG_QUIET_VAR) ){
		iP2 = 1;
	}else if( iCompileFlag & EXPR_FLAG_RMW_LOAD ){
		/* Warn-then-create: the read half of `$x++` / `$x .= ...` still needs a
		 * writable slot, so it cannot use the read-only load above. */
		iP2 = 2;
	}else if( iCompileFlag & EXPR_FLAG_DEFER_ARG ){
		/* D1 deferred call argument. For a plain `$var` (iVv == 0, p3 holds the name)
		 * emit the deferred load (iP1 stays 1 = no create, iP2 = 3): an undefined
		 * variable is left uncreated and silent, carrying a lazy-lvalue marker that
		 * OP_CALL resolves against the callee's by-ref flags. A variable-variable
		 * ($$x) computes its name on the stack (p3 == 0), so it cannot carry the
		 * marker — fall back to the historical eager create (iP1 = 0), which keeps
		 * its by-ref binding working exactly as before. */
		if( iVv == 0 ){
			iP2 = 3;
		}else{
			iP1 = 0;
		}
	}
	/* Emit the load instruction(s). For a variable-variable ($$x, $$$x, ...) every
	 * load EXCEPT the final dereference resolves a NAME: a pure read that warns on an
	 * undefined name (php) and never creates it. Only the last load is the actual
	 * variable and carries the caller's write/create context (iP1). Emitting the
	 * outer create-mode for the name loads silently invented $n in `$$n = 5` and
	 * skipped php's `Undefined variable $n` warning; a quiet outer (isset/empty)
	 * still suppresses the name warning as php does. */
	if( iVv > 0 ){
		sxi32 iP2Name = (iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET|EXPR_FLAG_LOAD_IDX_EMPTY))
			? 1 /* isset()/empty() suppress the name warning too; `??` (QUIET_VAR)
			     * does NOT — it warns the name and quiets only the target read. */ : 0;
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,1/* read-only name */,iP2Name,p3,0);
		while( iVv > 1 ){
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,1/* read-only name */,iP2Name,0,0);
			iVv--;
		}
		/* Final dereference: the actual variable, in the caller's context. */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,iP2,0,0);
	}else{
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,iP2,p3,0);
	}
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Load a literal.
 */
static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)
{
	SyToken *pToken = pGen->pIn;
	ph7_value *pObj;
	SyString *pStr;
	sxu32 nIdx;
	/* Extract token value */
	pStr = &pToken->sData;
	/* Deal with the reserved literals [i.e: null,false,true,...] first. A reserved
	 * word used as a member NAME (Enum::Null, C::Array, $o->list()) is a plain
	 * identifier — the parser flagged its token so the whole value-folding chain is
	 * skipped and it falls through to the ordinary string-literal emit below. */
	if( pToken->nType & PH7_TK_MEMBER_NAME ){
		/* fall through to the plain-string literal path */
	}else if( pStr->nByte == sizeof("NULL") - 1 ){
		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){
			/* NULL constant are always indexed at 0 */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);
			return SXRET_OK;
		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){
			/* TRUE constant are always indexed at 1 */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);
			return SXRET_OK;
		}
	}else if (pStr->nByte == sizeof("FALSE") - 1 &&
		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){
			/* FALSE constant are always indexed at 2 */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);
			return SXRET_OK;
	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&
		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){
			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */
			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
			if( pObj == 0 ){
				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");
				return SXERR_ABORT;
			}
			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);
			/* Emit the load constant instruction */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
			return SXRET_OK;
	}else if( (pStr->nByte == sizeof("__FILE__") - 1 &&
		SyMemcmp(pStr->zString,"__FILE__",sizeof("__FILE__")-1) == 0) ||
		(pStr->nByte == sizeof("__DIR__") - 1 &&
		SyMemcmp(pStr->zString,"__DIR__",sizeof("__DIR__")-1) == 0) ){
			/* __FILE__ / __DIR__ are magic constants resolved at COMPILE time to the
			 * file being compiled (where the token is written), NOT the runtime
			 * execution file. A function defined in a.php reporting __FILE__ must say
			 * a.php even when called from b.php — php semantics, and what Composer's
			 * autoloader (loadClassLoader's `require __DIR__ . '/ClassLoader.php'`)
			 * relies on. The runtime-constant path returned the caller's file. */
			int bDir = (pStr->zString[2] == 'D'); /* __DIR__ vs __FILE__ */
			SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);
			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
			if( pObj == 0 ){
				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");
				return SXERR_ABORT;
			}
			if( pFile && pFile->nByte > 0 ){
				if( bDir ){
					const char *zDir;
					int nLen;
					SyString sDir;
					zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);
					SyStringInitFromBuf(&sDir,zDir,nLen);
					PH7_MemObjInitFromString(pGen->pVm,pObj,&sDir);
				}else{
					PH7_MemObjInitFromString(pGen->pVm,pObj,pFile);
				}
			}else{
				SyString sMem;
				SyStringInitFromBuf(&sMem,":MEMORY:",sizeof(":MEMORY:")-1);
				PH7_MemObjInitFromString(pGen->pVm,pObj,&sMem);
			}
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
			return SXRET_OK;
	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&
		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){
			/* __NAMESPACE__ magic constant: resolved at compile time */
			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
			if( pObj == 0 ){
				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");
				return SXERR_ABORT;
			}
			if( SyBlobLength(&pGen->sNamespace) > 0 ){
				SyString sNs;
				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));
				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);
			}else{
				PH7_MemObjInitFromString(pGen->pVm,pObj,0);
			}
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
			return SXRET_OK;
	}else if( pStr->nByte == sizeof("__TRAIT__") - 1 &&
		SyMemcmp(pStr->zString,"__TRAIT__",sizeof("__TRAIT__")-1) == 0 ){
			/* __TRAIT__ magic constant: the name of the trait whose SOURCE lexically
			 * encloses this token. php resolves it at compile time, and it is shared
			 * across every using class because a trait method body compiles ONCE with
			 * the trait as its owner (PH7_ClassUseTrait adopts the same method pointer).
			 * Unlike __FUNCTION__/__METHOD__ (nearest function), __TRAIT__ is LEXICAL:
			 * closures and arrow-fns are TRANSPARENT (a closure inside a trait method still
			 * yields the trait), so we skip them and keep walking outward — but a class
			 * method or a plain function is an OPAQUE lexical boundary that fixes the answer.
			 * An anonymous class defined inside a trait method is a fresh scope, so
			 * __TRAIT__ is "" there, not the enclosing trait. "" outside any trait (global
			 * scope, plain functions, non-trait methods) — php renders it the empty string,
			 * not NULL. */
			ph7_class *pTrait = 0;
			if( pGen->iInMemberDefault > 0 ){
				/* A property/parameter DEFAULT is a const-expression that belongs to the
				 * class whose body is being compiled (pCurClass), never to a lexically-
				 * enclosing method. Read pCurClass directly — the block chain has no func
				 * block for the default and would leak into the enclosing function (an
				 * anonymous class's default inside a trait method is the anon's scope, "").*/
				if( pGen->pCurClass && (pGen->pCurClass->iFlags & PH7_CLASS_TRAIT) ){
					pTrait = pGen->pCurClass;
				}
			}else{
				GenBlock *pBlock = pGen->pCurrent;
				while( pBlock ){
					if( pBlock->iFlags & GEN_BLOCK_FUNC ){
						ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;
						if( pFunc == 0 ){
							/* A SYNTHETIC function block carries no ph7_vm_func — e.g. the
							 * per-arm throw-fixup block GenStateCompileMatchSubExpr enters to
							 * host a match() expression. It is not a real lexical scope
							 * boundary, so stay transparent and keep walking outward. */
							pBlock = pBlock->pParent;
							continue;
						}
						if( pFunc->iFlags & VM_FUNC_CLASS_METHOD ){
							/* A class method (of any class, including an anonymous one) is an
							 * OPAQUE scope boundary and fixes the answer: a trait method yields
							 * its trait, any other class's method yields "". Tested BEFORE the
							 * closure flags so a static method is never mistaken for transparent. */
							if( pFunc->pUserData
								&& (((ph7_class *)pFunc->pUserData)->iFlags & PH7_CLASS_TRAIT) ){
								pTrait = (ph7_class *)pFunc->pUserData;
							}
							break;
						}
						if( pFunc->iFlags & (VM_FUNC_CLOSURE|VM_FUNC_ARROW|VM_FUNC_STATIC_CL) ){
							/* Closure / arrow fn (VM_FUNC_CLOSURE is only set when the closure
							 * captures, so a capture-less static closure carries only
							 * VM_FUNC_STATIC_CL — include it). Transparent: keep walking outward. */
							pBlock = pBlock->pParent;
							continue;
						}
						/* A plain named function is an opaque boundary: __TRAIT__ is "". */
						break;
					}
					pBlock = pBlock->pParent;
				}
			}
			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
			if( pObj == 0 ){
				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");
				return SXERR_ABORT;
			}
			if( pTrait ){
				PH7_MemObjInitFromString(pGen->pVm,pObj,&pTrait->sName);
			}else{
				PH7_MemObjInitFromString(pGen->pVm,pObj,0); /* empty string */
			}
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
			return SXRET_OK;
	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&
		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) ||
		(pStr->nByte == sizeof("__METHOD__") - 1 &&
		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){
			GenBlock *pBlock = pGen->pCurrent;
			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */
			/* Skip SYNTHETIC function blocks (GEN_BLOCK_FUNC with no ph7_vm_func in
			 * pUserData — e.g. the per-arm throw-fixup block a match() expression enters):
			 * they are not real function scopes. Without this a __FUNCTION__/__METHOD__
			 * inside a match arm reached a NULL pUserData and dereferenced it (compile-time
			 * crash); php resolves to the enclosing real function, which the walk now finds. */
			while( pBlock && ((pBlock->iFlags & GEN_BLOCK_FUNC) == 0 || pBlock->pUserData == 0) ){
				/* Point to the upper block */
				pBlock = pBlock->pParent;
			}
			if( pBlock == 0 ){
				/* Called in the global scope,load NULL */
				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);
			}else{
				/* Extract the target function/method */
				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;
				int bMethod = (pStr->zString[2] == 'M'); /* __METHOD__ vs __FUNCTION__ */
				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
				if( pObj == 0 ){
					PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");
					return SXERR_ABORT;
				}
				/*
				 * __METHOD__ is the QUALIFIED name: "C::m" inside a method, and the plain
				 * function name inside a plain function (php does not answer "" there —
				 * PH7 loaded NULL, so __METHOD__ was empty in every free function and
				 * unqualified in every method).
				 */
				if( bMethod && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){
					SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;
					SyBlob sQual;
					SyString sOut;
					SyBlobInit(&sQual,&pGen->pVm->sAllocator);
					SyBlobFormat(&sQual,"%z::%z",pCls,&pFunc->sName);
					SyStringInitFromBuf(&sOut,SyBlobData(&sQual),SyBlobLength(&sQual));
					PH7_MemObjInitFromString(pGen->pVm,pObj,&sOut);
					SyBlobRelease(&sQual);
				}else{
					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);
				}
				/* Emit the load constant instruction */
				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
			}
			return SXRET_OK;
	}
	/* Query literal table */
	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){
		ph7_value *pLitObj;
		/* Unknown literal,install it in the literal table */
		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
		if( pLitObj == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");
			return SXERR_ABORT;
		}
		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);
		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);
	}
	/* Emit the load constant instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);
	return SXRET_OK;
}
/*
 * Resolve a namespace path or simply load a literal.
 * If the token stream contains namespace separators (backslashes),
 * assemble them into a single literal string (e.g. "Foo\Bar\Baz").
 * Otherwise, load the simple literal directly.
 */
static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)
{
	sxi32 rc;
	if( pGen->pIn >= pGen->pEnd ){
		return SXRET_OK;
	}
	/* Check if this is a multi-token namespace path */
	if( pGen->pIn < &pGen->pEnd[-1] ){
		/* Multiple tokens: assemble the full path into sWorker */
		SyBlob *pWorker = &pGen->sWorker;
		int isAbsolute = 0;
		SyBlobReset(pWorker);
		/* Check for leading backslash (absolute path) */
		if( pGen->pIn->nType & PH7_TK_NSSEP ){
			isAbsolute = 1;
			pGen->pIn++; /* Skip leading backslash */
		}
		/* Collect the raw path (no prefix yet) so its FIRST segment can be resolved
		 * against use-imports below — php resolves `A\B\C` by mapping the leading
		 * `A` through the imports (`use X\A;` makes it `X\A\B\C`), and only prepends
		 * the current namespace when `A` matches no import. Blindly prefixing the
		 * namespace here produced e.g. `Ns\Ev\Bus` for `use ...\Event as Ev; Ev\Bus`. */
		{
			SyBlob sRaw;
			SyBlobInit(&sRaw,&pGen->pVm->sAllocator);
			while( pGen->pIn <= &pGen->pEnd[-1] ){
				if( pGen->pIn->nType & PH7_TK_NSSEP ){
					SyBlobAppend(&sRaw,"\\",1);
				}else{
					SyBlobAppend(&sRaw,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);
				}
				if( pGen->pIn == &pGen->pEnd[-1] ){
					pGen->pIn++;
					break;
				}
				pGen->pIn++;
			}
			if( isAbsolute ){
				SyBlobAppend(pWorker,SyBlobData(&sRaw),SyBlobLength(&sRaw)); /* FQN as written */
			}else{
				const char *zRaw = (const char *)SyBlobData(&sRaw);
				sxu32 nRaw = SyBlobLength(&sRaw);
				sxu32 nFirst = 0;
				SyHashEntry *pNsImp;
				while( nFirst < nRaw && zRaw[nFirst] != '\\' ){ nFirst++; }
				pNsImp = SyHashGet(&pGen->hUseImports,(const void *)zRaw,nFirst);
				if( pNsImp ){
					/* Leading segment is an imported alias: substitute its FQN. */
					const char *zFQN = (const char *)pNsImp->pUserData;
					SyBlobAppend(pWorker,zFQN,SyStrlen(zFQN));
					SyBlobAppend(pWorker,zRaw + nFirst,nRaw - nFirst);
				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){
					SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));
					SyBlobAppend(pWorker,"\\",1);
					SyBlobAppend(pWorker,zRaw,nRaw);
				}else{
					SyBlobAppend(pWorker,zRaw,nRaw); /* global scope, no import */
				}
			}
			SyBlobRelease(&sRaw);
		}
		if( SyBlobLength(pWorker) > 0 ){
			ph7_value *pObj;
			SyString sPath;
			sxu32 nIdx;
			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));
			/* Install in the literal table */
			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){
				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
				if( pObj == 0 ){
					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");
					return SXERR_ABORT;
				}
				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);
				GenStateInstallLiteral(&(*pGen),pObj,nIdx);
			}
			/* Emit the load constant instruction.
			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.
			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,
				isAbsolute ? (PH7_LOADC_EXPAND|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,
				nIdx,0,0);
			return SXRET_OK;
		}
	}
	/* Single-token literal: load directly */
	rc = GenStateLoadLiteral(&(*pGen));
	return rc;
}
/*
 * Compile a literal which is an identifier(name) for a simple value.
 */
/*
 * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of
 * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument
 * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...`
 * appeared outside a call argument list — a syntax error (PHP rejects it likewise).
 */
PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SXUNUSED(iCompileFlag);
	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,
		"Cannot use the first-class callable syntax '...' here");
	return SXERR_SYNTAX;
}
PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	sxi32 rc;
	rc = GenStateResolveNamespaceLiteral(&(*pGen));
	if( rc != SXRET_OK ){
		SXUNUSED(iCompileFlag); /* cc warning */
		return rc;
	}
	/* Node successfully compiled */
	return SXRET_OK;
}
