/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include "compile_int.h"
/*
 * Section:
 *    Function compilation: argument collection and default values, the
 *    union/return type-declaration parsers, function bodies and the
 *    'function' statement itself.
 * Status:
 *    Stable.
 */
/*
 * Process default argument values. That is,a function may define C++-style default value
 * as follows:
 * function makecoffee($type = "cappuccino")
 * {
 *   return "Making a cup of $type.\n";
 * }
 * Symisc eXtension.
 *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous
 *      functions,array member,..] unlike the zend which would allow only single scalar value.
 *      Example: Work only with PH7,generate error under zend
 *      function test($a = 'Hello'.'World: '.rand_str(3))
 *      {
 *       var_dump($a);
 *      }
 *     //call test without args
 *      test();
 * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)
 *      Example:
 *           function a(string $a){} function b(int $a,string $c,float $d){}
 * 3 -) Function overloading!!
 *      Example:
 *      function foo($a) {
 *   	  return $a.PHP_EOL;
 *	    }
 *	    function foo($a, $b) {
 *   	  return $a + $b;
 *	    }
 *	    echo foo(5); // Prints "5"
 *	    echo foo(5, 2); // Prints "7"
 *      // Same arg
 *	   function foo(string $a)
 *	   {
 *	     echo "a is a string\n";
 *	     var_dump($a);
 *	   }
 *	  function foo(int $a)
 *	  {
 *	    echo "a is integer\n";
 *	    var_dump($a);
 *	  }
 *	  function foo(array $a)
 *	  {
 * 	    echo "a is an array\n";
 * 	    var_dump($a);
 *	  }
 *	  foo('This is a great feature'); // a is a string [first foo]
 *	  foo(52); // a is integer [second foo]
 *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]
 * Please refer to the official documentation for more information on the powerful extension
 * introduced by the PH7 engine.
 */
static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)
{
	SyToken *pTmpIn,*pTmpEnd;
	SySet *pInstrContainer;
	sxi32 rc;
	/* Swap token stream */
	SWAP_DELIMITER(pGen,pIn,pEnd);
	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);
	/* Compile the expression holding the argument value. A parameter default is a
	 * const-expression belonging to the current class (see iInMemberDefault) — so
	 * __TRAIT__ in it reads pCurClass rather than walking into the enclosing method. */
	pGen->iInMemberDefault++;
	rc = PH7_CompileExpr(&(*pGen),0,0);
	pGen->iInMemberDefault--;
	/* Emit the done instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);
	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	RE_SWAP_DELIMITER(pGen);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	return SXRET_OK;
}
/*
 * Collect function arguments one after one.
 * According to the PHP language reference manual.
 * Information may be passed to functions via the argument list, which is a comma-delimited
 * list of expressions.
 * PHP supports passing arguments by value (the default), passing by reference
 * and default argument values. Variable-length argument lists are also supported,
 * see also the function references for func_num_args(), func_get_arg(), and func_get_args()
 * for more information.
 * Example #1 Passing arrays to functions
 * <?php
 * function takes_array($input)
 * {
 *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];
 * }
 * ?>
 * Making arguments be passed by reference
 * By default, function arguments are passed by value (so that if the value of the argument
 * within the function is changed, it does not get changed outside of the function).
 * To allow a function to modify its arguments, they must be passed by reference.
 * To have an argument to a function always passed by reference, prepend an ampersand (&)
 * to the argument name in the function definition:
 * Example #2 Passing function parameters by reference
 * <?php
 * function add_some_extra(&$string)
 * {
 *   $string .= 'and something extra.';
 * }
 * $str = 'This is a string, ';
 * add_some_extra($str);
 * echo $str;    // outputs 'This is a string, and something extra.'
 * ?>
 *
 * PH7 have introduced powerful extension including full type hinting,function overloading
 * complex agrument values.Please refer to the official documentation for more information
 * on these extension.
 */
