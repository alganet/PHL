# src/ph7/compile_func.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1169/1326 lines (88.16%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "ph7int.h"` |
|        - |    7 | `#include "compile_int.h"` |
|        - |    8 | `/*` |
|        - |    9 | ` * Section:` |
|        - |   10 | ` *    Function compilation: argument collection and default values, the` |
|        - |   11 | ` *    union/return type-declaration parsers, function bodies and the` |
|        - |   12 | ` *    'function' statement itself.` |
|        - |   13 | ` * Status:` |
|        - |   14 | ` *    Stable.` |
|        - |   15 | ` */` |
|        - |   16 | `/*` |
|        - |   17 | ` * Process default argument values. That is,a function may define C++-style default value` |
|        - |   18 | ` * as follows:` |
|        - |   19 | ` * function makecoffee($type = "cappuccino")` |
|        - |   20 | ` * {` |
|        - |   21 | ` *   return "Making a cup of $type.\n";` |
|        - |   22 | ` * }` |
|        - |   23 | ` * Symisc eXtension.` |
|        - |   24 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|        - |   25 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|        - |   26 | ` *      Example: Work only with PH7,generate error under zend` |
|        - |   27 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|        - |   28 | ` *      {` |
|        - |   29 | ` *       var_dump($a);` |
|        - |   30 | ` *      }` |
|        - |   31 | ` *     //call test without args` |
|        - |   32 | ` *      test();` |
|        - |   33 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|        - |   34 | ` *      Example:` |
|        - |   35 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|        - |   36 | ` * 3 -) Function overloading!!` |
|        - |   37 | ` *      Example:` |
|        - |   38 | ` *      function foo($a) {` |
|        - |   39 | ` *   	  return $a.PHP_EOL;` |
|        - |   40 | ` *	    }` |
|        - |   41 | ` *	    function foo($a, $b) {` |
|        - |   42 | ` *   	  return $a + $b;` |
|        - |   43 | ` *	    }` |
|        - |   44 | ` *	    echo foo(5); // Prints "5"` |
|        - |   45 | ` *	    echo foo(5, 2); // Prints "7"` |
|        - |   46 | ` *      // Same arg` |
|        - |   47 | ` *	   function foo(string $a)` |
|        - |   48 | ` *	   {` |
|        - |   49 | ` *	     echo "a is a string\n";` |
|        - |   50 | ` *	     var_dump($a);` |
|        - |   51 | ` *	   }` |
|        - |   52 | ` *	  function foo(int $a)` |
|        - |   53 | ` *	  {` |
|        - |   54 | ` *	    echo "a is integer\n";` |
|        - |   55 | ` *	    var_dump($a);` |
|        - |   56 | ` *	  }` |
|        - |   57 | ` *	  function foo(array $a)` |
|        - |   58 | ` *	  {` |
|        - |   59 | ` * 	    echo "a is an array\n";` |
|        - |   60 | ` * 	    var_dump($a);` |
|        - |   61 | ` *	  }` |
|        - |   62 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|        - |   63 | ` *	  foo(52); // a is integer [second foo]` |
|        - |   64 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|        - |   65 | ` * Please refer to the official documentation for more information on the powerful extension` |
|        - |   66 | ` * introduced by the PH7 engine.` |
|        - |   67 | ` */` |
|    48476 |   68 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|        5 |   69 | `{` |
|        - |   70 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |   71 | `	SySet *pInstrContainer;` |
|        - |   72 | `	sxi32 rc;` |
|        - |   73 | `	/* Swap token stream */` |
|    48481 |   74 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|        - |   75 | `	/* A parameter default is a constant expression: php applies the same rules here` |
|        - |   76 | ``	 * as to a class constant, minus `new` (PHP 8.1 allows `new` in an initializer).`` |
|        - |   77 | `	 * The swap above has left pGen->pIn/pEnd spanning exactly this default. */` |
|        - |   78 | `	{` |
|    48481 |   79 | `		const char *zCErr = PH7_GenStateConstExprError(pGen,1);` |
|    48481 |   80 | `		if( zCErr ){` |
|        6 |   81 | `			sxi32 rcErr = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"%s",zCErr);` |
|        6 |   82 | `			RE_SWAP_DELIMITER(pGen);` |
|        6 |   83 | `			return rcErr == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |   84 | `		}` |
|        - |   85 | `	}` |
|    48477 |   86 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    48477 |   87 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|        - |   88 | `	/* Compile the expression holding the argument value. A parameter default is a` |
|        - |   89 | `	 * const-expression belonging to the current class (see iInMemberDefault) — so` |
|        - |   90 | `	 * __TRAIT__ in it reads pCurClass rather than walking into the enclosing method. */` |
|    48477 |   91 | `	pGen->iInMemberDefault++;` |
|    48477 |   92 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    48477 |   93 | `	pGen->iInMemberDefault--;` |
|        - |   94 | `	/* Emit the done instruction */` |
|    48477 |   95 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    48477 |   96 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - |   97 | `	/* Finished, like a function body: give back the doubling slack. A default is a` |
|        - |   98 | `	 * handful of instructions in a container that opened at eight. */` |
|    48477 |   99 | `	SySetShrinkToFit(&pArg->aByteCode);` |
|    48477 |  100 | `	RE_SWAP_DELIMITER(pGen);` |
|    48477 |  101 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  102 | `		return SXERR_ABORT;` |
|        - |  103 | `	}` |
|    48477 |  104 | `	return SXRET_OK;` |
|    24209 |  105 | `}` |
|        - |  106 | `/*` |
|        - |  107 | ` * Collect function arguments one after one.` |
|        - |  108 | ` * According to the PHP language reference manual.` |
|        - |  109 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|        - |  110 | ` * list of expressions.` |
|        - |  111 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|        - |  112 | ` * and default argument values. Variable-length argument lists are also supported,` |
|        - |  113 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|        - |  114 | ` * for more information.` |
|        - |  115 | ` * Example #1 Passing arrays to functions` |
|        - |  116 | ` * <?php` |
|        - |  117 | ` * function takes_array($input)` |
|        - |  118 | ` * {` |
|        - |  119 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|        - |  120 | ` * }` |
|        - |  121 | ` * ?>` |
|        - |  122 | ` * Making arguments be passed by reference` |
|        - |  123 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|        - |  124 | ` * within the function is changed, it does not get changed outside of the function).` |
|        - |  125 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|        - |  126 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|        - |  127 | ` * to the argument name in the function definition:` |
|        - |  128 | ` * Example #2 Passing function parameters by reference` |
|        - |  129 | ` * <?php` |
|        - |  130 | ` * function add_some_extra(&$string)` |
|        - |  131 | ` * {` |
|        - |  132 | ` *   $string .= 'and something extra.';` |
|        - |  133 | ` * }` |
|        - |  134 | ` * $str = 'This is a string, ';` |
|        - |  135 | ` * add_some_extra($str);` |
|        - |  136 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|        - |  137 | ` * ?>` |
|        - |  138 | ` *` |
|        - |  139 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|        - |  140 | ` * complex agrument values.Please refer to the official documentation for more information` |
|        - |  141 | ` * on these extension.` |
|        - |  142 | ` */` |
|   173719 |  143 | `PH7_PRIVATE sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|        5 |  144 | `{` |
|        - |  145 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|        - |  146 | `	SyToken *pIn;  /* Token stream */` |
|        - |  147 | `	SyBlob sSig;         /* Function signature */` |
|        - |  148 | `	char *zDup;          /* Copy of argument name */` |
|   173724 |  149 | ``	int bSeenVariadic = 0; /* a `...$x` was declared already: php refuses what follows */`` |
|        - |  150 | `	sxu32 nArgLine;      /* the line the current parameter starts on */` |
|        - |  151 | `	sxi32 rc;` |
|        - |  152 |  |
|   173724 |  153 | `	pIn = pGen->pIn;` |
|   173724 |  154 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|        - |  155 | `	/* Process arguments one after one */` |
|   219034 |  156 | `	for(;;){` |
|   438708 |  157 | `		if( pIn >= pEnd ){` |
|        - |  158 | `			/* No more arguments to process */` |
|   173674 |  159 | `			break;` |
|        - |  160 | `		}` |
|   265039 |  161 | `		nArgLine = pIn->nLine;` |
|   265039 |  162 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   265039 |  163 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   265039 |  164 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   265039 |  165 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   265039 |  166 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|        - |  167 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|        - |  168 | `		 * first token inside the main token stream */` |
|   265039 |  169 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  170 | `			return SXERR_ABORT;` |
|        - |  171 | `		}` |
|        - |  172 | `		/* Parse the promotion-modifier RUN (constructor property promotion, PHP` |
|        - |  173 | ``		 * 8.0+; `readonly` 8.1, asymmetric `(set)` 8.4, `final` 8.4). php takes`` |
|        - |  174 | `		 * them as a SET in any order, and ANY of them promotes the parameter --` |
|        - |  175 | ``		 * `final int $x` alone is a public final property, exactly as `readonly`` |
|        - |  176 | ``		 * int $x` alone is a public readonly one. The old fixed ladder read`` |
|        - |  177 | ``		 * readonly, then a visibility, then readonly again, so a `final` anywhere`` |
|        - |  178 | `		 * fell through to the type parser as "syntax error, unexpected token` |
|        - |  179 | `		 * final, expecting variable". */` |
|        - |  180 | `		{` |
|   265039 |  181 | `			int bReadonly = 0, bVisSeen = 0, bReadVis = 0, bSetSeen = 0, bFinal = 0;` |
|   265039 |  182 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   265039 |  183 | `			sxi32 iSetVisFlag = 0;` |
|   265039 |  184 | `			int nSetTok = 0;   /* left unset when bNameHead skips the peek below */` |
|        - |  185 | `			sxi32 nSetVis;` |
|   265039 |  186 | `			sxu32 nModLine = (pIn < pEnd) ? pIn->nLine : 0;` |
|   265039 |  187 | `			const char *zTwice = 0;    /* a modifier written twice; "" = the access-type one */` |
|   265039 |  188 | `			const char *zNotHere = 0;  /* ...and one a PARAMETER never takes */` |
|        - |  189 | ``			/* php lexes `private\Q` as one T_NAME_QUALIFIED, so a modifier word a`` |
|        - |  190 | ``			 * `\` follows is the head of a TYPE name and no modifier at all:`` |
|        - |  191 | ``			 * `function f(private\Q $x)` is an ordinary parameter, not a promoted`` |
|        - |  192 | `			 * property outside a constructor. */` |
|   265046 |  193 | `			int bNameHead = ( pIn + 1 < pEnd && (pIn[1].nType & PH7_TK_NSSEP)` |
|   397744 |  194 | `				&& GenStateTokensGlued(pIn,&pIn[1]) );` |
|   265265 |  195 | `			while( !bNameHead && zTwice == 0 && zNotHere == 0 && pIn < pEnd ){` |
|   265247 |  196 | `				if( GenStateIsReadonly(pIn) ){` |
|       33 |  197 | `					if( bReadonly ){` |
|      ! 0 |  198 | `						zTwice = "readonly";` |
|      ! 0 |  199 | `					}` |
|       33 |  200 | `					bReadonly = 1;` |
|       33 |  201 | `					pIn++;` |
|       33 |  202 | `					continue;` |
|        - |  203 | `				}` |
|   265219 |  204 | `				if( (pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    41622 |  205 | `					break;` |
|        - |  206 | `				}` |
|   223602 |  207 | `				nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   223602 |  208 | `				if( nSetVis ){` |
|        5 |  209 | `					if( bSetSeen ){` |
|      ! 0 |  210 | `						zTwice = "";` |
|      ! 0 |  211 | `					}` |
|        5 |  212 | `					bSetSeen = 1;` |
|        5 |  213 | `					iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|        5 |  214 | `					bVisSeen = 1;` |
|        5 |  215 | `					pIn += nSetTok;` |
|        5 |  216 | `					continue;` |
|        - |  217 | `				}` |
|        - |  218 | `				{` |
|   223598 |  219 | `					sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   223593 |  220 | `					if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED` |
|   223443 |  221 | `					 \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      189 |  222 | `						if( bReadVis ){` |
|        3 |  223 | `							zTwice = "";` |
|        1 |  224 | `						}` |
|      189 |  225 | `						bReadVis = 1;` |
|      189 |  226 | `						bVisSeen = 1;` |
|      189 |  227 | `						iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      248 |  228 | `							: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|       78 |  229 | `							: PH7_CLASS_PROT_PUBLIC;` |
|   223506 |  230 | `					}else if( nKw == PH7_TKWRD_FINAL ){` |
|        7 |  231 | `						if( bFinal ){` |
|      ! 0 |  232 | `							zTwice = "final";` |
|      ! 0 |  233 | `						}` |
|        7 |  234 | `						bFinal = 1;` |
|   223411 |  235 | `					}else if( nKw == PH7_TKWRD_STATIC ){` |
|        3 |  236 | `						zNotHere = "static";` |
|   223407 |  237 | `					}else if( nKw == PH7_TKWRD_ABSTRACT ){` |
|        3 |  238 | `						zNotHere = "abstract";` |
|        2 |  239 | `					}else{` |
|   223404 |  240 | ``						break; /* the type or the `$name` starts here */`` |
|        - |  241 | `					}` |
|      199 |  242 | `					pIn++;` |
|        - |  243 | `				}` |
|        5 |  244 | `			}` |
|        - |  245 | `			/* The same duplicate rules the class body's run enforces -- php words` |
|        - |  246 | `			 * them identically wherever the modifier was written -- plus the two a` |
|        - |  247 | `			 * PARAMETER never takes, which php words against "a parameter". */` |
|   265039 |  248 | `			if( zTwice ){` |
|        3 |  249 | `				pGen->iFatalTrace = PH7_FATAL_TRACE_NONE;` |
|        4 |  250 | `				rc = zTwice[0]` |
|      ! 0 |  251 | `					? PH7_GenCompileError(pGen,E_ERROR,nModLine,` |
|      ! 0 |  252 | `						"Multiple %s modifiers are not allowed",zTwice)` |
|        2 |  253 | `					: PH7_GenCompileError(pGen,E_ERROR,nModLine,` |
|        - |  254 | `						"Multiple access type modifiers are not allowed");` |
|        3 |  255 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  256 | `					return SXERR_ABORT;` |
|        - |  257 | `				}` |
|        3 |  258 | `				return SXERR_SYNTAX;` |
|        - |  259 | `			}` |
|   265037 |  260 | `			if( zNotHere ){` |
|        6 |  261 | `				pGen->iFatalTrace = PH7_FATAL_TRACE_NONE;` |
|        8 |  262 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nModLine,` |
|        2 |  263 | `					"Cannot use the %s modifier on a parameter",zNotHere);` |
|        6 |  264 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  265 | `					return SXERR_ABORT;` |
|        - |  266 | `				}` |
|        6 |  267 | `				return SXERR_SYNTAX;` |
|        - |  268 | `			}` |
|   265033 |  269 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|        5 |  270 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   265031 |  271 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|      ! 0 |  272 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|      ! 0 |  273 | `			}` |
|   265033 |  274 | `			if( bFinal ){` |
|        7 |  275 | `				sArg.iFlags \|= VM_FUNC_ARG_FINAL;` |
|        3 |  276 | `			}` |
|   265033 |  277 | `			if( bVisSeen \|\| bReadonly \|\| bFinal ){` |
|      191 |  278 | `				if( !bCtorCtx ){` |
|        6 |  279 | `					if( bAbstractCtx ){` |
|        3 |  280 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - |  281 | `							"Cannot declare promoted property in an abstract constructor");` |
|        2 |  282 | `					}else{` |
|        3 |  283 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - |  284 | `							"Cannot declare promoted property outside a constructor");` |
|        - |  285 | `					}` |
|        6 |  286 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  287 | `						return SXERR_ABORT;` |
|        - |  288 | `					}` |
|        6 |  289 | `					return SXERR_SYNTAX;` |
|        - |  290 | `				}` |
|      187 |  291 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      187 |  292 | `				sArg.iPromoteVis = iVis;` |
|      187 |  293 | `				if( bReadonly ){` |
|       33 |  294 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|       14 |  295 | `				}` |
|       91 |  296 | `			}` |
|        - |  297 | `		}` |
|        - |  298 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   265024 |  299 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   256745 |  300 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   248266 |  301 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   247869 |  302 | `			sxu32 nLineLocal = pIn->nLine;` |
|   247869 |  303 | `			sxi32 iTFlags = 0;` |
|   247869 |  304 | `			pGen->pIn = pIn;` |
|   247869 |  305 | `			rc = GenStateParseUnionTypeDecl(` |
|   123750 |  306 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   123750 |  307 | `				&iTFlags, &sArg.sTypeName,` |
|        - |  308 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|        - |  309 | `				/* bAllowVoid */ 0, /* bParamCtx */ 1,` |
|   123750 |  310 | `						nLineLocal);` |
|   247869 |  311 | `			pIn = pGen->pIn;` |
|   247869 |  312 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  313 | `				return SXERR_ABORT;` |
|   247869 |  314 | `			}else if( rc == SXERR_CORRUPT ){` |
|        - |  315 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|        3 |  316 | `				return SXERR_SYNTAX;` |
|   247867 |  317 | `			}else if( rc == SXERR_SYNTAX ){` |
|       22 |  318 | `				if( pIn < pEnd ){` |
|       31 |  319 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|        - |  320 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|        9 |  321 | `						&pIn->sData);` |
|       13 |  322 | `				}else{` |
|      ! 0 |  323 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|        - |  324 | `						"syntax error, unexpected end of file");` |
|        - |  325 | `				}` |
|       22 |  326 | `				return SXERR_SYNTAX;` |
|        - |  327 | `			}` |
|   247849 |  328 | `			sArg.iFlags \|= iTFlags;` |
|   123740 |  329 | `		}` |
|   265009 |  330 | `		if( pIn >= pEnd ){` |
|      ! 0 |  331 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,"Missing argument name");` |
|      ! 0 |  332 | `			return rc;` |
|        - |  333 | `		}` |
|   265009 |  334 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|        - |  335 | `			/* Pass by reference,record that */` |
|      499 |  336 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|      499 |  337 | `			pIn++;` |
|      247 |  338 | `		}` |
|   265009 |  339 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|        - |  340 | `			/* Variadic parameter: ...$args */` |
|     8233 |  341 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|     8233 |  342 | `			pIn++;` |
|     4108 |  343 | `		}` |
|   265009 |  344 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  345 | `			/* Invalid argument */` |
|        3 |  346 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|        3 |  347 | `			return rc;` |
|        - |  348 | `		}` |
|   265007 |  349 | `		pIn++; /* Jump the dollar sign */` |
|        - |  350 | `		/* Copy argument name */` |
|   265007 |  351 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   265007 |  352 | `		if( zDup == 0 ){` |
|      ! 0 |  353 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  354 | `			return SXERR_ABORT;` |
|        - |  355 | `		}` |
|   265007 |  356 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   265007 |  357 | `		pIn++;` |
|        - |  358 | `		/* php's four refusals of a parameter LIST, made in php's order once the` |
|        - |  359 | `		 * name is known, and each reported on the line the parameter STARTS on` |
|        - |  360 | ``		 * (its type or modifier, not its `$`): a name written twice, `$this`, a`` |
|        - |  361 | `		 * parameter after a variadic one, and a variadic one with a default.` |
|        - |  362 | `` 		 * None of the four was checked at all: `function q(...$a, $b) {}` `` |
|        - |  363 | `		 * compiled and ran here where php stops at compile time, which is a` |
|        - |  364 | ``		 * program `php -l` refuses and `phl -l` passed. */`` |
|        - |  365 | `		{` |
|   265007 |  366 | `			ph7_vm_func_arg *aPrev = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|   265007 |  367 | `			sxu32 nPrev = SySetUsed(&pFunc->aArgs);` |
|   265007 |  368 | `			int bRefused = 1;   /* MSVC cannot see that this implies rc was set: seed rc */` |
|        - |  369 | `			sxu32 n;` |
|   265007 |  370 | `			rc = SXRET_OK;` |
|   373541 |  371 | `			for( n = 0; n < nPrev; n++ ){` |
|   108541 |  372 | `				if( SyStringCmp(&aPrev[n].sName,&sArg.sName,SyMemcmp) == 0 ){` |
|        3 |  373 | `					break;` |
|        - |  374 | `				}` |
|    54191 |  375 | `			}` |
|   265007 |  376 | `			if( n < nPrev ){` |
|        4 |  377 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nArgLine,` |
|        1 |  378 | `					"Redefinition of parameter $%z",&sArg.sName);` |
|   265002 |  379 | `			}else if( SyStringLength(&sArg.sName) == sizeof("this")-1` |
|   152689 |  380 | `				&& SyMemcmp(SyStringData(&sArg.sName),"this",sizeof("this")-1) == 0 ){` |
|        3 |  381 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nArgLine,` |
|        - |  382 | `					"Cannot use $this as parameter");` |
|   265004 |  383 | `			}else if( bSeenVariadic ){` |
|       11 |  384 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nArgLine,` |
|        - |  385 | `					"Only the last parameter can be variadic");` |
|   264999 |  386 | `			}else if( (sArg.iFlags & VM_FUNC_ARG_VARIADIC) && pIn < pEnd && (pIn->nType & PH7_TK_EQUAL) ){` |
|        3 |  387 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nArgLine,` |
|        - |  388 | `					"Variadic parameter cannot have a default value");` |
|        2 |  389 | `			}else{` |
|   264993 |  390 | `				bRefused = 0;` |
|   264993 |  391 | `				if( sArg.iFlags & VM_FUNC_ARG_VARIADIC ){` |
|     8227 |  392 | `					bSeenVariadic = 1;` |
|     4105 |  393 | `				}` |
|        - |  394 | `			}` |
|   265007 |  395 | `			if( bRefused ){` |
|       18 |  396 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  397 | `					return SXERR_ABORT;` |
|        - |  398 | `				}` |
|       18 |  399 | `				return SXERR_SYNTAX;` |
|        - |  400 | `			}` |
|        - |  401 | `		}` |
|   264993 |  402 | `		if( pIn < pEnd ){` |
|   123610 |  403 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|        - |  404 | `				SyToken *pDefend;` |
|    48483 |  405 | `				sxi32 iNest = 0;` |
|    48483 |  406 | `				pIn++; /* Jump the equal sign */` |
|    48483 |  407 | `				pDefend = pIn;` |
|        - |  408 | `				/* Process the default value associated with this argument */` |
|    97283 |  409 | `				while( pDefend < pEnd ){` |
|    65003 |  410 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    16203 |  411 | `						break;` |
|        - |  412 | `					}` |
|    48805 |  413 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|        - |  414 | `						/* Increment nesting level */` |
|       71 |  415 | `						iNest++;` |
|    48772 |  416 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|        - |  417 | `						/* Decrement nesting level */` |
|       71 |  418 | `						iNest--;` |
|       33 |  419 | `					}` |
|    48805 |  420 | `					pDefend++;` |
|        5 |  421 | `				}` |
|    48483 |  422 | `				if( pIn >= pDefend ){` |
|        3 |  423 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,pIn->nLine,"Missing argument default value");` |
|        3 |  424 | `					return rc;` |
|        - |  425 | `				}` |
|        - |  426 | `				/* Process default value */` |
|    48481 |  427 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|    48481 |  428 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  429 | `					return rc;` |
|        - |  430 | `				}` |
|        - |  431 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|        - |  432 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|        - |  433 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|        - |  434 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|        - |  435 | `				 * arg-type check lets null through. */` |
|    48476 |  436 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|    44121 |  437 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|    44113 |  438 | `					&& &pIn[1] == pDefend` |
|    39744 |  439 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|    31749 |  440 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|    11889 |  441 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|        - |  442 | `` 					/* php 8.4 DEPRECATED the implicit-nullable form (`int $x = null` `` |
|        - |  443 | ``					 * without the `?`). PHL targets php's *non-deprecated* surface and`` |
|        - |  444 | ``					 * rejects it outright — the explicit `?int` must be written.`` |
|        - |  445 | ``					 * `mixed $x = null` is fine: mixed already includes null (explicit`` |
|        - |  446 | `					 * ?T / T\|null are already excluded via VM_FUNC_ARG_NULLABLE above). */` |
|        4 |  447 | `					if( sArg.sClass.nByte == sizeof("mixed")-1` |
|        5 |  448 | `						&& SyStrnicmp(SyStringData(&sArg.sClass),"mixed",sizeof("mixed")-1) == 0 ){` |
|        3 |  449 | `						sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|        2 |  450 | `					}else{` |
|        3 |  451 | `						const char *zSep = "";` |
|        3 |  452 | `						SyString sCls = { "", 0 };` |
|        3 |  453 | `						if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|      ! 0 |  454 | `							sCls = ((ph7_class *)pFunc->pUserData)->sDisp;` |
|      ! 0 |  455 | `							zSep = "::";` |
|      ! 0 |  456 | `						}` |
|        4 |  457 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,` |
|        - |  458 | `							"%z%s%z(): Cannot use null as the default for non-nullable parameter $%z; write the explicit ?T type instead",` |
|        1 |  459 | `							&sCls,zSep,&pFunc->sName,&sArg.sName);` |
|        3 |  460 | `						return SXERR_ABORT;` |
|        - |  461 | `					}` |
|        1 |  462 | `				}` |
|        - |  463 | `				/* Point beyond the default value */` |
|    48479 |  464 | `				pIn = pDefend;` |
|    24203 |  465 | `			}` |
|   123606 |  466 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 |  467 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|      ! 0 |  468 | `				return rc;` |
|        - |  469 | `			}` |
|   123606 |  470 | `			pIn++; /* Jump the trailing comma */` |
|    61709 |  471 | `		}` |
|        - |  472 | `		/* Append argument signature */` |
|   264989 |  473 | `		if( sArg.nType > 0 ){` |
|   247697 |  474 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|        - |  475 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    24394 |  476 | `				int marker = 'o';` |
|    24394 |  477 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    24394 |  478 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    12181 |  479 | `			}else{` |
|        - |  480 | `				int c;` |
|   223308 |  481 | `				c = 'n'; /* cc warning */` |
|        - |  482 | `				/* Type leading character */` |
|   223308 |  483 | `				switch(sArg.nType){` |
|    15947 |  484 | `				case MEMOBJ_HASHMAP:` |
|        - |  485 | `					/* Hashmap aka 'array' */` |
|    31854 |  486 | `					c = 'h';` |
|    31854 |  487 | `					break;` |
|    28082 |  488 | `				case MEMOBJ_INT:` |
|        - |  489 | `					/* Integer */` |
|    56088 |  490 | `					c = 'i';` |
|    56088 |  491 | `					break;` |
|     4009 |  492 | `				case MEMOBJ_BOOL:` |
|        - |  493 | `					/* Bool */` |
|     8011 |  494 | `					c = 'b';` |
|     8011 |  495 | `					break;` |
|    19853 |  496 | `				case MEMOBJ_REAL:` |
|        - |  497 | `					/* Float */` |
|    39656 |  498 | `					c = 'f';` |
|    39656 |  499 | `					break;` |
|    43900 |  500 | `				case MEMOBJ_STRING:` |
|        - |  501 | `					/* String */` |
|    87671 |  502 | `					c = 's';` |
|    87671 |  503 | `					break;` |
|       23 |  504 | `				case MEMOBJ_OBJ:` |
|        - |  505 | `					/* Object */` |
|       50 |  506 | `					c = 'o';` |
|       46 |  507 | `					break;` |
|        1 |  508 | `				default:` |
|        2 |  509 | `					break;` |
|        - |  510 | `				}` |
|   223308 |  511 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|        - |  512 | `			}` |
|   123669 |  513 | `		}else{` |
|        - |  514 | `			/* No type is associated with this parameter which mean` |
|        - |  515 | `			 * that this function is not condidate for overloading.` |
|        - |  516 | `			 */` |
|    17297 |  517 | `			SyBlobRelease(&sSig);` |
|        - |  518 | `		}` |
|        - |  519 | `		/* php's attribute placement rules, once the promotion modifiers are read:` |
|        - |  520 | `` 		 * a PROMOTED parameter is a property as well, so `#[\Override] public $p` `` |
|        - |  521 | `		 * in a constructor signature is accepted here and judged as the property` |
|        - |  522 | `		 * claim it is -- while php still NAMES the target "parameter". */` |
|   397283 |  523 | `		if( GenStateCheckAttrPlacement(&(*pGen),&sArg.aAttrs,32,` |
|   397288 |  524 | `				(sArg.iFlags & VM_FUNC_ARG_PROMOTED) ? (32\|8) : 32,0,0) == SXERR_ABORT ){` |
|      ! 0 |  525 | `			return SXERR_ABORT;` |
|        - |  526 | `		}` |
|        - |  527 | `		/* Save in the argument set */` |
|   264989 |  528 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|        5 |  529 | `	}` |
|   173674 |  530 | `	if( SyBlobLength(&sSig) > 0 ){` |
|        - |  531 | `		/* Save function signature */` |
|   159911 |  532 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|    79838 |  533 | `	}` |
|   173674 |  534 | `	return SXRET_OK;` |
|    86740 |  535 | `}` |
|        - |  536 | `/*` |
|        - |  537 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|        - |  538 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|        - |  539 | ` * the enclosing function. Returns the token just past the nested construct.` |
|        - |  540 | ` */` |
|      506 |  541 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|        5 |  542 | `{` |
|      511 |  543 | `	sxi32 iParen = 0;` |
|      511 |  544 | `	pIn++; /* past 'function'/'fn' */` |
|        - |  545 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|        - |  546 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|        - |  547 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|     3537 |  548 | `	while( pIn < pEnd ){` |
|     3537 |  549 | `		sxu32 t = pIn->nType;` |
|     3537 |  550 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|     2865 |  551 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|     2193 |  552 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|     1687 |  553 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|     3031 |  554 | `		pIn++;` |
|        5 |  555 | `	}` |
|      511 |  556 | `	if( pIn >= pEnd ){ return pIn; }` |
|        - |  557 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|        - |  558 | `	{` |
|      511 |  559 | `		sxi32 d = 0;` |
|     5629 |  560 | `		while( pIn < pEnd ){` |
|     5629 |  561 | `			sxu32 t = pIn->nType;` |
|     5629 |  562 | `			if( t & PH7_TK_OCB ){ d++; }` |
|     5095 |  563 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|     5123 |  564 | `			pIn++;` |
|        5 |  565 | `		}` |
|        - |  566 | `	}` |
|      511 |  567 | `	return pIn;` |
|      258 |  568 | `}` |
|        - |  569 | `/*` |
|        - |  570 | `` * ROOT C helper: from an `fn` keyword token, skip the whole arrow function -- its`` |
|        - |  571 | ` * signature, its optional return type and the single expression that is its body.` |
|        - |  572 | `` * The body ends where the expression parser would end it: a `,` or `;` at nesting`` |
|        - |  573 | ` * depth zero, or a closer that would unbalance the group the arrow sits in.` |
|        - |  574 | ` */` |
|      788 |  575 | `static SyToken * GenStateSkipArrowBody(SyToken *pIn, SyToken *pEnd)` |
|        5 |  576 | `{` |
|      793 |  577 | `	sxi32 iNest = 0;` |
|      793 |  578 | `	pIn++; /* past 'fn' */` |
|        - |  579 | ``	/* The signature: skip the balanced `( … )`. Only an optional `&` can stand`` |
|        - |  580 | ``	 * between `fn` and the `(`, so anything that ends a statement or a block first`` |
|        - |  581 | `	 * means this was never an arrow function -- stop rather than run off into the` |
|        - |  582 | ``	 * enclosing body, where a real `yield` would then go unseen. */`` |
|      893 |  583 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      120 |  584 | `		if( pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB\|PH7_TK_CCB) ){ return pIn; }` |
|      102 |  585 | `		pIn++;` |
|        2 |  586 | `	}` |
|     2176 |  587 | `	while( pIn < pEnd ){` |
|     2176 |  588 | `		sxu32 t = pIn->nType;` |
|     2176 |  589 | `		if( t & PH7_TK_LPAREN ){ iNest++; }` |
|     1406 |  590 | `		else if( t & PH7_TK_RPAREN ){ iNest--; if( iNest <= 0 ){ pIn++; break; } }` |
|     1406 |  591 | `		pIn++;` |
|        5 |  592 | `	}` |
|        - |  593 | ``	/* The `=>` that opens the body (a return type may sit before it). */`` |
|     1447 |  594 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     1119 |  595 | `		if( pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB) ){ return pIn; }` |
|      677 |  596 | `		pIn++;` |
|        5 |  597 | `	}` |
|      333 |  598 | `	if( pIn < pEnd ){ pIn++; } /* past '=>' */` |
|        - |  599 | `	/* The body expression. */` |
|      333 |  600 | `	iNest = 0;` |
|     2940 |  601 | `	while( pIn < pEnd ){` |
|     2940 |  602 | `		sxu32 t = pIn->nType;` |
|     2940 |  603 | `		if( t & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){ iNest++; }` |
|     2552 |  604 | `		else if( t & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      524 |  605 | `			if( iNest <= 0 ){ break; }` |
|      393 |  606 | `			iNest--;` |
|     2225 |  607 | `		}else if( (t & (PH7_TK_COMMA\|PH7_TK_SEMI)) && iNest <= 0 ){` |
|      202 |  608 | `			break;` |
|        - |  609 | `		}` |
|     2612 |  610 | `		pIn++;` |
|        5 |  611 | `	}` |
|      333 |  612 | `	return pIn;` |
|      398 |  613 | `}` |
|        - |  614 | `/*` |
|        - |  615 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|        - |  616 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|        - |  617 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|        - |  618 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|        - |  619 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|        - |  620 | ` * detached-mini-program path untouched.` |
|        - |  621 | ` */` |
|        - |  622 | `/*` |
|        - |  623 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|        - |  624 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|        - |  625 | ` * mixed, object.` |
|        - |  626 | ` */` |
|       30 |  627 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|        5 |  628 | `{` |
|        - |  629 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|        - |  630 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|        - |  631 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|        - |  632 | `	};` |
|        - |  633 | `	sxu32 i;` |
|       35 |  634 | `	if( nName > 0 && zName[0] == '\\' ){` |
|      ! 0 |  635 | `		zName++;` |
|      ! 0 |  636 | `		nName--;` |
|      ! 0 |  637 | `	}` |
|       49 |  638 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|       49 |  639 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|       35 |  640 | `			return 1;` |
|        - |  641 | `		}` |
|        9 |  642 | `	}` |
|      ! 0 |  643 | `	return 0;` |
|       20 |  644 | `}` |
|        - |  645 | `/*` |
|        - |  646 | ` * One atom of a generator's declared return type: is it a supertype of` |
|        - |  647 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|        - |  648 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|        - |  649 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|        - |  650 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|        - |  651 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|        - |  652 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|        - |  653 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|        - |  654 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|        - |  655 | ` * both rather than fatal on valid code (a recorded divergence).` |
|        - |  656 | ` */` |
|       32 |  657 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|        5 |  658 | `{` |
|       37 |  659 | `	if( nType == MEMOBJ_OBJ ){` |
|      ! 0 |  660 | ``		return 1; /* bare `object` */`` |
|        - |  661 | `	}` |
|       37 |  662 | `	if( nType != SXU32_HIGH ){` |
|        3 |  663 | `		return 0; /* scalar/array/void/never/null/... */` |
|        - |  664 | `	}` |
|       35 |  665 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|       35 |  666 | `		return 1;` |
|        - |  667 | `	}` |
|        - |  668 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|        - |  669 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|        - |  670 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|        - |  671 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|        - |  672 | `	{` |
|        - |  673 | `		SyBlob sFQN;` |
|        - |  674 | `		int bOk;` |
|      ! 0 |  675 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      ! 0 |  676 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|      ! 0 |  677 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|      ! 0 |  678 | `		SyBlobRelease(&sFQN);` |
|      ! 0 |  679 | `		return bOk;` |
|        - |  680 | `	}` |
|       21 |  681 | `}` |
|        - |  682 | `/*` |
|        - |  683 | ` * php 8: a generator function may only declare a return type that is a` |
|        - |  684 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|        - |  685 | ` * group qualifies only if every member does. Anything else is php's exact` |
|        - |  686 | ` * compile-time fatal "Generator return type must be a supertype of` |
|        - |  687 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|        - |  688 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|        - |  689 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|        - |  690 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|        - |  691 | ` */` |
|      470 |  692 | `PH7_PRIVATE sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|        5 |  693 | `{` |
|      475 |  694 | `	int bOk = 0;` |
|        - |  695 | `	sxu32 nLine;` |
|        - |  696 | `	sxi32 rc;` |
|      475 |  697 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|      443 |  698 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|        - |  699 | `	}` |
|       37 |  700 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|      ! 0 |  701 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  702 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|        - |  703 | `		sxu32 i,j;` |
|      ! 0 |  704 | `		for( i = 0; i < n && !bOk; i++ ){` |
|        - |  705 | `			int bGroupOk;` |
|      ! 0 |  706 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|      ! 0 |  707 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|        - |  708 | `			}` |
|      ! 0 |  709 | `			bGroupOk = 1;` |
|      ! 0 |  710 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|      ! 0 |  711 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|      ! 0 |  712 | `					bGroupOk = 0;` |
|      ! 0 |  713 | `					break;` |
|        - |  714 | `				}` |
|      ! 0 |  715 | `			}` |
|      ! 0 |  716 | `			bOk = bGroupOk;` |
|      ! 0 |  717 | `		}` |
|      ! 0 |  718 | `	}else{` |
|       37 |  719 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|        - |  720 | `	}` |
|       37 |  721 | `	if( bOk ){` |
|       35 |  722 | `		return SXRET_OK;` |
|        - |  723 | `	}` |
|        - |  724 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|        - |  725 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|        - |  726 | `	 * token of this stream — its line is the function's closing brace. php` |
|        - |  727 | `	 * reports the SIGNATURE line instead; the drift is the error-` |
|        - |  728 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|        3 |  729 | `	nLine = pGen->pIn[-1].nLine;` |
|        - |  730 | `	{` |
|        3 |  731 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|        3 |  732 | `		if( sGiven.nByte < 1 ){` |
|      ! 0 |  733 | `			sGiven = pFunc->sReturnClass;` |
|      ! 0 |  734 | `		}` |
|        3 |  735 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  736 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|        - |  737 | `	}` |
|        3 |  738 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      240 |  739 | `}` |
|   190133 |  740 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|        5 |  741 | `{` |
|   190138 |  742 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|   190138 |  743 | `	SyToken *pEnd = pGen->pEnd;` |
|   190138 |  744 | `	sxi32 iDepth = 0;` |
|   190138 |  745 | `	int bStarted = 0;` |
| 23308245 |  746 | `	while( pIn < pEnd ){` |
| 23308245 |  747 | `		sxu32 t = pIn->nType;` |
| 23308245 |  748 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 22303168 |  749 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 21298703 |  750 | `		if( t & PH7_TK_KEYWORD ){` |
|  1874505 |  751 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  1874505 |  752 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  1874055 |  753 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|        - |  754 | ``			/* An arrow body is a single EXPRESSION, but php takes a `yield` in one --`` |
|        - |  755 | ``			 * `fn() => yield 7` is a generator, whose yield belongs to the ARROW. Skip`` |
|        - |  756 | `			 * it, or the enclosing function would be classified a generator by a yield` |
|        - |  757 | `			 * that is not its own. */` |
|  1873549 |  758 | `			if( kw == PH7_TKWRD_FN ){ pIn = GenStateSkipArrowBody(pIn,pEnd); continue; }` |
|   935068 |  759 | `		}` |
| 21296959 |  760 | `		pIn++;` |
|        5 |  761 | `	}` |
|   189688 |  762 | `	return FALSE;` |
|    94928 |  763 | `}` |
|        - |  764 | `/*` |
|        - |  765 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|        - |  766 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  767 | ` * and this routine takes care of generating the appropriate error message.` |
|        - |  768 | ` */` |
|   190133 |  769 | `PH7_PRIVATE sxi32 GenStateCompileFuncBody(` |
|        - |  770 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |  771 | `	ph7_vm_func *pFunc    /* Function state */` |
|        - |  772 | `	)` |
|        5 |  773 | `{` |
|        - |  774 | `	SySet *pInstrContainer; /* Instruction container */` |
|        - |  775 | `	GenBlock *pBlock;` |
|        - |  776 | `	sxu32 nGotoOfft;` |
|        - |  777 | `	sxi32 rc;` |
|        - |  778 | `	/* Attach the new function */` |
|   190138 |  779 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   190138 |  780 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  781 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|        - |  782 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  783 | `		return SXERR_ABORT;` |
|        - |  784 | `	}` |
|   190138 |  785 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|        - |  786 | `	/* Swap bytecode containers */` |
|   190138 |  787 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   190138 |  788 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|        - |  789 | `	/* Emit constructor property promotion prologue:` |
|        - |  790 | `	 *   $this->NAME = $NAME;` |
|        - |  791 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|        - |  792 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|        - |  793 | `	{` |
|   190138 |  794 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|        - |  795 | `		sxu32 i;` |
|   454040 |  796 | `		for( i = 0; i < nArg; i++ ){` |
|   263907 |  797 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|        - |  798 | `			char *zSrc;` |
|        - |  799 | `			sxu32 nSrc,nName;` |
|        - |  800 | `			SySet sToken;` |
|        - |  801 | `			SyToken *pTmpIn,*pTmpEnd;` |
|        - |  802 | `			sxi32 rcPromote;` |
|   263907 |  803 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   263735 |  804 | `				continue;` |
|        - |  805 | `			}` |
|        - |  806 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|        - |  807 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|        - |  808 | `			 * copied), so it must outlive the function — never free it. The` |
|        - |  809 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|        - |  810 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      177 |  811 | `			nName = SyStringLength(&pArg->sName);` |
|      177 |  812 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      177 |  813 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      177 |  814 | `			if( zSrc == 0 ){` |
|      ! 0 |  815 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  816 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  817 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  818 | `				return SXERR_ABORT;` |
|        - |  819 | `			}` |
|        - |  820 | `			{` |
|      177 |  821 | `				char *z = zSrc;` |
|      177 |  822 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      177 |  823 | `				z += sizeof("$this->")-1;` |
|      177 |  824 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      177 |  825 | `				z += nName;` |
|      177 |  826 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      177 |  827 | `				z += sizeof(" = $")-1;` |
|      177 |  828 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      177 |  829 | `				z += nName;` |
|      177 |  830 | `				*z = 0;` |
|        - |  831 | `			}` |
|      177 |  832 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      177 |  833 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|      177 |  834 | `			pTmpIn = pGen->pIn;` |
|      177 |  835 | `			pTmpEnd = pGen->pEnd;` |
|      177 |  836 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      177 |  837 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      177 |  838 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      177 |  839 | `			pGen->pIn = pTmpIn;` |
|      177 |  840 | `			pGen->pEnd = pTmpEnd;` |
|      177 |  841 | `			SySetRelease(&sToken);` |
|      177 |  842 | `			if( rcPromote == SXERR_ABORT ){` |
|      ! 0 |  843 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  844 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  845 | `				return SXERR_ABORT;` |
|        - |  846 | `			}` |
|        - |  847 | `			/* Discard the assignment result — this is a statement expression. */` |
|      177 |  848 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       91 |  849 | `		}` |
|        - |  850 | `	}` |
|        - |  851 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|        - |  852 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|        - |  853 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|        - |  854 | `	 * generator — and vice versa — is classified independently. */` |
|        - |  855 | `	{` |
|   190138 |  856 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|   190138 |  857 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|        - |  858 | `		/* Compile the body */` |
|   190138 |  859 | `		PH7_CompileBlock(&(*pGen),0);` |
|   190138 |  860 | `		pGen->bInGenerator = bSavedGen;` |
|        - |  861 | `	}` |
|        - |  862 | `	/* Fix exception jumps now the destination is resolved */` |
|   190138 |  863 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - |  864 | `	/* Emit the final return if not yet done */` |
|   190138 |  865 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - |  866 | `	/* Fix gotos jumps now the destination is resolved */` |
|   190138 |  867 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|      ! 0 |  868 | `		rc = SXERR_ABORT;` |
|      ! 0 |  869 | `	}` |
|   190138 |  870 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|        - |  871 | `	/* Restore the default container */` |
|   190138 |  872 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - |  873 | `	/* Leave function block */` |
|   190138 |  874 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   190138 |  875 | `	if( rc == SXERR_ABORT ){` |
|        - |  876 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  877 | `		return SXERR_ABORT;` |
|        - |  878 | `	}` |
|        - |  879 | `	/* Scan for yield opcodes to detect generator functions */` |
|        - |  880 | `	{` |
|   190138 |  881 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|        - |  882 | `		sxu32 i;` |
| 15420825 |  883 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 15231142 |  884 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|      455 |  885 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      455 |  886 | `				break;` |
|        - |  887 | `			}` |
|  7604502 |  888 | `		}` |
|        - |  889 | `	}` |
|   190138 |  890 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  891 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|      455 |  892 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|      ! 0 |  893 | `			return SXERR_ABORT;` |
|        - |  894 | `		}` |
|      225 |  895 | `	}` |
|        - |  896 | `	/* This body is finished: the emit container has been restored above, so nothing` |
|        - |  897 | `	 * appends to it again, and the doubling slack it is holding -- up to as much as` |
|        - |  898 | `	 * it uses -- is dead for the rest of the process. The execution paths re-read` |
|        - |  899 | `	 * SySetBasePtr on every invocation, and the first invocation cannot precede this` |
|        - |  900 | `	 * point, so moving the buffer here is invisible to them. */` |
|   190138 |  901 | `	SySetShrinkToFit(&pFunc->aByteCode);` |
|        - |  902 | `	/* All done, function body compiled */` |
|   190138 |  903 | `	return SXRET_OK;` |
|    94928 |  904 | `}` |
|        - |  905 | `/*` |
|        - |  906 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|        - |  907 | ` * According to the PHP language reference manual.` |
|        - |  908 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|        - |  909 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|        - |  910 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|        - |  911 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - |  912 | ` *  Functions need not be defined before they are referenced.` |
|        - |  913 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|        - |  914 | ` *  a function even if they were defined inside and vice versa.` |
|        - |  915 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|        - |  916 | ` *  calls with over 32-64 recursion levels.` |
|        - |  917 | ` *` |
|        - |  918 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|        - |  919 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|        - |  920 | ` * on these extension.` |
|        - |  921 | ` */` |
|        - |  922 | `/*` |
|        - |  923 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|        - |  924 | ` */` |
|    81088 |  925 | `PH7_PRIVATE int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|        5 |  926 | `{` |
|        - |  927 | `	sxu32 i;` |
|   115851 |  928 | `	for( i = 0; i < n; i++ ){` |
|   107203 |  929 | `		int a = zA[i], b = zB[i];` |
|   107203 |  930 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|   107203 |  931 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|   107203 |  932 | `		if( a != b ) return a - b;` |
|    17332 |  933 | `	}` |
|     8653 |  934 | `	return 0;` |
|    40479 |  935 | `}` |
|        - |  936 | `/*` |
|        - |  937 | ` * Internal type-atom kinds used during union type parsing.` |
|        - |  938 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|        - |  939 | ` * (which are positive bit values stored in sxu32).` |
|        - |  940 | ` */` |
|        - |  941 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|        - |  942 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|        - |  943 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|        - |  944 |  |
|        - |  945 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|        - |  946 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|        - |  947 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|        - |  948 |  |
|        - |  949 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|        - |  950 | `struct PhlTypeAtom {` |
|        - |  951 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|        - |  952 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|        - |  953 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|        - |  954 | `	sxu32 nCanon;` |
|        - |  955 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|        - |  956 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|        - |  957 | `};` |
|        - |  958 |  |
|        - |  959 | `/*` |
|        - |  960 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|        - |  961 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|        - |  962 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|        - |  963 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|        - |  964 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|        - |  965 | ` * already be consumed by the caller.` |
|        - |  966 | ` */` |
|        - |  967 | `/*` |
|        - |  968 | ` * TRUE if pName is a reserved PHP type keyword (never a class name), so a type` |
|        - |  969 | ` * hint that spells it must not be namespace-qualified. Only the words that can` |
|        - |  970 | ` * reach GenStateParseOneTypeAtom's identifier branch as a bare name matter here` |
|        - |  971 | ` * (bool/int/float/string/array/object/self/static/parent arrive as keywords, and` |
|        - |  972 | ` * null/void/never are matched before the class path), but the full set is listed` |
|        - |  973 | ` * so the guard is robust to lexer changes.` |
|        - |  974 | ` */` |
|    64514 |  975 | `static int GenStateIsReservedTypeWord(const SyString *pName)` |
|        5 |  976 | `{` |
|        - |  977 | `	static const char *azWords[] = {` |
|        - |  978 | `		"false","true","mixed","iterable","callable","null","void","never",` |
|        - |  979 | `		"bool","boolean","int","integer","float","double","string","array",` |
|        - |  980 | `		"object","self","static","parent"` |
|        - |  981 | `	};` |
|        - |  982 | `	sxu32 i;` |
|   123477 |  983 | `	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){` |
|   122997 |  984 | `		sxu32 n = (sxu32)SyStrlen(azWords[i]);` |
|   122997 |  985 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azWords[i],n) == 0 ){` |
|    64039 |  986 | `			return 1;` |
|        - |  987 | `		}` |
|    29443 |  988 | `	}` |
|      485 |  989 | `	return 0;` |
|    32216 |  990 | `}` |
|        - |  991 | `/*` |
|        - |  992 | ` * php's builtin type words as looked up BY NAME (zend_lookup_builtin_type_by_name):` |
|        - |  993 | ``  * every type a declaration can spell with a plain label. `array` and `callable` `` |
|        - |  994 | ` * are deliberately absent — php's parser hands those two their own tokens — which` |
|        - |  995 | ``  * is why a qualified `\array` takes the "reserved" wording below while `\int` `` |
|        - |  996 | ` * takes "must be unqualified".` |
|        - |  997 | ` */` |
|       92 |  998 | `static int GenStateIsBuiltinTypeWord(const SyString *pName)` |
|        5 |  999 | `{` |
|        - | 1000 | `	static const char *azWords[] = {` |
|        - | 1001 | `		"int","float","string","bool","void","iterable","object","mixed",` |
|        - | 1002 | `		"never","null","false","true"` |
|        - | 1003 | `	};` |
|        - | 1004 | `	sxu32 i;` |
|     1153 | 1005 | `	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){` |
|     1065 | 1006 | `		sxu32 n = (sxu32)SyStrlen(azWords[i]);` |
|     1065 | 1007 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azWords[i],n) == 0 ){` |
|        6 | 1008 | `			return 1;` |
|        - | 1009 | `		}` |
|      532 | 1010 | `	}` |
|       92 | 1011 | `	return 0;` |
|       51 | 1012 | `}` |
|        - | 1013 | `/* The three class keywords that name a class RELATIVE to the current one. php` |
|        - | 1014 | ` * refuses to see them behind a qualifier at all, with its own wording. */` |
|       88 | 1015 | `static int GenStateIsRelativeClassWord(const SyString *pName)` |
|        4 | 1016 | `{` |
|       93 | 1017 | `	return (pName->nByte == 4 && SyStrnicmp(pName->zString,"self",4) == 0)` |
|       87 | 1018 | `	    \|\| (pName->nByte == 6 && SyStrnicmp(pName->zString,"parent",6) == 0)` |
|      132 | 1019 | `	    \|\| (pName->nByte == 6 && SyStrnicmp(pName->zString,"static",6) == 0);` |
|        4 | 1020 | `}` |
|        - | 1021 | `/*` |
|        - | 1022 | ` * php's reserved-name screens on a class-like type atom, in php's own order.` |
|        - | 1023 | ` * A name is screened only once a qualifier is present — a BARE reserved word is` |
|        - | 1024 | ` * a type, and the keyword / null-void-never branches above have already claimed` |
|        - | 1025 | ` * every one of them.` |
|        - | 1026 | ` *` |
|        - | 1027 | ` *   \int, namespace\false   -> "Type declaration 'int' must be unqualified"` |
|        - | 1028 | ` *   \self, \parent, \static -> "'\self' is an invalid class name"` |
|        - | 1029 | ` *   \array, \callable, A\int, A\self` |
|        - | 1030 | ` *                           -> "Cannot use \"NAME\" as a type name as it is` |
|        - | 1031 | ` *                              reserved", NAME being the name after resolution` |
|        - | 1032 | ` *` |
|        - | 1033 | ` * pLastSeg is the atom's trailing segment AS WRITTEN (php's screen looks only at` |
|        - | 1034 | ` * that), pResolved the whole atom after namespace resolution.` |
|        - | 1035 | ` */` |
|   449654 | 1036 | `static sxi32 GenStateScreenTypeName(` |
|        - | 1037 | `	ph7_gen_state *pGen,` |
|        - | 1038 | `	const SyString *pLastSeg,  /* trailing segment, as written */` |
|        - | 1039 | `	const SyString *pResolved, /* whole atom, after resolution */` |
|        - | 1040 | `	int bMulti,                /* the written name had more than one segment */` |
|        - | 1041 | ``	int bFQ,                   /* written absolute (`\X`) or `namespace\X` */`` |
|        - | 1042 | `	sxu32 nLine` |
|        5 | 1043 | `){` |
|        - | 1044 | `	sxi32 rc;` |
|   449659 | 1045 | `	if( !bMulti && !bFQ ){` |
|   449593 | 1046 | `		return SXRET_OK;` |
|        - | 1047 | `	}` |
|       71 | 1048 | `	if( !bMulti ){` |
|       35 | 1049 | `		if( GenStateIsBuiltinTypeWord(pLastSeg) ){` |
|        - | 1050 | `			char zLower[16];` |
|        - | 1051 | `			sxu32 i;` |
|        9 | 1052 | `			for( i = 0 ; i < pLastSeg->nByte && i < sizeof(zLower) ; i++ ){` |
|        7 | 1053 | `				unsigned char c = (unsigned char)pLastSeg->zString[i];` |
|        7 | 1054 | `				zLower[i] = (char)((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);` |
|        4 | 1055 | `			}` |
|        4 | 1056 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1057 | `				"Type declaration '%.*s' must be unqualified",(int)i,zLower);` |
|        3 | 1058 | `			return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1059 | `		}` |
|       32 | 1060 | `		if( GenStateIsRelativeClassWord(pLastSeg) ){` |
|        4 | 1061 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1062 | `				"'\\%z' is an invalid class name",pLastSeg);` |
|        3 | 1063 | `			return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1064 | `		}` |
|       13 | 1065 | `	}` |
|       62 | 1066 | `	if( GenStateIsBuiltinTypeWord(pLastSeg) \|\| GenStateIsRelativeClassWord(pLastSeg)` |
|       60 | 1067 | `	 \|\| (pLastSeg->nByte == 5 && SyStrnicmp(pLastSeg->zString,"array",5) == 0)` |
|       64 | 1068 | `	 \|\| (pLastSeg->nByte == 8 && SyStrnicmp(pLastSeg->zString,"callable",8) == 0) ){` |
|        4 | 1069 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1070 | `			"Cannot use \"%z\" as a type name as it is reserved",pResolved);` |
|        3 | 1071 | `		return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1072 | `	}` |
|       64 | 1073 | `	return SXRET_OK;` |
|   224503 | 1074 | `}` |
|   449658 | 1075 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|        5 | 1076 | `{` |
|   449663 | 1077 | `	SyToken *pIn = pGen->pIn;` |
|        - | 1078 | `	SyString sLastSeg;      /* trailing segment of the atom, as written */` |
|        - | 1079 | `	sxu32 nLine;` |
|   449663 | 1080 | `	int bAbsolute = 0;` |
|        - | 1081 | `	int bQualified;` |
|   449663 | 1082 | `	int bMulti = 0;         /* the written name had more than one segment */` |
|   449663 | 1083 | ``	int bFQ = 0;            /* written absolute or `namespace\`-relative */`` |
|        - | 1084 | `	sxi32 rcScreen;` |
|   449663 | 1085 | `	SyStringInitFromBuf(&sLastSeg, 0, 0);` |
|   449663 | 1086 | `	SyZero(pOut, sizeof(*pOut));` |
|   449663 | 1087 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   449663 | 1088 | `	if( pIn >= pGen->pEnd ){` |
|      ! 0 | 1089 | `		return SXERR_SYNTAX;` |
|        - | 1090 | `	}` |
|   449663 | 1091 | `	nLine = pIn->nLine;` |
|        - | 1092 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   449663 | 1093 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       37 | 1094 | `		bAbsolute = bFQ = 1; /* fully-qualified: never prefix the current namespace */` |
|       37 | 1095 | `		pIn++;` |
|       37 | 1096 | `		if( pIn >= pGen->pEnd ){` |
|      ! 0 | 1097 | `			return SXERR_SYNTAX;` |
|        - | 1098 | `		}` |
|       16 | 1099 | `	}` |
|        - | 1100 | ``	/* `namespace\X` type hint: the CURRENT namespace spelled out, fully qualified`` |
|        - | 1101 | `` 	 * from there. Collected here rather than below because the leading `namespace` `` |
|        - | 1102 | `	 * is a KEYWORD token, which the atom parser would otherwise reject outright. */` |
|   449663 | 1103 | `	if( !bAbsolute ){` |
|        - | 1104 | `		SyBlob sRel;` |
|   449631 | 1105 | `		SyBlobInit(&sRel,&pGen->pVm->sAllocator);` |
|   449631 | 1106 | `		if( GenStateNsRelPrefix(pGen,&pIn,pGen->pEnd,&sRel) ){` |
|        - | 1107 | `			char *zDup;` |
|        7 | 1108 | `			bFQ = 1;` |
|        7 | 1109 | `			SyBlobAppend(&sRel,pIn->sData.zString,pIn->sData.nByte);` |
|        7 | 1110 | `			sLastSeg = pIn->sData;` |
|        7 | 1111 | `			pIn++;` |
|        9 | 1112 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|        7 | 1113 | `				&& (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      ! 0 | 1114 | `				SyBlobAppend(&sRel,"\\",1);` |
|      ! 0 | 1115 | `				SyBlobAppend(&sRel,pIn[1].sData.zString,pIn[1].sData.nByte);` |
|      ! 0 | 1116 | `				sLastSeg = pIn[1].sData;` |
|      ! 0 | 1117 | `				bMulti = 1;` |
|      ! 0 | 1118 | `				pIn += 2;` |
|      ! 0 | 1119 | `			}` |
|       10 | 1120 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        6 | 1121 | `				(const char *)SyBlobData(&sRel),SyBlobLength(&sRel));` |
|        7 | 1122 | `			if( zDup == 0 ){` |
|      ! 0 | 1123 | `				SyBlobRelease(&sRel);` |
|      ! 0 | 1124 | `				return SXERR_ABORT;` |
|        - | 1125 | `			}` |
|        7 | 1126 | `			pOut->nType = SXU32_HIGH;` |
|        7 | 1127 | `			SyStringInitFromBuf(&pOut->sClass,zDup,SyBlobLength(&sRel));` |
|        7 | 1128 | `			SyBlobRelease(&sRel);` |
|        7 | 1129 | `			rcScreen = GenStateScreenTypeName(pGen,&sLastSeg,&pOut->sClass,bMulti,bFQ,nLine);` |
|        7 | 1130 | `			if( rcScreen != SXRET_OK ){` |
|      ! 0 | 1131 | `				return rcScreen;` |
|        - | 1132 | `			}` |
|        7 | 1133 | `			pGen->pIn = pIn;` |
|        7 | 1134 | `			return SXRET_OK;` |
|        - | 1135 | `		}` |
|   449625 | 1136 | `		SyBlobRelease(&sRel);` |
|   224481 | 1137 | `	}` |
|   449657 | 1138 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        2 | 1139 | `		return SXERR_SYNTAX;` |
|        - | 1140 | `	}` |
|        - | 1141 | ``	/* php's lexer matches `{LABEL}("\\"{LABEL})+` as ONE T_NAME_QUALIFIED token`` |
|        - | 1142 | `	 * BEFORE it ever looks a label up in the keyword table, so every reserved word` |
|        - | 1143 | ``	 * is a legal SEGMENT of a qualified name: `Default\Q`, `A\Default`, even`` |
|        - | 1144 | ``	 * `static\Q` and `array\Q` are names, and only a BARE reserved word is a`` |
|        - | 1145 | ``	 * keyword. A `\` on either side is therefore what decides, so a keyword the`` |
|        - | 1146 | `	 * separator follows takes the class-name path below rather than the scalar` |
|        - | 1147 | ``	 * keyword branch (`Default\Q $x` used to be a syntax error). The `\` has to be`` |
|        - | 1148 | ``	 * GLUED, exactly as php's lexer requires: `private \Q $x` is still a modifier`` |
|        - | 1149 | `	 * and a type, and reading it as a name broke every promoted property. The` |
|        - | 1150 | `	 * TRAILING segments below stay loose about spacing, as every other name` |
|        - | 1151 | `	 * collector in the compiler is — there a stray space only widens what invalid` |
|        - | 1152 | `	 * source is accepted, it never re-reads valid source as something else. */` |
|   449673 | 1153 | `	bQualified = ( pIn + 1 < pGen->pEnd && (pIn[1].nType & PH7_TK_NSSEP)` |
|   674804 | 1154 | `		&& GenStateTokensGlued(pIn,&pIn[1]) );` |
|   449655 | 1155 | `	sLastSeg = pIn->sData;` |
|   449655 | 1156 | `	if( (pIn->nType & PH7_TK_KEYWORD) && !bQualified ){` |
|   376447 | 1157 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   376447 | 1158 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|    71661 | 1159 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   340569 | 1160 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|    55819 | 1161 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   276844 | 1162 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|    65123 | 1163 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   216372 | 1164 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   128018 | 1165 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   119757 | 1166 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|    55556 | 1167 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|    28032 | 1168 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|       65 | 1169 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      265 | 1170 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       99 | 1171 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|      233 | 1172 | `			pOut->nType = SXU32_HIGH;` |
|      233 | 1173 | `			pOut->sClass = pIn->sData;` |
|      119 | 1174 | `		}else{` |
|        3 | 1175 | `			return SXERR_SYNTAX;` |
|        - | 1176 | `		}` |
|   376445 | 1177 | `		pIn++;` |
|   187955 | 1178 | `	}else{` |
|        - | 1179 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|        - | 1180 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    73213 | 1181 | `		SyString *pT = &pIn->sData;` |
|    73213 | 1182 | `		if( !bQualified && pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|       41 | 1183 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|       41 | 1184 | `			pIn++;` |
|    73195 | 1185 | `		}else if( !bQualified && pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     8573 | 1186 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     8573 | 1187 | `			pIn++;` |
|    68880 | 1188 | `		}else if( !bQualified && pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       39 | 1189 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       39 | 1190 | `			pIn++;` |
|       22 | 1191 | `		}else{` |
|        - | 1192 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    64575 | 1193 | `			SyToken *pFirst = pIn;` |
|    64575 | 1194 | `			SyToken *pLast = pIn;` |
|    64575 | 1195 | `			pOut->nType = SXU32_HIGH;` |
|    64575 | 1196 | `			pOut->sClass = pIn->sData;` |
|    64575 | 1197 | `			pIn++;` |
|    96958 | 1198 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    64632 | 1199 | `				&& (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       41 | 1200 | `				pLast = &pIn[1];` |
|       41 | 1201 | `				pIn += 2;` |
|        3 | 1202 | `			}` |
|    64575 | 1203 | `			if( pLast != pFirst ){` |
|       39 | 1204 | `				const char *zFirst = pFirst->sData.zString;` |
|       39 | 1205 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       39 | 1206 | `				pOut->sClass.zString = zFirst;` |
|       39 | 1207 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       39 | 1208 | `				sLastSeg = pLast->sData;` |
|       39 | 1209 | `				bMulti = 1;` |
|       18 | 1210 | `			}` |
|        - | 1211 | `` 			/* Namespace-qualify a non-absolute class type so a `: Base` / `Base $x` `` |
|        - | 1212 | ``			 * hint in namespace N resolves to N\Base (or a `use` alias) at`` |
|        - | 1213 | `			 * type-check time instead of the global \Base — mirrors the` |
|        - | 1214 | ``			 * NEW/CALL/instanceof qualification. A QUALIFIED `A\B` hint resolves`` |
|        - | 1215 | ``			 * exactly the way `new A\B` does: GenStateResolveName maps the LEADING`` |
|        - | 1216 | `			 * segment through the imports and keeps the tail, else prepends the` |
|        - | 1217 | `			 * current namespace. Only absolute (\Base) names stay as written.` |
|        - | 1218 | ``			 * Qualified hints used to be stored AS WRITTEN, so `: Sub\Thing` inside`` |
|        - | 1219 | ``			 * `namespace App;` compared against the literal `Sub\Thing` while the`` |
|        - | 1220 | ``			 * value's class was `App\Sub\Thing` — every qualified type face in a`` |
|        - | 1221 | `			 * namespaced tree raised a TypeError. */` |
|        - | 1222 | `			/* Reserved type words that reach this identifier branch (false, true,` |
|        - | 1223 | `			 * mixed, iterable, callable) are NOT classes and must not be qualified` |
|        - | 1224 | ``			 * (else `false\|string` becomes `Ns\false\|string`); they are always`` |
|        - | 1225 | `			 * single-segment, so a qualified name never consults the list. */` |
|    64570 | 1226 | `			if( !bAbsolute` |
|    64561 | 1227 | `			 && !(pLast == pFirst && GenStateIsReservedTypeWord(&pOut->sClass)) ){` |
|        - | 1228 | `				SyBlob sFqn;` |
|      513 | 1229 | `				SyBlobInit(&sFqn,&pGen->pVm->sAllocator);` |
|      513 | 1230 | `				GenStateResolveName(pGen,&pOut->sClass,&sFqn);` |
|      508 | 1231 | `				if( SyBlobLength(&sFqn) != pOut->sClass.nByte` |
|      491 | 1232 | `				 \|\| SyMemcmp(SyBlobData(&sFqn),(const void *)pOut->sClass.zString,pOut->sClass.nByte) != 0 ){` |
|       69 | 1233 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       44 | 1234 | `						(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));` |
|       47 | 1235 | `					if( zDup ){` |
|       47 | 1236 | `						SyStringInitFromBuf(&pOut->sClass,zDup,SyBlobLength(&sFqn));` |
|       22 | 1237 | `					}` |
|       22 | 1238 | `				}` |
|      513 | 1239 | `				SyBlobRelease(&sFqn);` |
|      254 | 1240 | `			}` |
|        - | 1241 | `		}` |
|        - | 1242 | `	}` |
|        - | 1243 | `	/* php screens the name only once a qualifier is in play; a scalar atom that` |
|        - | 1244 | ``	 * got here bare (`int`, `array`) carries no sClass, so the written word is`` |
|        - | 1245 | `	 * both the segment and the resolved name. */` |
|   674148 | 1246 | `	rcScreen = GenStateScreenTypeName(pGen,&sLastSeg,` |
|   449648 | 1247 | `		pOut->nType == SXU32_HIGH ? &pOut->sClass : &sLastSeg,bMulti,bFQ,nLine);` |
|   449653 | 1248 | `	if( rcScreen != SXRET_OK ){` |
|        9 | 1249 | `		return rcScreen;` |
|        - | 1250 | `	}` |
|   449647 | 1251 | `	pGen->pIn = pIn;` |
|   449647 | 1252 | `	return SXRET_OK;` |
|   224505 | 1253 | `}` |
|        - | 1254 |  |
|        - | 1255 | `/* Class-name ATOMS that php files with the BUILT-IN types rather than the` |
|        - | 1256 | `` * classes. `callable`, `false` and `true` are type-mask bits in zend, so they`` |
|        - | 1257 | `` * print AFTER every class name however they were written (`false\|A1` reads`` |
|        - | 1258 | `` * `A1\|false`). `iterable` is two types at once: the class `Traversable`, which`` |
|        - | 1259 | `` * keeps the atom's declaration position among the classes, plus `array`, which`` |
|        - | 1260 | `` * prints with the builtins (`iterable\|A1` reads `Traversable\|A1\|array`). PH7`` |
|        - | 1261 | ` * parses all four as class-name atoms, which is why they used to sort with the` |
|        - | 1262 | ` * classes. */` |
|        - | 1263 | `#define GEN_ATOM_PLAIN    0` |
|        - | 1264 | `#define GEN_ATOM_CALLABLE 1` |
|        - | 1265 | `#define GEN_ATOM_FALSE    2` |
|        - | 1266 | `#define GEN_ATOM_TRUE     3` |
|        - | 1267 | `#define GEN_ATOM_ITERABLE 4` |
|  1774278 | 1268 | `static int GenAtomMaskKind(const PhlTypeAtom *pAtom)` |
|        5 | 1269 | `{` |
|  1774283 | 1270 | `	const SyString *p = &pAtom->sClass;` |
|  1774283 | 1271 | `	if( pAtom->nType != SXU32_HIGH \|\| p->zString == 0 ){` |
|  1466871 | 1272 | `		return GEN_ATOM_PLAIN;` |
|        - | 1273 | `	}` |
|   307417 | 1274 | `	if( p->nByte == 8 && SyStrnicmp(p->zString,"callable",8) == 0 ) return GEN_ATOM_CALLABLE;` |
|   306039 | 1275 | `	if( p->nByte == 5 && SyStrnicmp(p->zString,"false",5) == 0 )    return GEN_ATOM_FALSE;` |
|   123592 | 1276 | `	if( p->nByte == 4 && SyStrnicmp(p->zString,"true",4) == 0 )     return GEN_ATOM_TRUE;` |
|   123532 | 1277 | `	if( p->nByte == 8 && SyStrnicmp(p->zString,"iterable",8) == 0 ) return GEN_ATOM_ITERABLE;` |
|   123238 | 1278 | `	return GEN_ATOM_PLAIN;` |
|   885843 | 1279 | `}` |
|        - | 1280 | ``/* Emit one class-like atom: when *bExpandIterable*, `iterable` contributes only`` |
|        - | 1281 | `` * its Traversable half here (the `array` half rides the built-in pass). php`` |
|        - | 1282 | `` * expands it only in a COMPOUND type — a standalone `iterable`/`?iterable` keeps`` |
|        - | 1283 | ` * its name in the canonical text (which is what Reflection prints; the TypeError` |
|        - | 1284 | ` * for a standalone hint is rendered separately, by VmClassHintTypeName). */` |
|    24739 | 1285 | `static void GenAppendClassAtom(SyBlob *pBlob, const PhlTypeAtom *pAtom, int bExpandIterable)` |
|        5 | 1286 | `{` |
|    24744 | 1287 | `	if( bExpandIterable && GenAtomMaskKind(pAtom) == GEN_ATOM_ITERABLE ){` |
|       16 | 1288 | `		SyBlobAppend(pBlob, "Traversable", sizeof("Traversable")-1);` |
|       16 | 1289 | `		return;` |
|        - | 1290 | `	}` |
|    24730 | 1291 | `	SyBlobAppend(pBlob, pAtom->sClass.zString, pAtom->sClass.nByte);` |
|    12358 | 1292 | `}` |
|        - | 1293 | `/*` |
|        - | 1294 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|        - | 1295 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|        - | 1296 | ` *   classes (in declaration order)` |
|        - | 1297 | ` *   \| callable \| object \| array \| string \| int \| float \| bool \| false \| true` |
|        - | 1298 | ` *   [\| null]` |
|        - | 1299 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|        - | 1300 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|        - | 1301 | ` */` |
|   409673 | 1302 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|        5 | 1303 | `{` |
|        - | 1304 | `	int i;` |
|   409678 | 1305 | `	int nNonNull = 0;` |
|   409678 | 1306 | `	int bAnyIntersection = 0;` |
|        - | 1307 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|   409678 | 1308 | `	sxu32 nMaxGroup = 0;` |
| 13519214 | 1309 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   859290 | 1310 | `	for( i = 0; i < nAtoms; i++ ){` |
|   449617 | 1311 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   449581 | 1312 | `			nNonNull++;` |
|   449581 | 1313 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|   449581 | 1314 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|   449581 | 1315 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|   224459 | 1316 | `			}` |
|   224459 | 1317 | `		}` |
|   224482 | 1318 | `	}` |
|   859194 | 1319 | `	for( i = 0; i < nAtoms; i++ ){` |
|   449563 | 1320 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       47 | 1321 | `			bAnyIntersection = 1;` |
|       47 | 1322 | `			break;` |
|        - | 1323 | `		}` |
|   224434 | 1324 | `	}` |
|   409678 | 1325 | `	if( bAnyIntersection ){` |
|        - | 1326 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|        - | 1327 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|        - | 1328 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|       47 | 1329 | `		sxu32 g, nGroups = 0;` |
|       47 | 1330 | `		int bFirstGroup = 1;` |
|      101 | 1331 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|      101 | 1332 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|       59 | 1333 | `			int bFirstMember = 1;` |
|        - | 1334 | `			int bWrap;` |
|       59 | 1335 | `			if( aGroupCount[g] == 0 ) continue;` |
|        - | 1336 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|        - | 1337 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|        - | 1338 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|        - | 1339 | `			 * parens, matching PHP's canonical text. */` |
|       80 | 1340 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|       59 | 1341 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|       59 | 1342 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      193 | 1343 | `			for( i = 0; i < nAtoms; i++ ){` |
|      139 | 1344 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|      101 | 1345 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|      101 | 1346 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       95 | 1347 | `					GenAppendClassAtom(pBlob, &aAtoms[i], 1);` |
|       50 | 1348 | `				}else{` |
|        9 | 1349 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - | 1350 | `				}` |
|      101 | 1351 | `				bFirstMember = 0;` |
|       53 | 1352 | `			}` |
|       59 | 1353 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|       59 | 1354 | `			bFirstGroup = 0;` |
|       32 | 1355 | `		}` |
|        - | 1356 | ``		/* `iterable` printed its Traversable half in place; its `array` half goes`` |
|        - | 1357 | ``		 * last, as php does (`(I1&I2)\|iterable` reads `(I1&I2)\|Traversable\|array`). */`` |
|      143 | 1358 | `		for( i = 0; i < nAtoms; i++ ){` |
|      103 | 1359 | `			if( GenAtomMaskKind(&aAtoms[i]) == GEN_ATOM_ITERABLE ){` |
|        3 | 1360 | `				SyBlobAppend(pBlob, "\|", 1);` |
|        3 | 1361 | `				SyBlobAppend(pBlob, "array", sizeof("array")-1);` |
|        3 | 1362 | `				break;` |
|        - | 1363 | `			}` |
|       53 | 1364 | `		}` |
|       47 | 1365 | `		if( bNullable ){` |
|        3 | 1366 | `			SyBlobAppend(pBlob, "\|", 1);` |
|        3 | 1367 | `			SyBlobAppend(pBlob, "null", 4);` |
|        1 | 1368 | `		}` |
|      182 | 1369 | `		return;` |
|        - | 1370 | `	}` |
|   409636 | 1371 | `	if( nNonNull == 1 && bNullable ){` |
|        - | 1372 | `		/* Shorthand: ?T */` |
|      275 | 1373 | `		for( i = 0; i < nAtoms; i++ ){` |
|      275 | 1374 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      275 | 1375 | `			SyBlobAppend(pBlob, "?", 1);` |
|      275 | 1376 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      109 | 1377 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       57 | 1378 | `			}else{` |
|      171 | 1379 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - | 1380 | `			}` |
|      275 | 1381 | `			return;` |
|      ! 0 | 1382 | `		}` |
|      ! 0 | 1383 | `	}` |
|        - | 1384 | `	{` |
|   409366 | 1385 | `		int bFirst = 1;` |
|        - | 1386 | `		/* 1) Classes in declaration order — minus the class-name atoms php counts` |
|        - | 1387 | ``		 * as built-in types (see GenAtomMaskKind); `iterable` leaves Traversable. */`` |
|   858594 | 1388 | `		for( i = 0; i < nAtoms; i++ ){` |
|        - | 1389 | `			int nKind;` |
|   449233 | 1390 | `			if( aAtoms[i].nType != SXU32_HIGH ) continue;` |
|    64601 | 1391 | `			nKind = GenAtomMaskKind(&aAtoms[i]);` |
|    64601 | 1392 | `			if( nKind == GEN_ATOM_CALLABLE \|\| nKind == GEN_ATOM_FALSE \|\| nKind == GEN_ATOM_TRUE ){` |
|    39952 | 1393 | `				continue;` |
|        - | 1394 | `			}` |
|    24654 | 1395 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    24654 | 1396 | `			GenAppendClassAtom(pBlob, &aAtoms[i], nNonNull > 1);` |
|    24654 | 1397 | `			bFirst = 0;` |
|    12313 | 1398 | `		}` |
|        - | 1399 | `		/* 2) Built-ins in php's canonical order. A slot is filled either by a` |
|        - | 1400 | `		 * plain atom of that MEMOBJ_* type or by a class-name atom of the matching` |
|        - | 1401 | ``		 * kind; the `array` slot also takes `iterable`'s second half. */`` |
|        - | 1402 | `		{` |
|        - | 1403 | `			static const struct {` |
|        - | 1404 | `				sxu32 nType;        /* plain atom type, 0 when kind-only */` |
|        - | 1405 | `				int nKind;          /* GEN_ATOM_* atom, GEN_ATOM_PLAIN when type-only */` |
|        - | 1406 | `				const char *zText;` |
|        - | 1407 | `				sxu32 nText;` |
|        - | 1408 | `			} aOrder[] = {` |
|        - | 1409 | `				{ 0,               GEN_ATOM_CALLABLE, "callable", sizeof("callable")-1 },` |
|        - | 1410 | `				{ MEMOBJ_OBJ,      GEN_ATOM_PLAIN,    "object",   sizeof("object")-1 },` |
|        - | 1411 | `				{ MEMOBJ_HASHMAP,  GEN_ATOM_ITERABLE, "array",    sizeof("array")-1 },` |
|        - | 1412 | `				{ MEMOBJ_STRING,   GEN_ATOM_PLAIN,    "string",   sizeof("string")-1 },` |
|        - | 1413 | `				{ MEMOBJ_INT,      GEN_ATOM_PLAIN,    "int",      sizeof("int")-1 },` |
|        - | 1414 | `				{ MEMOBJ_REAL,     GEN_ATOM_PLAIN,    "float",    sizeof("float")-1 },` |
|        - | 1415 | `				{ MEMOBJ_BOOL,     GEN_ATOM_PLAIN,    "bool",     sizeof("bool")-1 },` |
|        - | 1416 | `				{ 0,               GEN_ATOM_FALSE,    "false",    sizeof("false")-1 },` |
|        - | 1417 | `				{ 0,               GEN_ATOM_TRUE,     "true",     sizeof("true")-1 },` |
|        - | 1418 | ``				/* `void` and `never` are RETURN-only and may not share a type with`` |
|        - | 1419 | `				 * anything, so their place in the order is never observable -- what` |
|        - | 1420 | `				 * matters is that they have one. Missing here, the canonical text of` |
|        - | 1421 | ``				 * `: void` came out EMPTY, and every reader of it had to name the two`` |
|        - | 1422 | `				 * back from nReturnType (Reflection's getReturnType did, the generator` |
|        - | 1423 | `				 * fatal did, and the declaration renderer would have had to). */` |
|        - | 1424 | `				{ UTA_VOID_FLAG,   GEN_ATOM_PLAIN,    "void",     sizeof("void")-1 },` |
|        - | 1425 | `				{ UTA_NEVER_FLAG,  GEN_ATOM_PLAIN,    "never",    sizeof("never")-1 }` |
|        - | 1426 | `			};` |
|        - | 1427 | `			int k;` |
|  4912337 | 1428 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  8980074 | 1429 | `				for( i = 0; i < nAtoms; i++ ){` |
|  6482078 | 1430 | `					int bHit = (aOrder[k].nType != 0 && aAtoms[i].nType == aOrder[k].nType)` |
|  8390662 | 1431 | `						\|\| (aOrder[k].nKind != GEN_ATOM_PLAIN` |
|  3111166 | 1432 | `						    && GenAtomMaskKind(&aAtoms[i]) == aOrder[k].nKind` |
|   873467 | 1433 | `						    && (aOrder[k].nKind != GEN_ATOM_ITERABLE \|\| nNonNull > 1));` |
|  4901676 | 1434 | `					if( !bHit ) continue;` |
|   424578 | 1435 | `					if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   424578 | 1436 | `					SyBlobAppend(pBlob, aOrder[k].zText, aOrder[k].nText);` |
|   424578 | 1437 | `					bFirst = 0;` |
|   424578 | 1438 | `					break;` |
|      ! 0 | 1439 | `				}` |
|  2248174 | 1440 | `			}` |
|        - | 1441 | `		}` |
|        - | 1442 | `		/* 3) null suffix */` |
|   409366 | 1443 | `		if( bNullable ){` |
|       22 | 1444 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|       22 | 1445 | `			SyBlobAppend(pBlob, "null", 4);` |
|        9 | 1446 | `		}` |
|        - | 1447 | `	}` |
|   204540 | 1448 | `}` |
|        - | 1449 |  |
|        - | 1450 | `/*` |
|        - | 1451 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|        - | 1452 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|        - | 1453 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|        - | 1454 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|        - | 1455 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|        - | 1456 | ` * whether it was parenthesized.` |
|        - | 1457 | ` *` |
|        - | 1458 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|        - | 1459 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|        - | 1460 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|        - | 1461 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|        - | 1462 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|        - | 1463 | ` */` |
|   449612 | 1464 | `static sxi32 GenStateParsePart(` |
|        - | 1465 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|        - | 1466 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|        5 | 1467 | `{` |
|        - | 1468 | `	sxi32 rc;` |
|   449617 | 1469 | `	int nMembers = 0;` |
|   449617 | 1470 | `	int bParen = 0;` |
|   449617 | 1471 | `	*pnMembers = 0;` |
|   449617 | 1472 | `	*pbParen = 0;` |
|   449617 | 1473 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       18 | 1474 | `		bParen = 1;` |
|       18 | 1475 | `		pGen->pIn++; /* skip '(' */` |
|        7 | 1476 | `	}` |
|   224477 | 1477 | `	for(;;){` |
|   449663 | 1478 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|      ! 0 | 1479 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1480 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|      ! 0 | 1481 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1482 | `		}` |
|   449663 | 1483 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|   449663 | 1484 | `		if( rc != SXRET_OK ){` |
|       13 | 1485 | `			return rc;` |
|        - | 1486 | `		}` |
|   449653 | 1487 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|   449653 | 1488 | `		(*pnAtoms)++;` |
|   449653 | 1489 | `		nMembers++;` |
|        - | 1490 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|   449653 | 1491 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       91 | 1492 | `			SyToken *pNext = &pGen->pIn[1];` |
|       86 | 1493 | `			if( pNext < pGen->pEnd` |
|       91 | 1494 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       51 | 1495 | `				pGen->pIn++; /* skip '&' */` |
|       51 | 1496 | `				continue;` |
|        - | 1497 | `			}` |
|       20 | 1498 | `		}` |
|   449607 | 1499 | `		break;` |
|      ! 0 | 1500 | `	}` |
|   449607 | 1501 | `	if( bParen ){` |
|       18 | 1502 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 1503 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1504 | `				"Malformed DNF type: expecting ')'");` |
|      ! 0 | 1505 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1506 | `		}` |
|       18 | 1507 | `		pGen->pIn++; /* skip ')' */` |
|       18 | 1508 | `		if( nMembers < 2 ){` |
|      ! 0 | 1509 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1510 | `				"Parenthesized type must be an intersection of at least two types");` |
|      ! 0 | 1511 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1512 | `		}` |
|        7 | 1513 | `	}` |
|   449607 | 1514 | `	*pnMembers = nMembers;` |
|   449607 | 1515 | `	*pbParen = bParen;` |
|   449607 | 1516 | `	return SXRET_OK;` |
|   224482 | 1517 | `}` |
|        - | 1518 |  |
|        - | 1519 | `/*` |
|        - | 1520 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|        - | 1521 | ` *` |
|        - | 1522 | ` * Outputs:` |
|        - | 1523 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|        - | 1524 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|        - | 1525 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|        - | 1526 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|        - | 1527 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|        - | 1528 | ` *     already be initialized by the caller (allocator set, etc).` |
|        - | 1529 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|        - | 1530 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|        - | 1531 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|        - | 1532 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|        - | 1533 | ` *` |
|        - | 1534 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|        - | 1535 | ` * SXERR_ABORT on fatal compile errors.` |
|        - | 1536 | ` */` |
|   409707 | 1537 | `PH7_PRIVATE sxi32 GenStateParseUnionTypeDecl(` |
|        - | 1538 | `	ph7_gen_state *pGen,` |
|        - | 1539 | `	sxu32 *pnType,` |
|        - | 1540 | `	SyString *pClass,` |
|        - | 1541 | `	SySet *pAlts,` |
|        - | 1542 | `	sxi32 *piTypeFlags,` |
|        - | 1543 | `	SyString *pTypeText,` |
|        - | 1544 | `	int iNullableFlag,` |
|        - | 1545 | `	int iUnionFlag,` |
|        - | 1546 | `	int bAllowVoid,` |
|        - | 1547 | ``	int bParamCtx,   /* this type is a PARAMETER's: `static` is not one in php's grammar */`` |
|        - | 1548 | `	sxu32 nLine` |
|        5 | 1549 | `){` |
|        - | 1550 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   409712 | 1551 | `	int nAtoms = 0;` |
|   409712 | 1552 | `	int bShortNullable = 0;` |
|   409712 | 1553 | `	int bExplicitNull = 0;` |
|        - | 1554 | `	sxi32 rc;` |
|   409712 | 1555 | `	*pnType = 0;` |
|   409712 | 1556 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   409712 | 1557 | `	*piTypeFlags = 0;` |
|   409712 | 1558 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|        - | 1559 |  |
|   409712 | 1560 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 1561 | `		return SXRET_OK;` |
|        - | 1562 | `	}` |
|        - | 1563 | ``	/* Optional `?` shorthand prefix */`` |
|   409707 | 1564 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      263 | 1565 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      263 | 1566 | `		bShortNullable = 1;` |
|      263 | 1567 | `		pGen->pIn++;` |
|      263 | 1568 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 1569 | `			return SXERR_SYNTAX;` |
|        - | 1570 | `		}` |
|      129 | 1571 | `	}` |
|        - | 1572 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|        - | 1573 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|        - | 1574 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|        - | 1575 | `	{` |
|        - | 1576 | `		int nMembers, bParen;` |
|   409712 | 1577 | `		sxu32 iGroup = 0;` |
|   409712 | 1578 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|   409712 | 1579 | `		if( rc != SXRET_OK ){` |
|       14 | 1580 | `			return rc;` |
|        - | 1581 | `		}` |
|        - | 1582 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|        - | 1583 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|        - | 1584 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|        - | 1585 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|        - | 1586 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|   674698 | 1587 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   469608 | 1588 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|    39912 | 1589 | `			if( bShortNullable ){` |
|        - | 1590 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|        - | 1591 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|        - | 1592 | `				 * already reported" so callers skip their own error emission. */` |
|        3 | 1593 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - | 1594 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|        3 | 1595 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|        - | 1596 | `			}` |
|    39910 | 1597 | `			if( nMembers >= 2 && !bParen ){` |
|      ! 0 | 1598 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|        - | 1599 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 | 1600 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1601 | `			}` |
|    39910 | 1602 | ``			pGen->pIn++; /* skip `\|` */`` |
|    39910 | 1603 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|    39910 | 1604 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1605 | `				return rc;` |
|        - | 1606 | `			}` |
|        5 | 1607 | `		}` |
|   409700 | 1608 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|      ! 0 | 1609 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1610 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 | 1611 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1612 | `		}` |
|        - | 1613 | `	}` |
|        - | 1614 | `	/* Validation pass.` |
|        - | 1615 | `	 *` |
|        - | 1616 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|        - | 1617 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|        - | 1618 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|        - | 1619 | `	 */` |
|        - | 1620 | `	{` |
|        - | 1621 | `		int i, j;` |
|   409700 | 1622 | `		int bHasNonNull = 0;` |
|   409700 | 1623 | `		int bAnyIntersection = 0;` |
|        - | 1624 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|        - | 1625 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|        - | 1626 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
| 13519940 | 1627 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   859346 | 1628 | `		for( i = 0; i < nAtoms; i++ ){` |
|   449651 | 1629 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|   224499 | 1630 | `		}` |
|   859242 | 1631 | `		for( i = 0; i < nAtoms; i++ ){` |
|   449593 | 1632 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|   224447 | 1633 | `		}` |
|        - | 1634 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|        - | 1635 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|   409700 | 1636 | `		if( bShortNullable && bAnyIntersection ){` |
|      ! 0 | 1637 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1638 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|      ! 0 | 1639 | `			return SXERR_SYNTAX;` |
|        - | 1640 | `		}` |
|        - | 1641 | `		/*` |
|        - | 1642 | `		 * php's declaration-time screen for the three SCOPE keywords, which runs` |
|        - | 1643 | `		 * BEFORE the intersection-member kinds below (that is php's order: a` |
|        - | 1644 | ``		 * `parent&Countable` in a base-less class is refused for the parent, not`` |
|        - | 1645 | `		 * for the intersection).` |
|        - | 1646 | `		 *` |
|        - | 1647 | `		 * Each keyword names something relative to WHERE the declaration is` |
|        - | 1648 | `		 * written, and one written where that thing does not exist is refused` |
|        - | 1649 | `		 * before the program runs. A CLOSURE is exempt — its scope is decided when` |
|        - | 1650 | `		 * it is bound — and so is a TRAIT, which defers the question to whatever` |
|        - | 1651 | ``		 * composes it. An interface has no `parent` however many it extends.`` |
|        - | 1652 | `		 *` |
|        - | 1653 | ``		 * `static` is not a PARAMETER type in php's grammar at all: the modifier`` |
|        - | 1654 | `` 		 * run ahead of this parser catches the bare and `&` spellings, and `?static` `` |
|        - | 1655 | `		 * reaches here, where php answers with the parse error its grammar produces.` |
|        - | 1656 | `		 */` |
|   859340 | 1657 | `		for( i = 0; i < nAtoms; i++ ){` |
|        - | 1658 | `			const SyString *pKw;` |
|        - | 1659 | `			ph7_class *pScope;` |
|        - | 1660 | `			const char *zKw;` |
|   449651 | 1661 | `			if( aAtoms[i].nType != SXU32_HIGH ){` |
|   384851 | 1662 | `				continue;` |
|        - | 1663 | `			}` |
|    64805 | 1664 | `			pKw = &aAtoms[i].sClass;` |
|   129491 | 1665 | `			zKw = (pKw->nByte == 4 && SyStrnicmp(pKw->zString,"self",4) == 0)   ? "self"   :` |
|    96963 | 1666 | `			      (pKw->nByte == 6 && SyStrnicmp(pKw->zString,"parent",6) == 0) ? "parent" :` |
|    64646 | 1667 | `			      (pKw->nByte == 6 && SyStrnicmp(pKw->zString,"static",6) == 0) ? "static" : 0;` |
|    64805 | 1668 | `			if( zKw == 0 ){` |
|    64579 | 1669 | `				continue;` |
|        - | 1670 | `			}` |
|      231 | 1671 | `			if( bParamCtx && zKw[0] == 's' && zKw[1] == 't' ){` |
|        - | 1672 | ``				/* GRAMMAR, not scope: `static` is no parameter type at all, so a`` |
|        - | 1673 | `				 * CLOSURE is not exempt from this one. The modifier run ahead of this` |
|        - | 1674 | ``				 * parser already catches the bare and `&` spellings; `?static` and`` |
|        - | 1675 | ``				 * `int\|static` reach here. */`` |
|        3 | 1676 | `				PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|        - | 1677 | `					"syntax error, unexpected token \"static\"");` |
|        3 | 1678 | `				return SXERR_SYNTAX;` |
|        - | 1679 | `			}` |
|      229 | 1680 | `			if( pGen->iSigScope == PH7_SIGSCOPE_CLOSURE ){` |
|       17 | 1681 | `				continue;` |
|        - | 1682 | `			}` |
|      215 | 1683 | `			pScope = ( pGen->iSigScope == PH7_SIGSCOPE_FUNC ) ? 0 : pGen->pCurClass;` |
|      215 | 1684 | `			if( pScope == 0 ){` |
|        4 | 1685 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        1 | 1686 | `					"Cannot use \"%s\" when no class scope is active", zKw);` |
|        3 | 1687 | `				return SXERR_SYNTAX;` |
|        - | 1688 | `			}` |
|      208 | 1689 | `			if( zKw[0] == 'p' && pGen->pCurBase == 0` |
|       29 | 1690 | `			 && (pScope->iFlags & (PH7_CLASS_TRAIT\|PH7_CLASS_LINT_UNBOUND)) == 0 ){` |
|        3 | 1691 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1692 | `					"Cannot use \"parent\" when current class scope has no parent");` |
|        3 | 1693 | `				return SXERR_SYNTAX;` |
|        - | 1694 | `			}` |
|      108 | 1695 | `		}` |
|   859314 | 1696 | `		for( i = 0; i < nAtoms; i++ ){` |
|        - | 1697 | `			/* Intersection members must be class/interface types (PHP rejects` |
|        - | 1698 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|        - | 1699 | ``			 * `true`/`false` in an intersection). */`` |
|   449641 | 1700 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       93 | 1701 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|       93 | 1702 | `				int bLowerKw = 0;` |
|       93 | 1703 | `				if( bClassLike ){` |
|       91 | 1704 | `					SyString *pC = &aAtoms[i].sClass;` |
|       86 | 1705 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|       86 | 1706 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|       86 | 1707 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|       91 | 1708 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|      ! 0 | 1709 | `						bClassLike = 0;` |
|      ! 0 | 1710 | `					}` |
|        - | 1711 | `					/* A SCOPE keyword is class-like only where php can substitute a` |
|        - | 1712 | ``					 * class NAME for it while the body compiles: `static` never (the`` |
|        - | 1713 | `` 					 * called class is not known until the call), and `self`/`parent` `` |
|        - | 1714 | `					 * not in a closure, a trait or an ANONYMOUS class, none of which` |
|        - | 1715 | `					 * has a name to put there. In a named class, interface or enum` |
|        - | 1716 | `					 * php resolves them and the intersection stands. */` |
|       91 | 1717 | `					else if( pC->nByte == 6 && SyMemcmpNoCase(pC->zString,"static",6) == 0 ){` |
|      ! 0 | 1718 | `						bClassLike = 0;` |
|      ! 0 | 1719 | `						bLowerKw = 1; /* php prints the keyword, not what was written */` |
|       86 | 1720 | `					}else if( (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"self",4) == 0)` |
|       89 | 1721 | `					       \|\| (pC->nByte == 6 && SyMemcmpNoCase(pC->zString,"parent",6) == 0) ){` |
|        8 | 1722 | `						ph7_class *pScope = ( pGen->iSigScope == PH7_SIGSCOPE_FUNC )` |
|        4 | 1723 | `							? 0 : pGen->pCurClass;` |
|        4 | 1724 | `						if( pGen->iSigScope == PH7_SIGSCOPE_CLOSURE \|\| pScope == 0` |
|        6 | 1725 | `						 \|\| (pScope->iFlags & (PH7_CLASS_TRAIT\|PH7_CLASS_ANON)) != 0 ){` |
|        3 | 1726 | `							bClassLike = 0;` |
|        1 | 1727 | `						}` |
|        2 | 1728 | `					}` |
|       43 | 1729 | `				}` |
|       93 | 1730 | `				if( !bClassLike ){` |
|        - | 1731 | `					const char *zName; sxu32 nName;` |
|        6 | 1732 | `					if( bLowerKw ){` |
|      ! 0 | 1733 | `						zName = "static"; nName = sizeof("static")-1;` |
|        6 | 1734 | `					}else if( aAtoms[i].nType == SXU32_HIGH ){` |
|        3 | 1735 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|        2 | 1736 | `					}else{` |
|        3 | 1737 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|        - | 1738 | `					}` |
|        8 | 1739 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1740 | `						"Type %.*s cannot be part of an intersection type",` |
|        2 | 1741 | `						(int)nName, zName);` |
|        6 | 1742 | `					return SXERR_SYNTAX;` |
|        - | 1743 | `				}` |
|       42 | 1744 | `			}` |
|   449637 | 1745 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     8573 | 1746 | `				if( nAtoms > 1 ){` |
|        3 | 1747 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1748 | `						"Void can only be used as a standalone type");` |
|        3 | 1749 | `					return SXERR_SYNTAX;` |
|        - | 1750 | `				}` |
|     8571 | 1751 | `				if( !bAllowVoid ){` |
|        - | 1752 | `					/* php names the position when it is a parameter's type. */` |
|        4 | 1753 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        1 | 1754 | `						bParamCtx ? "void cannot be used as a parameter type"` |
|        - | 1755 | `						          : "void cannot be used here");` |
|        3 | 1756 | `					return SXERR_SYNTAX;` |
|        - | 1757 | `				}` |
|     8569 | 1758 | `				if( bShortNullable ){` |
|      ! 0 | 1759 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1760 | `						"Void type cannot be nullable");` |
|      ! 0 | 1761 | `					return SXERR_SYNTAX;` |
|        - | 1762 | `				}` |
|     4269 | 1763 | `			}` |
|   449633 | 1764 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|        - | 1765 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|        - | 1766 | `				 * type (never = the function does not return). Mirrors the void` |
|        - | 1767 | `				 * validation above; accepted here and enforced at compile time` |
|        - | 1768 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|       39 | 1769 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|        - | 1770 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|        - | 1771 | `					 * same as any other non-standalone use. */` |
|        6 | 1772 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1773 | `						"never can only be used as a standalone type");` |
|        6 | 1774 | `					return SXERR_SYNTAX;` |
|        - | 1775 | `				}` |
|       34 | 1776 | `				if( !bAllowVoid ){` |
|        - | 1777 | `					/* Return-only: params call with bAllowVoid=0. */` |
|        3 | 1778 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1779 | `						"never cannot be used as a parameter type");` |
|        3 | 1780 | `					return SXERR_SYNTAX;` |
|        - | 1781 | `				}` |
|       14 | 1782 | `			}` |
|   449627 | 1783 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|       41 | 1784 | `				bExplicitNull = 1;` |
|       23 | 1785 | `			}else{` |
|   449591 | 1786 | `				bHasNonNull = 1;` |
|        - | 1787 | `			}` |
|        - | 1788 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|        - | 1789 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|        - | 1790 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|        - | 1791 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|        - | 1792 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|   489636 | 1793 | `			for( j = 0; j < i; j++ ){` |
|    40016 | 1794 | `				int bDup = 0;` |
|    40016 | 1795 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|    80004 | 1796 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|    40011 | 1797 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|    40016 | 1798 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|    39988 | 1799 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      105 | 1800 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       98 | 1801 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|       77 | 1802 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       23 | 1803 | `								aAtoms[j].sClass.zString,` |
|       46 | 1804 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|      ! 0 | 1805 | `							bDup = 1;` |
|      ! 0 | 1806 | `						}` |
|       54 | 1807 | `					}else{` |
|        3 | 1808 | `						bDup = 1;` |
|        - | 1809 | `					}` |
|       50 | 1810 | `				}` |
|    39988 | 1811 | `				if( bDup ){` |
|        - | 1812 | `					const char *zName;` |
|        - | 1813 | `					sxu32 nName;` |
|        3 | 1814 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 | 1815 | `						zName = aAtoms[i].sClass.zString;` |
|      ! 0 | 1816 | `						nName = aAtoms[i].sClass.nByte;` |
|      ! 0 | 1817 | `					}else{` |
|        3 | 1818 | `						zName = aAtoms[i].zCanon;` |
|        3 | 1819 | `						nName = aAtoms[i].nCanon;` |
|        - | 1820 | `					}` |
|        4 | 1821 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        1 | 1822 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|        3 | 1823 | `					return SXERR_SYNTAX;` |
|        - | 1824 | `				}` |
|    19968 | 1825 | `			}` |
|   224486 | 1826 | `		}` |
|   409678 | 1827 | `		if( !bHasNonNull && bExplicitNull ){` |
|        7 | 1828 | `			if( bShortNullable ){` |
|        - | 1829 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|      ! 0 | 1830 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1831 | `					"Null can not be used as a standalone type");` |
|      ! 0 | 1832 | `				return SXERR_SYNTAX;` |
|        - | 1833 | `			}` |
|        - | 1834 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|        - | 1835 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|        - | 1836 | `			 * path below leaves *pnType untouched when there is no non-null` |
|        - | 1837 | `			 * atom, so set it here. */` |
|        7 | 1838 | `			*pnType = MEMOBJ_NULL;` |
|        3 | 1839 | `		}` |
|        - | 1840 | `	}` |
|        - | 1841 | `	/* Compute nullability flag */` |
|   409678 | 1842 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      295 | 1843 | `		*piTypeFlags \|= iNullableFlag;` |
|      145 | 1844 | `	}` |
|        - | 1845 | `	/* Build canonical type text */` |
|   409678 | 1846 | `	if( pTypeText ){` |
|        - | 1847 | `		SyBlob sBlob;` |
|   409678 | 1848 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|   614086 | 1849 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   204535 | 1850 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   409678 | 1851 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|   614213 | 1852 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   409673 | 1853 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   409678 | 1854 | `			if( zDup ){` |
|   409678 | 1855 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   204535 | 1856 | `			}` |
|   204535 | 1857 | `		}` |
|   409678 | 1858 | `		SyBlobRelease(&sBlob);` |
|   204535 | 1859 | `	}` |
|        - | 1860 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|        - | 1861 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|        - | 1862 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|        - | 1863 | `	{` |
|   409678 | 1864 | `		int nNonNull = 0;` |
|   409678 | 1865 | `		int iNonNullIdx = -1;` |
|        - | 1866 | `		int i;` |
|   859290 | 1867 | `		for( i = 0; i < nAtoms; i++ ){` |
|   449617 | 1868 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   449581 | 1869 | `				nNonNull++;` |
|   449581 | 1870 | `				iNonNullIdx = i;` |
|   224459 | 1871 | `			}` |
|   224482 | 1872 | `		}` |
|   409678 | 1873 | `		if( nNonNull <= 1 ){` |
|        - | 1874 | `			/* Fast path: store as single type. */` |
|   369809 | 1875 | `			if( iNonNullIdx >= 0 ){` |
|   369803 | 1876 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   369803 | 1877 | `				if( pA->nType == SXU32_HIGH ){` |
|    37353 | 1878 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    12437 | 1879 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    24916 | 1880 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    24916 | 1881 | `					*pnType = SXU32_HIGH;` |
|    24916 | 1882 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   357329 | 1883 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     8569 | 1884 | `					*pnType = MEMOBJ_VOID;` |
|   340597 | 1885 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|       31 | 1886 | `					*pnType = MEMOBJ_NEVER;` |
|       17 | 1887 | `				}else{` |
|   336300 | 1888 | `					*pnType = pA->nType;` |
|        - | 1889 | `				}` |
|   184625 | 1890 | `			}` |
|   184633 | 1891 | `		}else{` |
|        - | 1892 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|    39874 | 1893 | `			*piTypeFlags \|= iUnionFlag;` |
|   119666 | 1894 | `			for( i = 0; i < nAtoms; i++ ){` |
|        - | 1895 | `				ph7_type_alt sAlt;` |
|    79797 | 1896 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|    79783 | 1897 | `				SyZero(&sAlt, sizeof(sAlt));` |
|    79783 | 1898 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|    79783 | 1899 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|    59796 | 1900 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    19912 | 1901 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    39884 | 1902 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    39884 | 1903 | `					sAlt.nType = SXU32_HIGH;` |
|    39884 | 1904 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|    19917 | 1905 | `				}else{` |
|    39904 | 1906 | `					sAlt.nType = aAtoms[i].nType;` |
|    39904 | 1907 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|        - | 1908 | `				}` |
|    79783 | 1909 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|    39839 | 1910 | `			}` |
|        - | 1911 | `		}` |
|        - | 1912 | `	}` |
|   409678 | 1913 | `	return SXRET_OK;` |
|   204557 | 1914 | `}` |
|        - | 1915 |  |
|        - | 1916 | `/*` |
|        - | 1917 | `` * php 8.5's `#[\NoDiscard]`, decided where the declaration is WRITTEN.`` |
|        - | 1918 | ` *` |
|        - | 1919 | ` * The attribute says a caller must do something with the answer, so php refuses` |
|        - | 1920 | `` * it on a declaration that HAS no answer -- a `void` or `never` return type --`` |
|        - | 1921 | ` * and on a constructor, which is called for its object rather than its return.` |
|        - | 1922 | ` * The nouns are php's: a "function" everywhere but a class body, where the same` |
|        - | 1923 | ` * sentence says "method". Run once the return type is parsed, since that is` |
|        - | 1924 | ` * what it judges; a declaration that already failed says nothing more.` |
|        - | 1925 | ` */` |
|   205384 | 1926 | `PH7_PRIVATE sxi32 GenStateApplyNoDiscard(ph7_gen_state *pGen,ph7_vm_func *pFunc,` |
|        - | 1927 | `	ph7_class *pClass,int bCtor)` |
|        5 | 1928 | `{` |
|   205389 | 1929 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(&pFunc->aAttrs);` |
|   205389 | 1930 | `	const char *zKind = pClass ? "method" : "function";` |
|        - | 1931 | `	sxu32 n;` |
|   205599 | 1932 | `	for( n = 0 ; n < SySetUsed(&pFunc->aAttrs) ; ++n ){` |
|      386 | 1933 | `		if( SyStringLength(&aAttr[n].sName) != sizeof("NoDiscard")-1` |
|      287 | 1934 | `		 \|\| SyStrnicmp(SyStringData(&aAttr[n].sName),"NoDiscard",sizeof("NoDiscard")-1) != 0 ){` |
|      215 | 1935 | `			continue;` |
|        - | 1936 | `		}` |
|      177 | 1937 | `		if( bCtor ){` |
|        4 | 1938 | `			return PH7_GenCompileError(&(*pGen),E_ERROR,pFunc->nLine,` |
|        1 | 1939 | `				"Method %z::%z cannot be #[\\NoDiscard]",&pClass->sDisp,&pFunc->sName);` |
|        - | 1940 | `		}` |
|      175 | 1941 | `		if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|        7 | 1942 | `			return PH7_GenCompileError(&(*pGen),E_ERROR,pFunc->nLine,` |
|        - | 1943 | `				"A void %s does not return a value, but #[\\NoDiscard] requires a return value",` |
|        2 | 1944 | `				zKind);` |
|        - | 1945 | `		}` |
|      171 | 1946 | `		if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|        7 | 1947 | `			return PH7_GenCompileError(&(*pGen),E_ERROR,pFunc->nLine,` |
|        - | 1948 | `				"A never returning %s does not return a value, but #[\\NoDiscard] requires a return value",` |
|        2 | 1949 | `				zKind);` |
|        - | 1950 | `		}` |
|      167 | 1951 | `		pFunc->iFlags \|= VM_FUNC_NODISCARD;` |
|      167 | 1952 | `		return SXRET_OK;` |
|      ! 0 | 1953 | `	}` |
|   205213 | 1954 | `	return SXRET_OK;` |
|   102435 | 1955 | `}` |
|        - | 1956 | `/*` |
|        - | 1957 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|        - | 1958 | `` * pGen->pIn should point to the token after `)`.`` |
|        - | 1959 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|        - | 1960 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|        - | 1961 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|        - | 1962 | `` *          and union types `: T\|U`.`` |
|        - | 1963 | ` */` |
|   198801 | 1964 | `PH7_PRIVATE sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|        5 | 1965 | `{` |
|   198806 | 1966 | `	sxi32 iFlags = 0;` |
|        - | 1967 | `	sxi32 rc;` |
|        - | 1968 | `	sxu32 nLine;` |
|   198806 | 1969 | `	pFunc->nReturnType = 0;` |
|   198806 | 1970 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|   198806 | 1971 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|        - | 1972 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|        - | 1973 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|        - | 1974 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|        - | 1975 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|        - | 1976 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|   198806 | 1977 | `	SySetReset(&pFunc->aReturnUnion);` |
|   198806 | 1978 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|   198806 | 1979 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|    37867 | 1980 | `		return SXRET_OK;` |
|        - | 1981 | `	}` |
|   160944 | 1982 | `	pGen->pIn++; /* Skip ':' */` |
|   160944 | 1983 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 1984 | `		return SXRET_OK;` |
|        - | 1985 | `	}` |
|   160944 | 1986 | `	nLine = pGen->pIn->nLine;` |
|   160944 | 1987 | `	rc = GenStateParseUnionTypeDecl(` |
|    80350 | 1988 | `		pGen,` |
|    80350 | 1989 | `		&pFunc->nReturnType,` |
|    80350 | 1990 | `		&pFunc->sReturnClass,` |
|    80350 | 1991 | `		&pFunc->aReturnUnion,` |
|        - | 1992 | `		&iFlags,` |
|    80350 | 1993 | `		&pFunc->sReturnTypeName,` |
|        - | 1994 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|        - | 1995 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|        - | 1996 | `		/* iUnionFlag */ 0,` |
|        - | 1997 | `		/* bAllowVoid */ 1, /* bParamCtx */ 0,` |
|    80350 | 1998 | `		nLine);` |
|   160944 | 1999 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2000 | `		return SXERR_ABORT;` |
|        - | 2001 | `	}` |
|   160944 | 2002 | `	if( rc == SXERR_CORRUPT ){` |
|        - | 2003 | `		/* Error already reported */` |
|      ! 0 | 2004 | `		return SXERR_SYNTAX;` |
|        - | 2005 | `	}` |
|   160944 | 2006 | `	if( rc == SXERR_SYNTAX ){` |
|       17 | 2007 | `		if( pGen->pIn < pGen->pEnd ){` |
|       24 | 2008 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - | 2009 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       14 | 2010 | `				&pGen->pIn->sData);` |
|       10 | 2011 | `		}else{` |
|      ! 0 | 2012 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|        - | 2013 | `				"syntax error, unexpected end of file in return type declaration");` |
|        - | 2014 | `		}` |
|       17 | 2015 | `		return SXERR_SYNTAX;` |
|        - | 2016 | `	}` |
|   160930 | 2017 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|   160930 | 2018 | `	return SXRET_OK;` |
|    99160 | 2019 | `}` |
|        - | 2020 |  |
|   185279 | 2021 | `PH7_PRIVATE sxi32 GenStateCompileFunc(` |
|        - | 2022 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 2023 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|        - | 2024 | `	sxi32 iFlags,        /* Control flags */` |
|        - | 2025 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|        - | 2026 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|        - | 2027 | `	)` |
|        5 | 2028 | `{` |
|        - | 2029 | `	ph7_vm_func *pFunc;` |
|        - | 2030 | `	SyToken *pEnd;` |
|        - | 2031 | `	sxu32 nLine;` |
|        - | 2032 | `	char *zName;` |
|        - | 2033 | `	sxi32 rc;` |
|        - | 2034 | `	/* Extract line number */` |
|   185284 | 2035 | `	nLine = pGen->pIn->nLine;` |
|        - | 2036 | `	/* Jump the left parenthesis '(' */` |
|   185284 | 2037 | `	pGen->pIn++;` |
|        - | 2038 | `	/* Delimit the function signature */` |
|   185284 | 2039 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   185284 | 2040 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 2041 | `		/* Syntax error */` |
|       11 | 2042 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable");` |
|        4 | 2043 | `		(void)pName;` |
|       11 | 2044 | `		if( rc == SXERR_ABORT ){` |
|        - | 2045 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 2046 | `			return SXERR_ABORT;` |
|        - | 2047 | `		}` |
|       11 | 2048 | `		pGen->pIn = pGen->pEnd;` |
|       11 | 2049 | `		return SXRET_OK;` |
|        - | 2050 | `	}` |
|        - | 2051 | `	/* Create the function state */` |
|   185276 | 2052 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   185276 | 2053 | `	if( pFunc == 0 ){` |
|      ! 0 | 2054 | `		goto OutOfMem;` |
|        - | 2055 | `	}` |
|        - | 2056 | `	/* Build the function name, prepending namespace if active.` |
|        - | 2057 | `	 * A NAMED function (never a closure) also answers to php's import rules: its` |
|        - | 2058 | ``	 * short name must not already be a local `use function` import, and the name it`` |
|        - | 2059 | ``	 * takes is remembered so a later `use function` in this unit sees it. */`` |
|   185320 | 2060 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|        - | 2061 | `		SyBlob sFQN;` |
|        - | 2062 | `		sxu32 nLen;` |
|       93 | 2063 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       93 | 2064 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       93 | 2065 | `		SyBlobAppend(&sFQN,"\\",1);` |
|       93 | 2066 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|       93 | 2067 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|       93 | 2068 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|       93 | 2069 | `		SyBlobRelease(&sFQN);` |
|       93 | 2070 | `		if( zName == 0 ){` |
|      ! 0 | 2071 | `			goto OutOfMem;` |
|        - | 2072 | `		}` |
|       93 | 2073 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       49 | 2074 | `	}else{` |
|   185188 | 2075 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   185188 | 2076 | `		if( zName == 0 ){` |
|      ! 0 | 2077 | `			goto OutOfMem;` |
|        - | 2078 | `		}` |
|   185188 | 2079 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|        - | 2080 | `	}` |
|   185276 | 2081 | `	if( !bHandleClosure ){` |
|   178677 | 2082 | `		if( GenStateGuardImportRedeclare(pGen,1,pName,&pFunc->sName,nLine) == SXERR_ABORT ){` |
|      ! 0 | 2083 | `			return SXERR_ABORT;` |
|        - | 2084 | `		}` |
|   178677 | 2085 | `		GenStateRecordDeclaredName(pGen,1,&pFunc->sName);` |
|    89209 | 2086 | `	}` |
|        - | 2087 | ``	/* Take php's `{closure:SCOPE:LINE}` name the caller built for this closure. It has to`` |
|        - | 2088 | `	 * land here, ahead of the body, because a __FUNCTION__ inside the body reads it at` |
|        - | 2089 | `	 * compile time — and it must be cleared, since a NESTED declaration reaches this same` |
|        - | 2090 | `	 * point and would otherwise inherit its parent's. */` |
|   185276 | 2091 | `	if( SyStringLength(&pGen->sPendingClosureName) > 0 ){` |
|     6604 | 2092 | `		if( bHandleClosure ){` |
|     6604 | 2093 | `			pFunc->sClosureName = pGen->sPendingClosureName;` |
|     6604 | 2094 | `			pFunc->sClosureScope = pGen->sPendingClosureScope;` |
|     3283 | 2095 | `		}` |
|     6604 | 2096 | `		SyStringInitFromBuf(&pGen->sPendingClosureName,0,0);` |
|     3283 | 2097 | `	}` |
|        - | 2098 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|        - | 2099 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|   185276 | 2100 | `	pFunc->nLine = nLine;` |
|   185276 | 2101 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|   185276 | 2102 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 2103 | `		return SXERR_ABORT;` |
|        - | 2104 | `	}` |
|   185276 | 2105 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pFunc->aAttrs,2,2,0,0) == SXERR_ABORT ){` |
|      ! 0 | 2106 | `		return SXERR_ABORT;` |
|        - | 2107 | `	}` |
|        - | 2108 | `	/* Whose signature this is, for php's scope-keyword screen (see iSigScope): a` |
|        - | 2109 | `	 * closure's is EXEMPT, and a NAMED function has no class scope even when it is` |
|        - | 2110 | `	 * written inside a method body. */` |
|        - | 2111 | `	{` |
|   185276 | 2112 | `		int iSavedSig = pGen->iSigScope;` |
|   185276 | 2113 | `		pGen->iSigScope = bHandleClosure ? PH7_SIGSCOPE_CLOSURE : PH7_SIGSCOPE_FUNC;` |
|   185276 | 2114 | `		if( pGen->pIn < pEnd ){` |
|        - | 2115 | `			/* Collect function arguments */` |
|   170951 | 2116 | `			rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   170951 | 2117 | `			if( rc == SXERR_ABORT ){` |
|        - | 2118 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|        3 | 2119 | `				pGen->iSigScope = iSavedSig;` |
|        3 | 2120 | `				return SXERR_ABORT;` |
|        - | 2121 | `			}` |
|    85349 | 2122 | `		}` |
|        - | 2123 | `		/* Point past ')' and parse optional return type ': type' */` |
|   185274 | 2124 | `		pGen->pIn = &pEnd[1];` |
|        - | 2125 | `		{` |
|   185274 | 2126 | `			sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   185274 | 2127 | `			pGen->iSigScope = iSavedSig;` |
|   185274 | 2128 | `			if( rcRt == SXERR_ABORT ){` |
|      ! 0 | 2129 | `				return SXERR_ABORT;` |
|   185274 | 2130 | `			}else if( rcRt == SXERR_SYNTAX ){` |
|       15 | 2131 | `				return SXERR_SYNTAX;` |
|        - | 2132 | `			}` |
|        - | 2133 | `		}` |
|        - | 2134 | `	}` |
|        - | 2135 | `	/* php's #[\NoDiscard] declaration rules, which want the return type. A` |
|        - | 2136 | ``	 * closure's second (post-`use`) return-type parse re-runs below; the flag is`` |
|        - | 2137 | `	 * idempotent and the refusals are the same either way. */` |
|   185262 | 2138 | `	if( GenStateApplyNoDiscard(&(*pGen),pFunc,0,0) == SXERR_ABORT ){` |
|      ! 0 | 2139 | `		return SXERR_ABORT;` |
|        - | 2140 | `	}` |
|   185262 | 2141 | `	if( bHandleClosure ){` |
|        - | 2142 | `		ph7_vm_func_closure_env sEnv;` |
|     6604 | 2143 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     6599 | 2144 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     4285 | 2145 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|     1984 | 2146 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - | 2147 | `				/* Closure,record environment variable */` |
|     1984 | 2148 | `				pGen->pIn++;` |
|     1984 | 2149 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 2150 | `					rc = PH7_GenCompileError(pGen,E_PARSE,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|      ! 0 | 2151 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2152 | `						return SXERR_ABORT;` |
|        - | 2153 | `					}` |
|      ! 0 | 2154 | `				}` |
|     1984 | 2155 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|        - | 2156 | `				/* Compile until we hit the first closing parenthesis */` |
|     4225 | 2157 | `				while( pGen->pIn < pGen->pEnd ){` |
|     4225 | 2158 | `					int iFlagsLocal = 0;` |
|     4225 | 2159 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|     1982 | 2160 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|     1982 | 2161 | `						break;` |
|        - | 2162 | `					}` |
|     2248 | 2163 | `					nLineLocal = pGen->pIn->nLine;` |
|     2248 | 2164 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - | 2165 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|        - | 2166 | `						 * to the variable's memory slot instead of copying its value. */` |
|      515 | 2167 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|      515 | 2168 | `						pGen->pIn++;` |
|      251 | 2169 | `					}` |
|     2243 | 2170 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|     2248 | 2171 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2172 | `							rc = PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|        - | 2173 | `								"Closure: Unexpected token. Expecting a variable name");` |
|      ! 0 | 2174 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 2175 | `								return SXERR_ABORT;` |
|        - | 2176 | `							}` |
|        - | 2177 | `							/* Find the closing parenthesis */` |
|      ! 0 | 2178 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 2179 | `								pGen->pIn++;` |
|      ! 0 | 2180 | `							}` |
|      ! 0 | 2181 | `							if(pGen->pIn < pGen->pEnd){` |
|      ! 0 | 2182 | `								pGen->pIn++;` |
|      ! 0 | 2183 | `							}` |
|      ! 0 | 2184 | `							break;` |
|        - | 2185 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|      ! 0 | 2186 | `					}else{` |
|        - | 2187 | `						SyString *pNameLocal;` |
|        - | 2188 | `						char *zDup;` |
|        - | 2189 | `						/* Duplicate variable name */` |
|     2248 | 2190 | `						pNameLocal = &pGen->pIn[1].sData;` |
|     2248 | 2191 | `						if( PH7_VmIsAutoGlobal(pNameLocal->zString,pNameLocal->nByte) ){` |
|        - | 2192 | `							/* php's compile fatal. It is a real protection, not a` |
|        - | 2193 | `							 * style rule: the import resolves through hSuper, so` |
|        - | 2194 | `							 * installing the captured value would overwrite the` |
|        - | 2195 | ``							 * superglobal's own slot — `use ($GLOBALS)` replaced the`` |
|        - | 2196 | `							 * live symbol-table view with a snapshot and every later` |
|        - | 2197 | `							 * global went missing program-wide. */` |
|        3 | 2198 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - | 2199 | `								"Cannot use auto-global as lexical variable");` |
|        3 | 2200 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 2201 | `								return SXERR_ABORT;` |
|        - | 2202 | `							}` |
|        3 | 2203 | `							return SXERR_SYNTAX;` |
|        - | 2204 | `						}` |
|     2246 | 2205 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|     2246 | 2206 | `						if( zDup ){` |
|        - | 2207 | `							/* Zero the structure */` |
|     2246 | 2208 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     2246 | 2209 | `							sEnv.iFlags = iFlagsLocal;` |
|     2246 | 2210 | `							sEnv.nLine = nLineLocal; /* the capture's own source line (php warns here) */` |
|     2246 | 2211 | `							sEnv.nIdx = SXU32_HIGH;` |
|     2246 | 2212 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     2246 | 2213 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|     2380 | 2214 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|      267 | 2215 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|      ! 0 | 2216 | `									got_this = 1;` |
|      ! 0 | 2217 | `							}` |
|        - | 2218 | `							/* Save imported variable */` |
|     2246 | 2219 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|     1114 | 2220 | `						}else{` |
|      ! 0 | 2221 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2222 | `							 return SXERR_ABORT;` |
|        - | 2223 | `						}` |
|        - | 2224 | `					}` |
|     2246 | 2225 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|     2512 | 2226 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - | 2227 | `						/* Ignore trailing commas */` |
|      271 | 2228 | `						pGen->pIn++;` |
|        5 | 2229 | `					}` |
|        5 | 2230 | `				}` |
|        - | 2231 | `				/* php 7.1+: the return type follows the use clause —` |
|        - | 2232 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|        - | 2233 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|        - | 2234 | `				 * so an unconditional call would wipe a type parsed at the` |
|        - | 2235 | `				 * legacy pre-use position. */` |
|     1982 | 2236 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|       26 | 2237 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|       26 | 2238 | `					if( rcRt2 == SXERR_ABORT ){` |
|      ! 0 | 2239 | `						return SXERR_ABORT;` |
|       26 | 2240 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|      ! 0 | 2241 | `						return SXERR_SYNTAX;` |
|        - | 2242 | `					}` |
|        - | 2243 | `					/* The type this closure really declared is only known now. */` |
|       26 | 2244 | `					if( GenStateApplyNoDiscard(&(*pGen),pFunc,0,0) == SXERR_ABORT ){` |
|      ! 0 | 2245 | `						return SXERR_ABORT;` |
|        - | 2246 | `					}` |
|        9 | 2247 | `				}` |
|      981 | 2248 | `		}` |
|     6602 | 2249 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|        - | 2250 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|        - | 2251 | `			 * available to the closure environment — for EVERY non-static` |
|        - | 2252 | `			 * anonymous function, use list or not (php binds $this to any` |
|        - | 2253 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|        - | 2254 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|        - | 2255 | `			 * a global-scope closure is silently dropped at install. A static` |
|        - | 2256 | `			 * closure never binds $this (php). */` |
|     6206 | 2257 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     6206 | 2258 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|     6206 | 2259 | `			sEnv.nIdx = SXU32_HIGH;` |
|     6206 | 2260 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     6206 | 2261 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|     6206 | 2262 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|     3084 | 2263 | `		}` |
|     6602 | 2264 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|        - | 2265 | `			/* Mark as closure */` |
|     6286 | 2266 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|     3124 | 2267 | `		}` |
|     3282 | 2268 | `	}` |
|        - | 2269 | `	/* Compile the body */` |
|   185260 | 2270 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   185260 | 2271 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2272 | `		return SXERR_ABORT;` |
|        - | 2273 | `	}` |
|        - | 2274 | `	/* The cursor sits just past the body's closing brace */` |
|   185260 | 2275 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|   185260 | 2276 | `	if( ppFunc ){` |
|   185260 | 2277 | `		*ppFunc = pFunc;` |
|    92484 | 2278 | `	}` |
|   185260 | 2279 | `	rc = SXRET_OK;` |
|   185260 | 2280 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|   178979 | 2281 | `		if( GenStateDeclIsConditional(&(*pGen)) ){` |
|        - | 2282 | `			/* php binds this one when execution REACHES it, not now: a declaration` |
|        - | 2283 | ``			 * inside an `if`, a loop, a `try` or another function's body is not`` |
|        - | 2284 | `			 * early-bound. Binding it here made every` |
|        - | 2285 | ``			 * `if (!function_exists('x')) { function x(){} }` -- the shape every`` |
|        - | 2286 | `			 * symfony/polyfill-* package is written in -- REPLACE the engine's own` |
|        - | 2287 | ``			 * builtin, and declared the body of an `if (false)` besides. The`` |
|        - | 2288 | `			 * redeclaration screen moves to the opcode with it: two branches may` |
|        - | 2289 | `			 * each declare the name, and only the one that RUNS binds. */` |
|        - | 2290 | `` 			/* iP1 marks an ANONYMOUS function: `static function () {}` with no `use` `` |
|        - | 2291 | `			 * captures nothing, so it is not flagged a closure and lands here under a` |
|        - | 2292 | ``			 * synthesized `[lambda_N]` name -- a name no program wrote and that php has`` |
|        - | 2293 | `			 * no DECLARE_FUNCTION for at all. Running such a declaration twice is` |
|        - | 2294 | ``			 * ordinary (guzzle's `Middleware::redirect()` returns one), so the opcode`` |
|        - | 2295 | `			 * installs it and asks no redeclaration question. */` |
|      189 | 2296 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FUNC_DECL,bHandleClosure ? 1 : 0,0,(void *)pFunc,0);` |
|      189 | 2297 | `			return SXRET_OK;` |
|        - | 2298 | `		}` |
|        - | 2299 | `		/* Reject a php-fatal redeclaration before hoisting the function */` |
|   178795 | 2300 | `		if( GenStateGuardFuncRedeclaration(pGen,pFunc) == SXERR_ABORT ){` |
|       14 | 2301 | `			return SXERR_ABORT;` |
|        - | 2302 | `		}` |
|        - | 2303 | `		/* Finally register the function */` |
|   178783 | 2304 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    89262 | 2305 | `	}` |
|   185064 | 2306 | `	if( rc == SXRET_OK ){` |
|   185064 | 2307 | `		return SXRET_OK;` |
|        - | 2308 | `	}` |
|        - | 2309 | `	/* Fall through if something goes wrong */` |
|      ! 0 | 2310 | `OutOfMem:` |
|        - | 2311 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - | 2312 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|        - | 2313 | `	 */` |
|      ! 0 | 2314 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 | 2315 | `	return SXERR_ABORT;` |
|    92501 | 2316 | `}` |
|        - | 2317 | `/*` |
|        - | 2318 | ` * Compile a standard PHP function.` |
|        - | 2319 | ` *  Refer to the block-comment above for more information.` |
|        - | 2320 | ` */` |
|   178686 | 2321 | `PH7_PRIVATE sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|        5 | 2322 | `{` |
|        - | 2323 | `	SyString *pName;` |
|        - | 2324 | `	sxi32 iFlags;` |
|        - | 2325 | `	sxu32 nKwLine;` |
|        - | 2326 | `	sxu32 nLine;` |
|        - | 2327 | `	sxi32 rc;` |
|        - | 2328 |  |
|   178691 | 2329 | `	nLine = pGen->pIn->nLine;` |
|   178691 | 2330 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|   178691 | 2331 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   178691 | 2332 | `	iFlags = 0;` |
|   178691 | 2333 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - | 2334 | `		/* Return by reference,remember that */` |
|       34 | 2335 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|        - | 2336 | `		/* Jump the '&' token */` |
|       34 | 2337 | `		pGen->pIn++;` |
|       15 | 2338 | `	}` |
|   178691 | 2339 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 2340 | `		/* Invalid function name */` |
|        6 | 2341 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|        6 | 2342 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2343 | `			return SXERR_ABORT;` |
|        - | 2344 | `		}` |
|        - | 2345 | `		/* Sychronize with the next semi-colon or braces*/` |
|       18 | 2346 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       14 | 2347 | `			pGen->pIn++;` |
|        2 | 2348 | `		}` |
|        6 | 2349 | `		return SXRET_OK;` |
|        - | 2350 | `	}` |
|   178687 | 2351 | `	pName = &pGen->pIn->sData;` |
|   178687 | 2352 | `	nLine = pGen->pIn->nLine;` |
|        - | 2353 | `	/* Jump the function name */` |
|   178687 | 2354 | `	pGen->pIn++;` |
|   178687 | 2355 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 2356 | `		/* Syntax error */` |
|        3 | 2357 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|        3 | 2358 | `		if( rc == SXERR_ABORT ){` |
|        - | 2359 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 2360 | `			return SXERR_ABORT;` |
|        - | 2361 | `		}` |
|        - | 2362 | `		/* Sychronize with the next semi-colon or '{' */` |
|        3 | 2363 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 2364 | `			pGen->pIn++;` |
|      ! 0 | 2365 | `		}` |
|        3 | 2366 | `		return SXRET_OK;` |
|        - | 2367 | `	}` |
|        - | 2368 | `	/* Compile function body */` |
|        - | 2369 | `	{` |
|   178685 | 2370 | `		ph7_vm_func *pFuncState = 0;` |
|   178685 | 2371 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|   178685 | 2372 | `		if( pFuncState ){` |
|        - | 2373 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|   178663 | 2374 | `			pFuncState->nLine = nKwLine;` |
|    89202 | 2375 | `		}` |
|        - | 2376 | `	}` |
|   178685 | 2377 | `	return rc;` |
|    89221 | 2378 | `}` |
|        - | 2379 |  |
