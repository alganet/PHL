# src/ph7/compile_func.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1216/1373 lines (88.57%)

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
|    51916 |   68 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|        5 |   69 | `{` |
|        - |   70 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |   71 | `	SySet *pInstrContainer;` |
|        - |   72 | `	sxi32 rc;` |
|        - |   73 | `	/* Swap token stream */` |
|    51921 |   74 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|        - |   75 | `	/* A parameter default is a constant expression: php applies the same rules here` |
|        - |   76 | ``	 * as to a class constant, minus `new` (PHP 8.1 allows `new` in an initializer).`` |
|        - |   77 | `	 * The swap above has left pGen->pIn/pEnd spanning exactly this default. */` |
|        - |   78 | `	{` |
|    51921 |   79 | `		const char *zCErr = PH7_GenStateConstExprError(pGen,1);` |
|    51921 |   80 | `		if( zCErr ){` |
|        6 |   81 | `			sxi32 rcErr = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"%s",zCErr);` |
|        6 |   82 | `			RE_SWAP_DELIMITER(pGen);` |
|        6 |   83 | `			return rcErr == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |   84 | `		}` |
|        - |   85 | `	}` |
|    51917 |   86 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    51917 |   87 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|        - |   88 | `	/* Compile the expression holding the argument value. A parameter default is a` |
|        - |   89 | `	 * const-expression belonging to the current class (see iInMemberDefault) — so` |
|        - |   90 | `	 * __TRAIT__ in it reads pCurClass rather than walking into the enclosing method. */` |
|    51917 |   91 | `	pGen->iInMemberDefault++;` |
|    51917 |   92 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    51917 |   93 | `	pGen->iInMemberDefault--;` |
|        - |   94 | `	/* Emit the done instruction */` |
|    51917 |   95 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    51917 |   96 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - |   97 | `	/* Finished, like a function body: give back the doubling slack. A default is a` |
|        - |   98 | `	 * handful of instructions in a container that opened at eight. */` |
|    51917 |   99 | `	SySetShrinkToFit(&pArg->aByteCode);` |
|    51917 |  100 | `	RE_SWAP_DELIMITER(pGen);` |
|    51917 |  101 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  102 | `		return SXERR_ABORT;` |
|        - |  103 | `	}` |
|    51917 |  104 | `	return SXRET_OK;` |
|    25929 |  105 | `}` |
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
|   177548 |  143 | `PH7_PRIVATE sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|        5 |  144 | `{` |
|        - |  145 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|        - |  146 | `	SyToken *pIn;  /* Token stream */` |
|        - |  147 | `	SyBlob sSig;         /* Function signature */` |
|        - |  148 | `	char *zDup;          /* Copy of argument name */` |
|   177553 |  149 | ``	int bSeenVariadic = 0; /* a `...$x` was declared already: php refuses what follows */`` |
|        - |  150 | `	sxu32 nArgLine;      /* the line the current parameter starts on */` |
|        - |  151 | `	sxi32 rc;` |
|        - |  152 |  |
|   177553 |  153 | `	pIn = pGen->pIn;` |
|   177553 |  154 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|        - |  155 | `	/* Process arguments one after one */` |
|   226098 |  156 | `	for(;;){` |
|   452814 |  157 | `		if( pIn >= pEnd ){` |
|        - |  158 | `			/* No more arguments to process */` |
|   177487 |  159 | `			break;` |
|        - |  160 | `		}` |
|   275332 |  161 | `		nArgLine = pIn->nLine;` |
|   275332 |  162 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   275332 |  163 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   275332 |  164 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   275332 |  165 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   275332 |  166 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|        - |  167 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|        - |  168 | `		 * first token inside the main token stream */` |
|   275332 |  169 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
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
|   275332 |  181 | `			int bReadonly = 0, bVisSeen = 0, bReadVis = 0, bSetSeen = 0, bFinal = 0;` |
|   275332 |  182 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   275332 |  183 | `			sxi32 iSetVisFlag = 0;` |
|   275332 |  184 | `			int nSetTok = 0;   /* left unset when bNameHead skips the peek below */` |
|        - |  185 | `			sxi32 nSetVis;` |
|   275332 |  186 | `			sxu32 nModLine = (pIn < pEnd) ? pIn->nLine : 0;` |
|   275332 |  187 | `			const char *zTwice = 0;    /* a modifier written twice; "" = the access-type one */` |
|   275332 |  188 | `			const char *zNotHere = 0;  /* ...and one a PARAMETER never takes */` |
|        - |  189 | ``			/* php lexes `private\Q` as one T_NAME_QUALIFIED, so a modifier word a`` |
|        - |  190 | ``			 * `\` follows is the head of a TYPE name and no modifier at all:`` |
|        - |  191 | ``			 * `function f(private\Q $x)` is an ordinary parameter, not a promoted`` |
|        - |  192 | `			 * property outside a constructor. */` |
|   275352 |  193 | `			int bNameHead = ( pIn + 1 < pEnd && (pIn[1].nType & PH7_TK_NSSEP)` |
|   413178 |  194 | `				&& GenStateTokensGlued(pIn,&pIn[1]) );` |
|   275616 |  195 | `			while( !bNameHead && zTwice == 0 && zNotHere == 0 && pIn < pEnd ){` |
|   275574 |  196 | `				if( GenStateIsReadonly(pIn) ){` |
|       36 |  197 | `					if( bReadonly ){` |
|      ! 0 |  198 | `						zTwice = "readonly";` |
|      ! 0 |  199 | `					}` |
|       36 |  200 | `					bReadonly = 1;` |
|       36 |  201 | `					pIn++;` |
|       36 |  202 | `					continue;` |
|        - |  203 | `				}` |
|   275542 |  204 | `				if( (pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    36939 |  205 | `					break;` |
|        - |  206 | `				}` |
|   238608 |  207 | `				nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   238608 |  208 | `				if( nSetVis ){` |
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
|   238604 |  219 | `					sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   238599 |  220 | `					if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED` |
|   238397 |  221 | `					 \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      243 |  222 | `						if( bReadVis ){` |
|        3 |  223 | `							zTwice = "";` |
|        1 |  224 | `						}` |
|      243 |  225 | `						bReadVis = 1;` |
|      243 |  226 | `						bVisSeen = 1;` |
|      243 |  227 | `						iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      327 |  228 | `							: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|      104 |  229 | `							: PH7_CLASS_PROT_PUBLIC;` |
|   238485 |  230 | `					}else if( nKw == PH7_TKWRD_FINAL ){` |
|        7 |  231 | `						if( bFinal ){` |
|      ! 0 |  232 | `							zTwice = "final";` |
|      ! 0 |  233 | `						}` |
|        7 |  234 | `						bFinal = 1;` |
|   238363 |  235 | `					}else if( nKw == PH7_TKWRD_STATIC ){` |
|        3 |  236 | `						zNotHere = "static";` |
|   238359 |  237 | `					}else if( nKw == PH7_TKWRD_ABSTRACT ){` |
|        3 |  238 | `						zNotHere = "abstract";` |
|        2 |  239 | `					}else{` |
|   238356 |  240 | ``						break; /* the type or the `$name` starts here */`` |
|        - |  241 | `					}` |
|      253 |  242 | `					pIn++;` |
|        - |  243 | `				}` |
|        5 |  244 | `			}` |
|        - |  245 | `			/* The same duplicate rules the class body's run enforces -- php words` |
|        - |  246 | `			 * them identically wherever the modifier was written -- plus the two a` |
|        - |  247 | `			 * PARAMETER never takes, which php words against "a parameter". */` |
|   275332 |  248 | `			if( zTwice ){` |
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
|   275330 |  260 | `			if( zNotHere ){` |
|        6 |  261 | `				pGen->iFatalTrace = PH7_FATAL_TRACE_NONE;` |
|        8 |  262 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nModLine,` |
|        2 |  263 | `					"Cannot use the %s modifier on a parameter",zNotHere);` |
|        6 |  264 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  265 | `					return SXERR_ABORT;` |
|        - |  266 | `				}` |
|        6 |  267 | `				return SXERR_SYNTAX;` |
|        - |  268 | `			}` |
|   275326 |  269 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|        5 |  270 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   275324 |  271 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|      ! 0 |  272 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|      ! 0 |  273 | `			}` |
|   275326 |  274 | `			if( bFinal ){` |
|        7 |  275 | `				sArg.iFlags \|= VM_FUNC_ARG_FINAL;` |
|        3 |  276 | `			}` |
|   275326 |  277 | `			if( bVisSeen \|\| bReadonly \|\| bFinal ){` |
|      245 |  278 | `				if( !bCtorCtx ){` |
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
|      241 |  291 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      241 |  292 | `				sArg.iPromoteVis = iVis;` |
|      241 |  293 | `				if( bReadonly ){` |
|       36 |  294 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|       16 |  295 | `				}` |
|      118 |  296 | `			}` |
|        - |  297 | `		}` |
|        - |  298 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   275317 |  299 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   266101 |  300 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   256647 |  301 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   256130 |  302 | `			sxu32 nLineLocal = pIn->nLine;` |
|   256130 |  303 | `			sxi32 iTFlags = 0;` |
|        - |  304 | ``			/* php's `?T` is T's own node with a flag set, so the line is T's */`` |
|   128017 |  305 | `			sArg.nLine = ( (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1` |
|   256188 |  306 | `				&& pIn->sData.zString[0] == '?' && pIn + 1 < pEnd ) ? pIn[1].nLine : nLineLocal;` |
|   256130 |  307 | `			pGen->pIn = pIn;` |
|   256130 |  308 | `			rc = GenStateParseUnionTypeDecl(` |
|   127886 |  309 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   127886 |  310 | `				&iTFlags, &sArg.sTypeName,` |
|        - |  311 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|        - |  312 | `				/* bAllowVoid */ 0, /* bParamCtx */ 1,` |
|   127886 |  313 | `						nLineLocal);` |
|   256130 |  314 | `			pIn = pGen->pIn;` |
|   256130 |  315 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  316 | `				return SXERR_ABORT;` |
|   256130 |  317 | `			}else if( rc == SXERR_CORRUPT ){` |
|        - |  318 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|        3 |  319 | `				return SXERR_SYNTAX;` |
|   256128 |  320 | `			}else if( rc == SXERR_SYNTAX ){` |
|       22 |  321 | `				if( pIn < pEnd ){` |
|       31 |  322 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|        - |  323 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|        9 |  324 | `						&pIn->sData);` |
|       13 |  325 | `				}else{` |
|      ! 0 |  326 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|        - |  327 | `						"syntax error, unexpected end of file");` |
|        - |  328 | `				}` |
|       22 |  329 | `				return SXERR_SYNTAX;` |
|        - |  330 | `			}` |
|   256110 |  331 | `			sArg.iFlags \|= iTFlags;` |
|   127876 |  332 | `		}` |
|   275302 |  333 | `		if( pIn >= pEnd ){` |
|      ! 0 |  334 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,"Missing argument name");` |
|      ! 0 |  335 | `			return rc;` |
|        - |  336 | `		}` |
|   275302 |  337 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|        - |  338 | `			/* Pass by reference,record that */` |
|      577 |  339 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|      577 |  340 | `			pIn++;` |
|      286 |  341 | `		}` |
|   275302 |  342 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|        - |  343 | `			/* Variadic parameter: ...$args */` |
|     8857 |  344 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|     8857 |  345 | `			pIn++;` |
|     4420 |  346 | `		}` |
|   275302 |  347 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  348 | `			/* Invalid argument */` |
|        3 |  349 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|        3 |  350 | `			return rc;` |
|        - |  351 | `		}` |
|   275300 |  352 | `		if( sArg.nLine == 0 ){` |
|    19197 |  353 | ``			sArg.nLine = pIn->nLine; /* untyped: php's parameter sits on its `$name` */`` |
|     9585 |  354 | `		}` |
|   275300 |  355 | `		pIn++; /* Jump the dollar sign */` |
|        - |  356 | `		/* Copy argument name */` |
|   275300 |  357 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   275300 |  358 | `		if( zDup == 0 ){` |
|      ! 0 |  359 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  360 | `			return SXERR_ABORT;` |
|        - |  361 | `		}` |
|   275300 |  362 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   275300 |  363 | `		pIn++;` |
|        - |  364 | `		/* php's four refusals of a parameter LIST, made in php's order once the` |
|        - |  365 | `		 * name is known, and each reported on the line the parameter STARTS on` |
|        - |  366 | ``		 * (its type or modifier, not its `$`): a name written twice, `$this`, a`` |
|        - |  367 | `		 * parameter after a variadic one, and a variadic one with a default.` |
|        - |  368 | `` 		 * None of the four was checked at all: `function q(...$a, $b) {}` `` |
|        - |  369 | `		 * compiled and ran here where php stops at compile time, which is a` |
|        - |  370 | ``		 * program `php -l` refuses and `phl -l` passed. */`` |
|        - |  371 | `		{` |
|   275300 |  372 | `			ph7_vm_func_arg *aPrev = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|   275300 |  373 | `			sxu32 nPrev = SySetUsed(&pFunc->aArgs);` |
|   275300 |  374 | `			int bRefused = 1;   /* MSVC cannot see that this implies rc was set: seed rc */` |
|        - |  375 | `			sxu32 n;` |
|   275300 |  376 | `			rc = SXRET_OK;` |
|   391580 |  377 | `			for( n = 0; n < nPrev; n++ ){` |
|   116287 |  378 | `				if( SyStringCmp(&aPrev[n].sName,&sArg.sName,SyMemcmp) == 0 ){` |
|        3 |  379 | `					break;` |
|        - |  380 | `				}` |
|    58064 |  381 | `			}` |
|   275300 |  382 | `			if( n < nPrev ){` |
|        4 |  383 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nArgLine,` |
|        1 |  384 | `					"Redefinition of parameter $%z",&sArg.sName);` |
|   275295 |  385 | `			}else if( SyStringLength(&sArg.sName) == sizeof("this")-1` |
|   159182 |  386 | `				&& SyMemcmp(SyStringData(&sArg.sName),"this",sizeof("this")-1) == 0 ){` |
|        3 |  387 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nArgLine,` |
|        - |  388 | `					"Cannot use $this as parameter");` |
|   275297 |  389 | `			}else if( bSeenVariadic ){` |
|       11 |  390 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nArgLine,` |
|        - |  391 | `					"Only the last parameter can be variadic");` |
|   275292 |  392 | `			}else if( (sArg.iFlags & VM_FUNC_ARG_VARIADIC) && pIn < pEnd && (pIn->nType & PH7_TK_EQUAL) ){` |
|        3 |  393 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,nArgLine,` |
|        - |  394 | `					"Variadic parameter cannot have a default value");` |
|        2 |  395 | `			}else{` |
|   275286 |  396 | `				bRefused = 0;` |
|   275286 |  397 | `				if( sArg.iFlags & VM_FUNC_ARG_VARIADIC ){` |
|     8851 |  398 | `					bSeenVariadic = 1;` |
|     4417 |  399 | `				}` |
|        - |  400 | `			}` |
|   275300 |  401 | `			if( bRefused ){` |
|       18 |  402 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  403 | `					return SXERR_ABORT;` |
|        - |  404 | `				}` |
|       18 |  405 | `				return SXERR_SYNTAX;` |
|        - |  406 | `			}` |
|        - |  407 | `		}` |
|   275286 |  408 | `		if( pIn < pEnd ){` |
|   132372 |  409 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|        - |  410 | `				SyToken *pDefend;` |
|    51923 |  411 | `				sxi32 iNest = 0;` |
|    51923 |  412 | `				pIn++; /* Jump the equal sign */` |
|    51923 |  413 | `				pDefend = pIn;` |
|        - |  414 | `				/* Process the default value associated with this argument */` |
|   104265 |  415 | `				while( pDefend < pEnd ){` |
|    69689 |  416 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    17347 |  417 | `						break;` |
|        - |  418 | `					}` |
|    52347 |  419 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|        - |  420 | `						/* Increment nesting level */` |
|       77 |  421 | `						iNest++;` |
|    52311 |  422 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|        - |  423 | `						/* Decrement nesting level */` |
|       77 |  424 | `						iNest--;` |
|       36 |  425 | `					}` |
|    52347 |  426 | `					pDefend++;` |
|        5 |  427 | `				}` |
|    51923 |  428 | `				if( pIn >= pDefend ){` |
|        3 |  429 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,pIn->nLine,"Missing argument default value");` |
|        3 |  430 | `					return rc;` |
|        - |  431 | `				}` |
|        - |  432 | `				/* Process default value */` |
|    51921 |  433 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|    51921 |  434 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  435 | `					return rc;` |
|        - |  436 | `				}` |
|        - |  437 | `				/* php folds a typed parameter's default at compile time and refuses` |
|        - |  438 | `				 * one its type does not hold (zend_compile_params). A plain` |
|        - |  439 | `				 * parameter's null is the implicit-nullable form below; a PROMOTED` |
|        - |  440 | `				 * one's is refused here like any other value. */` |
|    51921 |  441 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION)) ){` |
|        - |  442 | `					ph7_value sVal;` |
|    42512 |  443 | `					PH7_MemObjInit(pGen->pVm,&sVal);` |
|    42507 |  444 | `					if( PH7_ClassFoldDefault(pGen->pVm,&sArg.aByteCode,&sVal)` |
|    34021 |  445 | `						&& ((sVal.iFlags & MEMOBJ_NULL) == 0 \|\| (sArg.iFlags & VM_FUNC_ARG_PROMOTED)) ){` |
|        - |  446 | `						SyBlob sMsg;` |
|    25528 |  447 | `						if( (sVal.iFlags & MEMOBJ_REAL) && PH7_GenStateIsRealLiteral(pIn,pDefend) ){` |
|       11 |  448 | ``							sVal.iFlags &= ~MEMOBJ_INT; /* `= 1.0` is a float to php */`` |
|        4 |  449 | `						}` |
|        - |  450 | `						/* php names a METHOD's self/parent by class; a closure's scope can` |
|        - |  451 | `						 * be rebound, so it keeps the keyword. The base is linked only` |
|        - |  452 | `						 * after the body compiles: lend it for the sentence. */` |
|    25528 |  453 | `						ph7_class *pScope = pGen->iSigScope == PH7_SIGSCOPE_MEMBER ? pGen->pCurClass : 0;` |
|        - |  454 | `						int bRefused;` |
|    25528 |  455 | `						SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|    25528 |  456 | `						if( pScope && pScope->pBase == 0 && pGen->pCurBase ){` |
|       19 |  457 | `							pScope->pBase = pGen->pCurBase;` |
|       19 |  458 | `							bRefused = VmArgDefaultRefusal(pGen->pVm,pScope,&sArg,&sVal,&sMsg);` |
|       19 |  459 | `							pScope->pBase = 0;` |
|       12 |  460 | `						}else{` |
|    25514 |  461 | `							bRefused = VmArgDefaultRefusal(pGen->pVm,pScope,&sArg,&sVal,&sMsg);` |
|        - |  462 | `						}` |
|    25528 |  463 | `						if( bRefused ){` |
|       20 |  464 | `							SyBlobNullAppend(&sMsg);` |
|       20 |  465 | `							PH7_MemObjRelease(&sVal);` |
|       20 |  466 | `							PH7_GenCompileError(&(*pGen),E_ERROR,sArg.nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       20 |  467 | `							SyBlobRelease(&sMsg);` |
|       20 |  468 | `							return SXERR_ABORT;` |
|        - |  469 | `						}` |
|    25512 |  470 | `						SyBlobRelease(&sMsg);` |
|    25512 |  471 | `						if( VmArgDefaultWidens(&sArg,&sVal) ){` |
|        - |  472 | `							/* php stores the int it let into a float AS a float, so the` |
|        - |  473 | `							 * default itself (reflection, the export) reads float(1):` |
|        - |  474 | `							 * cast it ahead of the trailing DONE. */` |
|       31 |  475 | `							VmInstr *pDone = (VmInstr *)SySetPeek(&sArg.aByteCode);` |
|       31 |  476 | `							if( pDone && pDone->iOp == PH7_OP_DONE ){` |
|       31 |  477 | `								VmInstr sDone = *pDone;` |
|       31 |  478 | `								pDone->iOp = PH7_OP_CVT_REAL;` |
|       31 |  479 | `								pDone->iP1 = 0;` |
|       31 |  480 | `								pDone->iP2 = 0;` |
|       31 |  481 | `								pDone->p3 = 0;` |
|       31 |  482 | `								pDone->nAux = 0;` |
|       31 |  483 | `								pDone->nSite = 0;` |
|       31 |  484 | `								SySetPut(&sArg.aByteCode,(const void *)&sDone);` |
|       14 |  485 | `							}` |
|       14 |  486 | `						}` |
|    12736 |  487 | `					}` |
|    42496 |  488 | `					PH7_MemObjRelease(&sVal);` |
|    21217 |  489 | `				}` |
|        - |  490 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|        - |  491 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|        - |  492 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|        - |  493 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|        - |  494 | `				 * arg-type check lets null through. */` |
|    51900 |  495 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|    47190 |  496 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|    47177 |  497 | `					&& &pIn[1] == pDefend` |
|    42431 |  498 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|    33877 |  499 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|    12683 |  500 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|        - |  501 | `` 					/* php 8.4 DEPRECATED the implicit-nullable form (`int $x = null` `` |
|        - |  502 | ``					 * without the `?`). PHL targets php's *non-deprecated* surface and`` |
|        - |  503 | ``					 * rejects it outright — the explicit `?int` must be written.`` |
|        - |  504 | ``					 * `mixed $x = null` is fine: mixed already includes null (explicit`` |
|        - |  505 | `					 * ?T / T\|null are already excluded via VM_FUNC_ARG_NULLABLE above). */` |
|        6 |  506 | `					if( (sArg.sClass.nByte == sizeof("mixed")-1` |
|        5 |  507 | `						&& SyStrnicmp(SyStringData(&sArg.sClass),"mixed",sizeof("mixed")-1) == 0)` |
|        7 |  508 | `						\|\| pGen->bDeclCheck ){` |
|        - |  509 | `						/* A deferred declaration's check compile leaves this refusal to` |
|        - |  510 | `						 * the real compile: php only deprecates the form, so it must not` |
|        - |  511 | `						 * stop a file whose polyfill branch never runs. */` |
|        6 |  512 | `						sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|        4 |  513 | `					}else{` |
|        3 |  514 | `						const char *zSep = "";` |
|        3 |  515 | `						SyString sCls = { "", 0 };` |
|        3 |  516 | `						if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|      ! 0 |  517 | `							sCls = ((ph7_class *)pFunc->pUserData)->sDisp;` |
|      ! 0 |  518 | `							zSep = "::";` |
|      ! 0 |  519 | `						}` |
|        4 |  520 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,` |
|        - |  521 | `							"%z%s%z(): Cannot use null as the default for non-nullable parameter $%z; write the explicit ?T type instead",` |
|        1 |  522 | `							&sCls,zSep,&pFunc->sName,&sArg.sName);` |
|        3 |  523 | `						return SXERR_ABORT;` |
|        - |  524 | `					}` |
|        2 |  525 | `				}` |
|        - |  526 | `				/* Point beyond the default value */` |
|    51903 |  527 | `				pIn = pDefend;` |
|    25915 |  528 | `			}` |
|   132352 |  529 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 |  530 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|      ! 0 |  531 | `				return rc;` |
|        - |  532 | `			}` |
|   132352 |  533 | `			pIn++; /* Jump the trailing comma */` |
|    66082 |  534 | `		}` |
|        - |  535 | `		/* Append argument signature */` |
|   275266 |  536 | `		if( sArg.nType > 0 ){` |
|   255920 |  537 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|        - |  538 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    17673 |  539 | `				int marker = 'o';` |
|    17673 |  540 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    17673 |  541 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     8826 |  542 | `			}else{` |
|        - |  543 | `				int c;` |
|   238252 |  544 | `				c = 'n'; /* cc warning */` |
|        - |  545 | `				/* Type leading character */` |
|   238252 |  546 | `				switch(sArg.nType){` |
|    16996 |  547 | `				case MEMOBJ_HASHMAP:` |
|        - |  548 | `					/* Hashmap aka 'array' */` |
|    33952 |  549 | `					c = 'h';` |
|    33952 |  550 | `					break;` |
|    29988 |  551 | `				case MEMOBJ_INT:` |
|        - |  552 | `					/* Integer */` |
|    59900 |  553 | `					c = 'i';` |
|    59900 |  554 | `					break;` |
|     4273 |  555 | `				case MEMOBJ_BOOL:` |
|        - |  556 | `					/* Bool */` |
|     8539 |  557 | `					c = 'b';` |
|     8539 |  558 | `					break;` |
|    21168 |  559 | `				case MEMOBJ_REAL:` |
|        - |  560 | `					/* Float */` |
|    42286 |  561 | `					c = 'f';` |
|    42286 |  562 | `					break;` |
|    46837 |  563 | `				case MEMOBJ_STRING:` |
|        - |  564 | `					/* String */` |
|    93545 |  565 | `					c = 's';` |
|    93545 |  566 | `					break;` |
|       24 |  567 | `				case MEMOBJ_OBJ:` |
|        - |  568 | `					/* Object */` |
|       52 |  569 | `					c = 'o';` |
|       48 |  570 | `					break;` |
|        1 |  571 | `				default:` |
|        2 |  572 | `					break;` |
|        - |  573 | `				}` |
|   238252 |  574 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|        - |  575 | `			}` |
|   127786 |  576 | `		}else{` |
|        - |  577 | `			/* No type is associated with this parameter which mean` |
|        - |  578 | `			 * that this function is not condidate for overloading.` |
|        - |  579 | `			 */` |
|    19351 |  580 | `			SyBlobRelease(&sSig);` |
|        - |  581 | `		}` |
|        - |  582 | `		/* php's attribute placement rules, once the promotion modifiers are read:` |
|        - |  583 | `` 		 * a PROMOTED parameter is a property as well, so `#[\Override] public $p` `` |
|        - |  584 | `		 * in a constructor signature is accepted here and judged as the property` |
|        - |  585 | `		 * claim it is -- while php still NAMES the target "parameter". */` |
|   412704 |  586 | `		if( GenStateCheckAttrPlacement(&(*pGen),&sArg.aAttrs,sArg.nLine,32,` |
|   412709 |  587 | `				(sArg.iFlags & VM_FUNC_ARG_PROMOTED) ? (32\|8) : 32,0,0) == SXERR_ABORT ){` |
|      ! 0 |  588 | `			return SXERR_ABORT;` |
|        - |  589 | `		}` |
|        - |  590 | `		/* Save in the argument set */` |
|   275266 |  591 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|        5 |  592 | `	}` |
|   177487 |  593 | `	if( SyBlobLength(&sSig) > 0 ){` |
|        - |  594 | `		/* Save function signature */` |
|   162246 |  595 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|    81011 |  596 | `	}` |
|   177487 |  597 | `	return SXRET_OK;` |
|    88660 |  598 | `}` |
|        - |  599 | `/*` |
|        - |  600 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|        - |  601 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|        - |  602 | ` * the enclosing function. Returns the token just past the nested construct.` |
|        - |  603 | ` */` |
|      700 |  604 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|        5 |  605 | `{` |
|      705 |  606 | `	sxi32 iParen = 0;` |
|      705 |  607 | `	pIn++; /* past 'function'/'fn' */` |
|        - |  608 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|        - |  609 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|        - |  610 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|     4531 |  611 | `	while( pIn < pEnd ){` |
|     4531 |  612 | `		sxu32 t = pIn->nType;` |
|     4531 |  613 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|     3623 |  614 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|     2715 |  615 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|     2017 |  616 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|     3831 |  617 | `		pIn++;` |
|        5 |  618 | `	}` |
|      703 |  619 | `	if( pIn >= pEnd ){ return pIn; }` |
|        - |  620 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|        - |  621 | `	{` |
|      703 |  622 | `		sxi32 d = 0;` |
|     8323 |  623 | `		while( pIn < pEnd ){` |
|     8323 |  624 | `			sxu32 t = pIn->nType;` |
|     8323 |  625 | `			if( t & PH7_TK_OCB ){ d++; }` |
|     7553 |  626 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|     7625 |  627 | `			pIn++;` |
|        5 |  628 | `		}` |
|        - |  629 | `	}` |
|      703 |  630 | `	return pIn;` |
|      355 |  631 | `}` |
|        - |  632 | `/*` |
|        - |  633 | `` * ROOT C helper: from an `fn` keyword token, skip the whole arrow function -- its`` |
|        - |  634 | ` * signature, its optional return type and the single expression that is its body.` |
|        - |  635 | `` * The body ends where the expression parser would end it: a `,` or `;` at nesting`` |
|        - |  636 | ` * depth zero, or a closer that would unbalance the group the arrow sits in.` |
|        - |  637 | ` */` |
|     1084 |  638 | `static SyToken * GenStateSkipArrowBody(SyToken *pIn, SyToken *pEnd)` |
|        5 |  639 | `{` |
|     1089 |  640 | `	sxi32 iNest = 0;` |
|     1089 |  641 | `	pIn++; /* past 'fn' */` |
|        - |  642 | ``	/* The signature: skip the balanced `( … )`. Only an optional `&` can stand`` |
|        - |  643 | ``	 * between `fn` and the `(`, so anything that ends a statement or a block first`` |
|        - |  644 | `	 * means this was never an arrow function -- stop rather than run off into the` |
|        - |  645 | ``	 * enclosing body, where a real `yield` would then go unseen. */`` |
|     1189 |  646 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      120 |  647 | `		if( pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB\|PH7_TK_CCB) ){ return pIn; }` |
|      102 |  648 | `		pIn++;` |
|        2 |  649 | `	}` |
|     2782 |  650 | `	while( pIn < pEnd ){` |
|     2782 |  651 | `		sxu32 t = pIn->nType;` |
|     2782 |  652 | `		if( t & PH7_TK_LPAREN ){ iNest++; }` |
|     1716 |  653 | `		else if( t & PH7_TK_RPAREN ){ iNest--; if( iNest <= 0 ){ pIn++; break; } }` |
|     1716 |  654 | `		pIn++;` |
|        5 |  655 | `	}` |
|        - |  656 | ``	/* The `=>` that opens the body (a return type may sit before it). */`` |
|     1783 |  657 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     1191 |  658 | `		if( pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB) ){ return pIn; }` |
|      717 |  659 | `		pIn++;` |
|        5 |  660 | `	}` |
|      597 |  661 | `	if( pIn < pEnd ){ pIn++; } /* past '=>' */` |
|        - |  662 | `	/* The body expression. */` |
|      597 |  663 | `	iNest = 0;` |
|     5380 |  664 | `	while( pIn < pEnd ){` |
|     5380 |  665 | `		sxu32 t = pIn->nType;` |
|     5380 |  666 | `		if( t & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){ iNest++; }` |
|     4546 |  667 | `		else if( t & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     1102 |  668 | `			if( iNest <= 0 ){ break; }` |
|      839 |  669 | `			iNest--;` |
|     3864 |  670 | `		}else if( (t & (PH7_TK_COMMA\|PH7_TK_SEMI)) && iNest <= 0 ){` |
|      334 |  671 | `			break;` |
|        - |  672 | `		}` |
|     4788 |  673 | `		pIn++;` |
|        5 |  674 | `	}` |
|      597 |  675 | `	return pIn;` |
|      546 |  676 | `}` |
|        - |  677 | `/*` |
|        - |  678 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|        - |  679 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|        - |  680 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|        - |  681 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|        - |  682 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|        - |  683 | ` * detached-mini-program path untouched.` |
|        - |  684 | ` */` |
|        - |  685 | `/*` |
|        - |  686 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|        - |  687 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|        - |  688 | ` * mixed, object.` |
|        - |  689 | ` */` |
|       32 |  690 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|        5 |  691 | `{` |
|        - |  692 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|        - |  693 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|        - |  694 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|        - |  695 | `	};` |
|        - |  696 | `	sxu32 i;` |
|       37 |  697 | `	if( nName > 0 && zName[0] == '\\' ){` |
|      ! 0 |  698 | `		zName++;` |
|      ! 0 |  699 | `		nName--;` |
|      ! 0 |  700 | `	}` |
|       51 |  701 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|       51 |  702 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|       37 |  703 | `			return 1;` |
|        - |  704 | `		}` |
|        9 |  705 | `	}` |
|      ! 0 |  706 | `	return 0;` |
|       21 |  707 | `}` |
|        - |  708 | `/*` |
|        - |  709 | ` * One atom of a generator's declared return type: is it a supertype of` |
|        - |  710 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|        - |  711 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|        - |  712 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|        - |  713 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|        - |  714 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|        - |  715 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|        - |  716 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|        - |  717 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|        - |  718 | ` * both rather than fatal on valid code (a recorded divergence).` |
|        - |  719 | ` */` |
|       34 |  720 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|        5 |  721 | `{` |
|       39 |  722 | `	if( nType == MEMOBJ_OBJ ){` |
|      ! 0 |  723 | ``		return 1; /* bare `object` */`` |
|        - |  724 | `	}` |
|       39 |  725 | `	if( nType != SXU32_HIGH ){` |
|        3 |  726 | `		return 0; /* scalar/array/void/never/null/... */` |
|        - |  727 | `	}` |
|       37 |  728 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|       37 |  729 | `		return 1;` |
|        - |  730 | `	}` |
|        - |  731 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|        - |  732 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|        - |  733 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|        - |  734 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|        - |  735 | `	{` |
|        - |  736 | `		SyBlob sFQN;` |
|        - |  737 | `		int bOk;` |
|      ! 0 |  738 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      ! 0 |  739 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|      ! 0 |  740 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|      ! 0 |  741 | `		SyBlobRelease(&sFQN);` |
|      ! 0 |  742 | `		return bOk;` |
|        - |  743 | `	}` |
|       22 |  744 | `}` |
|        - |  745 | `/*` |
|        - |  746 | ` * php 8: a generator function may only declare a return type that is a` |
|        - |  747 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|        - |  748 | ` * group qualifies only if every member does. Anything else is php's exact` |
|        - |  749 | ` * compile-time fatal "Generator return type must be a supertype of` |
|        - |  750 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|        - |  751 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|        - |  752 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|        - |  753 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|        - |  754 | ` */` |
|      540 |  755 | `PH7_PRIVATE sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|        5 |  756 | `{` |
|      545 |  757 | `	int bOk = 0;` |
|        - |  758 | `	sxu32 nLine;` |
|        - |  759 | `	sxi32 rc;` |
|      545 |  760 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|      511 |  761 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|        - |  762 | `	}` |
|       39 |  763 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|      ! 0 |  764 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  765 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|        - |  766 | `		sxu32 i,j;` |
|      ! 0 |  767 | `		for( i = 0; i < n && !bOk; i++ ){` |
|        - |  768 | `			int bGroupOk;` |
|      ! 0 |  769 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|      ! 0 |  770 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|        - |  771 | `			}` |
|      ! 0 |  772 | `			bGroupOk = 1;` |
|      ! 0 |  773 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|      ! 0 |  774 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|      ! 0 |  775 | `					bGroupOk = 0;` |
|      ! 0 |  776 | `					break;` |
|        - |  777 | `				}` |
|      ! 0 |  778 | `			}` |
|      ! 0 |  779 | `			bOk = bGroupOk;` |
|      ! 0 |  780 | `		}` |
|      ! 0 |  781 | `	}else{` |
|       39 |  782 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|        - |  783 | `	}` |
|       39 |  784 | `	if( bOk ){` |
|       37 |  785 | `		return SXRET_OK;` |
|        - |  786 | `	}` |
|        - |  787 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|        - |  788 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|        - |  789 | `	 * token of this stream — its line is the function's closing brace. php` |
|        - |  790 | `	 * reports the SIGNATURE line instead; the drift is the error-` |
|        - |  791 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|        3 |  792 | `	nLine = pGen->pIn[-1].nLine;` |
|        - |  793 | `	{` |
|        3 |  794 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|        3 |  795 | `		if( sGiven.nByte < 1 ){` |
|      ! 0 |  796 | `			sGiven = pFunc->sReturnClass;` |
|      ! 0 |  797 | `		}` |
|        3 |  798 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  799 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|        - |  800 | `	}` |
|        3 |  801 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      275 |  802 | `}` |
|   195902 |  803 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|        5 |  804 | `{` |
|   195907 |  805 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|   195907 |  806 | `	SyToken *pEnd = pGen->pEnd;` |
|   195907 |  807 | `	sxi32 iDepth = 0;` |
|   195907 |  808 | `	int bStarted = 0;` |
| 24812254 |  809 | `	while( pIn < pEnd ){` |
| 24812236 |  810 | `		sxu32 t = pIn->nType;` |
| 24812236 |  811 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 23747632 |  812 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 22683732 |  813 | `		if( t & PH7_TK_KEYWORD ){` |
|  1991626 |  814 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  1991626 |  815 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  1991106 |  816 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|        - |  817 | ``			/* An arrow body is a single EXPRESSION, but php takes a `yield` in one --`` |
|        - |  818 | ``			 * `fn() => yield 7` is a generator, whose yield belongs to the ARROW. Skip`` |
|        - |  819 | `			 * it, or the enclosing function would be classified a generator by a yield` |
|        - |  820 | `			 * that is not its own. */` |
|  1990406 |  821 | `			if( kw == PH7_TKWRD_FN ){ pIn = GenStateSkipArrowBody(pIn,pEnd); continue; }` |
|   993354 |  822 | `		}` |
| 22681428 |  823 | `		pIn++;` |
|        5 |  824 | `	}` |
|   195387 |  825 | `	return FALSE;` |
|    97818 |  826 | `}` |
|        - |  827 | `/*` |
|        - |  828 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|        - |  829 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  830 | ` * and this routine takes care of generating the appropriate error message.` |
|        - |  831 | ` */` |
|   195902 |  832 | `PH7_PRIVATE sxi32 GenStateCompileFuncBody(` |
|        - |  833 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |  834 | `	ph7_vm_func *pFunc    /* Function state */` |
|        - |  835 | `	)` |
|        5 |  836 | `{` |
|        - |  837 | `	SySet *pInstrContainer; /* Instruction container */` |
|        - |  838 | `	GenBlock *pBlock;` |
|        - |  839 | `	sxu32 nGotoOfft;` |
|        - |  840 | `	sxi32 rc;` |
|        - |  841 | `	/* Attach the new function */` |
|   195907 |  842 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   195907 |  843 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  844 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|        - |  845 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  846 | `		return SXERR_ABORT;` |
|        - |  847 | `	}` |
|   195907 |  848 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|        - |  849 | `	/* Swap bytecode containers */` |
|   195907 |  850 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   195907 |  851 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|        - |  852 | `	/* Emit constructor property promotion prologue:` |
|        - |  853 | `	 *   $this->NAME = $NAME;` |
|        - |  854 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|        - |  855 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|        - |  856 | `	{` |
|   195907 |  857 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|        - |  858 | `		sxu32 i;` |
|   469916 |  859 | `		for( i = 0; i < nArg; i++ ){` |
|   274014 |  860 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|        - |  861 | `			char *zSrc;` |
|        - |  862 | `			sxu32 nSrc,nName;` |
|        - |  863 | `			SySet sToken;` |
|        - |  864 | `			SyToken *pTmpIn,*pTmpEnd;` |
|        - |  865 | `			sxi32 rcPromote;` |
|   274014 |  866 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   273794 |  867 | `				continue;` |
|        - |  868 | `			}` |
|        - |  869 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|        - |  870 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|        - |  871 | `			 * copied), so it must outlive the function — never free it. The` |
|        - |  872 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|        - |  873 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      225 |  874 | `			nName = SyStringLength(&pArg->sName);` |
|      225 |  875 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      225 |  876 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      225 |  877 | `			if( zSrc == 0 ){` |
|      ! 0 |  878 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  879 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  880 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  881 | `				return SXERR_ABORT;` |
|        - |  882 | `			}` |
|        - |  883 | `			{` |
|      225 |  884 | `				char *z = zSrc;` |
|      225 |  885 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      225 |  886 | `				z += sizeof("$this->")-1;` |
|      225 |  887 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      225 |  888 | `				z += nName;` |
|      225 |  889 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      225 |  890 | `				z += sizeof(" = $")-1;` |
|      225 |  891 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      225 |  892 | `				z += nName;` |
|      225 |  893 | `				*z = 0;` |
|        - |  894 | `			}` |
|      225 |  895 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      225 |  896 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|      225 |  897 | `			pTmpIn = pGen->pIn;` |
|      225 |  898 | `			pTmpEnd = pGen->pEnd;` |
|      225 |  899 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      225 |  900 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      225 |  901 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      225 |  902 | `			pGen->pIn = pTmpIn;` |
|      225 |  903 | `			pGen->pEnd = pTmpEnd;` |
|      225 |  904 | `			SySetRelease(&sToken);` |
|      225 |  905 | `			if( rcPromote == SXERR_ABORT ){` |
|      ! 0 |  906 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  907 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  908 | `				return SXERR_ABORT;` |
|        - |  909 | `			}` |
|        - |  910 | `			/* Discard the assignment result — this is a statement expression. */` |
|      225 |  911 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      115 |  912 | `		}` |
|        - |  913 | `	}` |
|        - |  914 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|        - |  915 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|        - |  916 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|        - |  917 | `	 * generator — and vice versa — is classified independently. */` |
|        - |  918 | `	{` |
|   195907 |  919 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|        - |  920 | `		/* ...and so is the frameless-arguments mark: a closure written in the` |
|        - |  921 | `		 * arguments of a frameless call is a body of its own. */` |
|   195907 |  922 | `		int bSavedFl = pGen->bInFramelessNsArgs;` |
|   195907 |  923 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|   195907 |  924 | `		pGen->bInFramelessNsArgs = 0;` |
|        - |  925 | `		/* Compile the body */` |
|   195907 |  926 | `		PH7_CompileBlock(&(*pGen),0);` |
|   195907 |  927 | `		pGen->bInGenerator = bSavedGen;` |
|   195907 |  928 | `		pGen->bInFramelessNsArgs = bSavedFl;` |
|        - |  929 | `	}` |
|        - |  930 | `	/* Fix exception jumps now the destination is resolved */` |
|   195907 |  931 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - |  932 | `	/* Emit the final return if not yet done */` |
|   195907 |  933 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - |  934 | `	/* Fix gotos jumps now the destination is resolved */` |
|   195907 |  935 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|      ! 0 |  936 | `		rc = SXERR_ABORT;` |
|      ! 0 |  937 | `	}` |
|   195907 |  938 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|        - |  939 | `	/* Restore the default container */` |
|   195907 |  940 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - |  941 | `	/* Leave function block */` |
|   195907 |  942 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   195907 |  943 | `	if( rc == SXERR_ABORT ){` |
|        - |  944 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  945 | `		return SXERR_ABORT;` |
|        - |  946 | `	}` |
|        - |  947 | `	/* Scan for yield opcodes to detect generator functions */` |
|        - |  948 | `	{` |
|   195907 |  949 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|        - |  950 | `		sxu32 i;` |
| 16527077 |  951 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 16331695 |  952 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|      525 |  953 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      525 |  954 | `				break;` |
|        - |  955 | `			}` |
|  8154690 |  956 | `		}` |
|        - |  957 | `	}` |
|   195907 |  958 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  959 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|      525 |  960 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|      ! 0 |  961 | `			return SXERR_ABORT;` |
|        - |  962 | `		}` |
|      260 |  963 | `	}` |
|        - |  964 | `	/* This body is finished: the emit container has been restored above, so nothing` |
|        - |  965 | `	 * appends to it again, and the doubling slack it is holding -- up to as much as` |
|        - |  966 | `	 * it uses -- is dead for the rest of the process. The execution paths re-read` |
|        - |  967 | `	 * SySetBasePtr on every invocation, and the first invocation cannot precede this` |
|        - |  968 | `	 * point, so moving the buffer here is invisible to them. */` |
|   195907 |  969 | `	SySetShrinkToFit(&pFunc->aByteCode);` |
|        - |  970 | `	/* All done, function body compiled */` |
|   195907 |  971 | `	return SXRET_OK;` |
|    97818 |  972 | `}` |
|        - |  973 | `/*` |
|        - |  974 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|        - |  975 | ` * According to the PHP language reference manual.` |
|        - |  976 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|        - |  977 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|        - |  978 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|        - |  979 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - |  980 | ` *  Functions need not be defined before they are referenced.` |
|        - |  981 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|        - |  982 | ` *  a function even if they were defined inside and vice versa.` |
|        - |  983 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|        - |  984 | ` *  calls with over 32-64 recursion levels.` |
|        - |  985 | ` *` |
|        - |  986 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|        - |  987 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|        - |  988 | ` * on these extension.` |
|        - |  989 | ` */` |
|        - |  990 | `/*` |
|        - |  991 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|        - |  992 | ` */` |
|    78021 |  993 | `PH7_PRIVATE int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|        5 |  994 | `{` |
|        - |  995 | `	sxu32 i;` |
|   115136 |  996 | `	for( i = 0; i < n; i++ ){` |
|   105906 |  997 | `		int a = zA[i], b = zB[i];` |
|   105906 |  998 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|   105906 |  999 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|   105906 | 1000 | `		if( a != b ) return a - b;` |
|    18508 | 1001 | `	}` |
|     9235 | 1002 | `	return 0;` |
|    38951 | 1003 | `}` |
|        - | 1004 | `/*` |
|        - | 1005 | ` * Internal type-atom kinds used during union type parsing.` |
|        - | 1006 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|        - | 1007 | ` * (which are positive bit values stored in sxu32).` |
|        - | 1008 | ` */` |
|        - | 1009 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|        - | 1010 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|        - | 1011 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|        - | 1012 |  |
|        - | 1013 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|        - | 1014 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|        - | 1015 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|        - | 1016 |  |
|        - | 1017 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|        - | 1018 | `struct PhlTypeAtom {` |
|        - | 1019 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|        - | 1020 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|        - | 1021 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|        - | 1022 | `	sxu32 nCanon;` |
|        - | 1023 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|        - | 1024 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|        - | 1025 | `};` |
|        - | 1026 |  |
|        - | 1027 | `/*` |
|        - | 1028 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|        - | 1029 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|        - | 1030 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|        - | 1031 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|        - | 1032 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|        - | 1033 | ` * already be consumed by the caller.` |
|        - | 1034 | ` */` |
|        - | 1035 | `/*` |
|        - | 1036 | ` * TRUE if pName is a reserved PHP type keyword (never a class name), so a type` |
|        - | 1037 | ` * hint that spells it must not be namespace-qualified. Only the words that can` |
|        - | 1038 | ` * reach GenStateParseOneTypeAtom's identifier branch as a bare name matter here` |
|        - | 1039 | ` * (bool/int/float/string/array/object/self/static/parent arrive as keywords, and` |
|        - | 1040 | ` * null/void/never are matched before the class path), but the full set is listed` |
|        - | 1041 | ` * so the guard is robust to lexer changes.` |
|        - | 1042 | ` */` |
|    60493 | 1043 | `static int GenStateIsReservedTypeWord(const SyString *pName)` |
|        5 | 1044 | `{` |
|        - | 1045 | `	static const char *azWords[] = {` |
|        - | 1046 | `		"false","true","mixed","iterable","callable","null","void","never",` |
|        - | 1047 | `		"bool","boolean","int","integer","float","double","string","array",` |
|        - | 1048 | `		"object","self","static","parent"` |
|        - | 1049 | `	};` |
|        - | 1050 | `	sxu32 i;` |
|   109250 | 1051 | `	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){` |
|   108610 | 1052 | `		sxu32 n = (sxu32)SyStrlen(azWords[i]);` |
|   108610 | 1053 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azWords[i],n) == 0 ){` |
|    59858 | 1054 | `			return 1;` |
|        - | 1055 | `		}` |
|    24351 | 1056 | `	}` |
|      645 | 1057 | `	return 0;` |
|    30211 | 1058 | `}` |
|        - | 1059 | `/*` |
|        - | 1060 | ` * php's builtin type words as looked up BY NAME (zend_lookup_builtin_type_by_name):` |
|        - | 1061 | ``  * every type a declaration can spell with a plain label. `array` and `callable` `` |
|        - | 1062 | ` * are deliberately absent — php's parser hands those two their own tokens — which` |
|        - | 1063 | ``  * is why a qualified `\array` takes the "reserved" wording below while `\int` `` |
|        - | 1064 | ` * takes "must be unqualified".` |
|        - | 1065 | ` */` |
|      148 | 1066 | `static int GenStateIsBuiltinTypeWord(const SyString *pName)` |
|        5 | 1067 | `{` |
|        - | 1068 | `	static const char *azWords[] = {` |
|        - | 1069 | `		"int","float","string","bool","void","iterable","object","mixed",` |
|        - | 1070 | `		"never","null","false","true"` |
|        - | 1071 | `	};` |
|        - | 1072 | `	sxu32 i;` |
|     1881 | 1073 | `	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){` |
|     1737 | 1074 | `		sxu32 n = (sxu32)SyStrlen(azWords[i]);` |
|     1737 | 1075 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azWords[i],n) == 0 ){` |
|        6 | 1076 | `			return 1;` |
|        - | 1077 | `		}` |
|      868 | 1078 | `	}` |
|      148 | 1079 | `	return 0;` |
|       79 | 1080 | `}` |
|        - | 1081 | `/* The three class keywords that name a class RELATIVE to the current one. php` |
|        - | 1082 | ` * refuses to see them behind a qualifier at all, with its own wording. */` |
|      144 | 1083 | `static int GenStateIsRelativeClassWord(const SyString *pName)` |
|        4 | 1084 | `{` |
|      151 | 1085 | `	return (pName->nByte == 4 && SyStrnicmp(pName->zString,"self",4) == 0)` |
|      143 | 1086 | `	    \|\| (pName->nByte == 6 && SyStrnicmp(pName->zString,"parent",6) == 0)` |
|      216 | 1087 | `	    \|\| (pName->nByte == 6 && SyStrnicmp(pName->zString,"static",6) == 0);` |
|        4 | 1088 | `}` |
|        - | 1089 | `/*` |
|        - | 1090 | ` * php's reserved-name screens on a class-like type atom, in php's own order.` |
|        - | 1091 | ` * A name is screened only once a qualifier is present — a BARE reserved word is` |
|        - | 1092 | ` * a type, and the keyword / null-void-never branches above have already claimed` |
|        - | 1093 | ` * every one of them.` |
|        - | 1094 | ` *` |
|        - | 1095 | ` *   \int, namespace\false   -> "Type declaration 'int' must be unqualified"` |
|        - | 1096 | ` *   \self, \parent, \static -> "'\self' is an invalid class name"` |
|        - | 1097 | ` *   \array, \callable, A\int, A\self` |
|        - | 1098 | ` *                           -> "Cannot use \"NAME\" as a type name as it is` |
|        - | 1099 | ` *                              reserved", NAME being the name after resolution` |
|        - | 1100 | ` *` |
|        - | 1101 | ` * pLastSeg is the atom's trailing segment AS WRITTEN (php's screen looks only at` |
|        - | 1102 | ` * that), pResolved the whole atom after namespace resolution.` |
|        - | 1103 | ` */` |
|   463256 | 1104 | `static sxi32 GenStateScreenTypeName(` |
|        - | 1105 | `	ph7_gen_state *pGen,` |
|        - | 1106 | `	const SyString *pLastSeg,  /* trailing segment, as written */` |
|        - | 1107 | `	const SyString *pResolved, /* whole atom, after resolution */` |
|        - | 1108 | `	int bMulti,                /* the written name had more than one segment */` |
|        - | 1109 | ``	int bFQ,                   /* written absolute (`\X`) or `namespace\X` */`` |
|        - | 1110 | `	sxu32 nLine` |
|        5 | 1111 | `){` |
|        - | 1112 | `	sxi32 rc;` |
|   463261 | 1113 | `	if( !bMulti && !bFQ ){` |
|   463147 | 1114 | `		return SXRET_OK;` |
|        - | 1115 | `	}` |
|      119 | 1116 | `	if( !bMulti ){` |
|       43 | 1117 | `		if( GenStateIsBuiltinTypeWord(pLastSeg) ){` |
|        - | 1118 | `			char zLower[16];` |
|        - | 1119 | `			sxu32 i;` |
|        9 | 1120 | `			for( i = 0 ; i < pLastSeg->nByte && i < sizeof(zLower) ; i++ ){` |
|        7 | 1121 | `				unsigned char c = (unsigned char)pLastSeg->zString[i];` |
|        7 | 1122 | `				zLower[i] = (char)((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);` |
|        4 | 1123 | `			}` |
|        4 | 1124 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1125 | `				"Type declaration '%.*s' must be unqualified",(int)i,zLower);` |
|        3 | 1126 | `			return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1127 | `		}` |
|       40 | 1128 | `		if( GenStateIsRelativeClassWord(pLastSeg) ){` |
|        4 | 1129 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1130 | `				"'\\%z' is an invalid class name",pLastSeg);` |
|        3 | 1131 | `			return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1132 | `		}` |
|       17 | 1133 | `	}` |
|      110 | 1134 | `	if( GenStateIsBuiltinTypeWord(pLastSeg) \|\| GenStateIsRelativeClassWord(pLastSeg)` |
|      108 | 1135 | `	 \|\| (pLastSeg->nByte == 5 && SyStrnicmp(pLastSeg->zString,"array",5) == 0)` |
|      112 | 1136 | `	 \|\| (pLastSeg->nByte == 8 && SyStrnicmp(pLastSeg->zString,"callable",8) == 0) ){` |
|        4 | 1137 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1138 | `			"Cannot use \"%z\" as a type name as it is reserved",pResolved);` |
|        3 | 1139 | `		return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1140 | `	}` |
|      112 | 1141 | `	return SXRET_OK;` |
|   231315 | 1142 | `}` |
|   463260 | 1143 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|        5 | 1144 | `{` |
|   463265 | 1145 | `	SyToken *pIn = pGen->pIn;` |
|        - | 1146 | `	SyString sLastSeg;      /* trailing segment of the atom, as written */` |
|        - | 1147 | `	sxu32 nLine;` |
|   463265 | 1148 | `	int bAbsolute = 0;` |
|        - | 1149 | `	int bQualified;` |
|   463265 | 1150 | `	int bMulti = 0;         /* the written name had more than one segment */` |
|   463265 | 1151 | ``	int bFQ = 0;            /* written absolute or `namespace\`-relative */`` |
|        - | 1152 | `	sxi32 rcScreen;` |
|   463265 | 1153 | `	SyStringInitFromBuf(&sLastSeg, 0, 0);` |
|   463265 | 1154 | `	SyZero(pOut, sizeof(*pOut));` |
|   463265 | 1155 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   463265 | 1156 | `	if( pIn >= pGen->pEnd ){` |
|      ! 0 | 1157 | `		return SXERR_SYNTAX;` |
|        - | 1158 | `	}` |
|   463265 | 1159 | `	nLine = pIn->nLine;` |
|        - | 1160 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   463265 | 1161 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       45 | 1162 | `		bAbsolute = bFQ = 1; /* fully-qualified: never prefix the current namespace */` |
|       45 | 1163 | `		pIn++;` |
|       45 | 1164 | `		if( pIn >= pGen->pEnd ){` |
|      ! 0 | 1165 | `			return SXERR_SYNTAX;` |
|        - | 1166 | `		}` |
|       20 | 1167 | `	}` |
|        - | 1168 | ``	/* `namespace\X` type hint: the CURRENT namespace spelled out, fully qualified`` |
|        - | 1169 | `` 	 * from there. Collected here rather than below because the leading `namespace` `` |
|        - | 1170 | `	 * is a KEYWORD token, which the atom parser would otherwise reject outright. */` |
|   463265 | 1171 | `	if( !bAbsolute ){` |
|        - | 1172 | `		SyBlob sRel;` |
|   463225 | 1173 | `		SyBlobInit(&sRel,&pGen->pVm->sAllocator);` |
|   463225 | 1174 | `		if( GenStateNsRelPrefix(pGen,&pIn,pGen->pEnd,&sRel) ){` |
|        - | 1175 | `			char *zDup;` |
|        7 | 1176 | `			bFQ = 1;` |
|        7 | 1177 | `			SyBlobAppend(&sRel,pIn->sData.zString,pIn->sData.nByte);` |
|        7 | 1178 | `			sLastSeg = pIn->sData;` |
|        7 | 1179 | `			pIn++;` |
|        9 | 1180 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|        7 | 1181 | `				&& (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      ! 0 | 1182 | `				SyBlobAppend(&sRel,"\\",1);` |
|      ! 0 | 1183 | `				SyBlobAppend(&sRel,pIn[1].sData.zString,pIn[1].sData.nByte);` |
|      ! 0 | 1184 | `				sLastSeg = pIn[1].sData;` |
|      ! 0 | 1185 | `				bMulti = 1;` |
|      ! 0 | 1186 | `				pIn += 2;` |
|      ! 0 | 1187 | `			}` |
|       10 | 1188 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        6 | 1189 | `				(const char *)SyBlobData(&sRel),SyBlobLength(&sRel));` |
|        7 | 1190 | `			if( zDup == 0 ){` |
|      ! 0 | 1191 | `				SyBlobRelease(&sRel);` |
|      ! 0 | 1192 | `				return SXERR_ABORT;` |
|        - | 1193 | `			}` |
|        7 | 1194 | `			pOut->nType = SXU32_HIGH;` |
|        7 | 1195 | `			SyStringInitFromBuf(&pOut->sClass,zDup,SyBlobLength(&sRel));` |
|        7 | 1196 | `			SyBlobRelease(&sRel);` |
|        7 | 1197 | `			rcScreen = GenStateScreenTypeName(pGen,&sLastSeg,&pOut->sClass,bMulti,bFQ,nLine);` |
|        7 | 1198 | `			if( rcScreen != SXRET_OK ){` |
|      ! 0 | 1199 | `				return rcScreen;` |
|        - | 1200 | `			}` |
|        7 | 1201 | `			pGen->pIn = pIn;` |
|        7 | 1202 | `			return SXRET_OK;` |
|        - | 1203 | `		}` |
|   463219 | 1204 | `		SyBlobRelease(&sRel);` |
|   231289 | 1205 | `	}` |
|   463259 | 1206 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        2 | 1207 | `		return SXERR_SYNTAX;` |
|        - | 1208 | `	}` |
|        - | 1209 | ``	/* php's lexer matches `{LABEL}("\\"{LABEL})+` as ONE T_NAME_QUALIFIED token`` |
|        - | 1210 | `	 * BEFORE it ever looks a label up in the keyword table, so every reserved word` |
|        - | 1211 | ``	 * is a legal SEGMENT of a qualified name: `Default\Q`, `A\Default`, even`` |
|        - | 1212 | ``	 * `static\Q` and `array\Q` are names, and only a BARE reserved word is a`` |
|        - | 1213 | ``	 * keyword. A `\` on either side is therefore what decides, so a keyword the`` |
|        - | 1214 | `	 * separator follows takes the class-name path below rather than the scalar` |
|        - | 1215 | ``	 * keyword branch (`Default\Q $x` used to be a syntax error). The `\` has to be`` |
|        - | 1216 | ``	 * GLUED, exactly as php's lexer requires: `private \Q $x` is still a modifier`` |
|        - | 1217 | `	 * and a type, and reading it as a name broke every promoted property. The` |
|        - | 1218 | `	 * TRAILING segments below stay loose about spacing, as every other name` |
|        - | 1219 | `	 * collector in the compiler is — there a stray space only widens what invalid` |
|        - | 1220 | `	 * source is accepted, it never re-reads valid source as something else. */` |
|   463295 | 1221 | `	bQualified = ( pIn + 1 < pGen->pEnd && (pIn[1].nType & PH7_TK_NSSEP)` |
|   695196 | 1222 | `		&& GenStateTokensGlued(pIn,&pIn[1]) );` |
|   463257 | 1223 | `	sLastSeg = pIn->sData;` |
|   463257 | 1224 | `	if( (pIn->nType & PH7_TK_KEYWORD) && !bQualified ){` |
|   393440 | 1225 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   393440 | 1226 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|    76369 | 1227 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   355208 | 1228 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|    59475 | 1229 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   287301 | 1230 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|    69799 | 1231 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   222663 | 1232 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   136610 | 1233 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   119414 | 1234 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|    50899 | 1235 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|    25727 | 1236 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|       67 | 1237 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      282 | 1238 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|      102 | 1239 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|      249 | 1240 | `			pOut->nType = SXU32_HIGH;` |
|      249 | 1241 | `			pOut->sClass = pIn->sData;` |
|      127 | 1242 | `		}else{` |
|        3 | 1243 | `			return SXERR_SYNTAX;` |
|        - | 1244 | `		}` |
|   393438 | 1245 | `		pIn++;` |
|   196457 | 1246 | `	}else{` |
|        - | 1247 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|        - | 1248 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    69822 | 1249 | `		SyString *pT = &pIn->sData;` |
|    69822 | 1250 | `		if( !bQualified && pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|       57 | 1251 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|       57 | 1252 | `			pIn++;` |
|    69796 | 1253 | `		}else if( !bQualified && pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     9139 | 1254 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     9139 | 1255 | `			pIn++;` |
|    65190 | 1256 | `		}else if( !bQualified && pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       39 | 1257 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       39 | 1258 | `			pIn++;` |
|       22 | 1259 | `		}else{` |
|        - | 1260 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    60602 | 1261 | `			SyToken *pFirst = pIn;` |
|    60602 | 1262 | `			SyToken *pLast = pIn;` |
|    60602 | 1263 | `			pOut->nType = SXU32_HIGH;` |
|    60602 | 1264 | `			pOut->sClass = pIn->sData;` |
|    60602 | 1265 | `			pIn++;` |
|    91052 | 1266 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    60718 | 1267 | `				&& (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       81 | 1268 | `				pLast = &pIn[1];` |
|       81 | 1269 | `				pIn += 2;` |
|        3 | 1270 | `			}` |
|    60602 | 1271 | `			if( pLast != pFirst ){` |
|       79 | 1272 | `				const char *zFirst = pFirst->sData.zString;` |
|       79 | 1273 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       79 | 1274 | `				pOut->sClass.zString = zFirst;` |
|       79 | 1275 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       79 | 1276 | `				sLastSeg = pLast->sData;` |
|       79 | 1277 | `				bMulti = 1;` |
|       38 | 1278 | `			}` |
|        - | 1279 | `` 			/* Namespace-qualify a non-absolute class type so a `: Base` / `Base $x` `` |
|        - | 1280 | ``			 * hint in namespace N resolves to N\Base (or a `use` alias) at`` |
|        - | 1281 | `			 * type-check time instead of the global \Base — mirrors the` |
|        - | 1282 | ``			 * NEW/CALL/instanceof qualification. A QUALIFIED `A\B` hint resolves`` |
|        - | 1283 | ``			 * exactly the way `new A\B` does: GenStateResolveName maps the LEADING`` |
|        - | 1284 | `			 * segment through the imports and keeps the tail, else prepends the` |
|        - | 1285 | `			 * current namespace. Only absolute (\Base) names stay as written.` |
|        - | 1286 | ``			 * Qualified hints used to be stored AS WRITTEN, so `: Sub\Thing` inside`` |
|        - | 1287 | ``			 * `namespace App;` compared against the literal `Sub\Thing` while the`` |
|        - | 1288 | ``			 * value's class was `App\Sub\Thing` — every qualified type face in a`` |
|        - | 1289 | `			 * namespaced tree raised a TypeError. */` |
|        - | 1290 | `			/* Reserved type words that reach this identifier branch (false, true,` |
|        - | 1291 | `			 * mixed, iterable, callable) are NOT classes and must not be qualified` |
|        - | 1292 | ``			 * (else `false\|string` becomes `Ns\false\|string`); they are always`` |
|        - | 1293 | `			 * single-segment, so a qualified name never consults the list. */` |
|    60597 | 1294 | `			if( !bAbsolute` |
|    60584 | 1295 | `			 && !(pLast == pFirst && GenStateIsReservedTypeWord(&pOut->sClass)) ){` |
|        - | 1296 | `				SyBlob sFqn;` |
|      713 | 1297 | `				SyBlobInit(&sFqn,&pGen->pVm->sAllocator);` |
|      713 | 1298 | `				GenStateResolveName(pGen,&pOut->sClass,&sFqn);` |
|      708 | 1299 | `				if( SyBlobLength(&sFqn) != pOut->sClass.nByte` |
|      684 | 1300 | `				 \|\| SyMemcmp(SyBlobData(&sFqn),(const void *)pOut->sClass.zString,pOut->sClass.nByte) != 0 ){` |
|       91 | 1301 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       58 | 1302 | `						(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));` |
|       62 | 1303 | `					if( zDup ){` |
|       62 | 1304 | `						SyStringInitFromBuf(&pOut->sClass,zDup,SyBlobLength(&sFqn));` |
|       29 | 1305 | `					}` |
|       29 | 1306 | `				}` |
|      713 | 1307 | `				SyBlobRelease(&sFqn);` |
|      354 | 1308 | `			}` |
|        - | 1309 | `		}` |
|        - | 1310 | `	}` |
|        - | 1311 | `	/* php screens the name only once a qualifier is in play; a scalar atom that` |
|        - | 1312 | ``	 * got here bare (`int`, `array`) carries no sClass, so the written word is`` |
|        - | 1313 | `	 * both the segment and the resolved name. */` |
|   694562 | 1314 | `	rcScreen = GenStateScreenTypeName(pGen,&sLastSeg,` |
|   463250 | 1315 | `		pOut->nType == SXU32_HIGH ? &pOut->sClass : &sLastSeg,bMulti,bFQ,nLine);` |
|   463255 | 1316 | `	if( rcScreen != SXRET_OK ){` |
|        9 | 1317 | `		return rcScreen;` |
|        - | 1318 | `	}` |
|   463249 | 1319 | `	pGen->pIn = pIn;` |
|   463249 | 1320 | `	return SXRET_OK;` |
|   231317 | 1321 | `}` |
|        - | 1322 |  |
|        - | 1323 | `/* Class-name ATOMS that php files with the BUILT-IN types rather than the` |
|        - | 1324 | `` * classes. `callable`, `false` and `true` are type-mask bits in zend, so they`` |
|        - | 1325 | `` * print AFTER every class name however they were written (`false\|A1` reads`` |
|        - | 1326 | `` * `A1\|false`). `iterable` is two types at once: the class `Traversable`, which`` |
|        - | 1327 | `` * keeps the atom's declaration position among the classes, plus `array`, which`` |
|        - | 1328 | `` * prints with the builtins (`iterable\|A1` reads `Traversable\|A1\|array`). PH7`` |
|        - | 1329 | ` * parses all four as class-name atoms, which is why they used to sort with the` |
|        - | 1330 | ` * classes. */` |
|        - | 1331 | `#define GEN_ATOM_PLAIN    0` |
|        - | 1332 | `#define GEN_ATOM_CALLABLE 1` |
|        - | 1333 | `#define GEN_ATOM_FALSE    2` |
|        - | 1334 | `#define GEN_ATOM_TRUE     3` |
|        - | 1335 | `#define GEN_ATOM_ITERABLE 4` |
|  1818519 | 1336 | `static int GenAtomMaskKind(const PhlTypeAtom *pAtom)` |
|        5 | 1337 | `{` |
|  1818524 | 1338 | `	const SyString *p = &pAtom->sClass;` |
|  1818524 | 1339 | `	if( pAtom->nType != SXU32_HIGH \|\| p->zString == 0 ){` |
|  1532061 | 1340 | `		return GEN_ATOM_PLAIN;` |
|        - | 1341 | `	}` |
|   286468 | 1342 | `	if( p->nByte == 8 && SyStrnicmp(p->zString,"callable",8) == 0 ) return GEN_ATOM_CALLABLE;` |
|   284720 | 1343 | `	if( p->nByte == 5 && SyStrnicmp(p->zString,"false",5) == 0 )    return GEN_ATOM_FALSE;` |
|    90303 | 1344 | `	if( p->nByte == 4 && SyStrnicmp(p->zString,"true",4) == 0 )     return GEN_ATOM_TRUE;` |
|    90233 | 1345 | `	if( p->nByte == 8 && SyStrnicmp(p->zString,"iterable",8) == 0 ) return GEN_ATOM_ITERABLE;` |
|    89885 | 1346 | `	return GEN_ATOM_PLAIN;` |
|   908013 | 1347 | `}` |
|        - | 1348 | ``/* Emit one class-like atom: when *bExpandIterable*, `iterable` contributes only`` |
|        - | 1349 | `` * its Traversable half here (the `array` half rides the built-in pass). php`` |
|        - | 1350 | `` * expands it only in a COMPOUND type — a standalone `iterable`/`?iterable` keeps`` |
|        - | 1351 | ` * its name in the canonical text (which is what Reflection prints; the TypeError` |
|        - | 1352 | ` * for a standalone hint is rendered separately, by VmClassHintTypeName). */` |
|    18076 | 1353 | `static void GenAppendClassAtom(SyBlob *pBlob, const PhlTypeAtom *pAtom, int bExpandIterable)` |
|        5 | 1354 | `{` |
|    18081 | 1355 | `	if( bExpandIterable && GenAtomMaskKind(pAtom) == GEN_ATOM_ITERABLE ){` |
|       21 | 1356 | `		SyBlobAppend(pBlob, "Traversable", sizeof("Traversable")-1);` |
|       21 | 1357 | `		return;` |
|        - | 1358 | `	}` |
|    18063 | 1359 | `	SyBlobAppend(pBlob, pAtom->sClass.zString, pAtom->sClass.nByte);` |
|     9032 | 1360 | `}` |
|        - | 1361 | `/*` |
|        - | 1362 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|        - | 1363 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|        - | 1364 | ` *   classes (in declaration order)` |
|        - | 1365 | ` *   \| callable \| object \| array \| string \| int \| float \| bool \| false \| true` |
|        - | 1366 | ` *   [\| null]` |
|        - | 1367 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|        - | 1368 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|        - | 1369 | ` */` |
|   420575 | 1370 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|        5 | 1371 | `{` |
|        - | 1372 | `	int i;` |
|   420580 | 1373 | `	int nNonNull = 0;` |
|   420580 | 1374 | `	int bAnyIntersection = 0;` |
|        - | 1375 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|   420580 | 1376 | `	sxu32 nMaxGroup = 0;` |
| 13878980 | 1377 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   883794 | 1378 | `	for( i = 0; i < nAtoms; i++ ){` |
|   463219 | 1379 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   463167 | 1380 | `			nNonNull++;` |
|   463167 | 1381 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|   463167 | 1382 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|   463167 | 1383 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|   231263 | 1384 | `			}` |
|   231263 | 1385 | `		}` |
|   231294 | 1386 | `	}` |
|   883698 | 1387 | `	for( i = 0; i < nAtoms; i++ ){` |
|   463165 | 1388 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       47 | 1389 | `			bAnyIntersection = 1;` |
|       47 | 1390 | `			break;` |
|        - | 1391 | `		}` |
|   231246 | 1392 | `	}` |
|   420580 | 1393 | `	if( bAnyIntersection ){` |
|        - | 1394 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|        - | 1395 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|        - | 1396 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|       47 | 1397 | `		sxu32 g, nGroups = 0;` |
|       47 | 1398 | `		int bFirstGroup = 1;` |
|      101 | 1399 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|      101 | 1400 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|       59 | 1401 | `			int bFirstMember = 1;` |
|        - | 1402 | `			int bWrap;` |
|       59 | 1403 | `			if( aGroupCount[g] == 0 ) continue;` |
|        - | 1404 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|        - | 1405 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|        - | 1406 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|        - | 1407 | `			 * parens, matching PHP's canonical text. */` |
|       80 | 1408 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|       59 | 1409 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|       59 | 1410 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      193 | 1411 | `			for( i = 0; i < nAtoms; i++ ){` |
|      139 | 1412 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|      101 | 1413 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|      101 | 1414 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       95 | 1415 | `					GenAppendClassAtom(pBlob, &aAtoms[i], 1);` |
|       50 | 1416 | `				}else{` |
|        8 | 1417 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - | 1418 | `				}` |
|      101 | 1419 | `				bFirstMember = 0;` |
|       53 | 1420 | `			}` |
|       59 | 1421 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|       59 | 1422 | `			bFirstGroup = 0;` |
|       32 | 1423 | `		}` |
|        - | 1424 | ``		/* `iterable` printed its Traversable half in place; its `array` half goes`` |
|        - | 1425 | ``		 * last, as php does (`(I1&I2)\|iterable` reads `(I1&I2)\|Traversable\|array`). */`` |
|      143 | 1426 | `		for( i = 0; i < nAtoms; i++ ){` |
|      103 | 1427 | `			if( GenAtomMaskKind(&aAtoms[i]) == GEN_ATOM_ITERABLE ){` |
|        3 | 1428 | `				SyBlobAppend(pBlob, "\|", 1);` |
|        3 | 1429 | `				SyBlobAppend(pBlob, "array", sizeof("array")-1);` |
|        3 | 1430 | `				break;` |
|        - | 1431 | `			}` |
|       53 | 1432 | `		}` |
|       47 | 1433 | `		if( bNullable ){` |
|        3 | 1434 | `			SyBlobAppend(pBlob, "\|", 1);` |
|        3 | 1435 | `			SyBlobAppend(pBlob, "null", 4);` |
|        1 | 1436 | `		}` |
|      231 | 1437 | `		return;` |
|        - | 1438 | `	}` |
|   420538 | 1439 | `	if( nNonNull == 1 && bNullable ){` |
|        - | 1440 | `		/* Shorthand: ?T */` |
|      373 | 1441 | `		for( i = 0; i < nAtoms; i++ ){` |
|      373 | 1442 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      373 | 1443 | `			SyBlobAppend(pBlob, "?", 1);` |
|      373 | 1444 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      137 | 1445 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       71 | 1446 | `			}else{` |
|      241 | 1447 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - | 1448 | `			}` |
|      373 | 1449 | `			return;` |
|      ! 0 | 1450 | `		}` |
|      ! 0 | 1451 | `	}` |
|        - | 1452 | `	{` |
|   420170 | 1453 | `		int bFirst = 1;` |
|        - | 1454 | `		/* 1) Classes in declaration order — minus the class-name atoms php counts` |
|        - | 1455 | ``		 * as built-in types (see GenAtomMaskKind); `iterable` leaves Traversable. */`` |
|   882890 | 1456 | `		for( i = 0; i < nAtoms; i++ ){` |
|        - | 1457 | `			int nKind;` |
|   462725 | 1458 | `			if( aAtoms[i].nType != SXU32_HIGH ) continue;` |
|    60616 | 1459 | `			nKind = GenAtomMaskKind(&aAtoms[i]);` |
|    60616 | 1460 | `			if( nKind == GEN_ATOM_CALLABLE \|\| nKind == GEN_ATOM_FALSE \|\| nKind == GEN_ATOM_TRUE ){` |
|    42630 | 1461 | `				continue;` |
|        - | 1462 | `			}` |
|    17991 | 1463 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    17991 | 1464 | `			GenAppendClassAtom(pBlob, &aAtoms[i], nNonNull > 1);` |
|    17991 | 1465 | `			bFirst = 0;` |
|     8987 | 1466 | `		}` |
|        - | 1467 | `		/* 2) Built-ins in php's canonical order. A slot is filled either by a` |
|        - | 1468 | `		 * plain atom of that MEMOBJ_* type or by a class-name atom of the matching` |
|        - | 1469 | ``		 * kind; the `array` slot also takes `iterable`'s second half. */`` |
|        - | 1470 | `		{` |
|        - | 1471 | `			static const struct {` |
|        - | 1472 | `				sxu32 nType;        /* plain atom type, 0 when kind-only */` |
|        - | 1473 | `				int nKind;          /* GEN_ATOM_* atom, GEN_ATOM_PLAIN when type-only */` |
|        - | 1474 | `				const char *zText;` |
|        - | 1475 | `				sxu32 nText;` |
|        - | 1476 | `			} aOrder[] = {` |
|        - | 1477 | `				{ 0,               GEN_ATOM_CALLABLE, "callable", sizeof("callable")-1 },` |
|        - | 1478 | `				{ MEMOBJ_OBJ,      GEN_ATOM_PLAIN,    "object",   sizeof("object")-1 },` |
|        - | 1479 | `				{ MEMOBJ_HASHMAP,  GEN_ATOM_ITERABLE, "array",    sizeof("array")-1 },` |
|        - | 1480 | `				{ MEMOBJ_STRING,   GEN_ATOM_PLAIN,    "string",   sizeof("string")-1 },` |
|        - | 1481 | `				{ MEMOBJ_INT,      GEN_ATOM_PLAIN,    "int",      sizeof("int")-1 },` |
|        - | 1482 | `				{ MEMOBJ_REAL,     GEN_ATOM_PLAIN,    "float",    sizeof("float")-1 },` |
|        - | 1483 | `				{ MEMOBJ_BOOL,     GEN_ATOM_PLAIN,    "bool",     sizeof("bool")-1 },` |
|        - | 1484 | `				{ 0,               GEN_ATOM_FALSE,    "false",    sizeof("false")-1 },` |
|        - | 1485 | `				{ 0,               GEN_ATOM_TRUE,     "true",     sizeof("true")-1 },` |
|        - | 1486 | ``				/* `void` and `never` are RETURN-only and may not share a type with`` |
|        - | 1487 | `				 * anything, so their place in the order is never observable -- what` |
|        - | 1488 | `				 * matters is that they have one. Missing here, the canonical text of` |
|        - | 1489 | ``				 * `: void` came out EMPTY, and every reader of it had to name the two`` |
|        - | 1490 | `				 * back from nReturnType (Reflection's getReturnType did, the generator` |
|        - | 1491 | `				 * fatal did, and the declaration renderer would have had to). */` |
|        - | 1492 | `				{ UTA_VOID_FLAG,   GEN_ATOM_PLAIN,    "void",     sizeof("void")-1 },` |
|        - | 1493 | `				{ UTA_NEVER_FLAG,  GEN_ATOM_PLAIN,    "never",    sizeof("never")-1 }` |
|        - | 1494 | `			};` |
|        - | 1495 | `			int k;` |
|  5041985 | 1496 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  9224489 | 1497 | `				for( i = 0; i < nAtoms; i++ ){` |
|  6671845 | 1498 | `					int bHit = (aOrder[k].nType != 0 && aAtoms[i].nType == aOrder[k].nType)` |
|  8637135 | 1499 | `						\|\| (aOrder[k].nKind != GEN_ATOM_PLAIN` |
|  3199467 | 1500 | `						    && GenAtomMaskKind(&aAtoms[i]) == aOrder[k].nKind` |
|   898958 | 1501 | `						    && (aOrder[k].nKind != GEN_ATOM_ITERABLE \|\| nNonNull > 1));` |
|  5047402 | 1502 | `					if( !bHit ) continue;` |
|   444733 | 1503 | `					if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   444733 | 1504 | `					SyBlobAppend(pBlob, aOrder[k].zText, aOrder[k].nText);` |
|   444733 | 1505 | `					bFirst = 0;` |
|   444733 | 1506 | `					break;` |
|      ! 0 | 1507 | `				}` |
|  2307717 | 1508 | `			}` |
|        - | 1509 | `		}` |
|        - | 1510 | `		/* 3) null suffix */` |
|   420170 | 1511 | `		if( bNullable ){` |
|       27 | 1512 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|       27 | 1513 | `			SyBlobAppend(pBlob, "null", 4);` |
|       11 | 1514 | `		}` |
|        - | 1515 | `	}` |
|   210002 | 1516 | `}` |
|        - | 1517 |  |
|        - | 1518 | `/*` |
|        - | 1519 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|        - | 1520 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|        - | 1521 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|        - | 1522 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|        - | 1523 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|        - | 1524 | ` * whether it was parenthesized.` |
|        - | 1525 | ` *` |
|        - | 1526 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|        - | 1527 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|        - | 1528 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|        - | 1529 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|        - | 1530 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|        - | 1531 | ` */` |
|   463214 | 1532 | `static sxi32 GenStateParsePart(` |
|        - | 1533 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|        - | 1534 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|        5 | 1535 | `{` |
|        - | 1536 | `	sxi32 rc;` |
|   463219 | 1537 | `	int nMembers = 0;` |
|   463219 | 1538 | `	int bParen = 0;` |
|   463219 | 1539 | `	*pnMembers = 0;` |
|   463219 | 1540 | `	*pbParen = 0;` |
|   463219 | 1541 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       18 | 1542 | `		bParen = 1;` |
|       18 | 1543 | `		pGen->pIn++; /* skip '(' */` |
|        7 | 1544 | `	}` |
|   231289 | 1545 | `	for(;;){` |
|   463265 | 1546 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|      ! 0 | 1547 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1548 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|      ! 0 | 1549 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1550 | `		}` |
|   463265 | 1551 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|   463265 | 1552 | `		if( rc != SXRET_OK ){` |
|       13 | 1553 | `			return rc;` |
|        - | 1554 | `		}` |
|   463255 | 1555 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|   463255 | 1556 | `		(*pnAtoms)++;` |
|   463255 | 1557 | `		nMembers++;` |
|        - | 1558 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|   463255 | 1559 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       93 | 1560 | `			SyToken *pNext = &pGen->pIn[1];` |
|       88 | 1561 | `			if( pNext < pGen->pEnd` |
|       93 | 1562 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       51 | 1563 | `				pGen->pIn++; /* skip '&' */` |
|       51 | 1564 | `				continue;` |
|        - | 1565 | `			}` |
|       21 | 1566 | `		}` |
|   463209 | 1567 | `		break;` |
|      ! 0 | 1568 | `	}` |
|   463209 | 1569 | `	if( bParen ){` |
|       18 | 1570 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 1571 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1572 | `				"Malformed DNF type: expecting ')'");` |
|      ! 0 | 1573 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1574 | `		}` |
|       18 | 1575 | `		pGen->pIn++; /* skip ')' */` |
|       18 | 1576 | `		if( nMembers < 2 ){` |
|      ! 0 | 1577 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1578 | `				"Parenthesized type must be an intersection of at least two types");` |
|      ! 0 | 1579 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1580 | `		}` |
|        7 | 1581 | `	}` |
|   463209 | 1582 | `	*pnMembers = nMembers;` |
|   463209 | 1583 | `	*pbParen = bParen;` |
|   463209 | 1584 | `	return SXRET_OK;` |
|   231294 | 1585 | `}` |
|        - | 1586 |  |
|        - | 1587 | `/*` |
|        - | 1588 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|        - | 1589 | ` *` |
|        - | 1590 | ` * Outputs:` |
|        - | 1591 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|        - | 1592 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|        - | 1593 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|        - | 1594 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|        - | 1595 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|        - | 1596 | ` *     already be initialized by the caller (allocator set, etc).` |
|        - | 1597 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|        - | 1598 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|        - | 1599 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|        - | 1600 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|        - | 1601 | ` *` |
|        - | 1602 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|        - | 1603 | ` * SXERR_ABORT on fatal compile errors.` |
|        - | 1604 | ` */` |
|   420609 | 1605 | `PH7_PRIVATE sxi32 GenStateParseUnionTypeDecl(` |
|        - | 1606 | `	ph7_gen_state *pGen,` |
|        - | 1607 | `	sxu32 *pnType,` |
|        - | 1608 | `	SyString *pClass,` |
|        - | 1609 | `	SySet *pAlts,` |
|        - | 1610 | `	sxi32 *piTypeFlags,` |
|        - | 1611 | `	SyString *pTypeText,` |
|        - | 1612 | `	int iNullableFlag,` |
|        - | 1613 | `	int iUnionFlag,` |
|        - | 1614 | `	int bAllowVoid,` |
|        - | 1615 | ``	int bParamCtx,   /* this type is a PARAMETER's: `static` is not one in php's grammar */`` |
|        - | 1616 | `	sxu32 nLine` |
|        5 | 1617 | `){` |
|        - | 1618 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   420614 | 1619 | `	int nAtoms = 0;` |
|   420614 | 1620 | `	int bShortNullable = 0;` |
|   420614 | 1621 | `	int bExplicitNull = 0;` |
|        - | 1622 | `	sxi32 rc;` |
|   420614 | 1623 | `	*pnType = 0;` |
|   420614 | 1624 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   420614 | 1625 | `	*piTypeFlags = 0;` |
|   420614 | 1626 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|        - | 1627 |  |
|   420614 | 1628 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 1629 | `		return SXRET_OK;` |
|        - | 1630 | `	}` |
|        - | 1631 | ``	/* Optional `?` shorthand prefix */`` |
|   420609 | 1632 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      349 | 1633 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      349 | 1634 | `		bShortNullable = 1;` |
|      349 | 1635 | `		pGen->pIn++;` |
|      349 | 1636 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 1637 | `			return SXERR_SYNTAX;` |
|        - | 1638 | `		}` |
|      172 | 1639 | `	}` |
|        - | 1640 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|        - | 1641 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|        - | 1642 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|        - | 1643 | `	{` |
|        - | 1644 | `		int nMembers, bParen;` |
|   420614 | 1645 | `		sxu32 iGroup = 0;` |
|   420614 | 1646 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|   420614 | 1647 | `		if( rc != SXRET_OK ){` |
|       14 | 1648 | `			return rc;` |
|        - | 1649 | `		}` |
|        - | 1650 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|        - | 1651 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|        - | 1652 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|        - | 1653 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|        - | 1654 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|   695091 | 1655 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   484561 | 1656 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|    42612 | 1657 | `			if( bShortNullable ){` |
|        - | 1658 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|        - | 1659 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|        - | 1660 | `				 * already reported" so callers skip their own error emission. */` |
|        3 | 1661 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - | 1662 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|        3 | 1663 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|        - | 1664 | `			}` |
|    42610 | 1665 | `			if( nMembers >= 2 && !bParen ){` |
|      ! 0 | 1666 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|        - | 1667 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 | 1668 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1669 | `			}` |
|    42610 | 1670 | ``			pGen->pIn++; /* skip `\|` */`` |
|    42610 | 1671 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|    42610 | 1672 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1673 | `				return rc;` |
|        - | 1674 | `			}` |
|        5 | 1675 | `		}` |
|   420602 | 1676 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|      ! 0 | 1677 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1678 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 | 1679 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1680 | `		}` |
|        - | 1681 | `	}` |
|        - | 1682 | `	/* Validation pass.` |
|        - | 1683 | `	 *` |
|        - | 1684 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|        - | 1685 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|        - | 1686 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|        - | 1687 | `	 */` |
|        - | 1688 | `	{` |
|        - | 1689 | `		int i, j;` |
|   420602 | 1690 | `		int bHasNonNull = 0;` |
|   420602 | 1691 | `		int bAnyIntersection = 0;` |
|        - | 1692 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|        - | 1693 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|        - | 1694 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
| 13879706 | 1695 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   883850 | 1696 | `		for( i = 0; i < nAtoms; i++ ){` |
|   463253 | 1697 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|   231311 | 1698 | `		}` |
|   883746 | 1699 | `		for( i = 0; i < nAtoms; i++ ){` |
|   463195 | 1700 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|   231259 | 1701 | `		}` |
|        - | 1702 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|        - | 1703 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|   420602 | 1704 | `		if( bShortNullable && bAnyIntersection ){` |
|      ! 0 | 1705 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1706 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|      ! 0 | 1707 | `			return SXERR_SYNTAX;` |
|        - | 1708 | `		}` |
|        - | 1709 | `		/*` |
|        - | 1710 | `		 * php's declaration-time screen for the three SCOPE keywords, which runs` |
|        - | 1711 | `		 * BEFORE the intersection-member kinds below (that is php's order: a` |
|        - | 1712 | ``		 * `parent&Countable` in a base-less class is refused for the parent, not`` |
|        - | 1713 | `		 * for the intersection).` |
|        - | 1714 | `		 *` |
|        - | 1715 | `		 * Each keyword names something relative to WHERE the declaration is` |
|        - | 1716 | `		 * written, and one written where that thing does not exist is refused` |
|        - | 1717 | `		 * before the program runs. A CLOSURE is exempt — its scope is decided when` |
|        - | 1718 | `		 * it is bound — and so is a TRAIT, which defers the question to whatever` |
|        - | 1719 | ``		 * composes it. An interface has no `parent` however many it extends.`` |
|        - | 1720 | `		 *` |
|        - | 1721 | ``		 * `static` is not a PARAMETER type in php's grammar at all: the modifier`` |
|        - | 1722 | `` 		 * run ahead of this parser catches the bare and `&` spellings, and `?static` `` |
|        - | 1723 | `		 * reaches here, where php answers with the parse error its grammar produces.` |
|        - | 1724 | `		 */` |
|   883844 | 1725 | `		for( i = 0; i < nAtoms; i++ ){` |
|        - | 1726 | `			const SyString *pKw;` |
|        - | 1727 | `			ph7_class *pScope;` |
|        - | 1728 | `			const char *zKw;` |
|   463253 | 1729 | `			if( aAtoms[i].nType != SXU32_HIGH ){` |
|   402410 | 1730 | `				continue;` |
|        - | 1731 | `			}` |
|    60848 | 1732 | `			pKw = &aAtoms[i].sClass;` |
|   121565 | 1733 | `			zKw = (pKw->nByte == 4 && SyStrnicmp(pKw->zString,"self",4) == 0)   ? "self"   :` |
|    91014 | 1734 | `			      (pKw->nByte == 6 && SyStrnicmp(pKw->zString,"parent",6) == 0) ? "parent" :` |
|    60675 | 1735 | `			      (pKw->nByte == 6 && SyStrnicmp(pKw->zString,"static",6) == 0) ? "static" : 0;` |
|    60848 | 1736 | `			if( zKw == 0 ){` |
|    60606 | 1737 | `				continue;` |
|        - | 1738 | `			}` |
|      247 | 1739 | `			if( bParamCtx && zKw[0] == 's' && zKw[1] == 't' ){` |
|        - | 1740 | ``				/* GRAMMAR, not scope: `static` is no parameter type at all, so a`` |
|        - | 1741 | `				 * CLOSURE is not exempt from this one. The modifier run ahead of this` |
|        - | 1742 | ``				 * parser already catches the bare and `&` spellings; `?static` and`` |
|        - | 1743 | ``				 * `int\|static` reach here. */`` |
|        3 | 1744 | `				PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|        - | 1745 | `					"syntax error, unexpected token \"static\"");` |
|        3 | 1746 | `				return SXERR_SYNTAX;` |
|        - | 1747 | `			}` |
|      245 | 1748 | `			if( pGen->iSigScope == PH7_SIGSCOPE_CLOSURE ){` |
|       20 | 1749 | `				continue;` |
|        - | 1750 | `			}` |
|      229 | 1751 | `			pScope = ( pGen->iSigScope == PH7_SIGSCOPE_FUNC ) ? 0 : pGen->pCurClass;` |
|      229 | 1752 | `			if( pScope == 0 ){` |
|        4 | 1753 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        1 | 1754 | `					"Cannot use \"%s\" when no class scope is active", zKw);` |
|        3 | 1755 | `				return SXERR_SYNTAX;` |
|        - | 1756 | `			}` |
|      222 | 1757 | `			if( zKw[0] == 'p' && pGen->pCurBase == 0` |
|       30 | 1758 | `			 && (pScope->iFlags & (PH7_CLASS_TRAIT\|PH7_CLASS_LINT_UNBOUND)) == 0 ){` |
|        3 | 1759 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1760 | `					"Cannot use \"parent\" when current class scope has no parent");` |
|        3 | 1761 | `				return SXERR_SYNTAX;` |
|        - | 1762 | `			}` |
|      115 | 1763 | `		}` |
|   883818 | 1764 | `		for( i = 0; i < nAtoms; i++ ){` |
|        - | 1765 | `			/* Intersection members must be class/interface types (PHP rejects` |
|        - | 1766 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|        - | 1767 | ``			 * `true`/`false` in an intersection). */`` |
|   463243 | 1768 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       93 | 1769 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|       93 | 1770 | `				int bLowerKw = 0;` |
|       93 | 1771 | `				if( bClassLike ){` |
|       91 | 1772 | `					SyString *pC = &aAtoms[i].sClass;` |
|       86 | 1773 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|       86 | 1774 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|       86 | 1775 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|       91 | 1776 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|      ! 0 | 1777 | `						bClassLike = 0;` |
|      ! 0 | 1778 | `					}` |
|        - | 1779 | `					/* A SCOPE keyword is class-like only where php can substitute a` |
|        - | 1780 | ``					 * class NAME for it while the body compiles: `static` never (the`` |
|        - | 1781 | `` 					 * called class is not known until the call), and `self`/`parent` `` |
|        - | 1782 | `					 * not in a closure, a trait or an ANONYMOUS class, none of which` |
|        - | 1783 | `					 * has a name to put there. In a named class, interface or enum` |
|        - | 1784 | `					 * php resolves them and the intersection stands. */` |
|       91 | 1785 | `					else if( pC->nByte == 6 && SyMemcmpNoCase(pC->zString,"static",6) == 0 ){` |
|      ! 0 | 1786 | `						bClassLike = 0;` |
|      ! 0 | 1787 | `						bLowerKw = 1; /* php prints the keyword, not what was written */` |
|       86 | 1788 | `					}else if( (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"self",4) == 0)` |
|       89 | 1789 | `					       \|\| (pC->nByte == 6 && SyMemcmpNoCase(pC->zString,"parent",6) == 0) ){` |
|        8 | 1790 | `						ph7_class *pScope = ( pGen->iSigScope == PH7_SIGSCOPE_FUNC )` |
|        4 | 1791 | `							? 0 : pGen->pCurClass;` |
|        4 | 1792 | `						if( pGen->iSigScope == PH7_SIGSCOPE_CLOSURE \|\| pScope == 0` |
|        6 | 1793 | `						 \|\| (pScope->iFlags & (PH7_CLASS_TRAIT\|PH7_CLASS_ANON)) != 0 ){` |
|        3 | 1794 | `							bClassLike = 0;` |
|        1 | 1795 | `						}` |
|        2 | 1796 | `					}` |
|       43 | 1797 | `				}` |
|       93 | 1798 | `				if( !bClassLike ){` |
|        - | 1799 | `					const char *zName; sxu32 nName;` |
|        6 | 1800 | `					if( bLowerKw ){` |
|      ! 0 | 1801 | `						zName = "static"; nName = sizeof("static")-1;` |
|        6 | 1802 | `					}else if( aAtoms[i].nType == SXU32_HIGH ){` |
|        3 | 1803 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|        2 | 1804 | `					}else{` |
|        3 | 1805 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|        - | 1806 | `					}` |
|        8 | 1807 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1808 | `						"Type %.*s cannot be part of an intersection type",` |
|        2 | 1809 | `						(int)nName, zName);` |
|        6 | 1810 | `					return SXERR_SYNTAX;` |
|        - | 1811 | `				}` |
|       42 | 1812 | `			}` |
|   463239 | 1813 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     9139 | 1814 | `				if( nAtoms > 1 ){` |
|        3 | 1815 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1816 | `						"Void can only be used as a standalone type");` |
|        3 | 1817 | `					return SXERR_SYNTAX;` |
|        - | 1818 | `				}` |
|     9137 | 1819 | `				if( !bAllowVoid ){` |
|        - | 1820 | `					/* php names the position when it is a parameter's type. */` |
|        4 | 1821 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        1 | 1822 | `						bParamCtx ? "void cannot be used as a parameter type"` |
|        - | 1823 | `						          : "void cannot be used here");` |
|        3 | 1824 | `					return SXERR_SYNTAX;` |
|        - | 1825 | `				}` |
|     9135 | 1826 | `				if( bShortNullable ){` |
|      ! 0 | 1827 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1828 | `						"Void type cannot be nullable");` |
|      ! 0 | 1829 | `					return SXERR_SYNTAX;` |
|        - | 1830 | `				}` |
|     4552 | 1831 | `			}` |
|   463235 | 1832 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|        - | 1833 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|        - | 1834 | `				 * type (never = the function does not return). Mirrors the void` |
|        - | 1835 | `				 * validation above; accepted here and enforced at compile time` |
|        - | 1836 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|       39 | 1837 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|        - | 1838 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|        - | 1839 | `					 * same as any other non-standalone use. */` |
|        5 | 1840 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1841 | `						"never can only be used as a standalone type");` |
|        5 | 1842 | `					return SXERR_SYNTAX;` |
|        - | 1843 | `				}` |
|       34 | 1844 | `				if( !bAllowVoid ){` |
|        - | 1845 | `					/* Return-only: params call with bAllowVoid=0. */` |
|        3 | 1846 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1847 | `						"never cannot be used as a parameter type");` |
|        3 | 1848 | `					return SXERR_SYNTAX;` |
|        - | 1849 | `				}` |
|       14 | 1850 | `			}` |
|   463229 | 1851 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|       57 | 1852 | `				bExplicitNull = 1;` |
|       31 | 1853 | `			}else{` |
|   463177 | 1854 | `				bHasNonNull = 1;` |
|        - | 1855 | `			}` |
|        - | 1856 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|        - | 1857 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|        - | 1858 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|        - | 1859 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|        - | 1860 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|   505950 | 1861 | `			for( j = 0; j < i; j++ ){` |
|    42728 | 1862 | `				int bDup = 0;` |
|    42728 | 1863 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|    85428 | 1864 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|    42723 | 1865 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|    42728 | 1866 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|    42700 | 1867 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      109 | 1868 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      102 | 1869 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|       80 | 1870 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       24 | 1871 | `								aAtoms[j].sClass.zString,` |
|       48 | 1872 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|      ! 0 | 1873 | `							bDup = 1;` |
|      ! 0 | 1874 | `						}` |
|       56 | 1875 | `					}else{` |
|        3 | 1876 | `						bDup = 1;` |
|        - | 1877 | `					}` |
|       52 | 1878 | `				}` |
|    42700 | 1879 | `				if( bDup ){` |
|        - | 1880 | `					const char *zName;` |
|        - | 1881 | `					sxu32 nName;` |
|        3 | 1882 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 | 1883 | `						zName = aAtoms[i].sClass.zString;` |
|      ! 0 | 1884 | `						nName = aAtoms[i].sClass.nByte;` |
|      ! 0 | 1885 | `					}else{` |
|        3 | 1886 | `						zName = aAtoms[i].zCanon;` |
|        3 | 1887 | `						nName = aAtoms[i].nCanon;` |
|        - | 1888 | `					}` |
|        4 | 1889 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        1 | 1890 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|        3 | 1891 | `					return SXERR_SYNTAX;` |
|        - | 1892 | `				}` |
|    21324 | 1893 | `			}` |
|   231298 | 1894 | `		}` |
|   420580 | 1895 | `		if( !bHasNonNull && bExplicitNull ){` |
|        7 | 1896 | `			if( bShortNullable ){` |
|        - | 1897 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|      ! 0 | 1898 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1899 | `					"Null can not be used as a standalone type");` |
|      ! 0 | 1900 | `				return SXERR_SYNTAX;` |
|        - | 1901 | `			}` |
|        - | 1902 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|        - | 1903 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|        - | 1904 | `			 * path below leaves *pnType untouched when there is no non-null` |
|        - | 1905 | `			 * atom, so set it here. */` |
|        7 | 1906 | `			*pnType = MEMOBJ_NULL;` |
|        3 | 1907 | `		}` |
|        - | 1908 | `	}` |
|        - | 1909 | `	/* Compute nullability flag */` |
|   420580 | 1910 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      397 | 1911 | `		*piTypeFlags \|= iNullableFlag;` |
|      196 | 1912 | `	}` |
|        - | 1913 | `	/* Build canonical type text */` |
|   420580 | 1914 | `	if( pTypeText ){` |
|        - | 1915 | `		SyBlob sBlob;` |
|   420580 | 1916 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|   630407 | 1917 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   209997 | 1918 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   420580 | 1919 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|   630577 | 1920 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   420575 | 1921 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   420580 | 1922 | `			if( zDup ){` |
|   420580 | 1923 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   209997 | 1924 | `			}` |
|   209997 | 1925 | `		}` |
|   420580 | 1926 | `		SyBlobRelease(&sBlob);` |
|   209997 | 1927 | `	}` |
|        - | 1928 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|        - | 1929 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|        - | 1930 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|        - | 1931 | `	{` |
|   420580 | 1932 | `		int nNonNull = 0;` |
|   420580 | 1933 | `		int iNonNullIdx = -1;` |
|        - | 1934 | `		int i;` |
|   883794 | 1935 | `		for( i = 0; i < nAtoms; i++ ){` |
|   463219 | 1936 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   463167 | 1937 | `				nNonNull++;` |
|   463167 | 1938 | `				iNonNullIdx = i;` |
|   231263 | 1939 | `			}` |
|   231294 | 1940 | `		}` |
|   420580 | 1941 | `		if( nNonNull <= 1 ){` |
|        - | 1942 | `			/* Fast path: store as single type. */` |
|   378035 | 1943 | `			if( iNonNullIdx >= 0 ){` |
|   378029 | 1944 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   378029 | 1945 | `				if( pA->nType == SXU32_HIGH ){` |
|    27484 | 1946 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     9151 | 1947 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    18333 | 1948 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    18333 | 1949 | `					*pnType = SXU32_HIGH;` |
|    18333 | 1950 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   368852 | 1951 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     9135 | 1952 | `					*pnType = MEMOBJ_VOID;` |
|   355123 | 1953 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|       32 | 1954 | `					*pnType = MEMOBJ_NEVER;` |
|       18 | 1955 | `				}else{` |
|   350543 | 1956 | `					*pnType = pA->nType;` |
|        - | 1957 | `				}` |
|   188749 | 1958 | `			}` |
|   188757 | 1959 | `		}else{` |
|        - | 1960 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|    42550 | 1961 | `			*piTypeFlags \|= iUnionFlag;` |
|   127706 | 1962 | `			for( i = 0; i < nAtoms; i++ ){` |
|        - | 1963 | `				ph7_type_alt sAlt;` |
|    85161 | 1964 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|    85143 | 1965 | `				SyZero(&sAlt, sizeof(sAlt));` |
|    85143 | 1966 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|    85143 | 1967 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|    63735 | 1968 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    21225 | 1969 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    42510 | 1970 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    42510 | 1971 | `					sAlt.nType = SXU32_HIGH;` |
|    42510 | 1972 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|    21230 | 1973 | `				}else{` |
|    42638 | 1974 | `					sAlt.nType = aAtoms[i].nType;` |
|    42638 | 1975 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|        - | 1976 | `				}` |
|    85143 | 1977 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|    42519 | 1978 | `			}` |
|        - | 1979 | `		}` |
|        - | 1980 | `	}` |
|   420580 | 1981 | `	return SXRET_OK;` |
|   210019 | 1982 | `}` |
|        - | 1983 |  |
|        - | 1984 | `/*` |
|        - | 1985 | `` * php 8.5's `#[\NoDiscard]`, decided where the declaration is WRITTEN.`` |
|        - | 1986 | ` *` |
|        - | 1987 | ` * The attribute says a caller must do something with the answer, so php refuses` |
|        - | 1988 | `` * it on a declaration that HAS no answer -- a `void` or `never` return type --`` |
|        - | 1989 | ` * and on a constructor, which is called for its object rather than its return.` |
|        - | 1990 | ` * The nouns are php's: a "function" everywhere but a class body, where the same` |
|        - | 1991 | ` * sentence says "method". Run once the return type is parsed, since that is` |
|        - | 1992 | ` * what it judges; a declaration that already failed says nothing more.` |
|        - | 1993 | ` */` |
|   214325 | 1994 | `PH7_PRIVATE sxi32 GenStateApplyNoDiscard(ph7_gen_state *pGen,ph7_vm_func *pFunc,` |
|        - | 1995 | `	ph7_class *pClass,int bCtor)` |
|        5 | 1996 | `{` |
|   214330 | 1997 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(&pFunc->aAttrs);` |
|   214330 | 1998 | `	const char *zKind = pClass ? "method" : "function";` |
|        - | 1999 | `	sxu32 n;` |
|   214556 | 2000 | `	for( n = 0 ; n < SySetUsed(&pFunc->aAttrs) ; ++n ){` |
|      404 | 2001 | `		if( SyStringLength(&aAttr[n].sName) != sizeof("NoDiscard")-1` |
|      302 | 2002 | `		 \|\| SyStrnicmp(SyStringData(&aAttr[n].sName),"NoDiscard",sizeof("NoDiscard")-1) != 0 ){` |
|      231 | 2003 | `			continue;` |
|        - | 2004 | `		}` |
|      180 | 2005 | `		if( bCtor ){` |
|        4 | 2006 | `			return PH7_GenCompileError(&(*pGen),E_ERROR,pFunc->nLine,` |
|        1 | 2007 | `				"Method %z::%z cannot be #[\\NoDiscard]",&pClass->sDisp,&pFunc->sName);` |
|        - | 2008 | `		}` |
|      178 | 2009 | `		if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|        7 | 2010 | `			return PH7_GenCompileError(&(*pGen),E_ERROR,pFunc->nLine,` |
|        - | 2011 | `				"A void %s does not return a value, but #[\\NoDiscard] requires a return value",` |
|        2 | 2012 | `				zKind);` |
|        - | 2013 | `		}` |
|      174 | 2014 | `		if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|        7 | 2015 | `			return PH7_GenCompileError(&(*pGen),E_ERROR,pFunc->nLine,` |
|        - | 2016 | `				"A never returning %s does not return a value, but #[\\NoDiscard] requires a return value",` |
|        2 | 2017 | `				zKind);` |
|        - | 2018 | `		}` |
|      170 | 2019 | `		pFunc->iFlags \|= VM_FUNC_NODISCARD;` |
|      170 | 2020 | `		return SXRET_OK;` |
|      ! 0 | 2021 | `	}` |
|   214152 | 2022 | `	return SXRET_OK;` |
|   106911 | 2023 | `}` |
|        - | 2024 | `/*` |
|        - | 2025 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|        - | 2026 | `` * pGen->pIn should point to the token after `)`.`` |
|        - | 2027 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|        - | 2028 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|        - | 2029 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|        - | 2030 | `` *          and union types `: T\|U`.`` |
|        - | 2031 | ` */` |
|   206600 | 2032 | `PH7_PRIVATE sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|        5 | 2033 | `{` |
|   206605 | 2034 | `	sxi32 iFlags = 0;` |
|        - | 2035 | `	sxi32 rc;` |
|        - | 2036 | `	sxu32 nLine;` |
|   206605 | 2037 | `	pFunc->nReturnType = 0;` |
|   206605 | 2038 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|   206605 | 2039 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|        - | 2040 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|        - | 2041 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|        - | 2042 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|        - | 2043 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|        - | 2044 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|   206605 | 2045 | `	SySetReset(&pFunc->aReturnUnion);` |
|   206605 | 2046 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|   206605 | 2047 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|    43547 | 2048 | `		return SXRET_OK;` |
|        - | 2049 | `	}` |
|   163063 | 2050 | `	pGen->pIn++; /* Skip ':' */` |
|   163063 | 2051 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 2052 | `		return SXRET_OK;` |
|        - | 2053 | `	}` |
|   163063 | 2054 | `	nLine = pGen->pIn->nLine;` |
|   163063 | 2055 | `	rc = GenStateParseUnionTypeDecl(` |
|    81415 | 2056 | `		pGen,` |
|    81415 | 2057 | `		&pFunc->nReturnType,` |
|    81415 | 2058 | `		&pFunc->sReturnClass,` |
|    81415 | 2059 | `		&pFunc->aReturnUnion,` |
|        - | 2060 | `		&iFlags,` |
|    81415 | 2061 | `		&pFunc->sReturnTypeName,` |
|        - | 2062 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|        - | 2063 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|        - | 2064 | `		/* iUnionFlag */ 0,` |
|        - | 2065 | `		/* bAllowVoid */ 1, /* bParamCtx */ 0,` |
|    81415 | 2066 | `		nLine);` |
|   163063 | 2067 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2068 | `		return SXERR_ABORT;` |
|        - | 2069 | `	}` |
|   163063 | 2070 | `	if( rc == SXERR_CORRUPT ){` |
|        - | 2071 | `		/* Error already reported */` |
|      ! 0 | 2072 | `		return SXERR_SYNTAX;` |
|        - | 2073 | `	}` |
|   163063 | 2074 | `	if( rc == SXERR_SYNTAX ){` |
|       17 | 2075 | `		if( pGen->pIn < pGen->pEnd ){` |
|       24 | 2076 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - | 2077 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       14 | 2078 | `				&pGen->pIn->sData);` |
|       10 | 2079 | `		}else{` |
|      ! 0 | 2080 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|        - | 2081 | `				"syntax error, unexpected end of file in return type declaration");` |
|        - | 2082 | `		}` |
|       17 | 2083 | `		return SXERR_SYNTAX;` |
|        - | 2084 | `	}` |
|   163049 | 2085 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|   163049 | 2086 | `	return SXRET_OK;` |
|   103065 | 2087 | `}` |
|        - | 2088 |  |
|   190084 | 2089 | `PH7_PRIVATE sxi32 GenStateCompileFunc(` |
|        - | 2090 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 2091 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|        - | 2092 | `	sxi32 iFlags,        /* Control flags */` |
|        - | 2093 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|        - | 2094 | `	sxu32 nKwLine,       /* Line of the 'function' keyword: php's line for the declaration */` |
|        - | 2095 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|        - | 2096 | `	)` |
|        5 | 2097 | `{` |
|        - | 2098 | `	ph7_vm_func *pFunc;` |
|        - | 2099 | `	SyToken *pEnd;` |
|        - | 2100 | `	sxu32 nLine;` |
|        - | 2101 | `	char *zName;` |
|        - | 2102 | `	sxi32 rc;` |
|        - | 2103 | `	/* Extract line number */` |
|   190089 | 2104 | `	nLine = pGen->pIn->nLine;` |
|        - | 2105 | `	/* Jump the left parenthesis '(' */` |
|   190089 | 2106 | `	pGen->pIn++;` |
|        - | 2107 | `	/* Delimit the function signature */` |
|   190089 | 2108 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   190089 | 2109 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 2110 | `		/* Syntax error */` |
|       11 | 2111 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable");` |
|        4 | 2112 | `		(void)pName;` |
|       11 | 2113 | `		if( rc == SXERR_ABORT ){` |
|        - | 2114 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 2115 | `			return SXERR_ABORT;` |
|        - | 2116 | `		}` |
|       11 | 2117 | `		pGen->pIn = pGen->pEnd;` |
|       11 | 2118 | `		return SXRET_OK;` |
|        - | 2119 | `	}` |
|        - | 2120 | `	/* Create the function state */` |
|   190081 | 2121 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   190081 | 2122 | `	if( pFunc == 0 ){` |
|      ! 0 | 2123 | `		goto OutOfMem;` |
|        - | 2124 | `	}` |
|        - | 2125 | `	/* Build the function name, prepending namespace if active.` |
|        - | 2126 | `	 * A NAMED function (never a closure) also answers to php's import rules: its` |
|        - | 2127 | ``	 * short name must not already be a local `use function` import, and the name it`` |
|        - | 2128 | ``	 * takes is remembered so a later `use function` in this unit sees it. */`` |
|   190145 | 2129 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|        - | 2130 | `		SyBlob sFQN;` |
|        - | 2131 | `		sxu32 nLen;` |
|      133 | 2132 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      133 | 2133 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      133 | 2134 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      133 | 2135 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      133 | 2136 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      133 | 2137 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      133 | 2138 | `		SyBlobRelease(&sFQN);` |
|      133 | 2139 | `		if( zName == 0 ){` |
|      ! 0 | 2140 | `			goto OutOfMem;` |
|        - | 2141 | `		}` |
|      133 | 2142 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       69 | 2143 | `	}else{` |
|   189953 | 2144 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   189953 | 2145 | `		if( zName == 0 ){` |
|      ! 0 | 2146 | `			goto OutOfMem;` |
|        - | 2147 | `		}` |
|   189953 | 2148 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|        - | 2149 | `	}` |
|   190081 | 2150 | `	if( !bHandleClosure ){` |
|   182338 | 2151 | `		if( GenStateGuardImportRedeclare(pGen,1,pName,&pFunc->sName,nLine) == SXERR_ABORT ){` |
|      ! 0 | 2152 | `			return SXERR_ABORT;` |
|        - | 2153 | `		}` |
|   182338 | 2154 | `		GenStateRecordDeclaredName(pGen,1,&pFunc->sName);` |
|    91045 | 2155 | `	}` |
|        - | 2156 | ``	/* Take php's `{closure:SCOPE:LINE}` name the caller built for this closure. It has to`` |
|        - | 2157 | `	 * land here, ahead of the body, because a __FUNCTION__ inside the body reads it at` |
|        - | 2158 | `	 * compile time — and it must be cleared, since a NESTED declaration reaches this same` |
|        - | 2159 | `	 * point and would otherwise inherit its parent's. */` |
|   190081 | 2160 | `	if( SyStringLength(&pGen->sPendingClosureName) > 0 ){` |
|     7748 | 2161 | `		if( bHandleClosure ){` |
|     7748 | 2162 | `			pFunc->sClosureName = pGen->sPendingClosureName;` |
|     7748 | 2163 | `			pFunc->sClosureScope = pGen->sPendingClosureScope;` |
|     3855 | 2164 | `		}` |
|     7748 | 2165 | `		SyStringInitFromBuf(&pGen->sPendingClosureName,0,0);` |
|     3855 | 2166 | `	}` |
|        - | 2167 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|        - | 2168 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|   190081 | 2169 | `	pFunc->nLine = nLine;` |
|   190081 | 2170 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|   190081 | 2171 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 2172 | `		return SXERR_ABORT;` |
|        - | 2173 | `	}` |
|   190081 | 2174 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pFunc->aAttrs,nKwLine,2,2,0,0) == SXERR_ABORT ){` |
|      ! 0 | 2175 | `		return SXERR_ABORT;` |
|        - | 2176 | `	}` |
|        - | 2177 | `	/* Whose signature this is, for php's scope-keyword screen (see iSigScope): a` |
|        - | 2178 | `	 * closure's is EXEMPT, and a NAMED function has no class scope even when it is` |
|        - | 2179 | `	 * written inside a method body. */` |
|        - | 2180 | `	{` |
|   190081 | 2181 | `		int iSavedSig = pGen->iSigScope;` |
|   190081 | 2182 | `		pGen->iSigScope = bHandleClosure ? PH7_SIGSCOPE_CLOSURE : PH7_SIGSCOPE_FUNC;` |
|   190081 | 2183 | `		if( pGen->pIn < pEnd ){` |
|        - | 2184 | `			/* Collect function arguments */` |
|   174234 | 2185 | `			rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   174234 | 2186 | `			if( rc == SXERR_ABORT ){` |
|        - | 2187 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       16 | 2188 | `				pGen->iSigScope = iSavedSig;` |
|       16 | 2189 | `				return SXERR_ABORT;` |
|        - | 2190 | `			}` |
|    86991 | 2191 | `		}` |
|        - | 2192 | `		/* Point past ')' and parse optional return type ': type' */` |
|   190069 | 2193 | `		pGen->pIn = &pEnd[1];` |
|        - | 2194 | `		{` |
|   190069 | 2195 | `			sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   190069 | 2196 | `			pGen->iSigScope = iSavedSig;` |
|   190069 | 2197 | `			if( rcRt == SXERR_ABORT ){` |
|      ! 0 | 2198 | `				return SXERR_ABORT;` |
|   190069 | 2199 | `			}else if( rcRt == SXERR_SYNTAX ){` |
|       15 | 2200 | `				return SXERR_SYNTAX;` |
|        - | 2201 | `			}` |
|        - | 2202 | `		}` |
|        - | 2203 | `	}` |
|        - | 2204 | `	/* php's #[\NoDiscard] declaration rules, which want the return type. A` |
|        - | 2205 | ``	 * closure's second (post-`use`) return-type parse re-runs below; the flag is`` |
|        - | 2206 | `	 * idempotent and the refusals are the same either way. */` |
|   190057 | 2207 | `	if( GenStateApplyNoDiscard(&(*pGen),pFunc,0,0) == SXERR_ABORT ){` |
|      ! 0 | 2208 | `		return SXERR_ABORT;` |
|        - | 2209 | `	}` |
|   190057 | 2210 | `	if( bHandleClosure ){` |
|        - | 2211 | `		ph7_vm_func_closure_env sEnv;` |
|     7746 | 2212 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     7741 | 2213 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     5021 | 2214 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|     2314 | 2215 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - | 2216 | `				/* Closure,record environment variable */` |
|     2314 | 2217 | `				pGen->pIn++;` |
|     2314 | 2218 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 2219 | `					rc = PH7_GenCompileError(pGen,E_PARSE,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|      ! 0 | 2220 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2221 | `						return SXERR_ABORT;` |
|        - | 2222 | `					}` |
|      ! 0 | 2223 | `				}` |
|     2314 | 2224 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|        - | 2225 | `				/* Compile until we hit the first closing parenthesis */` |
|     4911 | 2226 | `				while( pGen->pIn < pGen->pEnd ){` |
|     4911 | 2227 | `					int iFlagsLocal = 0;` |
|     4911 | 2228 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|     2312 | 2229 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|     2312 | 2230 | `						break;` |
|        - | 2231 | `					}` |
|     2604 | 2232 | `					nLineLocal = pGen->pIn->nLine;` |
|     2604 | 2233 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - | 2234 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|        - | 2235 | `						 * to the variable's memory slot instead of copying its value. */` |
|      551 | 2236 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|      551 | 2237 | `						pGen->pIn++;` |
|      269 | 2238 | `					}` |
|     2599 | 2239 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|     2604 | 2240 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2241 | `							rc = PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|        - | 2242 | `								"Closure: Unexpected token. Expecting a variable name");` |
|      ! 0 | 2243 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 2244 | `								return SXERR_ABORT;` |
|        - | 2245 | `							}` |
|        - | 2246 | `							/* Find the closing parenthesis */` |
|      ! 0 | 2247 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 2248 | `								pGen->pIn++;` |
|      ! 0 | 2249 | `							}` |
|      ! 0 | 2250 | `							if(pGen->pIn < pGen->pEnd){` |
|      ! 0 | 2251 | `								pGen->pIn++;` |
|      ! 0 | 2252 | `							}` |
|      ! 0 | 2253 | `							break;` |
|        - | 2254 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|      ! 0 | 2255 | `					}else{` |
|        - | 2256 | `						SyString *pNameLocal;` |
|        - | 2257 | `						char *zDup;` |
|        - | 2258 | `						/* Duplicate variable name */` |
|     2604 | 2259 | `						pNameLocal = &pGen->pIn[1].sData;` |
|     2604 | 2260 | `						if( PH7_VmIsAutoGlobal(pNameLocal->zString,pNameLocal->nByte) ){` |
|        - | 2261 | `							/* php's compile fatal. It is a real protection, not a` |
|        - | 2262 | `							 * style rule: the import resolves through hSuper, so` |
|        - | 2263 | `							 * installing the captured value would overwrite the` |
|        - | 2264 | ``							 * superglobal's own slot — `use ($GLOBALS)` replaced the`` |
|        - | 2265 | `							 * live symbol-table view with a snapshot and every later` |
|        - | 2266 | `							 * global went missing program-wide. */` |
|        3 | 2267 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - | 2268 | `								"Cannot use auto-global as lexical variable");` |
|        3 | 2269 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 2270 | `								return SXERR_ABORT;` |
|        - | 2271 | `							}` |
|        3 | 2272 | `							return SXERR_SYNTAX;` |
|        - | 2273 | `						}` |
|     2602 | 2274 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|     2602 | 2275 | `						if( zDup ){` |
|        - | 2276 | `							/* Zero the structure */` |
|     2602 | 2277 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     2602 | 2278 | `							sEnv.iFlags = iFlagsLocal;` |
|     2602 | 2279 | `							sEnv.nLine = nLineLocal; /* the capture's own source line (php warns here) */` |
|     2602 | 2280 | `							sEnv.nIdx = SXU32_HIGH;` |
|     2602 | 2281 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     2602 | 2282 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|     2752 | 2283 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|      299 | 2284 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|      ! 0 | 2285 | `									got_this = 1;` |
|      ! 0 | 2286 | `							}` |
|        - | 2287 | `							/* Save imported variable */` |
|     2602 | 2288 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|     1292 | 2289 | `						}else{` |
|      ! 0 | 2290 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2291 | `							 return SXERR_ABORT;` |
|        - | 2292 | `						}` |
|        - | 2293 | `					}` |
|     2602 | 2294 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|     2894 | 2295 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - | 2296 | `						/* Ignore trailing commas */` |
|      297 | 2297 | `						pGen->pIn++;` |
|        5 | 2298 | `					}` |
|        5 | 2299 | `				}` |
|        - | 2300 | `				/* php 7.1+: the return type follows the use clause —` |
|        - | 2301 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|        - | 2302 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|        - | 2303 | `				 * so an unconditional call would wipe a type parsed at the` |
|        - | 2304 | `				 * legacy pre-use position. */` |
|     2312 | 2305 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|       30 | 2306 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|       30 | 2307 | `					if( rcRt2 == SXERR_ABORT ){` |
|      ! 0 | 2308 | `						return SXERR_ABORT;` |
|       30 | 2309 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|      ! 0 | 2310 | `						return SXERR_SYNTAX;` |
|        - | 2311 | `					}` |
|        - | 2312 | `					/* The type this closure really declared is only known now. */` |
|       30 | 2313 | `					if( GenStateApplyNoDiscard(&(*pGen),pFunc,0,0) == SXERR_ABORT ){` |
|      ! 0 | 2314 | `						return SXERR_ABORT;` |
|        - | 2315 | `					}` |
|       11 | 2316 | `				}` |
|     1146 | 2317 | `		}` |
|     7744 | 2318 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|        - | 2319 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|        - | 2320 | `			 * available to the closure environment — for EVERY non-static` |
|        - | 2321 | `			 * anonymous function, use list or not (php binds $this to any` |
|        - | 2322 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|        - | 2323 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|        - | 2324 | `			 * a global-scope closure is silently dropped at install. A static` |
|        - | 2325 | `			 * closure never binds $this (php). */` |
|     7274 | 2326 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     7274 | 2327 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|     7274 | 2328 | `			sEnv.nIdx = SXU32_HIGH;` |
|     7274 | 2329 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     7274 | 2330 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|     7274 | 2331 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|     3618 | 2332 | `		}` |
|     7744 | 2333 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|        - | 2334 | `			/* Mark as closure */` |
|     7366 | 2335 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|     3664 | 2336 | `		}` |
|     3853 | 2337 | `	}` |
|        - | 2338 | `	/* Compile the body */` |
|   190055 | 2339 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   190055 | 2340 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2341 | `		return SXERR_ABORT;` |
|        - | 2342 | `	}` |
|        - | 2343 | `	/* The cursor sits just past the body's closing brace */` |
|   190055 | 2344 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|   190055 | 2345 | `	if( ppFunc ){` |
|   190055 | 2346 | `		*ppFunc = pFunc;` |
|    94887 | 2347 | `	}` |
|   190055 | 2348 | `	rc = SXRET_OK;` |
|   190055 | 2349 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|   182694 | 2350 | `		if( GenStateDeclIsConditional(&(*pGen)) ){` |
|        - | 2351 | `			/* php binds this one when execution REACHES it, not now: a declaration` |
|        - | 2352 | ``			 * inside an `if`, a loop, a `try` or another function's body is not`` |
|        - | 2353 | `			 * early-bound. Binding it here made every` |
|        - | 2354 | ``			 * `if (!function_exists('x')) { function x(){} }` -- the shape every`` |
|        - | 2355 | `			 * symfony/polyfill-* package is written in -- REPLACE the engine's own` |
|        - | 2356 | ``			 * builtin, and declared the body of an `if (false)` besides. The`` |
|        - | 2357 | `			 * redeclaration screen moves to the opcode with it: two branches may` |
|        - | 2358 | `			 * each declare the name, and only the one that RUNS binds. */` |
|        - | 2359 | `` 			/* iP1 marks an ANONYMOUS function: `static function () {}` with no `use` `` |
|        - | 2360 | `			 * captures nothing, so it is not flagged a closure and lands here under a` |
|        - | 2361 | ``			 * synthesized `[lambda_N]` name -- a name no program wrote and that php has`` |
|        - | 2362 | `			 * no DECLARE_FUNCTION for at all. Running such a declaration twice is` |
|        - | 2363 | ``			 * ordinary (guzzle's `Middleware::redirect()` returns one), so the opcode`` |
|        - | 2364 | `			 * installs it and asks no redeclaration question. */` |
|      243 | 2365 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FUNC_DECL,bHandleClosure ? 1 : 0,0,(void *)pFunc,0);` |
|      243 | 2366 | `			return SXRET_OK;` |
|        - | 2367 | `		}` |
|        - | 2368 | `		/* Reject a php-fatal redeclaration before hoisting the function */` |
|   182456 | 2369 | `		if( GenStateGuardFuncRedeclaration(pGen,pFunc) == SXERR_ABORT ){` |
|       14 | 2370 | `			return SXERR_ABORT;` |
|        - | 2371 | `		}` |
|        - | 2372 | `		/* Finally register the function */` |
|   182444 | 2373 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    91098 | 2374 | `	}` |
|   189805 | 2375 | `	if( rc == SXRET_OK ){` |
|   189805 | 2376 | `		return SXRET_OK;` |
|        - | 2377 | `	}` |
|        - | 2378 | `	/* Fall through if something goes wrong */` |
|      ! 0 | 2379 | `OutOfMem:` |
|        - | 2380 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - | 2381 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|        - | 2382 | `	 */` |
|      ! 0 | 2383 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 | 2384 | `	return SXERR_ABORT;` |
|    94909 | 2385 | `}` |
|        - | 2386 | `/*` |
|        - | 2387 | ` * Compile a standard PHP function.` |
|        - | 2388 | ` *  Refer to the block-comment above for more information.` |
|        - | 2389 | ` */` |
|   182347 | 2390 | `PH7_PRIVATE sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|        5 | 2391 | `{` |
|        - | 2392 | `	SyString *pName;` |
|        - | 2393 | `	sxi32 iFlags;` |
|        - | 2394 | `	sxu32 nKwLine;` |
|        - | 2395 | `	sxu32 nLine;` |
|        - | 2396 | `	sxi32 rc;` |
|        - | 2397 |  |
|   182352 | 2398 | `	nLine = pGen->pIn->nLine;` |
|   182352 | 2399 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|   182352 | 2400 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   182352 | 2401 | `	iFlags = 0;` |
|   182352 | 2402 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - | 2403 | `		/* Return by reference,remember that */` |
|       39 | 2404 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|        - | 2405 | `		/* Jump the '&' token */` |
|       39 | 2406 | `		pGen->pIn++;` |
|       17 | 2407 | `	}` |
|   182352 | 2408 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 2409 | `		/* Invalid function name */` |
|        5 | 2410 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|        5 | 2411 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2412 | `			return SXERR_ABORT;` |
|        - | 2413 | `		}` |
|        - | 2414 | `		/* Sychronize with the next semi-colon or braces*/` |
|       17 | 2415 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       13 | 2416 | `			pGen->pIn++;` |
|        1 | 2417 | `		}` |
|        5 | 2418 | `		return SXRET_OK;` |
|        - | 2419 | `	}` |
|   182348 | 2420 | `	pName = &pGen->pIn->sData;` |
|   182348 | 2421 | `	nLine = pGen->pIn->nLine;` |
|        - | 2422 | `	/* Jump the function name */` |
|   182348 | 2423 | `	pGen->pIn++;` |
|   182348 | 2424 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 2425 | `		/* Syntax error */` |
|        3 | 2426 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|        3 | 2427 | `		if( rc == SXERR_ABORT ){` |
|        - | 2428 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 2429 | `			return SXERR_ABORT;` |
|        - | 2430 | `		}` |
|        - | 2431 | `		/* Sychronize with the next semi-colon or '{' */` |
|        3 | 2432 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 2433 | `			pGen->pIn++;` |
|      ! 0 | 2434 | `		}` |
|        3 | 2435 | `		return SXRET_OK;` |
|        - | 2436 | `	}` |
|        - | 2437 | `	/* Compile function body */` |
|        - | 2438 | `	{` |
|   182346 | 2439 | `		ph7_vm_func *pFuncState = 0;` |
|   182346 | 2440 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,nKwLine,&pFuncState);` |
|   182346 | 2441 | `		if( pFuncState ){` |
|        - | 2442 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|   182316 | 2443 | `			pFuncState->nLine = nKwLine;` |
|    91034 | 2444 | `		}` |
|        - | 2445 | `	}` |
|   182346 | 2446 | `	return rc;` |
|    91057 | 2447 | `}` |
|        - | 2448 |  |