PH7_PRIVATE sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)
{
	ph7_vm_func_arg sArg; /* Current processed argument */
	SyToken *pIn;  /* Token stream */
	SyBlob sSig;         /* Function signature */
	char *zDup;          /* Copy of argument name */
	sxi32 rc;

	pIn = pGen->pIn;
	SyBlobInit(&sSig,&pGen->pVm->sAllocator);
	/* Process arguments one after one */
	for(;;){
		if( pIn >= pEnd ){
			/* No more arguments to process */
			break;
		}
		SyZero(&sArg,sizeof(ph7_vm_func_arg));
		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));
		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));
		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));
		SyStringInitFromBuf(&sArg.sTypeName,0,0);
		/* Parameter #[...] attributes: the group precedes the parameter's
		 * first token inside the main token stream */
		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		/* Parse optional visibility + readonly modifiers (constructor property
		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility
		 * keyword and/or `readonly` is present; `readonly` may appear on either
		 * side of the visibility keyword (`public readonly T $x`,
		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */
		{
			int bReadonly = 0, bVisSeen = 0;
			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;
			sxi32 iSetVisFlag = 0;
			int nSetTok;
			sxi32 nSetVis;
			if( pIn < pEnd && GenStateIsReadonly(pIn) ){
				bReadonly = 1;
				pIn++;
			}
			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);
			if( nSetVis ){
				/* Leading `private(set)` etc: promoted with a public read side */
				iSetVisFlag = GenStateSetVisFlag(nSetVis);
				bVisSeen = 1;
				pIn += nSetTok;
				if( pIn < pEnd && GenStateIsReadonly(pIn) ){
					bReadonly = 1;
					pIn++;
				}
			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){
				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);
				if( nKw == PH7_TKWRD_PUBLIC || nKw == PH7_TKWRD_PROTECTED || nKw == PH7_TKWRD_PRIVATE ){
					bVisSeen = 1;
					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE
						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED
						: PH7_CLASS_PROT_PUBLIC;
					pIn++;
					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);
					if( nSetVis ){
						/* `public private(set) T $x` promoted form */
						iSetVisFlag = GenStateSetVisFlag(nSetVis);
						pIn += nSetTok;
					}
					if( pIn < pEnd && GenStateIsReadonly(pIn) ){
						bReadonly = 1;
						pIn++;
					}
				}
			}
			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){
				sArg.iFlags |= VM_FUNC_ARG_PRIV_SET;
			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){
				sArg.iFlags |= VM_FUNC_ARG_PROT_SET;
			}
			if( bVisSeen || bReadonly ){
				if( !bCtorCtx ){
					if( bAbstractCtx ){
						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,
							"Cannot declare promoted property in an abstract constructor");
					}else{
						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,
							"Cannot declare promoted property outside a constructor");
					}
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					return SXERR_SYNTAX;
				}
				sArg.iFlags |= VM_FUNC_ARG_PROMOTED;
				sArg.iPromoteVis = iVis;
				if( bReadonly ){
					sArg.iFlags |= VM_FUNC_ARG_READONLY;
				}
			}
		}
		/* Parse optional type hint (single, nullable shorthand, or union) */
		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0
			&& (pIn->nType & PH7_TK_AMPER) == 0
			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){
			sxu32 nLineLocal = pIn->nLine;
			sxi32 iTFlags = 0;
			pGen->pIn = pIn;
			rc = GenStateParseUnionTypeDecl(
				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,
				&iTFlags, &sArg.sTypeName,
				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,
				/* bAllowVoid */ 0,
						nLineLocal);
			pIn = pGen->pIn;
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}else if( rc == SXERR_CORRUPT ){
				/* Error already reported by GenStateParseUnionTypeDecl */
				return SXERR_SYNTAX;
			}else if( rc == SXERR_SYNTAX ){
				if( pIn < pEnd ){
					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,
						"syntax error, unexpected token \"%z\", expecting variable",
						&pIn->sData);
				}else{
					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,
						"syntax error, unexpected end of file");
				}
				return SXERR_SYNTAX;
			}
			sArg.iFlags |= iTFlags;
		}
		if( pIn >= pEnd ){
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");
			return rc;
		}
		if( pIn->nType & PH7_TK_AMPER ){
			/* Pass by reference,record that */
			sArg.iFlags |= VM_FUNC_ARG_BY_REF;
			pIn++;
		}
		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){
			/* Variadic parameter: ...$args */
			sArg.iFlags |= VM_FUNC_ARG_VARIADIC;
			pIn++;
		}
		if( pIn >= pEnd || (pIn->nType & PH7_TK_DOLLAR) == 0 || &pIn[1] >= pEnd || (pIn[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
			/* Invalid argument */
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");
			return rc;
		}
		pIn++; /* Jump the dollar sign */
		/* Copy argument name */
		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));
		if( zDup == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");
			return SXERR_ABORT;
		}
		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));
		pIn++;
		if( pIn < pEnd ){
			if( pIn->nType & PH7_TK_EQUAL ){
				SyToken *pDefend;
				sxi32 iNest = 0;
				pIn++; /* Jump the equal sign */
				pDefend = pIn;
				/* Process the default value associated with this argument */
				while( pDefend < pEnd ){
					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){
						break;
					}
					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/|PH7_TK_OCB/*'{'*/|PH7_TK_OSB/*[*/) ){
						/* Increment nesting level */
						iNest++;
					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/|PH7_TK_CCB/*'}'*/|PH7_TK_CSB/*]*/) ){
						/* Decrement nesting level */
						iNest--;
					}
					pDefend++;
				}
				if( pIn >= pDefend ){
					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");
					return rc;
				}
				/* Process default value */
				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);
				if( rc != SXRET_OK ){
					return rc;
				}
				/* PHP rule: a typed parameter whose default is the literal `null`
				 * (`C $c = null`, `int $x = null`, `A|B $x = null`) is implicitly
				 * nullable — an explicit null is accepted even though the type isn't
				 * written `?T`. Detect the single-token `null` default here so the VM
				 * arg-type check lets null through. */
				if( (sArg.nType > 0 || (sArg.iFlags & VM_FUNC_ARG_UNION))
					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0
					&& &pIn[1] == pDefend
					&& pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD)
					&& pIn->sData.nByte == sizeof("null")-1
					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){
					/* php 8.4 DEPRECATED the implicit-nullable form (`int $x = null`
					 * without the `?`). PHL targets php's *non-deprecated* surface and
					 * rejects it outright — the explicit `?int` must be written.
					 * `mixed $x = null` is fine: mixed already includes null (explicit
					 * ?T / T|null are already excluded via VM_FUNC_ARG_NULLABLE above). */
					if( sArg.sClass.nByte == sizeof("mixed")-1
						&& SyStrnicmp(SyStringData(&sArg.sClass),"mixed",sizeof("mixed")-1) == 0 ){
						sArg.iFlags |= VM_FUNC_ARG_NULLABLE;
					}else{
						const char *zSep = "";
						SyString sCls = { "", 0 };
						if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){
							sCls = ((ph7_class *)pFunc->pUserData)->sName;
							zSep = "::";
						}
						PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,
							"%z%s%z(): Cannot use null as the default for non-nullable parameter $%z; write the explicit ?T type instead",
							&sCls,zSep,&pFunc->sName,&sArg.sName);
						return SXERR_ABORT;
					}
				}
				/* Point beyond the default value */
				pIn = pDefend;
			}
			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);
				return rc;
			}
			pIn++; /* Jump the trailing comma */
		}
		/* Append argument signature */
		if( sArg.nType > 0 ){
			if( SyStringLength(&sArg.sClass) > 0 ){
				/* Class name — prefix with 'o' so generic object hint is a prefix match */
				int marker = 'o';
				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));
				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));
			}else{
				int c;
				c = 'n'; /* cc warning */
				/* Type leading character */
				switch(sArg.nType){
				case MEMOBJ_HASHMAP:
					/* Hashmap aka 'array' */
					c = 'h';
					break;
				case MEMOBJ_INT:
					/* Integer */
					c = 'i';
					break;
				case MEMOBJ_BOOL:
					/* Bool */
					c = 'b';
					break;
				case MEMOBJ_REAL:
					/* Float */
					c = 'f';
					break;
				case MEMOBJ_STRING:
					/* String */
					c = 's';
					break;
				case MEMOBJ_OBJ:
					/* Object */
					c = 'o';
					break;
				default:
					break;
				}
				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));
			}
		}else{
			/* No type is associated with this parameter which mean
			 * that this function is not condidate for overloading.
			 */
			SyBlobRelease(&sSig);
		}
		/* Save in the argument set */
		SySetPut(&pFunc->aArgs,(const void *)&sArg);
	}
	if( SyBlobLength(&sSig) > 0 ){
		/* Save function signature */
		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));
	}
	return SXRET_OK;
}
/*
 * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested
 * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to
 * the enclosing function. Returns the token just past the nested construct.
 */
static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)
{
	sxi32 iParen = 0;
	pIn++; /* past 'function'/'fn' */
	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a
	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a
	 * ';' at paren-depth 0 (an abstract/interface method has no body). */
	while( pIn < pEnd ){
		sxu32 t = pIn->nType;
		if( t & PH7_TK_LPAREN ){ iParen++; }
		else if( t & PH7_TK_RPAREN ){ iParen--; }
		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }
		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }
		pIn++;
	}
	if( pIn >= pEnd ){ return pIn; }
	/* pIn at the body '{' — skip the balanced brace block. */
	{
		sxi32 d = 0;
		while( pIn < pEnd ){
			sxu32 t = pIn->nType;
			if( t & PH7_TK_OCB ){ d++; }
			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }
			pIn++;
		}
	}
	return pIn;
}
/*
 * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening
 * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a
 * generator)? Nested function/closure bodies are skipped so their yields don't count.
 * Used to gate inline try/catch/finally compilation: only generators need it (so a
 * `yield` inside a catch/finally can suspend); every other function keeps the legacy
 * detached-mini-program path untouched.
 */
/*
 * Case-insensitive match of a (possibly '\'-prefixed) name against the
 * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,
 * mixed, object.
 */
static int GenStateGenRetNameOk(const char *zName,sxu32 nName)
{
	static const struct { const char *zName; sxu32 nLen; } aOk[] = {
		{"Generator",9},{"Iterator",8},{"Traversable",11},
		{"iterable",8},{"mixed",5},{"object",6}
	};
	sxu32 i;
	if( nName > 0 && zName[0] == '\\' ){
		zName++;
		nName--;
	}
	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){
		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){
			return 1;
		}
	}
	return 0;
}
/*
 * One atom of a generator's declared return type: is it a supertype of
 * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,
 * mixed and object (nullability is irrelevant — it only widens). A class
 * atom is accepted when its raw name matches OR its use-import/namespace
 * resolution (GenStateResolveName) matches — so `use Generator as Gen;
 * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:
 * the parser strips a leading `\`, so inside `namespace Foo;` a
 * fully-qualified `\Generator` (php: accept) and a bare `Generator`
 * (php: reject as Foo\Generator) are indistinguishable here — we accept
 * both rather than fatal on valid code (a recorded divergence).
 */
static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)
{
	if( nType == MEMOBJ_OBJ ){
		return 1; /* bare `object` */
	}
	if( nType != SXU32_HIGH ){
		return 0; /* scalar/array/void/never/null/... */
	}
	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){
		return 1;
	}
	/* Not a whitelist name as written — try the compile-time resolution
	 * (use-import aliases; namespace prefix). `use Iterator as It;` must
	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,
	 * matching php (a subinterface is not a SUPERtype of Generator). */
	{
		SyBlob sFQN;
		int bOk;
		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);
		GenStateResolveName(pGen,pName,&sFQN);
		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));
		SyBlobRelease(&sFQN);
		return bOk;
	}
}
/*
 * php 8: a generator function may only declare a return type that is a
 * supertype of Generator, alone or as a union alternative; an intersection
 * group qualifies only if every member does. Anything else is php's exact
 * compile-time fatal "Generator return type must be a supertype of
 * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the
 * canonical-order sReturnTypeName). Without this check the declared type
 * used to leak into the BODY's completion OP_DONE via the ctx resume paths
 * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).
 */
static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)
{
	int bOk = 0;
	sxu32 nLine;
	sxi32 rc;
	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){
		return SXRET_OK; /* untyped: nothing to validate */
	}
	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){
		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);
		sxu32 n = SySetUsed(&pFunc->aReturnUnion);
		sxu32 i,j;
		for( i = 0; i < n && !bOk; i++ ){
			int bGroupOk;
			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){
				continue; /* group already judged at its first member (ids are contiguous) */
			}
			bGroupOk = 1;
			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){
				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){
					bGroupOk = 0;
					break;
				}
			}
			bOk = bGroupOk;
		}
	}else{
		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);
	}
	if( bOk ){
		return SXRET_OK;
	}
	/* This validator runs at the end of GenStateCompileFuncBody, after the
	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a
	 * token of this stream — its line is the function's closing brace. php
	 * reports the SIGNATURE line instead; the drift is the §3.7 error-
	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */
	nLine = pGen->pIn[-1].nLine;
	{
		SyString sGiven = pFunc->sReturnTypeName;
		if( sGiven.nByte < 1 ){
			sGiven = pFunc->sReturnClass;
		}
		if( sGiven.nByte < 1 ){
			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the
			 * rendered type text, so sReturnTypeName arrives empty for them —
			 * name them here (the root fix belongs to that renderer, §3.7). */
			const char *zScalar =
				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :
				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";
			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));
		}
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
			"Generator return type must be a supertype of Generator, %z given",&sGiven);
	}
	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;
}
static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)
{
	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */
	SyToken *pEnd = pGen->pEnd;
	sxi32 iDepth = 0;
	int bStarted = 0;
	while( pIn < pEnd ){
		sxu32 t = pIn->nType;
		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }
		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }
		if( t & PH7_TK_KEYWORD ){
			int kw = SX_PTR_TO_INT(pIn->pUserData);
			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }
			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }
			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */
		}
		pIn++;
	}
	return FALSE;
}
/*
 * Compile function [i.e: standard function, annonymous function or closure ] body.
 * Return SXRET_OK on success. Any other return value indicates failure
 * and this routine takes care of generating the appropriate error message.
 */
PH7_PRIVATE sxi32 GenStateCompileFuncBody(
	ph7_gen_state *pGen,  /* Code generator state */
	ph7_vm_func *pFunc    /* Function state */
	)
{
	SySet *pInstrContainer; /* Instruction container */
	GenBlock *pBlock;
	sxu32 nGotoOfft;
	sxi32 rc;
	/* Attach the new function */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");
		/* Don't worry about freeing memory, everything will be released shortly */
		return SXERR_ABORT;
	}
	nGotoOfft = SySetUsed(&pGen->aGoto);
	/* Swap bytecode containers */
	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);
	/* Emit constructor property promotion prologue:
	 *   $this->NAME = $NAME;
	 * for each promoted parameter. Runtime typed-property store enforcement
	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */
	{
		sxu32 nArg = SySetUsed(&pFunc->aArgs);
		sxu32 i;
		for( i = 0; i < nArg; i++ ){
			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);
			char *zSrc;
			sxu32 nSrc,nName;
			SySet sToken;
			SyToken *pTmpIn,*pTmpEnd;
			sxi32 rcPromote;
			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){
				continue;
			}
			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.
			 * Tokens keep pointers into this buffer (identifier names are not
			 * copied), so it must outlive the function — never free it. The
			 * buffer is null-terminated because PH7_OP_LOAD reads the variable
			 * name via SyStrlen() on the token's sData pointer. */
			nName = SyStringLength(&pArg->sName);
			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;
			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);
			if( zSrc == 0 ){
				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
				GenStateLeaveBlock(&(*pGen),0);
				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");
				return SXERR_ABORT;
			}
			{
				char *z = zSrc;
				SyMemcpy("$this->",z,sizeof("$this->")-1);
				z += sizeof("$this->")-1;
				SyMemcpy(SyStringData(&pArg->sName),z,nName);
				z += nName;
				SyMemcpy(" = $",z,sizeof(" = $")-1);
				z += sizeof(" = $")-1;
				SyMemcpy(SyStringData(&pArg->sName),z,nName);
				z += nName;
				*z = 0;
			}
			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));
			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);
			pTmpIn = pGen->pIn;
			pTmpEnd = pGen->pEnd;
			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);
			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];
			rcPromote = PH7_CompileExpr(&(*pGen),0,0);
			pGen->pIn = pTmpIn;
			pGen->pEnd = pTmpEnd;
			SySetRelease(&sToken);
			if( rcPromote == SXERR_ABORT ){
				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
				GenStateLeaveBlock(&(*pGen),0);
				return SXERR_ABORT;
			}
			/* Discard the assignment result — this is a statement expression. */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}
	}
	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling
	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally
	 * suspends correctly). Saved/restored so a nested non-generator closure inside a
	 * generator — and vice versa — is classified independently. */
	{
		sxi8 bSavedGen = pGen->bInGenerator;
		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));
		/* Compile the body */
		PH7_CompileBlock(&(*pGen),0);
		pGen->bInGenerator = bSavedGen;
	}
	/* Fix exception jumps now the destination is resolved */
	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));
	/* Emit the final return if not yet done */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);
	/* Fix gotos jumps now the destination is resolved */
	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){
		rc = SXERR_ABORT;
	}
	SySetTruncate(&pGen->aGoto,nGotoOfft);
	/* Restore the default container */
	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	/* Leave function block */
	GenStateLeaveBlock(&(*pGen),0);
	if( rc == SXERR_ABORT ){
		/* Don't worry about freeing memory, everything will be released shortly */
		return SXERR_ABORT;
	}
	/* Scan for yield opcodes to detect generator functions */
	{
		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);
		sxu32 i;
		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){
			if( aInstr[i].iOp == PH7_OP_YIELD || aInstr[i].iOp == PH7_OP_YIELD_FROM ){
				pFunc->iFlags |= VM_FUNC_GENERATOR;
				break;
			}
		}
	}
	if( pFunc->iFlags & VM_FUNC_GENERATOR ){
		/* php-exact definition-time check; see the helper's block comment. */
		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){
			return SXERR_ABORT;
		}
	}
	/* All done, function body compiled */
	return SXRET_OK;
}
/*
 * Compile a PHP function whether is a Standard or Annonymous function.
 * According to the PHP language reference manual.
 *  Function names follow the same rules as other labels in PHP. A valid function name
 *  starts with a letter or underscore, followed by any number of letters, numbers, or
 *  underscores. As a regular expression, it would be expressed thus:
 *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.
 *  Functions need not be defined before they are referenced.
 *  All functions and classes in PHP have the global scope - they can be called outside
 *  a function even if they were defined inside and vice versa.
 *  It is possible to call recursive functions in PHP. However avoid recursive function/method
 *  calls with over 32-64 recursion levels.
 *
 * PH7 have introduced powerful extension including full type hinting, function overloading,
 * complex agrument values and more. Please refer to the official documentation for more information
 * on these extension.
 */
/*
 * Case-insensitive comparison for type names (PHP type names are case-insensitive).
 */
PH7_PRIVATE int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)
{
	sxu32 i;
	for( i = 0; i < n; i++ ){
		int a = zA[i], b = zB[i];
		if( a >= 'A' && a <= 'Z' ) a += 0x20;
		if( b >= 'A' && b <= 'Z' ) b += 0x20;
		if( a != b ) return a - b;
	}
	return 0;
}
/*
 * Internal type-atom kinds used during union type parsing.
 * Negative values are sentinels that never collide with MEMOBJ_* bitmasks
 * (which are positive bit values stored in sxu32).
 */
#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */
#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */
#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */

/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in
 * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array
 * below lives on the parser stack, so the cost is bounded: ~1 KiB. */

typedef struct PhlTypeAtom PhlTypeAtom;
struct PhlTypeAtom {
	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */
	SyString sClass;   /* class name when nType == SXU32_HIGH */
	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */
	sxu32 nCanon;
	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),
	                    * distinct groups are ORed; pure unions use one atom per group */
};

/*
 * Parse a single type atom (one alternative of a union, or a complete
 * single type). Recognises scalar keywords, `array`, `object`, `null`,
 * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).
 * pGen->pIn must point at the first token of the atom; on success it
 * is advanced past the atom. The previous nullable `?` prefix must
 * already be consumed by the caller.
 */
/*
 * TRUE if pName is a reserved PHP type keyword (never a class name), so a type
 * hint that spells it must not be namespace-qualified. Only the words that can
 * reach GenStateParseOneTypeAtom's identifier branch as a bare name matter here
 * (bool/int/float/string/array/object/self/static/parent arrive as keywords, and
 * null/void/never are matched before the class path), but the full set is listed
 * so the guard is robust to lexer changes.
 */
static int GenStateIsReservedTypeWord(const SyString *pName)
{
	static const char *azWords[] = {
		"false","true","mixed","iterable","callable","null","void","never",
		"bool","boolean","int","integer","float","double","string","array",
		"object","self","static","parent"
	};
	sxu32 i;
	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){
		sxu32 n = (sxu32)SyStrlen(azWords[i]);
		if( pName->nByte == n && SyStrnicmp(pName->zString,azWords[i],n) == 0 ){
			return 1;
		}
	}
	return 0;
}
static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)
{
	SyToken *pIn = pGen->pIn;
	int bAbsolute = 0;
	SyZero(pOut, sizeof(*pOut));
	SyStringInitFromBuf(&pOut->sClass, 0, 0);
	if( pIn >= pGen->pEnd ){
		return SXERR_SYNTAX;
	}
	/* Optional leading namespace separator '\' on FQN class types */
	if( pIn->nType & PH7_TK_NSSEP ){
		bAbsolute = 1; /* fully-qualified: never prefix the current namespace */
		pIn++;
		if( pIn >= pGen->pEnd ){
			return SXERR_SYNTAX;
		}
	}
	/* `namespace\X` type hint: the CURRENT namespace spelled out, fully qualified
	 * from there. Collected here rather than below because the leading `namespace`
	 * is a KEYWORD token, which the atom parser would otherwise reject outright. */
	if( !bAbsolute ){
		SyBlob sRel;
		SyBlobInit(&sRel,&pGen->pVm->sAllocator);
		if( GenStateNsRelPrefix(pGen,&pIn,pGen->pEnd,&sRel) ){
			char *zDup;
			SyBlobAppend(&sRel,pIn->sData.zString,pIn->sData.nByte);
			pIn++;
			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)
				&& (pIn[1].nType & PH7_TK_ID) ){
				SyBlobAppend(&sRel,"\\",1);
				SyBlobAppend(&sRel,pIn[1].sData.zString,pIn[1].sData.nByte);
				pIn += 2;
			}
			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
				(const char *)SyBlobData(&sRel),SyBlobLength(&sRel));
			if( zDup == 0 ){
				SyBlobRelease(&sRel);
				return SXERR_ABORT;
			}
			pOut->nType = SXU32_HIGH;
			SyStringInitFromBuf(&pOut->sClass,zDup,SyBlobLength(&sRel));
			SyBlobRelease(&sRel);
			pGen->pIn = pIn;
			return SXRET_OK;
		}
		SyBlobRelease(&sRel);
	}
	if( (pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
		return SXERR_SYNTAX;
	}
	if( pIn->nType & PH7_TK_KEYWORD ){
		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));
		if( nKey & PH7_TKWRD_ARRAY ){
			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;
		}else if( nKey & PH7_TKWRD_BOOL ){
			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;
		}else if( nKey & PH7_TKWRD_INT ){
			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;
		}else if( nKey & PH7_TKWRD_STRING ){
			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;
		}else if( nKey & PH7_TKWRD_FLOAT ){
			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;
		}else if( nKey & PH7_TKWRD_OBJECT ){
			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;
		}else if( nKey == PH7_TKWRD_SELF || nKey == PH7_TKWRD_PARENT
				|| nKey == PH7_TKWRD_STATIC ){
			pOut->nType = SXU32_HIGH;
			pOut->sClass = pIn->sData;
		}else{
			return SXERR_SYNTAX;
		}
		pIn++;
	}else{
		/* Identifier — `null`, `void`, `never`, or class name (possibly
		 * namespaced as a\b\c). Match the well-known names case-insensitively. */
		SyString *pT = &pIn->sData;
		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){
			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;
			pIn++;
		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){
			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;
			pIn++;
		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){
			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;
			pIn++;
		}else{
			/* Class / interface name; consume namespace path a\b\c */
			SyToken *pFirst = pIn;
			SyToken *pLast = pIn;
			pOut->nType = SXU32_HIGH;
			pOut->sClass = pIn->sData;
			pIn++;
			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)
				&& (pIn[1].nType & PH7_TK_ID) ){
				pLast = &pIn[1];
				pIn += 2;
			}
			if( pLast != pFirst ){
				const char *zFirst = pFirst->sData.zString;
				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;
				pOut->sClass.zString = zFirst;
				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);
			}
			/* Namespace-qualify a bare (single-segment, non-absolute) class type so
			 * a `: Base` / `Base $x` hint in namespace N resolves to N\Base (or a
			 * `use` alias) at type-check time instead of the global \Base — mirrors
			 * the NEW/CALL/instanceof qualification. Absolute (\Base) and already-
			 * qualified (A\B) names are left as written, matching GenStateNsQualifyName. */
			/* Reserved type words that reach this identifier branch (false, true,
			 * mixed, iterable, callable) are NOT classes and must not be qualified
			 * (else `false|string` becomes `Ns\false|string`). */
			if( !bAbsolute && pLast == pFirst && !GenStateIsReservedTypeWord(&pOut->sClass) ){
				SyBlob sFqn;
				SyBlobInit(&sFqn,&pGen->pVm->sAllocator);
				GenStateResolveName(pGen,&pOut->sClass,&sFqn);
				if( SyBlobLength(&sFqn) != pOut->sClass.nByte
				 || SyMemcmp(SyBlobData(&sFqn),(const void *)pOut->sClass.zString,pOut->sClass.nByte) != 0 ){
					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
						(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));
					if( zDup ){
						SyStringInitFromBuf(&pOut->sClass,zDup,SyBlobLength(&sFqn));
					}
				}
				SyBlobRelease(&sFqn);
			}
		}
	}
	pGen->pIn = pIn;
	return SXRET_OK;
}

/*
 * Build the canonical PHP-formatted type text into pBlob from a list of
 * atoms. Order matches PHP's `zend_type` rendering:
 *   classes (in declaration order) | object | array | string | int | float | bool [| null]
 * If exactly one non-null atom is present and bNullable is true, the
 * shorthand `?T` form is emitted instead of `T|null`.
 */
static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)
{
	int i;
	int nNonNull = 0;
	int bAnyIntersection = 0;
	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];
	sxu32 nMaxGroup = 0;
	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;
	for( i = 0; i < nAtoms; i++ ){
		if( aAtoms[i].nType != UTA_NULL_FLAG ){
			nNonNull++;
			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){
				aGroupCount[aAtoms[i].nGroup]++;
				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;
			}
		}
	}
	for( i = 0; i < nAtoms; i++ ){
		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){
			bAnyIntersection = 1;
			break;
		}
	}
	if( bAnyIntersection ){
		/* Intersection / DNF rendering, in declaration (group) order: each group's
		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the
		 * whole type has more than one group (so a standalone `A&B` stays bare). */
		sxu32 g, nGroups = 0;
		int bFirstGroup = 1;
		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }
		for( g = 0; g <= nMaxGroup; g++ ){
			int bFirstMember = 1;
			int bWrap;
			if( aGroupCount[g] == 0 ) continue;
			/* Wrap a ≥2-member group in `()` whenever it shares the type with any
			 * other alternative — another group OR a trailing `null` (which is not
			 * counted in nGroups). So `A&B` stays bare but `(A&B)|null` keeps its
			 * parens, matching PHP's canonical text. */
			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 || bNullable));
			if( !bFirstGroup ) SyBlobAppend(pBlob, "|", 1);
			if( bWrap ) SyBlobAppend(pBlob, "(", 1);
			for( i = 0; i < nAtoms; i++ ){
				if( aAtoms[i].nType == UTA_NULL_FLAG || aAtoms[i].nGroup != g ) continue;
				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);
				if( aAtoms[i].nType == SXU32_HIGH ){
					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);
				}else{
					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);
				}
				bFirstMember = 0;
			}
			if( bWrap ) SyBlobAppend(pBlob, ")", 1);
			bFirstGroup = 0;
		}
		if( bNullable ){
			SyBlobAppend(pBlob, "|", 1);
			SyBlobAppend(pBlob, "null", 4);
		}
		return;
	}
	if( nNonNull == 1 && bNullable ){
		/* Shorthand: ?T */
		for( i = 0; i < nAtoms; i++ ){
			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;
			SyBlobAppend(pBlob, "?", 1);
			if( aAtoms[i].nType == SXU32_HIGH ){
				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);
			}else{
				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);
			}
			return;
		}
	}
	{
		int bFirst = 1;
		/* 1) Classes in declaration order */
		for( i = 0; i < nAtoms; i++ ){
			if( aAtoms[i].nType == SXU32_HIGH ){
				if( !bFirst ) SyBlobAppend(pBlob, "|", 1);
				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);
				bFirst = 0;
			}
		}
		/* 2) Built-ins in canonical order */
		{
			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,
				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };
			int k;
			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){
				for( i = 0; i < nAtoms; i++ ){
					if( aAtoms[i].nType == aOrder[k] ){
						if( !bFirst ) SyBlobAppend(pBlob, "|", 1);
						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);
						bFirst = 0;
						break;
					}
				}
			}
		}
		/* 3) null suffix */
		if( bNullable ){
			if( !bFirst ) SyBlobAppend(pBlob, "|", 1);
			SyBlobAppend(pBlob, "null", 4);
		}
	}
}

/*
 * Parse one `|`-separated part of a type declaration into aAtoms[*pnAtoms..],
 * tagging each appended atom with group id iGroup. A part is one of:
 *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or
 *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.
 * On return *pnMembers is the number of atoms in this part and *pbParen records
 * whether it was parenthesized.
 *
 * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is
 * resolved by a one-token lookahead: `&` continues the intersection only when it
 * is followed by a type atom (namespace separator / identifier / keyword);
 * otherwise it belongs to a by-ref parameter marker and the part ends, leaving
 * the `&` for the caller (compile.c param loop) to consume.
 */
static sxi32 GenStateParsePart(
	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,
	int *pnMembers, int *pbParen, sxu32 nLine)
{
	sxi32 rc;
	int nMembers = 0;
	int bParen = 0;
	*pnMembers = 0;
	*pbParen = 0;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){
		bParen = 1;
		pGen->pIn++; /* skip '(' */
	}
	for(;;){
		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){
			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,
				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);
			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;
		}
		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);
		if( rc != SXRET_OK ){
			return rc;
		}
		aAtoms[*pnAtoms].nGroup = iGroup;
		(*pnAtoms)++;
		nMembers++;
		/* Continue the intersection while `&` is followed by another type atom. */
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){
			SyToken *pNext = &pGen->pIn[1];
			if( pNext < pGen->pEnd
			 && (pNext->nType & (PH7_TK_NSSEP|PH7_TK_ID|PH7_TK_KEYWORD)) ){
				pGen->pIn++; /* skip '&' */
				continue;
			}
		}
		break;
	}
	if( bParen ){
		if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){
			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,
				"Malformed DNF type: expecting ')'");
			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;
		}
		pGen->pIn++; /* skip ')' */
		if( nMembers < 2 ){
			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,
				"Parenthesized type must be an intersection of at least two types");
			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;
		}
	}
	*pnMembers = nMembers;
	*pbParen = bParen;
	return SXRET_OK;
}

/*
 * Parse an entire (possibly union) type declaration starting at pGen->pIn.
 *
 * Outputs:
 *   *pnType, *pClass — single-type fast path: filled when there is exactly
 *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or
 *     SXU32_HIGH for a class.  pClass receives the duplicated class name.
 *   *pAlts            — populated only when this is a true union (≥2
 *     non-null alternatives, OR ≥1 class+null union, etc). The set must
 *     already be initialized by the caller (allocator set, etc).
 *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE
 *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.
 *     The two flag values are passed in via iNullableFlag/iUnionFlag.
 *   *pTypeText        — duplicated canonical type text for error messages.
 *
 * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or
 * SXERR_ABORT on fatal compile errors.
 */
PH7_PRIVATE sxi32 GenStateParseUnionTypeDecl(
	ph7_gen_state *pGen,
	sxu32 *pnType,
	SyString *pClass,
	SySet *pAlts,
	sxi32 *piTypeFlags,
	SyString *pTypeText,
	int iNullableFlag,
	int iUnionFlag,
	int bAllowVoid,
	sxu32 nLine
){
	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];
	int nAtoms = 0;
	int bShortNullable = 0;
	int bExplicitNull = 0;
	sxi32 rc;
	*pnType = 0;
	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);
	*piTypeFlags = 0;
	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);

	if( pGen->pIn >= pGen->pEnd ){
		return SXRET_OK;
	}
	/* Optional `?` shorthand prefix */
	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1
	 && pGen->pIn->sData.zString[0] == '?' ){
		bShortNullable = 1;
		pGen->pIn++;
		if( pGen->pIn >= pGen->pEnd ){
			return SXERR_SYNTAX;
		}
	}
	/* Parse the first part (a single atom, a bare top-level intersection, or a
	 * parenthesized DNF intersection), then any further `|`-separated parts. Each
	 * part is one OR-group; atoms within an intersection share the group id. */
	{
		int nMembers, bParen;
		sxu32 iGroup = 0;
		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);
		if( rc != SXRET_OK ){
			return rc;
		}
		/* Subsequent parts separated by `|`. A bare (unparenthesized) intersection
		 * is legal only as the sole part; once a `|` makes this a union every part
		 * must be a single type or a parenthesized intersection (`A&B|C` is invalid,
		 * write `(A&B)|C`). The loop-top check rejects a bare intersection followed
		 * by `|`; the after-loop check rejects one as the trailing part of a union. */
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)
			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '|' ){
			if( bShortNullable ){
				/* Match PHP's wording — `?T|X` is rejected as a parse error.
				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error
				 * already reported" so callers skip their own error emission. */
				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,
					"syntax error, unexpected token \"|\", expecting variable");
				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;
			}
			if( nMembers >= 2 && !bParen ){
				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,
					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");
				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;
			}
			pGen->pIn++; /* skip `|` */
			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);
			if( rc != SXRET_OK ){
				return rc;
			}
		}
		if( iGroup > 0 && nMembers >= 2 && !bParen ){
			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,
				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");
			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;
		}
	}
	/* Validation pass.
	 *
	 * Order matters: the union-membership checks for void/never run *before*
	 * the duplicate scan, and `void` standalone-ness is checked *before* the
	 * `?void` check below — reordering them would let `?void` slip through.
	 */
	{
		int i, j;
		int bHasNonNull = 0;
		int bAnyIntersection = 0;
		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];
		/* Tally how many atoms each OR-group holds; a group of ≥2 is an
		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */
		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;
		for( i = 0; i < nAtoms; i++ ){
			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;
		}
		for( i = 0; i < nAtoms; i++ ){
			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }
		}
		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must
		 * be written `(A&B)|null` (handled by the explicit-null DNF path). */
		if( bShortNullable && bAnyIntersection ){
			PH7_GenCompileError(pGen, E_ERROR, nLine,
				"Nullable intersection types are not supported; use (A&B)|null instead");
			return SXERR_SYNTAX;
		}
		for( i = 0; i < nAtoms; i++ ){
			/* Intersection members must be class/interface types (PHP rejects
			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/
			 * `true`/`false` in an intersection). */
			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){
				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);
				if( bClassLike ){
					SyString *pC = &aAtoms[i].sClass;
					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)
					 || (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)
					 || (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)
					 || (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){
						bClassLike = 0;
					}
				}
				if( !bClassLike ){
					const char *zName; sxu32 nName;
					if( aAtoms[i].nType == SXU32_HIGH ){
						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;
					}else{
						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;
					}
					PH7_GenCompileError(pGen, E_ERROR, nLine,
						"Type %.*s cannot be part of an intersection type",
						(int)nName, zName);
					return SXERR_SYNTAX;
				}
			}
			if( aAtoms[i].nType == UTA_VOID_FLAG ){
				if( nAtoms > 1 ){
					PH7_GenCompileError(pGen, E_ERROR, nLine,
						"Void can only be used as a standalone type");
					return SXERR_SYNTAX;
				}
				if( !bAllowVoid ){
					PH7_GenCompileError(pGen, E_ERROR, nLine,
						"void cannot be used here");
					return SXERR_SYNTAX;
				}
				if( bShortNullable ){
					PH7_GenCompileError(pGen, E_ERROR, nLine,
						"Void type cannot be nullable");
					return SXERR_SYNTAX;
				}
			}
			if( aAtoms[i].nType == UTA_NEVER_FLAG ){
				/* `never` is a bottom type usable only as a standalone RETURN
				 * type (never = the function does not return). Mirrors the void
				 * validation above; accepted here and enforced at compile time
				 * (explicit `return` banned) and run time (fall-off TypeError). */
				if( nAtoms > 1 || bShortNullable ){
					/* `?never` is `never|null`, a union — PHP reports it the
					 * same as any other non-standalone use. */
					PH7_GenCompileError(pGen, E_ERROR, nLine,
						"never can only be used as a standalone type");
					return SXERR_SYNTAX;
				}
				if( !bAllowVoid ){
					/* Return-only: params call with bAllowVoid=0. */
					PH7_GenCompileError(pGen, E_ERROR, nLine,
						"never cannot be used as a parameter type");
					return SXERR_SYNTAX;
				}
			}
			if( aAtoms[i].nType == UTA_NULL_FLAG ){
				bExplicitNull = 1;
			}else{
				bHasNonNull = 1;
			}
			/* Duplicate detection. Flag a repeat only within the same group
			 * (intersection dup `A&A`) or between two singleton groups (union dup
			 * `int|int` / `A|A`); a class appearing in two distinct intersection
			 * groups (`(A&B)|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF
			 * subsumption — e.g. `(A&B)|A` — is deferred.) */
			for( j = 0; j < i; j++ ){
				int bDup = 0;
				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);
				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1
				                   && aGroupCount[aAtoms[j].nGroup] == 1);
				if( !bSameGroup && !bBothSingleton ) continue;
				if( aAtoms[i].nType == aAtoms[j].nType ){
					if( aAtoms[i].nType == SXU32_HIGH ){
						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte
						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,
								aAtoms[j].sClass.zString,
								aAtoms[i].sClass.nByte) == 0 ){
							bDup = 1;
						}
					}else{
						bDup = 1;
					}
				}
				if( bDup ){
					const char *zName;
					sxu32 nName;
					if( aAtoms[i].nType == SXU32_HIGH ){
						zName = aAtoms[i].sClass.zString;
						nName = aAtoms[i].sClass.nByte;
					}else{
						zName = aAtoms[i].zCanon;
						nName = aAtoms[i].nCanon;
					}
					PH7_GenCompileError(pGen, E_ERROR, nLine,
						"Duplicate type %.*s is redundant", (int)nName, zName);
					return SXERR_SYNTAX;
				}
			}
		}
		if( !bHasNonNull && bExplicitNull ){
			if( bShortNullable ){
				/* `?null` is not a valid type — PHP rejects the shorthand. */
				PH7_GenCompileError(pGen, E_ERROR, nLine,
					"Null can not be used as a standalone type");
				return SXERR_SYNTAX;
			}
			/* Bare `null` standalone type (PHP 8.2): represent it as the null
			 * type flag so enforcement accepts only null. The single-type fast
			 * path below leaves *pnType untouched when there is no non-null
			 * atom, so set it here. */
			*pnType = MEMOBJ_NULL;
		}
	}
	/* Compute nullability flag */
	if( bShortNullable || bExplicitNull ){
		*piTypeFlags |= iNullableFlag;
	}
	/* Build canonical type text */
	if( pTypeText ){
		SyBlob sBlob;
		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);
		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,
			(bShortNullable || bExplicitNull) ? 1 : 0);
		if( SyBlobLength(&sBlob) > 0 ){
			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));
			if( zDup ){
				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));
			}
		}
		SyBlobRelease(&sBlob);
	}
	/* Decide single-type vs union storage. A "union" is anything with more
	 * than one non-null atom, OR a single class atom + null. Single scalar
	 * + null collapses to the existing nullable single-type fast path. */
	{
		int nNonNull = 0;
		int iNonNullIdx = -1;
		int i;
		for( i = 0; i < nAtoms; i++ ){
			if( aAtoms[i].nType != UTA_NULL_FLAG ){
				nNonNull++;
				iNonNullIdx = i;
			}
		}
		if( nNonNull <= 1 ){
			/* Fast path: store as single type. */
			if( iNonNullIdx >= 0 ){
				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];
				if( pA->nType == SXU32_HIGH ){
					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
						pA->sClass.zString, pA->sClass.nByte);
					if( zDup == 0 ) return SXERR_ABORT;
					*pnType = SXU32_HIGH;
					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);
				}else if( pA->nType == UTA_VOID_FLAG ){
					*pnType = MEMOBJ_VOID;
				}else if( pA->nType == UTA_NEVER_FLAG ){
					*pnType = MEMOBJ_NEVER;
				}else{
					*pnType = pA->nType;
				}
			}
		}else{
			/* True union — populate the alts set, leave *pnType = 0. */
			*piTypeFlags |= iUnionFlag;
			for( i = 0; i < nAtoms; i++ ){
				ph7_type_alt sAlt;
				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;
				SyZero(&sAlt, sizeof(sAlt));
				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */
				if( aAtoms[i].nType == SXU32_HIGH ){
					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);
					if( zDup == 0 ) return SXERR_ABORT;
					sAlt.nType = SXU32_HIGH;
					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);
				}else{
					sAlt.nType = aAtoms[i].nType;
					SyStringInitFromBuf(&sAlt.sClass, 0, 0);
				}
				SySetPut(pAlts, (const void *)&sAlt);
			}
		}
	}
	return SXRET_OK;
}

/*
 * Parse a return type declaration (`: type`) after a function/method signature.
 * pGen->pIn should point to the token after `)`.
 * Sets pFunc->nReturnType and pFunc->sReturnClass.
 * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,
 *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,
 *          and union types `: T|U`.
 */
PH7_PRIVATE sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)
{
	sxi32 iFlags = 0;
	sxi32 rc;
	sxu32 nLine;
	pFunc->nReturnType = 0;
	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);
	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);
	/* Reset ALL declared-return-type state, not just the scalar fields: this
	 * parser can legitimately run twice for one closure (legacy pre-use colon
	 * position + the php post-use position). Leaving stale union alternatives
	 * or the nullable flag behind merges two declarations — enforcement then
	 * honored a wiped `: int|string` over the real `: bool`. */
	SySetReset(&pFunc->aReturnUnion);
	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_COLON) == 0 ){
		return SXRET_OK;
	}
	pGen->pIn++; /* Skip ':' */
	if( pGen->pIn >= pGen->pEnd ){
		return SXRET_OK;
	}
	nLine = pGen->pIn->nLine;
	rc = GenStateParseUnionTypeDecl(
		pGen,
		&pFunc->nReturnType,
		&pFunc->sReturnClass,
		&pFunc->aReturnUnion,
		&iFlags,
		&pFunc->sReturnTypeName,
		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored
		                          * in aReturnUnion, so the func carries it explicitly */
		/* iUnionFlag */ 0,
		/* bAllowVoid */ 1,
		nLine);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	if( rc == SXERR_CORRUPT ){
		/* Error already reported */
		return SXERR_SYNTAX;
	}
	if( rc == SXERR_SYNTAX ){
		if( pGen->pIn < pGen->pEnd ){
			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,
				"syntax error, unexpected token \"%z\" in return type declaration",
				&pGen->pIn->sData);
		}else{
			PH7_GenCompileError(pGen, E_PARSE, nLine,
				"syntax error, unexpected end of file in return type declaration");
		}
		return SXERR_SYNTAX;
	}
	pFunc->iFlags |= (iFlags & VM_FUNC_RETURN_NULLABLE);
	return SXRET_OK;
}

PH7_PRIVATE sxi32 GenStateCompileFunc(
	ph7_gen_state *pGen, /* Code generator state */
	SyString *pName,     /* Function name. NULL otherwise */
	sxi32 iFlags,        /* Control flags */
	int bHandleClosure,  /* TRUE if we are dealing with a closure */
	ph7_vm_func **ppFunc /* OUT: function state */
	)
{
	ph7_vm_func *pFunc;
	SyToken *pEnd;
	sxu32 nLine;
	char *zName;
	sxi32 rc;
	/* Extract line number */
	nLine = pGen->pIn->nLine;
	/* Jump the left parenthesis '(' */
	pGen->pIn++;
	/* Delimit the function signature */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
	if( pEnd >= pGen->pEnd ){
		/* Syntax error */
		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable");
		(void)pName;
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		pGen->pIn = pGen->pEnd;
		return SXRET_OK;
	}
	/* Create the function state */
	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));
	if( pFunc == 0 ){
		goto OutOfMem;
	}
	/* Build the function name, prepending namespace if active */
	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){
		SyBlob sFQN;
		sxu32 nLen;
		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);
		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));
		SyBlobAppend(&sFQN,"\\",1);
		SyBlobAppend(&sFQN,pName->zString,pName->nByte);
		nLen = (sxu32)SyBlobLength(&sFQN);
		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);
		SyBlobRelease(&sFQN);
		if( zName == 0 ){
			goto OutOfMem;
		}
		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);
	}else{
		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);
		if( zName == 0 ){
			goto OutOfMem;
		}
		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);
	}
	/* Fallback start line (the '(' token); callers that know the line of the
	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */
	pFunc->nLine = nLine;
	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);
	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	if( pGen->pIn < pEnd ){
		/* Collect function arguments */
		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);
		if( rc == SXERR_ABORT ){
			/* Don't worry about freeing memory, everything will be released shortly */
			return SXERR_ABORT;
		}
	}
	/* Point past ')' and parse optional return type ': type' */
	pGen->pIn = &pEnd[1];
	{
		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);
		if( rcRt == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if( rcRt == SXERR_SYNTAX ){
			return SXERR_SYNTAX;
		}
	}
	if( bHandleClosure ){
		ph7_vm_func_closure_env sEnv;
		int got_this = 0; /* TRUE if $this have been seen */
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)
			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){
				sxu32 nLineLocal = pGen->pIn->nLine;
				/* Closure,record environment variable */
				pGen->pIn++;
				if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */
				/* Compile until we hit the first closing parenthesis */
				while( pGen->pIn < pGen->pEnd ){
					int iFlagsLocal = 0;
					if( pGen->pIn->nType & PH7_TK_RPAREN ){
						pGen->pIn++; /* Jump the closing parenthesis */
						break;
					}
					nLineLocal = pGen->pIn->nLine;
					if( pGen->pIn->nType & PH7_TK_AMPER ){
						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry
						 * to the variable's memory slot instead of copying its value. */
						iFlagsLocal = VM_FUNC_ARG_BY_REF;
						pGen->pIn++;
					}
					if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 || &pGen->pIn[1] >= pGen->pEnd
						|| (pGen->pIn[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,
								"Closure: Unexpected token. Expecting a variable name");
							if( rc == SXERR_ABORT ){
								return SXERR_ABORT;
							}
							/* Find the closing parenthesis */
							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){
								pGen->pIn++;
							}
							if(pGen->pIn < pGen->pEnd){
								pGen->pIn++;
							}
							break;
							/* TICKET 1433-95: No need for the else block below.*/
					}else{
						SyString *pNameLocal;
						char *zDup;
						/* Duplicate variable name */
						pNameLocal = &pGen->pIn[1].sData;
						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);
						if( zDup ){
							/* Zero the structure */
							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));
							sEnv.iFlags = iFlagsLocal;
							sEnv.nLine = nLineLocal; /* the capture's own source line (php warns here) */
							sEnv.nIdx = SXU32_HIGH;
							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);
							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);
							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&
								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){
									got_this = 1;
							}
							/* Save imported variable */
							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);
						}else{
							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
							 return SXERR_ABORT;
						}
					}
					pGen->pIn += 2; /* $ + variable name or any other unexpected token */
					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){
						/* Ignore trailing commas */
						pGen->pIn++;
					}
				}
				/* php 7.1+: the return type follows the use clause —
				 * `function (...) use (...) : int {`. Gated on the colon:
				 * GenStateParseReturnType resets the type fields at entry,
				 * so an unconditional call would wipe a type parsed at the
				 * legacy pre-use position. */
				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){
					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);
					if( rcRt2 == SXERR_ABORT ){
						return SXERR_ABORT;
					}else if( rcRt2 == SXERR_SYNTAX ){
						return SXERR_SYNTAX;
					}
				}
		}
		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){
			/* Make the $this variable [Current processed Object (class instance)]
			 * available to the closure environment — for EVERY non-static
			 * anonymous function, use list or not (php binds $this to any
			 * closure declared in a method; pre-fix only `use (...)` closures
			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of
			 * a global-scope closure is silently dropped at install. A static
			 * closure never binds $this (php). */
			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));
			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */
			sEnv.nIdx = SXU32_HIGH;
			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);
			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);
			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);
		}
		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){
			/* Mark as closure */
			pFunc->iFlags |= VM_FUNC_CLOSURE;
		}
	}
	/* Compile the body */
	rc = GenStateCompileFuncBody(&(*pGen),pFunc);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* The cursor sits just past the body's closing brace */
	pFunc->nEndLine = pGen->pIn[-1].nLine;
	if( ppFunc ){
		*ppFunc = pFunc;
	}
	rc = SXRET_OK;
	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){
		/* Reject a php-fatal redeclaration before hoisting the function */
		if( GenStateGuardFuncRedeclaration(pGen,pFunc) == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		/* Finally register the function */
		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);
	}
	if( rc == SXRET_OK ){
		return SXRET_OK;
	}
	/* Fall through if something goes wrong */
OutOfMem:
	/* If the supplied memory subsystem is so sick that we are unable to allocate
	 * a tiny chunk of memory, there is no much we can do here.
	 */
	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");
	return SXERR_ABORT;
}
/*
 * Compile a standard PHP function.
 *  Refer to the block-comment above for more information.
 */
PH7_PRIVATE sxi32 PH7_CompileFunction(ph7_gen_state *pGen)
{
	SyString *pName;
	sxi32 iFlags;
	sxu32 nKwLine;
	sxu32 nLine;
	sxi32 rc;

	nLine = pGen->pIn->nLine;
	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */
	pGen->pIn++; /* Jump the 'function' keyword */
	iFlags = 0;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){
		/* Return by reference,remember that */
		iFlags |= VM_FUNC_REF_RETURN;
		/* Jump the '&' token */
		pGen->pIn++;
	}
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
		/* Invalid function name */
		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		/* Sychronize with the next semi-colon or braces*/
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI|PH7_TK_OCB)) == 0 ){
			pGen->pIn++;
		}
		return SXRET_OK;
	}
	pName = &pGen->pIn->sData;
	nLine = pGen->pIn->nLine;
	/* Jump the function name */
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		/* Syntax error */
		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		/* Sychronize with the next semi-colon or '{' */
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI|PH7_TK_OCB)) == 0 ){
			pGen->pIn++;
		}
		return SXRET_OK;
	}
	/* Compile function body */
	{
		ph7_vm_func *pFuncState = 0;
		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);
		if( pFuncState ){
			/* Reflection getStartLine(): line of the 'function' keyword */
			pFuncState->nLine = nKwLine;
		}
	}
	return rc;
}
