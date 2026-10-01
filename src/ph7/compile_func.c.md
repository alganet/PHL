# src/ph7/compile_func.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1133/1296 lines (87.42%)

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
|    41106 |   68 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|        5 |   69 | `{` |
|        - |   70 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |   71 | `	SySet *pInstrContainer;` |
|        - |   72 | `	sxi32 rc;` |
|        - |   73 | `	/* Swap token stream */` |
|    41111 |   74 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|        - |   75 | `	/* A parameter default is a constant expression: php applies the same rules here` |
|        - |   76 | ``	 * as to a class constant, minus `new` (PHP 8.1 allows `new` in an initializer).`` |
|        - |   77 | `	 * The swap above has left pGen->pIn/pEnd spanning exactly this default. */` |
|        - |   78 | `	{` |
|    41111 |   79 | `		const char *zCErr = PH7_GenStateConstExprError(pGen,1);` |
|    41111 |   80 | `		if( zCErr ){` |
|        6 |   81 | `			sxi32 rcErr = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"%s",zCErr);` |
|        6 |   82 | `			RE_SWAP_DELIMITER(pGen);` |
|        6 |   83 | `			return rcErr == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |   84 | `		}` |
|        - |   85 | `	}` |
|    41107 |   86 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    41107 |   87 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|        - |   88 | `	/* Compile the expression holding the argument value. A parameter default is a` |
|        - |   89 | `	 * const-expression belonging to the current class (see iInMemberDefault) — so` |
|        - |   90 | `	 * __TRAIT__ in it reads pCurClass rather than walking into the enclosing method. */` |
|    41107 |   91 | `	pGen->iInMemberDefault++;` |
|    41107 |   92 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    41107 |   93 | `	pGen->iInMemberDefault--;` |
|        - |   94 | `	/* Emit the done instruction */` |
|    41107 |   95 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    41107 |   96 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - |   97 | `	/* Finished, like a function body: give back the doubling slack. A default is a` |
|        - |   98 | `	 * handful of instructions in a container that opened at eight. */` |
|    41107 |   99 | `	SySetShrinkToFit(&pArg->aByteCode);` |
|    41107 |  100 | `	RE_SWAP_DELIMITER(pGen);` |
|    41107 |  101 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  102 | `		return SXERR_ABORT;` |
|        - |  103 | `	}` |
|    41107 |  104 | `	return SXRET_OK;` |
|    20530 |  105 | `}` |
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
|   147868 |  143 | `PH7_PRIVATE sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|        5 |  144 | `{` |
|        - |  145 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|        - |  146 | `	SyToken *pIn;  /* Token stream */` |
|        - |  147 | `	SyBlob sSig;         /* Function signature */` |
|        - |  148 | `	char *zDup;          /* Copy of argument name */` |
|        - |  149 | `	sxi32 rc;` |
|        - |  150 |  |
|   147873 |  151 | `	pIn = pGen->pIn;` |
|   147873 |  152 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|        - |  153 | `	/* Process arguments one after one */` |
|   186387 |  154 | `	for(;;){` |
|   373306 |  155 | `		if( pIn >= pEnd ){` |
|        - |  156 | `			/* No more arguments to process */` |
|   147841 |  157 | `			break;` |
|        - |  158 | `		}` |
|   225470 |  159 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   225470 |  160 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   225470 |  161 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   225470 |  162 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   225470 |  163 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|        - |  164 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|        - |  165 | `		 * first token inside the main token stream */` |
|   225470 |  166 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  167 | `			return SXERR_ABORT;` |
|        - |  168 | `		}` |
|        - |  169 | `		/* Parse the promotion-modifier RUN (constructor property promotion, PHP` |
|        - |  170 | ``		 * 8.0+; `readonly` 8.1, asymmetric `(set)` 8.4, `final` 8.4). php takes`` |
|        - |  171 | `		 * them as a SET in any order, and ANY of them promotes the parameter --` |
|        - |  172 | ``		 * `final int $x` alone is a public final property, exactly as `readonly`` |
|        - |  173 | ``		 * int $x` alone is a public readonly one. The old fixed ladder read`` |
|        - |  174 | ``		 * readonly, then a visibility, then readonly again, so a `final` anywhere`` |
|        - |  175 | `		 * fell through to the type parser as "syntax error, unexpected token` |
|        - |  176 | `		 * final, expecting variable". */` |
|        - |  177 | `		{` |
|   225470 |  178 | `			int bReadonly = 0, bVisSeen = 0, bReadVis = 0, bSetSeen = 0, bFinal = 0;` |
|   225470 |  179 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   225470 |  180 | `			sxi32 iSetVisFlag = 0;` |
|   225470 |  181 | `			int nSetTok = 0;   /* left unset when bNameHead skips the peek below */` |
|        - |  182 | `			sxi32 nSetVis;` |
|   225470 |  183 | `			sxu32 nModLine = (pIn < pEnd) ? pIn->nLine : 0;` |
|   225470 |  184 | `			const char *zTwice = 0;    /* a modifier written twice; "" = the access-type one */` |
|   225470 |  185 | `			const char *zNotHere = 0;  /* ...and one a PARAMETER never takes */` |
|        - |  186 | ``			/* php lexes `private\Q` as one T_NAME_QUALIFIED, so a modifier word a`` |
|        - |  187 | ``			 * `\` follows is the head of a TYPE name and no modifier at all:`` |
|        - |  188 | ``			 * `function f(private\Q $x)` is an ordinary parameter, not a promoted`` |
|        - |  189 | `			 * property outside a constructor. */` |
|   225477 |  190 | `			int bNameHead = ( pIn + 1 < pEnd && (pIn[1].nType & PH7_TK_NSSEP)` |
|   338358 |  191 | `				&& GenStateTokensGlued(pIn,&pIn[1]) );` |
|   225692 |  192 | `			while( !bNameHead && zTwice == 0 && zNotHere == 0 && pIn < pEnd ){` |
|   225674 |  193 | `				if( GenStateIsReadonly(pIn) ){` |
|       33 |  194 | `					if( bReadonly ){` |
|      ! 0 |  195 | `						zTwice = "readonly";` |
|      ! 0 |  196 | `					}` |
|       33 |  197 | `					bReadonly = 1;` |
|       33 |  198 | `					pIn++;` |
|       33 |  199 | `					continue;` |
|        - |  200 | `				}` |
|   225646 |  201 | `				if( (pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    35865 |  202 | `					break;` |
|        - |  203 | `				}` |
|   189786 |  204 | `				nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   189786 |  205 | `				if( nSetVis ){` |
|        5 |  206 | `					if( bSetSeen ){` |
|      ! 0 |  207 | `						zTwice = "";` |
|      ! 0 |  208 | `					}` |
|        5 |  209 | `					bSetSeen = 1;` |
|        5 |  210 | `					iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|        5 |  211 | `					bVisSeen = 1;` |
|        5 |  212 | `					pIn += nSetTok;` |
|        5 |  213 | `					continue;` |
|        - |  214 | `				}` |
|        - |  215 | `				{` |
|   189782 |  216 | `					sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   189777 |  217 | `					if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED` |
|   189629 |  218 | `					 \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      185 |  219 | `						if( bReadVis ){` |
|        3 |  220 | `							zTwice = "";` |
|        1 |  221 | `						}` |
|      185 |  222 | `						bReadVis = 1;` |
|      185 |  223 | `						bVisSeen = 1;` |
|      185 |  224 | `						iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      244 |  225 | `							: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|       77 |  226 | `							: PH7_CLASS_PROT_PUBLIC;` |
|   189692 |  227 | `					}else if( nKw == PH7_TKWRD_FINAL ){` |
|        7 |  228 | `						if( bFinal ){` |
|      ! 0 |  229 | `							zTwice = "final";` |
|      ! 0 |  230 | `						}` |
|        7 |  231 | `						bFinal = 1;` |
|   189599 |  232 | `					}else if( nKw == PH7_TKWRD_STATIC ){` |
|        3 |  233 | `						zNotHere = "static";` |
|   189595 |  234 | `					}else if( nKw == PH7_TKWRD_ABSTRACT ){` |
|        3 |  235 | `						zNotHere = "abstract";` |
|        2 |  236 | `					}else{` |
|   189592 |  237 | ``						break; /* the type or the `$name` starts here */`` |
|        - |  238 | `					}` |
|      195 |  239 | `					pIn++;` |
|        - |  240 | `				}` |
|        5 |  241 | `			}` |
|        - |  242 | `			/* The same duplicate rules the class body's run enforces -- php words` |
|        - |  243 | `			 * them identically wherever the modifier was written -- plus the two a` |
|        - |  244 | `			 * PARAMETER never takes, which php words against "a parameter". */` |
|   225470 |  245 | `			if( zTwice ){` |
|        3 |  246 | `				pGen->iFatalTrace = PH7_FATAL_TRACE_NONE;` |
|        4 |  247 | `				rc = zTwice[0]` |
|      ! 0 |  248 | `					? PH7_GenCompileError(pGen,E_ERROR,nModLine,` |
|      ! 0 |  249 | `						"Multiple %s modifiers are not allowed",zTwice)` |
|        2 |  250 | `					: PH7_GenCompileError(pGen,E_ERROR,nModLine,` |
|        - |  251 | `						"Multiple access type modifiers are not allowed");` |
|        3 |  252 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  253 | `					return SXERR_ABORT;` |
|        - |  254 | `				}` |
|        3 |  255 | `				return SXERR_SYNTAX;` |
|        - |  256 | `			}` |
|   225468 |  257 | `			if( zNotHere ){` |
|        6 |  258 | `				pGen->iFatalTrace = PH7_FATAL_TRACE_NONE;` |
|        8 |  259 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nModLine,` |
|        2 |  260 | `					"Cannot use the %s modifier on a parameter",zNotHere);` |
|        6 |  261 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  262 | `					return SXERR_ABORT;` |
|        - |  263 | `				}` |
|        6 |  264 | `				return SXERR_SYNTAX;` |
|        - |  265 | `			}` |
|   225464 |  266 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|        5 |  267 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   225462 |  268 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|      ! 0 |  269 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|      ! 0 |  270 | `			}` |
|   225464 |  271 | `			if( bFinal ){` |
|        7 |  272 | `				sArg.iFlags \|= VM_FUNC_ARG_FINAL;` |
|        3 |  273 | `			}` |
|   225464 |  274 | `			if( bVisSeen \|\| bReadonly \|\| bFinal ){` |
|      187 |  275 | `				if( !bCtorCtx ){` |
|        6 |  276 | `					if( bAbstractCtx ){` |
|        3 |  277 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - |  278 | `							"Cannot declare promoted property in an abstract constructor");` |
|        2 |  279 | `					}else{` |
|        3 |  280 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - |  281 | `							"Cannot declare promoted property outside a constructor");` |
|        - |  282 | `					}` |
|        6 |  283 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  284 | `						return SXERR_ABORT;` |
|        - |  285 | `					}` |
|        6 |  286 | `					return SXERR_SYNTAX;` |
|        - |  287 | `				}` |
|      183 |  288 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      183 |  289 | `				sArg.iPromoteVis = iVis;` |
|      183 |  290 | `				if( bReadonly ){` |
|       33 |  291 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|       14 |  292 | `				}` |
|       89 |  293 | `			}` |
|        - |  294 | `		}` |
|        - |  295 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   225455 |  296 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   218206 |  297 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   210778 |  298 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   210423 |  299 | `			sxu32 nLineLocal = pIn->nLine;` |
|   210423 |  300 | `			sxi32 iTFlags = 0;` |
|   210423 |  301 | `			pGen->pIn = pIn;` |
|   210423 |  302 | `			rc = GenStateParseUnionTypeDecl(` |
|   105058 |  303 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   105058 |  304 | `				&iTFlags, &sArg.sTypeName,` |
|        - |  305 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|        - |  306 | `				/* bAllowVoid */ 0, /* bParamCtx */ 1,` |
|   105058 |  307 | `						nLineLocal);` |
|   210423 |  308 | `			pIn = pGen->pIn;` |
|   210423 |  309 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  310 | `				return SXERR_ABORT;` |
|   210423 |  311 | `			}else if( rc == SXERR_CORRUPT ){` |
|        - |  312 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|        3 |  313 | `				return SXERR_SYNTAX;` |
|   210421 |  314 | `			}else if( rc == SXERR_SYNTAX ){` |
|       20 |  315 | `				if( pIn < pEnd ){` |
|       28 |  316 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|        - |  317 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|        8 |  318 | `						&pIn->sData);` |
|       12 |  319 | `				}else{` |
|      ! 0 |  320 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|        - |  321 | `						"syntax error, unexpected end of file");` |
|        - |  322 | `				}` |
|       20 |  323 | `				return SXERR_SYNTAX;` |
|        - |  324 | `			}` |
|   210405 |  325 | `			sArg.iFlags \|= iTFlags;` |
|   105049 |  326 | `		}` |
|   225442 |  327 | `		if( pIn >= pEnd ){` |
|      ! 0 |  328 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,"Missing argument name");` |
|      ! 0 |  329 | `			return rc;` |
|        - |  330 | `		}` |
|   225442 |  331 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|        - |  332 | `			/* Pass by reference,record that */` |
|      447 |  333 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|      447 |  334 | `			pIn++;` |
|      221 |  335 | `		}` |
|   225442 |  336 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|        - |  337 | `			/* Variadic parameter: ...$args */` |
|     7001 |  338 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|     7001 |  339 | `			pIn++;` |
|     3493 |  340 | `		}` |
|   225442 |  341 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  342 | `			/* Invalid argument */` |
|      ! 0 |  343 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|      ! 0 |  344 | `			return rc;` |
|        - |  345 | `		}` |
|   225442 |  346 | `		pIn++; /* Jump the dollar sign */` |
|        - |  347 | `		/* Copy argument name */` |
|   225442 |  348 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   225442 |  349 | `		if( zDup == 0 ){` |
|      ! 0 |  350 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  351 | `			return SXERR_ABORT;` |
|        - |  352 | `		}` |
|   225442 |  353 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   225442 |  354 | `		pIn++;` |
|   225442 |  355 | `		if( pIn < pEnd ){` |
|   104976 |  356 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|        - |  357 | `				SyToken *pDefend;` |
|    41113 |  358 | `				sxi32 iNest = 0;` |
|    41113 |  359 | `				pIn++; /* Jump the equal sign */` |
|    41113 |  360 | `				pDefend = pIn;` |
|        - |  361 | `				/* Process the default value associated with this argument */` |
|    82543 |  362 | `				while( pDefend < pEnd ){` |
|    55177 |  363 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    13747 |  364 | `						break;` |
|        - |  365 | `					}` |
|    41435 |  366 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|        - |  367 | `						/* Increment nesting level */` |
|       71 |  368 | `						iNest++;` |
|    41402 |  369 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|        - |  370 | `						/* Decrement nesting level */` |
|       71 |  371 | `						iNest--;` |
|       33 |  372 | `					}` |
|    41435 |  373 | `					pDefend++;` |
|        5 |  374 | `				}` |
|    41113 |  375 | `				if( pIn >= pDefend ){` |
|        3 |  376 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,pIn->nLine,"Missing argument default value");` |
|        3 |  377 | `					return rc;` |
|        - |  378 | `				}` |
|        - |  379 | `				/* Process default value */` |
|    41111 |  380 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|    41111 |  381 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  382 | `					return rc;` |
|        - |  383 | `				}` |
|        - |  384 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|        - |  385 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|        - |  386 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|        - |  387 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|        - |  388 | `				 * arg-type check lets null through. */` |
|    41106 |  389 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|    37420 |  390 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|    37413 |  391 | `					&& &pIn[1] == pDefend` |
|    33712 |  392 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|    26926 |  393 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|    10082 |  394 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|        - |  395 | `` 					/* php 8.4 DEPRECATED the implicit-nullable form (`int $x = null` `` |
|        - |  396 | ``					 * without the `?`). PHL targets php's *non-deprecated* surface and`` |
|        - |  397 | ``					 * rejects it outright — the explicit `?int` must be written.`` |
|        - |  398 | ``					 * `mixed $x = null` is fine: mixed already includes null (explicit`` |
|        - |  399 | `					 * ?T / T\|null are already excluded via VM_FUNC_ARG_NULLABLE above). */` |
|        4 |  400 | `					if( sArg.sClass.nByte == sizeof("mixed")-1` |
|        5 |  401 | `						&& SyStrnicmp(SyStringData(&sArg.sClass),"mixed",sizeof("mixed")-1) == 0 ){` |
|        3 |  402 | `						sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|        2 |  403 | `					}else{` |
|        3 |  404 | `						const char *zSep = "";` |
|        3 |  405 | `						SyString sCls = { "", 0 };` |
|        3 |  406 | `						if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|      ! 0 |  407 | `							sCls = ((ph7_class *)pFunc->pUserData)->sName;` |
|      ! 0 |  408 | `							zSep = "::";` |
|      ! 0 |  409 | `						}` |
|        4 |  410 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,` |
|        - |  411 | `							"%z%s%z(): Cannot use null as the default for non-nullable parameter $%z; write the explicit ?T type instead",` |
|        1 |  412 | `							&sCls,zSep,&pFunc->sName,&sArg.sName);` |
|        3 |  413 | `						return SXERR_ABORT;` |
|        - |  414 | `					}` |
|        1 |  415 | `				}` |
|        - |  416 | `				/* Point beyond the default value */` |
|    41109 |  417 | `				pIn = pDefend;` |
|    20524 |  418 | `			}` |
|   104972 |  419 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 |  420 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|      ! 0 |  421 | `				return rc;` |
|        - |  422 | `			}` |
|   104972 |  423 | `			pIn++; /* Jump the trailing comma */` |
|    52407 |  424 | `		}` |
|        - |  425 | `		/* Append argument signature */` |
|   225438 |  426 | `		if( sArg.nType > 0 ){` |
|   210259 |  427 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|        - |  428 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    20766 |  429 | `				int marker = 'o';` |
|    20766 |  430 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    20766 |  431 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    10370 |  432 | `			}else{` |
|        - |  433 | `				int c;` |
|   189498 |  434 | `				c = 'n'; /* cc warning */` |
|        - |  435 | `				/* Type leading character */` |
|   189498 |  436 | `				switch(sArg.nType){` |
|    13529 |  437 | `				case MEMOBJ_HASHMAP:` |
|        - |  438 | `					/* Hashmap aka 'array' */` |
|    27026 |  439 | `					c = 'h';` |
|    27026 |  440 | `					break;` |
|    23841 |  441 | `				case MEMOBJ_INT:` |
|        - |  442 | `					/* Integer */` |
|    47620 |  443 | `					c = 'i';` |
|    47620 |  444 | `					break;` |
|     3404 |  445 | `				case MEMOBJ_BOOL:` |
|        - |  446 | `					/* Bool */` |
|     6803 |  447 | `					c = 'b';` |
|     6803 |  448 | `					break;` |
|    16838 |  449 | `				case MEMOBJ_REAL:` |
|        - |  450 | `					/* Float */` |
|    33636 |  451 | `					c = 'f';` |
|    33636 |  452 | `					break;` |
|    37248 |  453 | `				case MEMOBJ_STRING:` |
|        - |  454 | `					/* String */` |
|    74389 |  455 | `					c = 's';` |
|    74389 |  456 | `					break;` |
|       21 |  457 | `				case MEMOBJ_OBJ:` |
|        - |  458 | `					/* Object */` |
|       46 |  459 | `					c = 'o';` |
|       42 |  460 | `					break;` |
|        1 |  461 | `				default:` |
|        2 |  462 | `					break;` |
|        - |  463 | `				}` |
|   189498 |  464 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|        - |  465 | `			}` |
|   104981 |  466 | `		}else{` |
|        - |  467 | `			/* No type is associated with this parameter which mean` |
|        - |  468 | `			 * that this function is not condidate for overloading.` |
|        - |  469 | `			 */` |
|    15184 |  470 | `			SyBlobRelease(&sSig);` |
|        - |  471 | `		}` |
|        - |  472 | `		/* php's attribute placement rules, once the promotion modifiers are read:` |
|        - |  473 | `` 		 * a PROMOTED parameter is a property as well, so `#[\Override] public $p` `` |
|        - |  474 | `		 * in a constructor signature is accepted here and judged as the property` |
|        - |  475 | `		 * claim it is -- while php still NAMES the target "parameter". */` |
|   337989 |  476 | `		if( GenStateCheckAttrPlacement(&(*pGen),&sArg.aAttrs,32,` |
|   337994 |  477 | `				(sArg.iFlags & VM_FUNC_ARG_PROMOTED) ? (32\|8) : 32,0,0) == SXERR_ABORT ){` |
|      ! 0 |  478 | `			return SXERR_ABORT;` |
|        - |  479 | `		}` |
|        - |  480 | `		/* Save in the argument set */` |
|   225438 |  481 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|        5 |  482 | `	}` |
|   147841 |  483 | `	if( SyBlobLength(&sSig) > 0 ){` |
|        - |  484 | `		/* Save function signature */` |
|   135759 |  485 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|    67782 |  486 | `	}` |
|   147841 |  487 | `	return SXRET_OK;` |
|    73836 |  488 | `}` |
|        - |  489 | `/*` |
|        - |  490 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|        - |  491 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|        - |  492 | ` * the enclosing function. Returns the token just past the nested construct.` |
|        - |  493 | ` */` |
|      424 |  494 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|        5 |  495 | `{` |
|      429 |  496 | `	sxi32 iParen = 0;` |
|      429 |  497 | `	pIn++; /* past 'function'/'fn' */` |
|        - |  498 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|        - |  499 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|        - |  500 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|     3021 |  501 | `	while( pIn < pEnd ){` |
|     3021 |  502 | `		sxu32 t = pIn->nType;` |
|     3021 |  503 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|     2477 |  504 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|     1933 |  505 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|     1509 |  506 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|     2597 |  507 | `		pIn++;` |
|        5 |  508 | `	}` |
|      429 |  509 | `	if( pIn >= pEnd ){ return pIn; }` |
|        - |  510 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|        - |  511 | `	{` |
|      429 |  512 | `		sxi32 d = 0;` |
|     4475 |  513 | `		while( pIn < pEnd ){` |
|     4475 |  514 | `			sxu32 t = pIn->nType;` |
|     4475 |  515 | `			if( t & PH7_TK_OCB ){ d++; }` |
|     4029 |  516 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|     4051 |  517 | `			pIn++;` |
|        5 |  518 | `		}` |
|        - |  519 | `	}` |
|      429 |  520 | `	return pIn;` |
|      217 |  521 | `}` |
|        - |  522 | `/*` |
|        - |  523 | `` * ROOT C helper: from an `fn` keyword token, skip the whole arrow function -- its`` |
|        - |  524 | ` * signature, its optional return type and the single expression that is its body.` |
|        - |  525 | `` * The body ends where the expression parser would end it: a `,` or `;` at nesting`` |
|        - |  526 | ` * depth zero, or a closer that would unbalance the group the arrow sits in.` |
|        - |  527 | ` */` |
|      734 |  528 | `static SyToken * GenStateSkipArrowBody(SyToken *pIn, SyToken *pEnd)` |
|        5 |  529 | `{` |
|      739 |  530 | `	sxi32 iNest = 0;` |
|      739 |  531 | `	pIn++; /* past 'fn' */` |
|        - |  532 | ``	/* The signature: skip the balanced `( … )`. Only an optional `&` can stand`` |
|        - |  533 | ``	 * between `fn` and the `(`, so anything that ends a statement or a block first`` |
|        - |  534 | `	 * means this was never an arrow function -- stop rather than run off into the` |
|        - |  535 | ``	 * enclosing body, where a real `yield` would then go unseen. */`` |
|      799 |  536 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       73 |  537 | `		if( pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB\|PH7_TK_CCB) ){ return pIn; }` |
|       61 |  538 | `		pIn++;` |
|        1 |  539 | `	}` |
|     2046 |  540 | `	while( pIn < pEnd ){` |
|     2046 |  541 | `		sxu32 t = pIn->nType;` |
|     2046 |  542 | `		if( t & PH7_TK_LPAREN ){ iNest++; }` |
|     1324 |  543 | `		else if( t & PH7_TK_RPAREN ){ iNest--; if( iNest <= 0 ){ pIn++; break; } }` |
|     1324 |  544 | `		pIn++;` |
|        5 |  545 | `	}` |
|        - |  546 | ``	/* The `=>` that opens the body (a return type may sit before it). */`` |
|     1399 |  547 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     1117 |  548 | `		if( pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB) ){ return pIn; }` |
|      677 |  549 | `		pIn++;` |
|        5 |  550 | `	}` |
|      286 |  551 | `	if( pIn < pEnd ){ pIn++; } /* past '=>' */` |
|        - |  552 | `	/* The body expression. */` |
|      286 |  553 | `	iNest = 0;` |
|     2445 |  554 | `	while( pIn < pEnd ){` |
|     2445 |  555 | `		sxu32 t = pIn->nType;` |
|     2445 |  556 | `		if( t & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){ iNest++; }` |
|     2155 |  557 | `		else if( t & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      407 |  558 | `			if( iNest <= 0 ){ break; }` |
|      294 |  559 | `			iNest--;` |
|     1895 |  560 | `		}else if( (t & (PH7_TK_COMMA\|PH7_TK_SEMI)) && iNest <= 0 ){` |
|      173 |  561 | `			break;` |
|        - |  562 | `		}` |
|     2163 |  563 | `		pIn++;` |
|        4 |  564 | `	}` |
|      286 |  565 | `	return pIn;` |
|      371 |  566 | `}` |
|        - |  567 | `/*` |
|        - |  568 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|        - |  569 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|        - |  570 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|        - |  571 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|        - |  572 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|        - |  573 | ` * detached-mini-program path untouched.` |
|        - |  574 | ` */` |
|        - |  575 | `/*` |
|        - |  576 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|        - |  577 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|        - |  578 | ` * mixed, object.` |
|        - |  579 | ` */` |
|       30 |  580 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|        4 |  581 | `{` |
|        - |  582 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|        - |  583 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|        - |  584 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|        - |  585 | `	};` |
|        - |  586 | `	sxu32 i;` |
|       34 |  587 | `	if( nName > 0 && zName[0] == '\\' ){` |
|      ! 0 |  588 | `		zName++;` |
|      ! 0 |  589 | `		nName--;` |
|      ! 0 |  590 | `	}` |
|       48 |  591 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|       48 |  592 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|       34 |  593 | `			return 1;` |
|        - |  594 | `		}` |
|        8 |  595 | `	}` |
|      ! 0 |  596 | `	return 0;` |
|       19 |  597 | `}` |
|        - |  598 | `/*` |
|        - |  599 | ` * One atom of a generator's declared return type: is it a supertype of` |
|        - |  600 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|        - |  601 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|        - |  602 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|        - |  603 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|        - |  604 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|        - |  605 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|        - |  606 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|        - |  607 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|        - |  608 | ` * both rather than fatal on valid code (a recorded divergence).` |
|        - |  609 | ` */` |
|       32 |  610 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|        4 |  611 | `{` |
|       36 |  612 | `	if( nType == MEMOBJ_OBJ ){` |
|      ! 0 |  613 | ``		return 1; /* bare `object` */`` |
|        - |  614 | `	}` |
|       36 |  615 | `	if( nType != SXU32_HIGH ){` |
|        3 |  616 | `		return 0; /* scalar/array/void/never/null/... */` |
|        - |  617 | `	}` |
|       34 |  618 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|       34 |  619 | `		return 1;` |
|        - |  620 | `	}` |
|        - |  621 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|        - |  622 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|        - |  623 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|        - |  624 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|        - |  625 | `	{` |
|        - |  626 | `		SyBlob sFQN;` |
|        - |  627 | `		int bOk;` |
|      ! 0 |  628 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      ! 0 |  629 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|      ! 0 |  630 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|      ! 0 |  631 | `		SyBlobRelease(&sFQN);` |
|      ! 0 |  632 | `		return bOk;` |
|        - |  633 | `	}` |
|       20 |  634 | `}` |
|        - |  635 | `/*` |
|        - |  636 | ` * php 8: a generator function may only declare a return type that is a` |
|        - |  637 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|        - |  638 | ` * group qualifies only if every member does. Anything else is php's exact` |
|        - |  639 | ` * compile-time fatal "Generator return type must be a supertype of` |
|        - |  640 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|        - |  641 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|        - |  642 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|        - |  643 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|        - |  644 | ` */` |
|      442 |  645 | `PH7_PRIVATE sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|        5 |  646 | `{` |
|      447 |  647 | `	int bOk = 0;` |
|        - |  648 | `	sxu32 nLine;` |
|        - |  649 | `	sxi32 rc;` |
|      447 |  650 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|      415 |  651 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|        - |  652 | `	}` |
|       36 |  653 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|      ! 0 |  654 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  655 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|        - |  656 | `		sxu32 i,j;` |
|      ! 0 |  657 | `		for( i = 0; i < n && !bOk; i++ ){` |
|        - |  658 | `			int bGroupOk;` |
|      ! 0 |  659 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|      ! 0 |  660 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|        - |  661 | `			}` |
|      ! 0 |  662 | `			bGroupOk = 1;` |
|      ! 0 |  663 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|      ! 0 |  664 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|      ! 0 |  665 | `					bGroupOk = 0;` |
|      ! 0 |  666 | `					break;` |
|        - |  667 | `				}` |
|      ! 0 |  668 | `			}` |
|      ! 0 |  669 | `			bOk = bGroupOk;` |
|      ! 0 |  670 | `		}` |
|      ! 0 |  671 | `	}else{` |
|       36 |  672 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|        - |  673 | `	}` |
|       36 |  674 | `	if( bOk ){` |
|       34 |  675 | `		return SXRET_OK;` |
|        - |  676 | `	}` |
|        - |  677 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|        - |  678 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|        - |  679 | `	 * token of this stream — its line is the function's closing brace. php` |
|        - |  680 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|        - |  681 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|        3 |  682 | `	nLine = pGen->pIn[-1].nLine;` |
|        - |  683 | `	{` |
|        3 |  684 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|        3 |  685 | `		if( sGiven.nByte < 1 ){` |
|      ! 0 |  686 | `			sGiven = pFunc->sReturnClass;` |
|      ! 0 |  687 | `		}` |
|        3 |  688 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  689 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|        - |  690 | `	}` |
|        3 |  691 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      226 |  692 | `}` |
|   162715 |  693 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|        5 |  694 | `{` |
|   162720 |  695 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|   162720 |  696 | `	SyToken *pEnd = pGen->pEnd;` |
|   162720 |  697 | `	sxi32 iDepth = 0;` |
|   162720 |  698 | `	int bStarted = 0;` |
| 19286359 |  699 | `	while( pIn < pEnd ){` |
| 19286359 |  700 | `		sxu32 t = pIn->nType;` |
| 19286359 |  701 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 18439285 |  702 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 17592795 |  703 | `		if( t & PH7_TK_KEYWORD ){` |
|  1577419 |  704 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  1577419 |  705 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  1576995 |  706 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|        - |  707 | ``			/* An arrow body is a single EXPRESSION, but php takes a `yield` in one --`` |
|        - |  708 | ``			 * `fn() => yield 7` is a generator, whose yield belongs to the ARROW. Skip`` |
|        - |  709 | `			 * it, or the enclosing function would be classified a generator by a yield` |
|        - |  710 | `			 * that is not its own. */` |
|  1576571 |  711 | `			if( kw == PH7_TKWRD_FN ){ pIn = GenStateSkipArrowBody(pIn,pEnd); continue; }` |
|   786849 |  712 | `		}` |
| 17591213 |  713 | `		pIn++;` |
|        5 |  714 | `	}` |
|   162296 |  715 | `	return FALSE;` |
|    81242 |  716 | `}` |
|        - |  717 | `/*` |
|        - |  718 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|        - |  719 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  720 | ` * and this routine takes care of generating the appropriate error message.` |
|        - |  721 | ` */` |
|   162715 |  722 | `PH7_PRIVATE sxi32 GenStateCompileFuncBody(` |
|        - |  723 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |  724 | `	ph7_vm_func *pFunc    /* Function state */` |
|        - |  725 | `	)` |
|        5 |  726 | `{` |
|        - |  727 | `	SySet *pInstrContainer; /* Instruction container */` |
|        - |  728 | `	GenBlock *pBlock;` |
|        - |  729 | `	sxu32 nGotoOfft;` |
|        - |  730 | `	sxi32 rc;` |
|        - |  731 | `	/* Attach the new function */` |
|   162720 |  732 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   162720 |  733 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  734 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|        - |  735 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  736 | `		return SXERR_ABORT;` |
|        - |  737 | `	}` |
|   162720 |  738 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|        - |  739 | `	/* Swap bytecode containers */` |
|   162720 |  740 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   162720 |  741 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|        - |  742 | `	/* Emit constructor property promotion prologue:` |
|        - |  743 | `	 *   $this->NAME = $NAME;` |
|        - |  744 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|        - |  745 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|        - |  746 | `	{` |
|   162720 |  747 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|        - |  748 | `		sxu32 i;` |
|   387163 |  749 | `		for( i = 0; i < nArg; i++ ){` |
|   224448 |  750 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|        - |  751 | `			char *zSrc;` |
|        - |  752 | `			sxu32 nSrc,nName;` |
|        - |  753 | `			SySet sToken;` |
|        - |  754 | `			SyToken *pTmpIn,*pTmpEnd;` |
|        - |  755 | `			sxi32 rcPromote;` |
|   224448 |  756 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   224280 |  757 | `				continue;` |
|        - |  758 | `			}` |
|        - |  759 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|        - |  760 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|        - |  761 | `			 * copied), so it must outlive the function — never free it. The` |
|        - |  762 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|        - |  763 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      173 |  764 | `			nName = SyStringLength(&pArg->sName);` |
|      173 |  765 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      173 |  766 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      173 |  767 | `			if( zSrc == 0 ){` |
|      ! 0 |  768 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  769 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  770 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  771 | `				return SXERR_ABORT;` |
|        - |  772 | `			}` |
|        - |  773 | `			{` |
|      173 |  774 | `				char *z = zSrc;` |
|      173 |  775 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      173 |  776 | `				z += sizeof("$this->")-1;` |
|      173 |  777 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      173 |  778 | `				z += nName;` |
|      173 |  779 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      173 |  780 | `				z += sizeof(" = $")-1;` |
|      173 |  781 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      173 |  782 | `				z += nName;` |
|      173 |  783 | `				*z = 0;` |
|        - |  784 | `			}` |
|      173 |  785 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      173 |  786 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|      173 |  787 | `			pTmpIn = pGen->pIn;` |
|      173 |  788 | `			pTmpEnd = pGen->pEnd;` |
|      173 |  789 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      173 |  790 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      173 |  791 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      173 |  792 | `			pGen->pIn = pTmpIn;` |
|      173 |  793 | `			pGen->pEnd = pTmpEnd;` |
|      173 |  794 | `			SySetRelease(&sToken);` |
|      173 |  795 | `			if( rcPromote == SXERR_ABORT ){` |
|      ! 0 |  796 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  797 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  798 | `				return SXERR_ABORT;` |
|        - |  799 | `			}` |
|        - |  800 | `			/* Discard the assignment result — this is a statement expression. */` |
|      173 |  801 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       89 |  802 | `		}` |
|        - |  803 | `	}` |
|        - |  804 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|        - |  805 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|        - |  806 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|        - |  807 | `	 * generator — and vice versa — is classified independently. */` |
|        - |  808 | `	{` |
|   162720 |  809 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|   162720 |  810 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|        - |  811 | `		/* Compile the body */` |
|   162720 |  812 | `		PH7_CompileBlock(&(*pGen),0);` |
|   162720 |  813 | `		pGen->bInGenerator = bSavedGen;` |
|        - |  814 | `	}` |
|        - |  815 | `	/* Fix exception jumps now the destination is resolved */` |
|   162720 |  816 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - |  817 | `	/* Emit the final return if not yet done */` |
|   162720 |  818 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - |  819 | `	/* Fix gotos jumps now the destination is resolved */` |
|   162720 |  820 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|      ! 0 |  821 | `		rc = SXERR_ABORT;` |
|      ! 0 |  822 | `	}` |
|   162720 |  823 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|        - |  824 | `	/* Restore the default container */` |
|   162720 |  825 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - |  826 | `	/* Leave function block */` |
|   162720 |  827 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   162720 |  828 | `	if( rc == SXERR_ABORT ){` |
|        - |  829 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  830 | `		return SXERR_ABORT;` |
|        - |  831 | `	}` |
|        - |  832 | `	/* Scan for yield opcodes to detect generator functions */` |
|        - |  833 | `	{` |
|   162720 |  834 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|        - |  835 | `		sxu32 i;` |
| 12573169 |  836 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 12410878 |  837 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|      429 |  838 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      429 |  839 | `				break;` |
|        - |  840 | `			}` |
|  6196638 |  841 | `		}` |
|        - |  842 | `	}` |
|   162720 |  843 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  844 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|      429 |  845 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|      ! 0 |  846 | `			return SXERR_ABORT;` |
|        - |  847 | `		}` |
|      212 |  848 | `	}` |
|        - |  849 | `	/* This body is finished: the emit container has been restored above, so nothing` |
|        - |  850 | `	 * appends to it again, and the doubling slack it is holding -- up to as much as` |
|        - |  851 | `	 * it uses -- is dead for the rest of the process. The execution paths re-read` |
|        - |  852 | `	 * SySetBasePtr on every invocation, and the first invocation cannot precede this` |
|        - |  853 | `	 * point, so moving the buffer here is invisible to them. */` |
|   162720 |  854 | `	SySetShrinkToFit(&pFunc->aByteCode);` |
|        - |  855 | `	/* All done, function body compiled */` |
|   162720 |  856 | `	return SXRET_OK;` |
|    81242 |  857 | `}` |
|        - |  858 | `/*` |
|        - |  859 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|        - |  860 | ` * According to the PHP language reference manual.` |
|        - |  861 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|        - |  862 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|        - |  863 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|        - |  864 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - |  865 | ` *  Functions need not be defined before they are referenced.` |
|        - |  866 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|        - |  867 | ` *  a function even if they were defined inside and vice versa.` |
|        - |  868 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|        - |  869 | ` *  calls with over 32-64 recursion levels.` |
|        - |  870 | ` *` |
|        - |  871 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|        - |  872 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|        - |  873 | ` * on these extension.` |
|        - |  874 | ` */` |
|        - |  875 | `/*` |
|        - |  876 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|        - |  877 | ` */` |
|    68988 |  878 | `PH7_PRIVATE int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|        5 |  879 | `{` |
|        - |  880 | `	sxu32 i;` |
|    98815 |  881 | `	for( i = 0; i < n; i++ ){` |
|    91401 |  882 | `		int a = zA[i], b = zB[i];` |
|    91401 |  883 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|    91401 |  884 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|    91401 |  885 | `		if( a != b ) return a - b;` |
|    14868 |  886 | `	}` |
|     7419 |  887 | `	return 0;` |
|    34439 |  888 | `}` |
|        - |  889 | `/*` |
|        - |  890 | ` * Internal type-atom kinds used during union type parsing.` |
|        - |  891 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|        - |  892 | ` * (which are positive bit values stored in sxu32).` |
|        - |  893 | ` */` |
|        - |  894 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|        - |  895 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|        - |  896 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|        - |  897 |  |
|        - |  898 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|        - |  899 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|        - |  900 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|        - |  901 |  |
|        - |  902 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|        - |  903 | `struct PhlTypeAtom {` |
|        - |  904 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|        - |  905 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|        - |  906 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|        - |  907 | `	sxu32 nCanon;` |
|        - |  908 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|        - |  909 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|        - |  910 | `};` |
|        - |  911 |  |
|        - |  912 | `/*` |
|        - |  913 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|        - |  914 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|        - |  915 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|        - |  916 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|        - |  917 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|        - |  918 | ` * already be consumed by the caller.` |
|        - |  919 | ` */` |
|        - |  920 | `/*` |
|        - |  921 | ` * TRUE if pName is a reserved PHP type keyword (never a class name), so a type` |
|        - |  922 | ` * hint that spells it must not be namespace-qualified. Only the words that can` |
|        - |  923 | ` * reach GenStateParseOneTypeAtom's identifier branch as a bare name matter here` |
|        - |  924 | ` * (bool/int/float/string/array/object/self/static/parent arrive as keywords, and` |
|        - |  925 | ` * null/void/never are matched before the class path), but the full set is listed` |
|        - |  926 | ` * so the guard is robust to lexer changes.` |
|        - |  927 | ` */` |
|    54854 |  928 | `static int GenStateIsReservedTypeWord(const SyString *pName)` |
|        5 |  929 | `{` |
|        - |  930 | `	static const char *azWords[] = {` |
|        - |  931 | `		"false","true","mixed","iterable","callable","null","void","never",` |
|        - |  932 | `		"bool","boolean","int","integer","float","double","string","array",` |
|        - |  933 | `		"object","self","static","parent"` |
|        - |  934 | `	};` |
|        - |  935 | `	sxu32 i;` |
|   106355 |  936 | `	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){` |
|   105883 |  937 | `		sxu32 n = (sxu32)SyStrlen(azWords[i]);` |
|   105883 |  938 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azWords[i],n) == 0 ){` |
|    54387 |  939 | `			return 1;` |
|        - |  940 | `		}` |
|    25718 |  941 | `	}` |
|      477 |  942 | `	return 0;` |
|    27394 |  943 | `}` |
|        - |  944 | `/*` |
|        - |  945 | ` * php's builtin type words as looked up BY NAME (zend_lookup_builtin_type_by_name):` |
|        - |  946 | ``  * every type a declaration can spell with a plain label. `array` and `callable` `` |
|        - |  947 | ` * are deliberately absent — php's parser hands those two their own tokens — which` |
|        - |  948 | ``  * is why a qualified `\array` takes the "reserved" wording below while `\int` `` |
|        - |  949 | ` * takes "must be unqualified".` |
|        - |  950 | ` */` |
|       74 |  951 | `static int GenStateIsBuiltinTypeWord(const SyString *pName)` |
|        4 |  952 | `{` |
|        - |  953 | `	static const char *azWords[] = {` |
|        - |  954 | `		"int","float","string","bool","void","iterable","object","mixed",` |
|        - |  955 | `		"never","null","false","true"` |
|        - |  956 | `	};` |
|        - |  957 | `	sxu32 i;` |
|      918 |  958 | `	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){` |
|      848 |  959 | `		sxu32 n = (sxu32)SyStrlen(azWords[i]);` |
|      848 |  960 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azWords[i],n) == 0 ){` |
|        6 |  961 | `			return 1;` |
|        - |  962 | `		}` |
|      423 |  963 | `	}` |
|       73 |  964 | `	return 0;` |
|       41 |  965 | `}` |
|        - |  966 | `/* The three class keywords that name a class RELATIVE to the current one. php` |
|        - |  967 | ` * refuses to see them behind a qualifier at all, with its own wording. */` |
|       70 |  968 | `static int GenStateIsRelativeClassWord(const SyString *pName)` |
|        3 |  969 | `{` |
|       74 |  970 | `	return (pName->nByte == 4 && SyStrnicmp(pName->zString,"self",4) == 0)` |
|       69 |  971 | `	    \|\| (pName->nByte == 6 && SyStrnicmp(pName->zString,"parent",6) == 0)` |
|      105 |  972 | `	    \|\| (pName->nByte == 6 && SyStrnicmp(pName->zString,"static",6) == 0);` |
|        3 |  973 | `}` |
|        - |  974 | `/*` |
|        - |  975 | ` * php's reserved-name screens on a class-like type atom, in php's own order.` |
|        - |  976 | ` * A name is screened only once a qualifier is present — a BARE reserved word is` |
|        - |  977 | ` * a type, and the keyword / null-void-never branches above have already claimed` |
|        - |  978 | ` * every one of them.` |
|        - |  979 | ` *` |
|        - |  980 | ` *   \int, namespace\false   -> "Type declaration 'int' must be unqualified"` |
|        - |  981 | ` *   \self, \parent, \static -> "'\self' is an invalid class name"` |
|        - |  982 | ` *   \array, \callable, A\int, A\self` |
|        - |  983 | ` *                           -> "Cannot use \"NAME\" as a type name as it is` |
|        - |  984 | ` *                              reserved", NAME being the name after resolution` |
|        - |  985 | ` *` |
|        - |  986 | ` * pLastSeg is the atom's trailing segment AS WRITTEN (php's screen looks only at` |
|        - |  987 | ` * that), pResolved the whole atom after namespace resolution.` |
|        - |  988 | ` */` |
|   381998 |  989 | `static sxi32 GenStateScreenTypeName(` |
|        - |  990 | `	ph7_gen_state *pGen,` |
|        - |  991 | `	const SyString *pLastSeg,  /* trailing segment, as written */` |
|        - |  992 | `	const SyString *pResolved, /* whole atom, after resolution */` |
|        - |  993 | `	int bMulti,                /* the written name had more than one segment */` |
|        - |  994 | ``	int bFQ,                   /* written absolute (`\X`) or `namespace\X` */`` |
|        - |  995 | `	sxu32 nLine` |
|        5 |  996 | `){` |
|        - |  997 | `	sxi32 rc;` |
|   382003 |  998 | `	if( !bMulti && !bFQ ){` |
|   381947 |  999 | `		return SXRET_OK;` |
|        - | 1000 | `	}` |
|       60 | 1001 | `	if( !bMulti ){` |
|       25 | 1002 | `		if( GenStateIsBuiltinTypeWord(pLastSeg) ){` |
|        - | 1003 | `			char zLower[16];` |
|        - | 1004 | `			sxu32 i;` |
|        9 | 1005 | `			for( i = 0 ; i < pLastSeg->nByte && i < sizeof(zLower) ; i++ ){` |
|        7 | 1006 | `				unsigned char c = (unsigned char)pLastSeg->zString[i];` |
|        7 | 1007 | `				zLower[i] = (char)((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);` |
|        4 | 1008 | `			}` |
|        4 | 1009 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1010 | `				"Type declaration '%.*s' must be unqualified",(int)i,zLower);` |
|        3 | 1011 | `			return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1012 | `		}` |
|       23 | 1013 | `		if( GenStateIsRelativeClassWord(pLastSeg) ){` |
|        4 | 1014 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1015 | `				"'\\%z' is an invalid class name",pLastSeg);` |
|        3 | 1016 | `			return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1017 | `		}` |
|        9 | 1018 | `	}` |
|       52 | 1019 | `	if( GenStateIsBuiltinTypeWord(pLastSeg) \|\| GenStateIsRelativeClassWord(pLastSeg)` |
|       50 | 1020 | `	 \|\| (pLastSeg->nByte == 5 && SyStrnicmp(pLastSeg->zString,"array",5) == 0)` |
|       54 | 1021 | `	 \|\| (pLastSeg->nByte == 8 && SyStrnicmp(pLastSeg->zString,"callable",8) == 0) ){` |
|        4 | 1022 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1023 | `			"Cannot use \"%z\" as a type name as it is reserved",pResolved);` |
|        3 | 1024 | `		return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1025 | `	}` |
|       53 | 1026 | `	return SXRET_OK;` |
|   190731 | 1027 | `}` |
|   382002 | 1028 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|        5 | 1029 | `{` |
|   382007 | 1030 | `	SyToken *pIn = pGen->pIn;` |
|        - | 1031 | `	SyString sLastSeg;      /* trailing segment of the atom, as written */` |
|        - | 1032 | `	sxu32 nLine;` |
|   382007 | 1033 | `	int bAbsolute = 0;` |
|        - | 1034 | `	int bQualified;` |
|   382007 | 1035 | `	int bMulti = 0;         /* the written name had more than one segment */` |
|   382007 | 1036 | ``	int bFQ = 0;            /* written absolute or `namespace\`-relative */`` |
|        - | 1037 | `	sxi32 rcScreen;` |
|   382007 | 1038 | `	SyStringInitFromBuf(&sLastSeg, 0, 0);` |
|   382007 | 1039 | `	SyZero(pOut, sizeof(*pOut));` |
|   382007 | 1040 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   382007 | 1041 | `	if( pIn >= pGen->pEnd ){` |
|      ! 0 | 1042 | `		return SXERR_SYNTAX;` |
|        - | 1043 | `	}` |
|   382007 | 1044 | `	nLine = pIn->nLine;` |
|        - | 1045 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   382007 | 1046 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       25 | 1047 | `		bAbsolute = bFQ = 1; /* fully-qualified: never prefix the current namespace */` |
|       25 | 1048 | `		pIn++;` |
|       25 | 1049 | `		if( pIn >= pGen->pEnd ){` |
|      ! 0 | 1050 | `			return SXERR_SYNTAX;` |
|        - | 1051 | `		}` |
|       11 | 1052 | `	}` |
|        - | 1053 | ``	/* `namespace\X` type hint: the CURRENT namespace spelled out, fully qualified`` |
|        - | 1054 | `` 	 * from there. Collected here rather than below because the leading `namespace` `` |
|        - | 1055 | `	 * is a KEYWORD token, which the atom parser would otherwise reject outright. */` |
|   382007 | 1056 | `	if( !bAbsolute ){` |
|        - | 1057 | `		SyBlob sRel;` |
|   381985 | 1058 | `		SyBlobInit(&sRel,&pGen->pVm->sAllocator);` |
|   381985 | 1059 | `		if( GenStateNsRelPrefix(pGen,&pIn,pGen->pEnd,&sRel) ){` |
|        - | 1060 | `			char *zDup;` |
|        7 | 1061 | `			bFQ = 1;` |
|        7 | 1062 | `			SyBlobAppend(&sRel,pIn->sData.zString,pIn->sData.nByte);` |
|        7 | 1063 | `			sLastSeg = pIn->sData;` |
|        7 | 1064 | `			pIn++;` |
|        9 | 1065 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|        7 | 1066 | `				&& (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      ! 0 | 1067 | `				SyBlobAppend(&sRel,"\\",1);` |
|      ! 0 | 1068 | `				SyBlobAppend(&sRel,pIn[1].sData.zString,pIn[1].sData.nByte);` |
|      ! 0 | 1069 | `				sLastSeg = pIn[1].sData;` |
|      ! 0 | 1070 | `				bMulti = 1;` |
|      ! 0 | 1071 | `				pIn += 2;` |
|      ! 0 | 1072 | `			}` |
|       10 | 1073 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        6 | 1074 | `				(const char *)SyBlobData(&sRel),SyBlobLength(&sRel));` |
|        7 | 1075 | `			if( zDup == 0 ){` |
|      ! 0 | 1076 | `				SyBlobRelease(&sRel);` |
|      ! 0 | 1077 | `				return SXERR_ABORT;` |
|        - | 1078 | `			}` |
|        7 | 1079 | `			pOut->nType = SXU32_HIGH;` |
|        7 | 1080 | `			SyStringInitFromBuf(&pOut->sClass,zDup,SyBlobLength(&sRel));` |
|        7 | 1081 | `			SyBlobRelease(&sRel);` |
|        7 | 1082 | `			rcScreen = GenStateScreenTypeName(pGen,&sLastSeg,&pOut->sClass,bMulti,bFQ,nLine);` |
|        7 | 1083 | `			if( rcScreen != SXRET_OK ){` |
|      ! 0 | 1084 | `				return rcScreen;` |
|        - | 1085 | `			}` |
|        7 | 1086 | `			pGen->pIn = pIn;` |
|        7 | 1087 | `			return SXRET_OK;` |
|        - | 1088 | `		}` |
|   381979 | 1089 | `		SyBlobRelease(&sRel);` |
|   190714 | 1090 | `	}` |
|   382001 | 1091 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        2 | 1092 | `		return SXERR_SYNTAX;` |
|        - | 1093 | `	}` |
|        - | 1094 | ``	/* php's lexer matches `{LABEL}("\\"{LABEL})+` as ONE T_NAME_QUALIFIED token`` |
|        - | 1095 | `	 * BEFORE it ever looks a label up in the keyword table, so every reserved word` |
|        - | 1096 | ``	 * is a legal SEGMENT of a qualified name: `Default\Q`, `A\Default`, even`` |
|        - | 1097 | ``	 * `static\Q` and `array\Q` are names, and only a BARE reserved word is a`` |
|        - | 1098 | ``	 * keyword. A `\` on either side is therefore what decides, so a keyword the`` |
|        - | 1099 | `	 * separator follows takes the class-name path below rather than the scalar` |
|        - | 1100 | ``	 * keyword branch (`Default\Q $x` used to be a syntax error). The `\` has to be`` |
|        - | 1101 | ``	 * GLUED, exactly as php's lexer requires: `private \Q $x` is still a modifier`` |
|        - | 1102 | `	 * and a type, and reading it as a name broke every promoted property. The` |
|        - | 1103 | `	 * TRAILING segments below stay loose about spacing, as every other name` |
|        - | 1104 | `	 * collector in the compiler is — there a stray space only widens what invalid` |
|        - | 1105 | `	 * source is accepted, it never re-reads valid source as something else. */` |
|   382016 | 1106 | `	bQualified = ( pIn + 1 < pGen->pEnd && (pIn[1].nType & PH7_TK_NSSEP)` |
|   573264 | 1107 | `		&& GenStateTokensGlued(pIn,&pIn[1]) );` |
|   381999 | 1108 | `	sLastSeg = pIn->sData;` |
|   381999 | 1109 | `	if( (pIn->nType & PH7_TK_KEYWORD) && !bQualified ){` |
|   319695 | 1110 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   319695 | 1111 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|    60805 | 1112 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   289254 | 1113 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|    47387 | 1114 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   235171 | 1115 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|    55435 | 1116 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   183760 | 1117 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   108676 | 1118 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   101668 | 1119 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|    47128 | 1120 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|    23819 | 1121 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|       61 | 1122 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      261 | 1123 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       97 | 1124 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|      231 | 1125 | `			pOut->nType = SXU32_HIGH;` |
|      231 | 1126 | `			pOut->sClass = pIn->sData;` |
|      118 | 1127 | `		}else{` |
|        3 | 1128 | `			return SXERR_SYNTAX;` |
|        - | 1129 | `		}` |
|   319693 | 1130 | `		pIn++;` |
|   159626 | 1131 | `	}else{` |
|        - | 1132 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|        - | 1133 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    62309 | 1134 | `		SyString *pT = &pIn->sData;` |
|    62309 | 1135 | `		if( !bQualified && pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|       38 | 1136 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|       38 | 1137 | `			pIn++;` |
|    62292 | 1138 | `		}else if( !bQualified && pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     7341 | 1139 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     7341 | 1140 | `			pIn++;` |
|    58595 | 1141 | `		}else if( !bQualified && pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       39 | 1142 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       39 | 1143 | `			pIn++;` |
|       22 | 1144 | `		}else{` |
|        - | 1145 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    54905 | 1146 | `			SyToken *pFirst = pIn;` |
|    54905 | 1147 | `			SyToken *pLast = pIn;` |
|    54905 | 1148 | `			pOut->nType = SXU32_HIGH;` |
|    54905 | 1149 | `			pOut->sClass = pIn->sData;` |
|    54905 | 1150 | `			pIn++;` |
|    82442 | 1151 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    54959 | 1152 | `				&& (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       39 | 1153 | `				pLast = &pIn[1];` |
|       39 | 1154 | `				pIn += 2;` |
|        3 | 1155 | `			}` |
|    54905 | 1156 | `			if( pLast != pFirst ){` |
|       37 | 1157 | `				const char *zFirst = pFirst->sData.zString;` |
|       37 | 1158 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       37 | 1159 | `				pOut->sClass.zString = zFirst;` |
|       37 | 1160 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       37 | 1161 | `				sLastSeg = pLast->sData;` |
|       37 | 1162 | `				bMulti = 1;` |
|       17 | 1163 | `			}` |
|        - | 1164 | `` 			/* Namespace-qualify a non-absolute class type so a `: Base` / `Base $x` `` |
|        - | 1165 | ``			 * hint in namespace N resolves to N\Base (or a `use` alias) at`` |
|        - | 1166 | `			 * type-check time instead of the global \Base — mirrors the` |
|        - | 1167 | ``			 * NEW/CALL/instanceof qualification. A QUALIFIED `A\B` hint resolves`` |
|        - | 1168 | ``			 * exactly the way `new A\B` does: GenStateResolveName maps the LEADING`` |
|        - | 1169 | `			 * segment through the imports and keeps the tail, else prepends the` |
|        - | 1170 | `			 * current namespace. Only absolute (\Base) names stay as written.` |
|        - | 1171 | ``			 * Qualified hints used to be stored AS WRITTEN, so `: Sub\Thing` inside`` |
|        - | 1172 | ``			 * `namespace App;` compared against the literal `Sub\Thing` while the`` |
|        - | 1173 | ``			 * value's class was `App\Sub\Thing` — every qualified type face in a`` |
|        - | 1174 | `			 * namespaced tree raised a TypeError. */` |
|        - | 1175 | `			/* Reserved type words that reach this identifier branch (false, true,` |
|        - | 1176 | `			 * mixed, iterable, callable) are NOT classes and must not be qualified` |
|        - | 1177 | ``			 * (else `false\|string` becomes `Ns\false\|string`); they are always`` |
|        - | 1178 | `			 * single-segment, so a qualified name never consults the list. */` |
|    54900 | 1179 | `			if( !bAbsolute` |
|    54896 | 1180 | `			 && !(pLast == pFirst && GenStateIsReservedTypeWord(&pOut->sClass)) ){` |
|        - | 1181 | `				SyBlob sFqn;` |
|      505 | 1182 | `				SyBlobInit(&sFqn,&pGen->pVm->sAllocator);` |
|      505 | 1183 | `				GenStateResolveName(pGen,&pOut->sClass,&sFqn);` |
|      500 | 1184 | `				if( SyBlobLength(&sFqn) != pOut->sClass.nByte` |
|      483 | 1185 | `				 \|\| SyMemcmp(SyBlobData(&sFqn),(const void *)pOut->sClass.zString,pOut->sClass.nByte) != 0 ){` |
|       70 | 1186 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       44 | 1187 | `						(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));` |
|       48 | 1188 | `					if( zDup ){` |
|       48 | 1189 | `						SyStringInitFromBuf(&pOut->sClass,zDup,SyBlobLength(&sFqn));` |
|       22 | 1190 | `					}` |
|       22 | 1191 | `				}` |
|      505 | 1192 | `				SyBlobRelease(&sFqn);` |
|      250 | 1193 | `			}` |
|        - | 1194 | `		}` |
|        - | 1195 | `	}` |
|        - | 1196 | `	/* php screens the name only once a qualifier is in play; a scalar atom that` |
|        - | 1197 | ``	 * got here bare (`int`, `array`) carries no sClass, so the written word is`` |
|        - | 1198 | `	 * both the segment and the resolved name. */` |
|   572720 | 1199 | `	rcScreen = GenStateScreenTypeName(pGen,&sLastSeg,` |
|   381992 | 1200 | `		pOut->nType == SXU32_HIGH ? &pOut->sClass : &sLastSeg,bMulti,bFQ,nLine);` |
|   381997 | 1201 | `	if( rcScreen != SXRET_OK ){` |
|        9 | 1202 | `		return rcScreen;` |
|        - | 1203 | `	}` |
|   381991 | 1204 | `	pGen->pIn = pIn;` |
|   381991 | 1205 | `	return SXRET_OK;` |
|   190733 | 1206 | `}` |
|        - | 1207 |  |
|        - | 1208 | `/* Class-name ATOMS that php files with the BUILT-IN types rather than the` |
|        - | 1209 | `` * classes. `callable`, `false` and `true` are type-mask bits in zend, so they`` |
|        - | 1210 | `` * print AFTER every class name however they were written (`false\|A1` reads`` |
|        - | 1211 | `` * `A1\|false`). `iterable` is two types at once: the class `Traversable`, which`` |
|        - | 1212 | `` * keeps the atom's declaration position among the classes, plus `array`, which`` |
|        - | 1213 | `` * prints with the builtins (`iterable\|A1` reads `Traversable\|A1\|array`). PH7`` |
|        - | 1214 | ` * parses all four as class-name atoms, which is why they used to sort with the` |
|        - | 1215 | ` * classes. */` |
|        - | 1216 | `#define GEN_ATOM_PLAIN    0` |
|        - | 1217 | `#define GEN_ATOM_CALLABLE 1` |
|        - | 1218 | `#define GEN_ATOM_FALSE    2` |
|        - | 1219 | `#define GEN_ATOM_TRUE     3` |
|        - | 1220 | `#define GEN_ATOM_ITERABLE 4` |
|  1507304 | 1221 | `static int GenAtomMaskKind(const PhlTypeAtom *pAtom)` |
|        5 | 1222 | `{` |
|  1507309 | 1223 | `	const SyString *p = &pAtom->sClass;` |
|  1507309 | 1224 | `	if( pAtom->nType != SXU32_HIGH \|\| p->zString == 0 ){` |
|  1245827 | 1225 | `		return GEN_ATOM_PLAIN;` |
|        - | 1226 | `	}` |
|   261487 | 1227 | `	if( p->nByte == 8 && SyStrnicmp(p->zString,"callable",8) == 0 ) return GEN_ATOM_CALLABLE;` |
|   260199 | 1228 | `	if( p->nByte == 5 && SyStrnicmp(p->zString,"false",5) == 0 )    return GEN_ATOM_FALSE;` |
|   105444 | 1229 | `	if( p->nByte == 4 && SyStrnicmp(p->zString,"true",4) == 0 )     return GEN_ATOM_TRUE;` |
|   105384 | 1230 | `	if( p->nByte == 8 && SyStrnicmp(p->zString,"iterable",8) == 0 ) return GEN_ATOM_ITERABLE;` |
|   105100 | 1231 | `	return GEN_ATOM_PLAIN;` |
|   752577 | 1232 | `}` |
|        - | 1233 | ``/* Emit one class-like atom: when *bExpandIterable*, `iterable` contributes only`` |
|        - | 1234 | `` * its Traversable half here (the `array` half rides the built-in pass). php`` |
|        - | 1235 | `` * expands it only in a COMPOUND type — a standalone `iterable`/`?iterable` keeps`` |
|        - | 1236 | ` * its name in the canonical text (which is what Reflection prints; the TypeError` |
|        - | 1237 | ` * for a standalone hint is rendered separately, by VmClassHintTypeName). */` |
|    21107 | 1238 | `static void GenAppendClassAtom(SyBlob *pBlob, const PhlTypeAtom *pAtom, int bExpandIterable)` |
|        5 | 1239 | `{` |
|    21112 | 1240 | `	if( bExpandIterable && GenAtomMaskKind(pAtom) == GEN_ATOM_ITERABLE ){` |
|       15 | 1241 | `		SyBlobAppend(pBlob, "Traversable", sizeof("Traversable")-1);` |
|       15 | 1242 | `		return;` |
|        - | 1243 | `	}` |
|    21098 | 1244 | `	SyBlobAppend(pBlob, pAtom->sClass.zString, pAtom->sClass.nByte);` |
|    10545 | 1245 | `}` |
|        - | 1246 | `/*` |
|        - | 1247 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|        - | 1248 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|        - | 1249 | ` *   classes (in declaration order)` |
|        - | 1250 | ` *   \| callable \| object \| array \| string \| int \| float \| bool \| false \| true` |
|        - | 1251 | ` *   [\| null]` |
|        - | 1252 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|        - | 1253 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|        - | 1254 | ` */` |
|   348045 | 1255 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|        5 | 1256 | `{` |
|        - | 1257 | `	int i;` |
|   348050 | 1258 | `	int nNonNull = 0;` |
|   348050 | 1259 | `	int bAnyIntersection = 0;` |
|        - | 1260 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|   348050 | 1261 | `	sxu32 nMaxGroup = 0;` |
| 11485490 | 1262 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   730008 | 1263 | `	for( i = 0; i < nAtoms; i++ ){` |
|   381963 | 1264 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   381929 | 1265 | `			nNonNull++;` |
|   381929 | 1266 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|   381929 | 1267 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|   381929 | 1268 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|   190689 | 1269 | `			}` |
|   190689 | 1270 | `		}` |
|   190711 | 1271 | `	}` |
|   729918 | 1272 | `	for( i = 0; i < nAtoms; i++ ){` |
|   381913 | 1273 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       44 | 1274 | `			bAnyIntersection = 1;` |
|       44 | 1275 | `			break;` |
|        - | 1276 | `		}` |
|   190666 | 1277 | `	}` |
|   348050 | 1278 | `	if( bAnyIntersection ){` |
|        - | 1279 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|        - | 1280 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|        - | 1281 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|       44 | 1282 | `		sxu32 g, nGroups = 0;` |
|       44 | 1283 | `		int bFirstGroup = 1;` |
|       96 | 1284 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|       96 | 1285 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|       56 | 1286 | `			int bFirstMember = 1;` |
|        - | 1287 | `			int bWrap;` |
|       56 | 1288 | `			if( aGroupCount[g] == 0 ) continue;` |
|        - | 1289 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|        - | 1290 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|        - | 1291 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|        - | 1292 | `			 * parens, matching PHP's canonical text. */` |
|       76 | 1293 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|       56 | 1294 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|       56 | 1295 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      184 | 1296 | `			for( i = 0; i < nAtoms; i++ ){` |
|      132 | 1297 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|       96 | 1298 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|       96 | 1299 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       90 | 1300 | `					GenAppendClassAtom(pBlob, &aAtoms[i], 1);` |
|       47 | 1301 | `				}else{` |
|        9 | 1302 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - | 1303 | `				}` |
|       96 | 1304 | `				bFirstMember = 0;` |
|       50 | 1305 | `			}` |
|       56 | 1306 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|       56 | 1307 | `			bFirstGroup = 0;` |
|       30 | 1308 | `		}` |
|        - | 1309 | ``		/* `iterable` printed its Traversable half in place; its `array` half goes`` |
|        - | 1310 | ``		 * last, as php does (`(I1&I2)\|iterable` reads `(I1&I2)\|Traversable\|array`). */`` |
|      134 | 1311 | `		for( i = 0; i < nAtoms; i++ ){` |
|       96 | 1312 | `			if( GenAtomMaskKind(&aAtoms[i]) == GEN_ATOM_ITERABLE ){` |
|        3 | 1313 | `				SyBlobAppend(pBlob, "\|", 1);` |
|        3 | 1314 | `				SyBlobAppend(pBlob, "array", sizeof("array")-1);` |
|        3 | 1315 | `				break;` |
|        - | 1316 | `			}` |
|       49 | 1317 | `		}` |
|       44 | 1318 | `		if( bNullable ){` |
|      ! 0 | 1319 | `			SyBlobAppend(pBlob, "\|", 1);` |
|      ! 0 | 1320 | `			SyBlobAppend(pBlob, "null", 4);` |
|      ! 0 | 1321 | `		}` |
|      175 | 1322 | `		return;` |
|        - | 1323 | `	}` |
|   348010 | 1324 | `	if( nNonNull == 1 && bNullable ){` |
|        - | 1325 | `		/* Shorthand: ?T */` |
|      267 | 1326 | `		for( i = 0; i < nAtoms; i++ ){` |
|      267 | 1327 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      267 | 1328 | `			SyBlobAppend(pBlob, "?", 1);` |
|      267 | 1329 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      107 | 1330 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       56 | 1331 | `			}else{` |
|      165 | 1332 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - | 1333 | `			}` |
|      267 | 1334 | `			return;` |
|      ! 0 | 1335 | `		}` |
|      ! 0 | 1336 | `	}` |
|        - | 1337 | `	{` |
|   347748 | 1338 | `		int bFirst = 1;` |
|        - | 1339 | `		/* 1) Classes in declaration order — minus the class-name atoms php counts` |
|        - | 1340 | ``		 * as built-in types (see GenAtomMaskKind); `iterable` leaves Traversable. */`` |
|   729336 | 1341 | `		for( i = 0; i < nAtoms; i++ ){` |
|        - | 1342 | `			int nKind;` |
|   381593 | 1343 | `			if( aAtoms[i].nType != SXU32_HIGH ) continue;` |
|    54935 | 1344 | `			nKind = GenAtomMaskKind(&aAtoms[i]);` |
|    54935 | 1345 | `			if( nKind == GEN_ATOM_CALLABLE \|\| nKind == GEN_ATOM_FALSE \|\| nKind == GEN_ATOM_TRUE ){` |
|    33914 | 1346 | `				continue;` |
|        - | 1347 | `			}` |
|    21026 | 1348 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    21026 | 1349 | `			GenAppendClassAtom(pBlob, &aAtoms[i], nNonNull > 1);` |
|    21026 | 1350 | `			bFirst = 0;` |
|    10502 | 1351 | `		}` |
|        - | 1352 | `		/* 2) Built-ins in php's canonical order. A slot is filled either by a` |
|        - | 1353 | `		 * plain atom of that MEMOBJ_* type or by a class-name atom of the matching` |
|        - | 1354 | ``		 * kind; the `array` slot also takes `iterable`'s second half. */`` |
|        - | 1355 | `		{` |
|        - | 1356 | `			static const struct {` |
|        - | 1357 | `				sxu32 nType;        /* plain atom type, 0 when kind-only */` |
|        - | 1358 | `				int nKind;          /* GEN_ATOM_* atom, GEN_ATOM_PLAIN when type-only */` |
|        - | 1359 | `				const char *zText;` |
|        - | 1360 | `				sxu32 nText;` |
|        - | 1361 | `			} aOrder[] = {` |
|        - | 1362 | `				{ 0,               GEN_ATOM_CALLABLE, "callable", sizeof("callable")-1 },` |
|        - | 1363 | `				{ MEMOBJ_OBJ,      GEN_ATOM_PLAIN,    "object",   sizeof("object")-1 },` |
|        - | 1364 | `				{ MEMOBJ_HASHMAP,  GEN_ATOM_ITERABLE, "array",    sizeof("array")-1 },` |
|        - | 1365 | `				{ MEMOBJ_STRING,   GEN_ATOM_PLAIN,    "string",   sizeof("string")-1 },` |
|        - | 1366 | `				{ MEMOBJ_INT,      GEN_ATOM_PLAIN,    "int",      sizeof("int")-1 },` |
|        - | 1367 | `				{ MEMOBJ_REAL,     GEN_ATOM_PLAIN,    "float",    sizeof("float")-1 },` |
|        - | 1368 | `				{ MEMOBJ_BOOL,     GEN_ATOM_PLAIN,    "bool",     sizeof("bool")-1 },` |
|        - | 1369 | `				{ 0,               GEN_ATOM_FALSE,    "false",    sizeof("false")-1 },` |
|        - | 1370 | `				{ 0,               GEN_ATOM_TRUE,     "true",     sizeof("true")-1 },` |
|        - | 1371 | ``				/* `void` and `never` are RETURN-only and may not share a type with`` |
|        - | 1372 | `				 * anything, so their place in the order is never observable -- what` |
|        - | 1373 | `				 * matters is that they have one. Missing here, the canonical text of` |
|        - | 1374 | ``				 * `: void` came out EMPTY, and every reader of it had to name the two`` |
|        - | 1375 | `				 * back from nReturnType (Reflection's getReturnType did, the generator` |
|        - | 1376 | `				 * fatal did, and the declaration renderer would have had to). */` |
|        - | 1377 | `				{ UTA_VOID_FLAG,   GEN_ATOM_PLAIN,    "void",     sizeof("void")-1 },` |
|        - | 1378 | `				{ UTA_NEVER_FLAG,  GEN_ATOM_PLAIN,    "never",    sizeof("never")-1 }` |
|        - | 1379 | `			};` |
|        - | 1380 | `			int k;` |
|  4172921 | 1381 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  7628270 | 1382 | `				for( i = 0; i < nAtoms; i++ ){` |
|  5506229 | 1383 | `					int bHit = (aOrder[k].nType != 0 && aAtoms[i].nType == aOrder[k].nType)` |
|  7127354 | 1384 | `						\|\| (aOrder[k].nKind != GEN_ATOM_PLAIN` |
|  2642845 | 1385 | `						    && GenAtomMaskKind(&aAtoms[i]) == aOrder[k].nKind` |
|   742006 | 1386 | `						    && (aOrder[k].nKind != GEN_ATOM_ITERABLE \|\| nNonNull > 1));` |
|  4163658 | 1387 | `					if( !bHit ) continue;` |
|   360566 | 1388 | `					if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   360566 | 1389 | `					SyBlobAppend(pBlob, aOrder[k].zText, aOrder[k].nText);` |
|   360566 | 1390 | `					bFirst = 0;` |
|   360566 | 1391 | `					break;` |
|      ! 0 | 1392 | `				}` |
|  1909836 | 1393 | `			}` |
|        - | 1394 | `		}` |
|        - | 1395 | `		/* 3) null suffix */` |
|   347748 | 1396 | `		if( bNullable ){` |
|       22 | 1397 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|       22 | 1398 | `			SyBlobAppend(pBlob, "null", 4);` |
|        9 | 1399 | `		}` |
|        - | 1400 | `	}` |
|   173777 | 1401 | `}` |
|        - | 1402 |  |
|        - | 1403 | `/*` |
|        - | 1404 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|        - | 1405 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|        - | 1406 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|        - | 1407 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|        - | 1408 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|        - | 1409 | ` * whether it was parenthesized.` |
|        - | 1410 | ` *` |
|        - | 1411 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|        - | 1412 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|        - | 1413 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|        - | 1414 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|        - | 1415 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|        - | 1416 | ` */` |
|   381958 | 1417 | `static sxi32 GenStateParsePart(` |
|        - | 1418 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|        - | 1419 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|        5 | 1420 | `{` |
|        - | 1421 | `	sxi32 rc;` |
|   381963 | 1422 | `	int nMembers = 0;` |
|   381963 | 1423 | `	int bParen = 0;` |
|   381963 | 1424 | `	*pnMembers = 0;` |
|   381963 | 1425 | `	*pbParen = 0;` |
|   381963 | 1426 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       16 | 1427 | `		bParen = 1;` |
|       16 | 1428 | `		pGen->pIn++; /* skip '(' */` |
|        6 | 1429 | `	}` |
|   190706 | 1430 | `	for(;;){` |
|   382007 | 1431 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|      ! 0 | 1432 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1433 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|      ! 0 | 1434 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1435 | `		}` |
|   382007 | 1436 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|   382007 | 1437 | `		if( rc != SXRET_OK ){` |
|       13 | 1438 | `			return rc;` |
|        - | 1439 | `		}` |
|   381997 | 1440 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|   381997 | 1441 | `		(*pnAtoms)++;` |
|   381997 | 1442 | `		nMembers++;` |
|        - | 1443 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|   381997 | 1444 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       85 | 1445 | `			SyToken *pNext = &pGen->pIn[1];` |
|       80 | 1446 | `			if( pNext < pGen->pEnd` |
|       85 | 1447 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       49 | 1448 | `				pGen->pIn++; /* skip '&' */` |
|       49 | 1449 | `				continue;` |
|        - | 1450 | `			}` |
|       18 | 1451 | `		}` |
|   381953 | 1452 | `		break;` |
|      ! 0 | 1453 | `	}` |
|   381953 | 1454 | `	if( bParen ){` |
|       16 | 1455 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 1456 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1457 | `				"Malformed DNF type: expecting ')'");` |
|      ! 0 | 1458 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1459 | `		}` |
|       16 | 1460 | `		pGen->pIn++; /* skip ')' */` |
|       16 | 1461 | `		if( nMembers < 2 ){` |
|      ! 0 | 1462 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1463 | `				"Parenthesized type must be an intersection of at least two types");` |
|      ! 0 | 1464 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1465 | `		}` |
|        6 | 1466 | `	}` |
|   381953 | 1467 | `	*pnMembers = nMembers;` |
|   381953 | 1468 | `	*pbParen = bParen;` |
|   381953 | 1469 | `	return SXRET_OK;` |
|   190711 | 1470 | `}` |
|        - | 1471 |  |
|        - | 1472 | `/*` |
|        - | 1473 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|        - | 1474 | ` *` |
|        - | 1475 | ` * Outputs:` |
|        - | 1476 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|        - | 1477 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|        - | 1478 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|        - | 1479 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|        - | 1480 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|        - | 1481 | ` *     already be initialized by the caller (allocator set, etc).` |
|        - | 1482 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|        - | 1483 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|        - | 1484 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|        - | 1485 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|        - | 1486 | ` *` |
|        - | 1487 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|        - | 1488 | ` * SXERR_ABORT on fatal compile errors.` |
|        - | 1489 | ` */` |
|   348077 | 1490 | `PH7_PRIVATE sxi32 GenStateParseUnionTypeDecl(` |
|        - | 1491 | `	ph7_gen_state *pGen,` |
|        - | 1492 | `	sxu32 *pnType,` |
|        - | 1493 | `	SyString *pClass,` |
|        - | 1494 | `	SySet *pAlts,` |
|        - | 1495 | `	sxi32 *piTypeFlags,` |
|        - | 1496 | `	SyString *pTypeText,` |
|        - | 1497 | `	int iNullableFlag,` |
|        - | 1498 | `	int iUnionFlag,` |
|        - | 1499 | `	int bAllowVoid,` |
|        - | 1500 | ``	int bParamCtx,   /* this type is a PARAMETER's: `static` is not one in php's grammar */`` |
|        - | 1501 | `	sxu32 nLine` |
|        5 | 1502 | `){` |
|        - | 1503 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   348082 | 1504 | `	int nAtoms = 0;` |
|   348082 | 1505 | `	int bShortNullable = 0;` |
|   348082 | 1506 | `	int bExplicitNull = 0;` |
|        - | 1507 | `	sxi32 rc;` |
|   348082 | 1508 | `	*pnType = 0;` |
|   348082 | 1509 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   348082 | 1510 | `	*piTypeFlags = 0;` |
|   348082 | 1511 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|        - | 1512 |  |
|   348082 | 1513 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 1514 | `		return SXRET_OK;` |
|        - | 1515 | `	}` |
|        - | 1516 | ``	/* Optional `?` shorthand prefix */`` |
|   348077 | 1517 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      255 | 1518 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      255 | 1519 | `		bShortNullable = 1;` |
|      255 | 1520 | `		pGen->pIn++;` |
|      255 | 1521 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 1522 | `			return SXERR_SYNTAX;` |
|        - | 1523 | `		}` |
|      125 | 1524 | `	}` |
|        - | 1525 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|        - | 1526 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|        - | 1527 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|        - | 1528 | `	{` |
|        - | 1529 | `		int nMembers, bParen;` |
|   348082 | 1530 | `		sxu32 iGroup = 0;` |
|   348082 | 1531 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|   348082 | 1532 | `		if( rc != SXRET_OK ){` |
|       14 | 1533 | `			return rc;` |
|        - | 1534 | `		}` |
|        - | 1535 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|        - | 1536 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|        - | 1537 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|        - | 1538 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|        - | 1539 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|   573169 | 1540 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   398935 | 1541 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|    33888 | 1542 | `			if( bShortNullable ){` |
|        - | 1543 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|        - | 1544 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|        - | 1545 | `				 * already reported" so callers skip their own error emission. */` |
|        3 | 1546 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - | 1547 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|        3 | 1548 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|        - | 1549 | `			}` |
|    33886 | 1550 | `			if( nMembers >= 2 && !bParen ){` |
|      ! 0 | 1551 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|        - | 1552 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 | 1553 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1554 | `			}` |
|    33886 | 1555 | ``			pGen->pIn++; /* skip `\|` */`` |
|    33886 | 1556 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|    33886 | 1557 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1558 | `				return rc;` |
|        - | 1559 | `			}` |
|        5 | 1560 | `		}` |
|   348070 | 1561 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|      ! 0 | 1562 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1563 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 | 1564 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1565 | `		}` |
|        - | 1566 | `	}` |
|        - | 1567 | `	/* Validation pass.` |
|        - | 1568 | `	 *` |
|        - | 1569 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|        - | 1570 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|        - | 1571 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|        - | 1572 | `	 */` |
|        - | 1573 | `	{` |
|        - | 1574 | `		int i, j;` |
|   348070 | 1575 | `		int bHasNonNull = 0;` |
|   348070 | 1576 | `		int bAnyIntersection = 0;` |
|        - | 1577 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|        - | 1578 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|        - | 1579 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
| 11486150 | 1580 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   730060 | 1581 | `		for( i = 0; i < nAtoms; i++ ){` |
|   381995 | 1582 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|   190727 | 1583 | `		}` |
|   729962 | 1584 | `		for( i = 0; i < nAtoms; i++ ){` |
|   381941 | 1585 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|   190678 | 1586 | `		}` |
|        - | 1587 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|        - | 1588 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|   348070 | 1589 | `		if( bShortNullable && bAnyIntersection ){` |
|      ! 0 | 1590 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1591 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|      ! 0 | 1592 | `			return SXERR_SYNTAX;` |
|        - | 1593 | `		}` |
|        - | 1594 | `		/*` |
|        - | 1595 | `		 * php's declaration-time screen for the three SCOPE keywords, which runs` |
|        - | 1596 | `		 * BEFORE the intersection-member kinds below (that is php's order: a` |
|        - | 1597 | ``		 * `parent&Countable` in a base-less class is refused for the parent, not`` |
|        - | 1598 | `		 * for the intersection).` |
|        - | 1599 | `		 *` |
|        - | 1600 | `		 * Each keyword names something relative to WHERE the declaration is` |
|        - | 1601 | `		 * written, and one written where that thing does not exist is refused` |
|        - | 1602 | `		 * before the program runs. A CLOSURE is exempt — its scope is decided when` |
|        - | 1603 | `		 * it is bound — and so is a TRAIT, which defers the question to whatever` |
|        - | 1604 | ``		 * composes it. An interface has no `parent` however many it extends.`` |
|        - | 1605 | `		 *` |
|        - | 1606 | ``		 * `static` is not a PARAMETER type in php's grammar at all: the modifier`` |
|        - | 1607 | `` 		 * run ahead of this parser catches the bare and `&` spellings, and `?static` `` |
|        - | 1608 | `		 * reaches here, where php answers with the parse error its grammar produces.` |
|        - | 1609 | `		 */` |
|   730054 | 1610 | `		for( i = 0; i < nAtoms; i++ ){` |
|        - | 1611 | `			const SyString *pKw;` |
|        - | 1612 | `			ph7_class *pScope;` |
|        - | 1613 | `			const char *zKw;` |
|   381995 | 1614 | `			if( aAtoms[i].nType != SXU32_HIGH ){` |
|   326867 | 1615 | `				continue;` |
|        - | 1616 | `			}` |
|    55133 | 1617 | `			pKw = &aAtoms[i].sClass;` |
|   110147 | 1618 | `			zKw = (pKw->nByte == 4 && SyStrnicmp(pKw->zString,"self",4) == 0)   ? "self"   :` |
|    82463 | 1619 | `			      (pKw->nByte == 6 && SyStrnicmp(pKw->zString,"parent",6) == 0) ? "parent" :` |
|    54974 | 1620 | `			      (pKw->nByte == 6 && SyStrnicmp(pKw->zString,"static",6) == 0) ? "static" : 0;` |
|    55133 | 1621 | `			if( zKw == 0 ){` |
|    54909 | 1622 | `				continue;` |
|        - | 1623 | `			}` |
|      229 | 1624 | `			if( bParamCtx && zKw[0] == 's' && zKw[1] == 't' ){` |
|        - | 1625 | ``				/* GRAMMAR, not scope: `static` is no parameter type at all, so a`` |
|        - | 1626 | `				 * CLOSURE is not exempt from this one. The modifier run ahead of this` |
|        - | 1627 | ``				 * parser already catches the bare and `&` spellings; `?static` and`` |
|        - | 1628 | ``				 * `int\|static` reach here. */`` |
|        3 | 1629 | `				PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|        - | 1630 | `					"syntax error, unexpected token \"static\"");` |
|        3 | 1631 | `				return SXERR_SYNTAX;` |
|        - | 1632 | `			}` |
|      227 | 1633 | `			if( pGen->iSigScope == PH7_SIGSCOPE_CLOSURE ){` |
|       15 | 1634 | `				continue;` |
|        - | 1635 | `			}` |
|      215 | 1636 | `			pScope = ( pGen->iSigScope == PH7_SIGSCOPE_FUNC ) ? 0 : pGen->pCurClass;` |
|      215 | 1637 | `			if( pScope == 0 ){` |
|        4 | 1638 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        1 | 1639 | `					"Cannot use \"%s\" when no class scope is active", zKw);` |
|        3 | 1640 | `				return SXERR_SYNTAX;` |
|        - | 1641 | `			}` |
|      208 | 1642 | `			if( zKw[0] == 'p' && pGen->pCurBase == 0` |
|       29 | 1643 | `			 && (pScope->iFlags & (PH7_CLASS_TRAIT\|PH7_CLASS_LINT_UNBOUND)) == 0 ){` |
|        3 | 1644 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1645 | `					"Cannot use \"parent\" when current class scope has no parent");` |
|        3 | 1646 | `				return SXERR_SYNTAX;` |
|        - | 1647 | `			}` |
|      108 | 1648 | `		}` |
|   730030 | 1649 | `		for( i = 0; i < nAtoms; i++ ){` |
|        - | 1650 | `			/* Intersection members must be class/interface types (PHP rejects` |
|        - | 1651 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|        - | 1652 | ``			 * `true`/`false` in an intersection). */`` |
|   381985 | 1653 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       89 | 1654 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|       89 | 1655 | `				int bLowerKw = 0;` |
|       89 | 1656 | `				if( bClassLike ){` |
|       86 | 1657 | `					SyString *pC = &aAtoms[i].sClass;` |
|       82 | 1658 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|       82 | 1659 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|       82 | 1660 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|       86 | 1661 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|      ! 0 | 1662 | `						bClassLike = 0;` |
|      ! 0 | 1663 | `					}` |
|        - | 1664 | `					/* A SCOPE keyword is class-like only where php can substitute a` |
|        - | 1665 | ``					 * class NAME for it while the body compiles: `static` never (the`` |
|        - | 1666 | `` 					 * called class is not known until the call), and `self`/`parent` `` |
|        - | 1667 | `					 * not in a closure, a trait or an ANONYMOUS class, none of which` |
|        - | 1668 | `					 * has a name to put there. In a named class, interface or enum` |
|        - | 1669 | `					 * php resolves them and the intersection stands. */` |
|       86 | 1670 | `					else if( pC->nByte == 6 && SyMemcmpNoCase(pC->zString,"static",6) == 0 ){` |
|      ! 0 | 1671 | `						bClassLike = 0;` |
|      ! 0 | 1672 | `						bLowerKw = 1; /* php prints the keyword, not what was written */` |
|       82 | 1673 | `					}else if( (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"self",4) == 0)` |
|       84 | 1674 | `					       \|\| (pC->nByte == 6 && SyMemcmpNoCase(pC->zString,"parent",6) == 0) ){` |
|        8 | 1675 | `						ph7_class *pScope = ( pGen->iSigScope == PH7_SIGSCOPE_FUNC )` |
|        4 | 1676 | `							? 0 : pGen->pCurClass;` |
|        4 | 1677 | `						if( pGen->iSigScope == PH7_SIGSCOPE_CLOSURE \|\| pScope == 0` |
|        6 | 1678 | `						 \|\| (pScope->iFlags & (PH7_CLASS_TRAIT\|PH7_CLASS_ANON)) != 0 ){` |
|        3 | 1679 | `							bClassLike = 0;` |
|        1 | 1680 | `						}` |
|        2 | 1681 | `					}` |
|       41 | 1682 | `				}` |
|       89 | 1683 | `				if( !bClassLike ){` |
|        - | 1684 | `					const char *zName; sxu32 nName;` |
|        6 | 1685 | `					if( bLowerKw ){` |
|      ! 0 | 1686 | `						zName = "static"; nName = sizeof("static")-1;` |
|        6 | 1687 | `					}else if( aAtoms[i].nType == SXU32_HIGH ){` |
|        3 | 1688 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|        2 | 1689 | `					}else{` |
|        3 | 1690 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|        - | 1691 | `					}` |
|        8 | 1692 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1693 | `						"Type %.*s cannot be part of an intersection type",` |
|        2 | 1694 | `						(int)nName, zName);` |
|        6 | 1695 | `					return SXERR_SYNTAX;` |
|        - | 1696 | `				}` |
|       40 | 1697 | `			}` |
|   381981 | 1698 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     7341 | 1699 | `				if( nAtoms > 1 ){` |
|        3 | 1700 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1701 | `						"Void can only be used as a standalone type");` |
|        3 | 1702 | `					return SXERR_SYNTAX;` |
|        - | 1703 | `				}` |
|     7339 | 1704 | `				if( !bAllowVoid ){` |
|      ! 0 | 1705 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1706 | `						"void cannot be used here");` |
|      ! 0 | 1707 | `					return SXERR_SYNTAX;` |
|        - | 1708 | `				}` |
|     7339 | 1709 | `				if( bShortNullable ){` |
|      ! 0 | 1710 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1711 | `						"Void type cannot be nullable");` |
|      ! 0 | 1712 | `					return SXERR_SYNTAX;` |
|        - | 1713 | `				}` |
|     3655 | 1714 | `			}` |
|   381979 | 1715 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|        - | 1716 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|        - | 1717 | `				 * type (never = the function does not return). Mirrors the void` |
|        - | 1718 | `				 * validation above; accepted here and enforced at compile time` |
|        - | 1719 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|       39 | 1720 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|        - | 1721 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|        - | 1722 | `					 * same as any other non-standalone use. */` |
|        6 | 1723 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1724 | `						"never can only be used as a standalone type");` |
|        6 | 1725 | `					return SXERR_SYNTAX;` |
|        - | 1726 | `				}` |
|       34 | 1727 | `				if( !bAllowVoid ){` |
|        - | 1728 | `					/* Return-only: params call with bAllowVoid=0. */` |
|        3 | 1729 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1730 | `						"never cannot be used as a parameter type");` |
|        3 | 1731 | `					return SXERR_SYNTAX;` |
|        - | 1732 | `				}` |
|       14 | 1733 | `			}` |
|   381973 | 1734 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|       38 | 1735 | `				bExplicitNull = 1;` |
|       21 | 1736 | `			}else{` |
|   381939 | 1737 | `				bHasNonNull = 1;` |
|        - | 1738 | `			}` |
|        - | 1739 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|        - | 1740 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|        - | 1741 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|        - | 1742 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|        - | 1743 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|   415954 | 1744 | `			for( j = 0; j < i; j++ ){` |
|    33988 | 1745 | `				int bDup = 0;` |
|    33988 | 1746 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|    67949 | 1747 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|    33983 | 1748 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|    33988 | 1749 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|    33964 | 1750 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      103 | 1751 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       96 | 1752 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|       76 | 1753 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       23 | 1754 | `								aAtoms[j].sClass.zString,` |
|       46 | 1755 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|      ! 0 | 1756 | `							bDup = 1;` |
|      ! 0 | 1757 | `						}` |
|       53 | 1758 | `					}else{` |
|        3 | 1759 | `						bDup = 1;` |
|        - | 1760 | `					}` |
|       49 | 1761 | `				}` |
|    33964 | 1762 | `				if( bDup ){` |
|        - | 1763 | `					const char *zName;` |
|        - | 1764 | `					sxu32 nName;` |
|        3 | 1765 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 | 1766 | `						zName = aAtoms[i].sClass.zString;` |
|      ! 0 | 1767 | `						nName = aAtoms[i].sClass.nByte;` |
|      ! 0 | 1768 | `					}else{` |
|        3 | 1769 | `						zName = aAtoms[i].zCanon;` |
|        3 | 1770 | `						nName = aAtoms[i].nCanon;` |
|        - | 1771 | `					}` |
|        4 | 1772 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        1 | 1773 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|        3 | 1774 | `					return SXERR_SYNTAX;` |
|        - | 1775 | `				}` |
|    16961 | 1776 | `			}` |
|   190715 | 1777 | `		}` |
|   348050 | 1778 | `		if( !bHasNonNull && bExplicitNull ){` |
|        7 | 1779 | `			if( bShortNullable ){` |
|        - | 1780 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|      ! 0 | 1781 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1782 | `					"Null can not be used as a standalone type");` |
|      ! 0 | 1783 | `				return SXERR_SYNTAX;` |
|        - | 1784 | `			}` |
|        - | 1785 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|        - | 1786 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|        - | 1787 | `			 * path below leaves *pnType untouched when there is no non-null` |
|        - | 1788 | `			 * atom, so set it here. */` |
|        7 | 1789 | `			*pnType = MEMOBJ_NULL;` |
|        3 | 1790 | `		}` |
|        - | 1791 | `	}` |
|        - | 1792 | `	/* Compute nullability flag */` |
|   348050 | 1793 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      285 | 1794 | `		*piTypeFlags \|= iNullableFlag;` |
|      140 | 1795 | `	}` |
|        - | 1796 | `	/* Build canonical type text */` |
|   348050 | 1797 | `	if( pTypeText ){` |
|        - | 1798 | `		SyBlob sBlob;` |
|   348050 | 1799 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|   521699 | 1800 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   173772 | 1801 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   348050 | 1802 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|   521822 | 1803 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   348045 | 1804 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   348050 | 1805 | `			if( zDup ){` |
|   348050 | 1806 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   173772 | 1807 | `			}` |
|   173772 | 1808 | `		}` |
|   348050 | 1809 | `		SyBlobRelease(&sBlob);` |
|   173772 | 1810 | `	}` |
|        - | 1811 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|        - | 1812 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|        - | 1813 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|        - | 1814 | `	{` |
|   348050 | 1815 | `		int nNonNull = 0;` |
|   348050 | 1816 | `		int iNonNullIdx = -1;` |
|        - | 1817 | `		int i;` |
|   730008 | 1818 | `		for( i = 0; i < nAtoms; i++ ){` |
|   381963 | 1819 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   381929 | 1820 | `				nNonNull++;` |
|   381929 | 1821 | `				iNonNullIdx = i;` |
|   190689 | 1822 | `			}` |
|   190711 | 1823 | `		}` |
|   348050 | 1824 | `		if( nNonNull <= 1 ){` |
|        - | 1825 | `			/* Fast path: store as single type. */` |
|   314205 | 1826 | `			if( iNonNullIdx >= 0 ){` |
|   314199 | 1827 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   314199 | 1828 | `				if( pA->nType == SXU32_HIGH ){` |
|    31884 | 1829 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    10616 | 1830 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    21268 | 1831 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    21268 | 1832 | `					*pnType = SXU32_HIGH;` |
|    21268 | 1833 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   303552 | 1834 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     7339 | 1835 | `					*pnType = MEMOBJ_VOID;` |
|   289257 | 1836 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|       32 | 1837 | `					*pnType = MEMOBJ_NEVER;` |
|       18 | 1838 | `				}else{` |
|   285574 | 1839 | `					*pnType = pA->nType;` |
|        - | 1840 | `				}` |
|   156869 | 1841 | `			}` |
|   156877 | 1842 | `		}else{` |
|        - | 1843 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|    33850 | 1844 | `			*piTypeFlags \|= iUnionFlag;` |
|   101592 | 1845 | `			for( i = 0; i < nAtoms; i++ ){` |
|        - | 1846 | `				ph7_type_alt sAlt;` |
|    67747 | 1847 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|    67735 | 1848 | `				SyZero(&sAlt, sizeof(sAlt));` |
|    67735 | 1849 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|    67735 | 1850 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|    50765 | 1851 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    16905 | 1852 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    33860 | 1853 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    33860 | 1854 | `					sAlt.nType = SXU32_HIGH;` |
|    33860 | 1855 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|    16910 | 1856 | `				}else{` |
|    33880 | 1857 | `					sAlt.nType = aAtoms[i].nType;` |
|    33880 | 1858 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|        - | 1859 | `				}` |
|    67735 | 1860 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|    33825 | 1861 | `			}` |
|        - | 1862 | `		}` |
|        - | 1863 | `	}` |
|   348050 | 1864 | `	return SXRET_OK;` |
|   173793 | 1865 | `}` |
|        - | 1866 |  |
|        - | 1867 | `/*` |
|        - | 1868 | `` * php 8.5's `#[\NoDiscard]`, decided where the declaration is WRITTEN.`` |
|        - | 1869 | ` *` |
|        - | 1870 | ` * The attribute says a caller must do something with the answer, so php refuses` |
|        - | 1871 | `` * it on a declaration that HAS no answer -- a `void` or `never` return type --`` |
|        - | 1872 | ` * and on a constructor, which is called for its object rather than its return.` |
|        - | 1873 | ` * The nouns are php's: a "function" everywhere but a class body, where the same` |
|        - | 1874 | ` * sentence says "method". Run once the return type is parsed, since that is` |
|        - | 1875 | ` * what it judges; a declaration that already failed says nothing more.` |
|        - | 1876 | ` */` |
|   177234 | 1877 | `PH7_PRIVATE sxi32 GenStateApplyNoDiscard(ph7_gen_state *pGen,ph7_vm_func *pFunc,` |
|        - | 1878 | `	ph7_class *pClass,int bCtor)` |
|        5 | 1879 | `{` |
|   177239 | 1880 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(&pFunc->aAttrs);` |
|   177239 | 1881 | `	const char *zKind = pClass ? "method" : "function";` |
|        - | 1882 | `	sxu32 n;` |
|   177449 | 1883 | `	for( n = 0 ; n < SySetUsed(&pFunc->aAttrs) ; ++n ){` |
|      386 | 1884 | `		if( SyStringLength(&aAttr[n].sName) != sizeof("NoDiscard")-1` |
|      287 | 1885 | `		 \|\| SyStrnicmp(SyStringData(&aAttr[n].sName),"NoDiscard",sizeof("NoDiscard")-1) != 0 ){` |
|      215 | 1886 | `			continue;` |
|        - | 1887 | `		}` |
|      177 | 1888 | `		if( bCtor ){` |
|        4 | 1889 | `			return PH7_GenCompileError(&(*pGen),E_ERROR,pFunc->nLine,` |
|        1 | 1890 | `				"Method %z::%z cannot be #[\\NoDiscard]",&pClass->sName,&pFunc->sName);` |
|        - | 1891 | `		}` |
|      175 | 1892 | `		if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|        7 | 1893 | `			return PH7_GenCompileError(&(*pGen),E_ERROR,pFunc->nLine,` |
|        - | 1894 | `				"A void %s does not return a value, but #[\\NoDiscard] requires a return value",` |
|        2 | 1895 | `				zKind);` |
|        - | 1896 | `		}` |
|      171 | 1897 | `		if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|        7 | 1898 | `			return PH7_GenCompileError(&(*pGen),E_ERROR,pFunc->nLine,` |
|        - | 1899 | `				"A never returning %s does not return a value, but #[\\NoDiscard] requires a return value",` |
|        2 | 1900 | `				zKind);` |
|        - | 1901 | `		}` |
|      167 | 1902 | `		pFunc->iFlags \|= VM_FUNC_NODISCARD;` |
|      167 | 1903 | `		return SXRET_OK;` |
|      ! 0 | 1904 | `	}` |
|   177063 | 1905 | `	return SXRET_OK;` |
|    88383 | 1906 | `}` |
|        - | 1907 | `/*` |
|        - | 1908 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|        - | 1909 | `` * pGen->pIn should point to the token after `)`.`` |
|        - | 1910 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|        - | 1911 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|        - | 1912 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|        - | 1913 | `` *          and union types `: T\|U`.`` |
|        - | 1914 | ` */` |
|   171003 | 1915 | `PH7_PRIVATE sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|        5 | 1916 | `{` |
|   171008 | 1917 | `	sxi32 iFlags = 0;` |
|        - | 1918 | `	sxi32 rc;` |
|        - | 1919 | `	sxu32 nLine;` |
|   171008 | 1920 | `	pFunc->nReturnType = 0;` |
|   171008 | 1921 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|   171008 | 1922 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|        - | 1923 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|        - | 1924 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|        - | 1925 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|        - | 1926 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|        - | 1927 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|   171008 | 1928 | `	SySetReset(&pFunc->aReturnUnion);` |
|   171008 | 1929 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|   171008 | 1930 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|    34243 | 1931 | `		return SXRET_OK;` |
|        - | 1932 | `	}` |
|   136770 | 1933 | `	pGen->pIn++; /* Skip ':' */` |
|   136770 | 1934 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 1935 | `		return SXRET_OK;` |
|        - | 1936 | `	}` |
|   136770 | 1937 | `	nLine = pGen->pIn->nLine;` |
|   136770 | 1938 | `	rc = GenStateParseUnionTypeDecl(` |
|    68283 | 1939 | `		pGen,` |
|    68283 | 1940 | `		&pFunc->nReturnType,` |
|    68283 | 1941 | `		&pFunc->sReturnClass,` |
|    68283 | 1942 | `		&pFunc->aReturnUnion,` |
|        - | 1943 | `		&iFlags,` |
|    68283 | 1944 | `		&pFunc->sReturnTypeName,` |
|        - | 1945 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|        - | 1946 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|        - | 1947 | `		/* iUnionFlag */ 0,` |
|        - | 1948 | `		/* bAllowVoid */ 1, /* bParamCtx */ 0,` |
|    68283 | 1949 | `		nLine);` |
|   136770 | 1950 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1951 | `		return SXERR_ABORT;` |
|        - | 1952 | `	}` |
|   136770 | 1953 | `	if( rc == SXERR_CORRUPT ){` |
|        - | 1954 | `		/* Error already reported */` |
|      ! 0 | 1955 | `		return SXERR_SYNTAX;` |
|        - | 1956 | `	}` |
|   136770 | 1957 | `	if( rc == SXERR_SYNTAX ){` |
|       17 | 1958 | `		if( pGen->pIn < pGen->pEnd ){` |
|       24 | 1959 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - | 1960 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       14 | 1961 | `				&pGen->pIn->sData);` |
|       10 | 1962 | `		}else{` |
|      ! 0 | 1963 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|        - | 1964 | `				"syntax error, unexpected end of file in return type declaration");` |
|        - | 1965 | `		}` |
|       17 | 1966 | `		return SXERR_SYNTAX;` |
|        - | 1967 | `	}` |
|   136756 | 1968 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|   136756 | 1969 | `	return SXRET_OK;` |
|    85284 | 1970 | `}` |
|        - | 1971 |  |
|   158017 | 1972 | `PH7_PRIVATE sxi32 GenStateCompileFunc(` |
|        - | 1973 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 1974 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|        - | 1975 | `	sxi32 iFlags,        /* Control flags */` |
|        - | 1976 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|        - | 1977 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|        - | 1978 | `	)` |
|        5 | 1979 | `{` |
|        - | 1980 | `	ph7_vm_func *pFunc;` |
|        - | 1981 | `	SyToken *pEnd;` |
|        - | 1982 | `	sxu32 nLine;` |
|        - | 1983 | `	char *zName;` |
|        - | 1984 | `	sxi32 rc;` |
|        - | 1985 | `	/* Extract line number */` |
|   158022 | 1986 | `	nLine = pGen->pIn->nLine;` |
|        - | 1987 | `	/* Jump the left parenthesis '(' */` |
|   158022 | 1988 | `	pGen->pIn++;` |
|        - | 1989 | `	/* Delimit the function signature */` |
|   158022 | 1990 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   158022 | 1991 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 1992 | `		/* Syntax error */` |
|       12 | 1993 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable");` |
|        4 | 1994 | `		(void)pName;` |
|       12 | 1995 | `		if( rc == SXERR_ABORT ){` |
|        - | 1996 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1997 | `			return SXERR_ABORT;` |
|        - | 1998 | `		}` |
|       12 | 1999 | `		pGen->pIn = pGen->pEnd;` |
|       12 | 2000 | `		return SXRET_OK;` |
|        - | 2001 | `	}` |
|        - | 2002 | `	/* Create the function state */` |
|   158014 | 2003 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   158014 | 2004 | `	if( pFunc == 0 ){` |
|      ! 0 | 2005 | `		goto OutOfMem;` |
|        - | 2006 | `	}` |
|        - | 2007 | `	/* Build the function name, prepending namespace if active.` |
|        - | 2008 | `	 * A NAMED function (never a closure) also answers to php's import rules: its` |
|        - | 2009 | ``	 * short name must not already be a local `use function` import, and the name it`` |
|        - | 2010 | ``	 * takes is remembered so a later `use function` in this unit sees it. */`` |
|   158057 | 2011 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|        - | 2012 | `		SyBlob sFQN;` |
|        - | 2013 | `		sxu32 nLen;` |
|       91 | 2014 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       91 | 2015 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       91 | 2016 | `		SyBlobAppend(&sFQN,"\\",1);` |
|       91 | 2017 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|       91 | 2018 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|       91 | 2019 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|       91 | 2020 | `		SyBlobRelease(&sFQN);` |
|       91 | 2021 | `		if( zName == 0 ){` |
|      ! 0 | 2022 | `			goto OutOfMem;` |
|        - | 2023 | `		}` |
|       91 | 2024 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       48 | 2025 | `	}else{` |
|   157928 | 2026 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   157928 | 2027 | `		if( zName == 0 ){` |
|      ! 0 | 2028 | `			goto OutOfMem;` |
|        - | 2029 | `		}` |
|   157928 | 2030 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|        - | 2031 | `	}` |
|   158014 | 2032 | `	if( !bHandleClosure ){` |
|   151767 | 2033 | `		if( GenStateGuardImportRedeclare(pGen,1,pName,&pFunc->sName,nLine) == SXERR_ABORT ){` |
|      ! 0 | 2034 | `			return SXERR_ABORT;` |
|        - | 2035 | `		}` |
|   151767 | 2036 | `		GenStateRecordDeclaredName(pGen,1,&pFunc->sName);` |
|    75777 | 2037 | `	}` |
|        - | 2038 | ``	/* Take php's `{closure:SCOPE:LINE}` name the caller built for this closure. It has to`` |
|        - | 2039 | `	 * land here, ahead of the body, because a __FUNCTION__ inside the body reads it at` |
|        - | 2040 | `	 * compile time — and it must be cleared, since a NESTED declaration reaches this same` |
|        - | 2041 | `	 * point and would otherwise inherit its parent's. */` |
|   158014 | 2042 | `	if( SyStringLength(&pGen->sPendingClosureName) > 0 ){` |
|     6252 | 2043 | `		if( bHandleClosure ){` |
|     6252 | 2044 | `			pFunc->sClosureName = pGen->sPendingClosureName;` |
|     6252 | 2045 | `			pFunc->sClosureScope = pGen->sPendingClosureScope;` |
|     3107 | 2046 | `		}` |
|     6252 | 2047 | `		SyStringInitFromBuf(&pGen->sPendingClosureName,0,0);` |
|     3107 | 2048 | `	}` |
|        - | 2049 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|        - | 2050 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|   158014 | 2051 | `	pFunc->nLine = nLine;` |
|   158014 | 2052 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|   158014 | 2053 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 2054 | `		return SXERR_ABORT;` |
|        - | 2055 | `	}` |
|   158014 | 2056 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pFunc->aAttrs,2,2,0,0) == SXERR_ABORT ){` |
|      ! 0 | 2057 | `		return SXERR_ABORT;` |
|        - | 2058 | `	}` |
|        - | 2059 | `	/* Whose signature this is, for php's scope-keyword screen (see iSigScope): a` |
|        - | 2060 | `	 * closure's is EXEMPT, and a NAMED function has no class scope even when it is` |
|        - | 2061 | `	 * written inside a method body. */` |
|        - | 2062 | `	{` |
|   158014 | 2063 | `		int iSavedSig = pGen->iSigScope;` |
|   158014 | 2064 | `		pGen->iSigScope = bHandleClosure ? PH7_SIGSCOPE_CLOSURE : PH7_SIGSCOPE_FUNC;` |
|   158014 | 2065 | `		if( pGen->pIn < pEnd ){` |
|        - | 2066 | `			/* Collect function arguments */` |
|   145244 | 2067 | `			rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   145244 | 2068 | `			if( rc == SXERR_ABORT ){` |
|        - | 2069 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|        3 | 2070 | `				pGen->iSigScope = iSavedSig;` |
|        3 | 2071 | `				return SXERR_ABORT;` |
|        - | 2072 | `			}` |
|    72517 | 2073 | `		}` |
|        - | 2074 | `		/* Point past ')' and parse optional return type ': type' */` |
|   158012 | 2075 | `		pGen->pIn = &pEnd[1];` |
|        - | 2076 | `		{` |
|   158012 | 2077 | `			sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   158012 | 2078 | `			pGen->iSigScope = iSavedSig;` |
|   158012 | 2079 | `			if( rcRt == SXERR_ABORT ){` |
|      ! 0 | 2080 | `				return SXERR_ABORT;` |
|   158012 | 2081 | `			}else if( rcRt == SXERR_SYNTAX ){` |
|       15 | 2082 | `				return SXERR_SYNTAX;` |
|        - | 2083 | `			}` |
|        - | 2084 | `		}` |
|        - | 2085 | `	}` |
|        - | 2086 | `	/* php's #[\NoDiscard] declaration rules, which want the return type. A` |
|        - | 2087 | ``	 * closure's second (post-`use`) return-type parse re-runs below; the flag is`` |
|        - | 2088 | `	 * idempotent and the refusals are the same either way. */` |
|   158000 | 2089 | `	if( GenStateApplyNoDiscard(&(*pGen),pFunc,0,0) == SXERR_ABORT ){` |
|      ! 0 | 2090 | `		return SXERR_ABORT;` |
|        - | 2091 | `	}` |
|   158000 | 2092 | `	if( bHandleClosure ){` |
|        - | 2093 | `		ph7_vm_func_closure_env sEnv;` |
|     6252 | 2094 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     6247 | 2095 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     4053 | 2096 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|     1872 | 2097 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - | 2098 | `				/* Closure,record environment variable */` |
|     1872 | 2099 | `				pGen->pIn++;` |
|     1872 | 2100 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 2101 | `					rc = PH7_GenCompileError(pGen,E_PARSE,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|      ! 0 | 2102 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2103 | `						return SXERR_ABORT;` |
|        - | 2104 | `					}` |
|      ! 0 | 2105 | `				}` |
|     1872 | 2106 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|        - | 2107 | `				/* Compile until we hit the first closing parenthesis */` |
|     3977 | 2108 | `				while( pGen->pIn < pGen->pEnd ){` |
|     3977 | 2109 | `					int iFlagsLocal = 0;` |
|     3977 | 2110 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|     1870 | 2111 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|     1870 | 2112 | `						break;` |
|        - | 2113 | `					}` |
|     2112 | 2114 | `					nLineLocal = pGen->pIn->nLine;` |
|     2112 | 2115 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - | 2116 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|        - | 2117 | `						 * to the variable's memory slot instead of copying its value. */` |
|      439 | 2118 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|      439 | 2119 | `						pGen->pIn++;` |
|      213 | 2120 | `					}` |
|     2107 | 2121 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|     2112 | 2122 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2123 | `							rc = PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|        - | 2124 | `								"Closure: Unexpected token. Expecting a variable name");` |
|      ! 0 | 2125 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 2126 | `								return SXERR_ABORT;` |
|        - | 2127 | `							}` |
|        - | 2128 | `							/* Find the closing parenthesis */` |
|      ! 0 | 2129 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 2130 | `								pGen->pIn++;` |
|      ! 0 | 2131 | `							}` |
|      ! 0 | 2132 | `							if(pGen->pIn < pGen->pEnd){` |
|      ! 0 | 2133 | `								pGen->pIn++;` |
|      ! 0 | 2134 | `							}` |
|      ! 0 | 2135 | `							break;` |
|        - | 2136 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|      ! 0 | 2137 | `					}else{` |
|        - | 2138 | `						SyString *pNameLocal;` |
|        - | 2139 | `						char *zDup;` |
|        - | 2140 | `						/* Duplicate variable name */` |
|     2112 | 2141 | `						pNameLocal = &pGen->pIn[1].sData;` |
|     2112 | 2142 | `						if( PH7_VmIsAutoGlobal(pNameLocal->zString,pNameLocal->nByte) ){` |
|        - | 2143 | `							/* php's compile fatal. It is a real protection, not a` |
|        - | 2144 | `							 * style rule: the import resolves through hSuper, so` |
|        - | 2145 | `							 * installing the captured value would overwrite the` |
|        - | 2146 | ``							 * superglobal's own slot — `use ($GLOBALS)` replaced the`` |
|        - | 2147 | `							 * live symbol-table view with a snapshot and every later` |
|        - | 2148 | `							 * global went missing program-wide. */` |
|        3 | 2149 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - | 2150 | `								"Cannot use auto-global as lexical variable");` |
|        3 | 2151 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 2152 | `								return SXERR_ABORT;` |
|        - | 2153 | `							}` |
|        3 | 2154 | `							return SXERR_SYNTAX;` |
|        - | 2155 | `						}` |
|     2110 | 2156 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|     2110 | 2157 | `						if( zDup ){` |
|        - | 2158 | `							/* Zero the structure */` |
|     2110 | 2159 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     2110 | 2160 | `							sEnv.iFlags = iFlagsLocal;` |
|     2110 | 2161 | `							sEnv.nLine = nLineLocal; /* the capture's own source line (php warns here) */` |
|     2110 | 2162 | `							sEnv.nIdx = SXU32_HIGH;` |
|     2110 | 2163 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     2110 | 2164 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|     2234 | 2165 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|      247 | 2166 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|      ! 0 | 2167 | `									got_this = 1;` |
|      ! 0 | 2168 | `							}` |
|        - | 2169 | `							/* Save imported variable */` |
|     2110 | 2170 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|     1046 | 2171 | `						}else{` |
|      ! 0 | 2172 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2173 | `							 return SXERR_ABORT;` |
|        - | 2174 | `						}` |
|        - | 2175 | `					}` |
|     2110 | 2176 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|     2352 | 2177 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - | 2178 | `						/* Ignore trailing commas */` |
|      247 | 2179 | `						pGen->pIn++;` |
|        5 | 2180 | `					}` |
|        5 | 2181 | `				}` |
|        - | 2182 | `				/* php 7.1+: the return type follows the use clause —` |
|        - | 2183 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|        - | 2184 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|        - | 2185 | `				 * so an unconditional call would wipe a type parsed at the` |
|        - | 2186 | `				 * legacy pre-use position. */` |
|     1870 | 2187 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|       26 | 2188 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|       26 | 2189 | `					if( rcRt2 == SXERR_ABORT ){` |
|      ! 0 | 2190 | `						return SXERR_ABORT;` |
|       26 | 2191 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|      ! 0 | 2192 | `						return SXERR_SYNTAX;` |
|        - | 2193 | `					}` |
|        - | 2194 | `					/* The type this closure really declared is only known now. */` |
|       26 | 2195 | `					if( GenStateApplyNoDiscard(&(*pGen),pFunc,0,0) == SXERR_ABORT ){` |
|      ! 0 | 2196 | `						return SXERR_ABORT;` |
|        - | 2197 | `					}` |
|        9 | 2198 | `				}` |
|      925 | 2199 | `		}` |
|     6250 | 2200 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|        - | 2201 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|        - | 2202 | `			 * available to the closure environment — for EVERY non-static` |
|        - | 2203 | `			 * anonymous function, use list or not (php binds $this to any` |
|        - | 2204 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|        - | 2205 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|        - | 2206 | `			 * a global-scope closure is silently dropped at install. A static` |
|        - | 2207 | `			 * closure never binds $this (php). */` |
|     5912 | 2208 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     5912 | 2209 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|     5912 | 2210 | `			sEnv.nIdx = SXU32_HIGH;` |
|     5912 | 2211 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     5912 | 2212 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|     5912 | 2213 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|     2937 | 2214 | `		}` |
|     6250 | 2215 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|        - | 2216 | `			/* Mark as closure */` |
|     5990 | 2217 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|     2976 | 2218 | `		}` |
|     3106 | 2219 | `	}` |
|        - | 2220 | `	/* Compile the body */` |
|   157998 | 2221 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   157998 | 2222 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2223 | `		return SXERR_ABORT;` |
|        - | 2224 | `	}` |
|        - | 2225 | `	/* The cursor sits just past the body's closing brace */` |
|   157998 | 2226 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|   157998 | 2227 | `	if( ppFunc ){` |
|   157998 | 2228 | `		*ppFunc = pFunc;` |
|    78876 | 2229 | `	}` |
|   157998 | 2230 | `	rc = SXRET_OK;` |
|   157998 | 2231 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|   152013 | 2232 | `		if( GenStateDeclIsConditional(&(*pGen)) ){` |
|        - | 2233 | `			/* php binds this one when execution REACHES it, not now: a declaration` |
|        - | 2234 | ``			 * inside an `if`, a loop, a `try` or another function's body is not`` |
|        - | 2235 | `			 * early-bound. Binding it here made every` |
|        - | 2236 | ``			 * `if (!function_exists('x')) { function x(){} }` -- the shape every`` |
|        - | 2237 | `			 * symfony/polyfill-* package is written in -- REPLACE the engine's own` |
|        - | 2238 | ``			 * builtin, and declared the body of an `if (false)` besides. The`` |
|        - | 2239 | `			 * redeclaration screen moves to the opcode with it: two branches may` |
|        - | 2240 | `			 * each declare the name, and only the one that RUNS binds. */` |
|      171 | 2241 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FUNC_DECL,0,0,(void *)pFunc,0);` |
|      171 | 2242 | `			return SXRET_OK;` |
|        - | 2243 | `		}` |
|        - | 2244 | `		/* Reject a php-fatal redeclaration before hoisting the function */` |
|   151847 | 2245 | `		if( GenStateGuardFuncRedeclaration(pGen,pFunc) == SXERR_ABORT ){` |
|       13 | 2246 | `			return SXERR_ABORT;` |
|        - | 2247 | `		}` |
|        - | 2248 | `		/* Finally register the function */` |
|   151837 | 2249 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    75812 | 2250 | `	}` |
|   157822 | 2251 | `	if( rc == SXRET_OK ){` |
|   157822 | 2252 | `		return SXRET_OK;` |
|        - | 2253 | `	}` |
|        - | 2254 | `	/* Fall through if something goes wrong */` |
|      ! 0 | 2255 | `OutOfMem:` |
|        - | 2256 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - | 2257 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|        - | 2258 | `	 */` |
|      ! 0 | 2259 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 | 2260 | `	return SXERR_ABORT;` |
|    78893 | 2261 | `}` |
|        - | 2262 | `/*` |
|        - | 2263 | ` * Compile a standard PHP function.` |
|        - | 2264 | ` *  Refer to the block-comment above for more information.` |
|        - | 2265 | ` */` |
|   151776 | 2266 | `PH7_PRIVATE sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|        5 | 2267 | `{` |
|        - | 2268 | `	SyString *pName;` |
|        - | 2269 | `	sxi32 iFlags;` |
|        - | 2270 | `	sxu32 nKwLine;` |
|        - | 2271 | `	sxu32 nLine;` |
|        - | 2272 | `	sxi32 rc;` |
|        - | 2273 |  |
|   151781 | 2274 | `	nLine = pGen->pIn->nLine;` |
|   151781 | 2275 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|   151781 | 2276 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   151781 | 2277 | `	iFlags = 0;` |
|   151781 | 2278 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - | 2279 | `		/* Return by reference,remember that */` |
|       34 | 2280 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|        - | 2281 | `		/* Jump the '&' token */` |
|       34 | 2282 | `		pGen->pIn++;` |
|       15 | 2283 | `	}` |
|   151781 | 2284 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 2285 | `		/* Invalid function name */` |
|        6 | 2286 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|        6 | 2287 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2288 | `			return SXERR_ABORT;` |
|        - | 2289 | `		}` |
|        - | 2290 | `		/* Sychronize with the next semi-colon or braces*/` |
|       18 | 2291 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       14 | 2292 | `			pGen->pIn++;` |
|        2 | 2293 | `		}` |
|        6 | 2294 | `		return SXRET_OK;` |
|        - | 2295 | `	}` |
|   151777 | 2296 | `	pName = &pGen->pIn->sData;` |
|   151777 | 2297 | `	nLine = pGen->pIn->nLine;` |
|        - | 2298 | `	/* Jump the function name */` |
|   151777 | 2299 | `	pGen->pIn++;` |
|   151777 | 2300 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 2301 | `		/* Syntax error */` |
|        3 | 2302 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|        3 | 2303 | `		if( rc == SXERR_ABORT ){` |
|        - | 2304 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 2305 | `			return SXERR_ABORT;` |
|        - | 2306 | `		}` |
|        - | 2307 | `		/* Sychronize with the next semi-colon or '{' */` |
|        3 | 2308 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 2309 | `			pGen->pIn++;` |
|      ! 0 | 2310 | `		}` |
|        3 | 2311 | `		return SXRET_OK;` |
|        - | 2312 | `	}` |
|        - | 2313 | `	/* Compile function body */` |
|        - | 2314 | `	{` |
|   151775 | 2315 | `		ph7_vm_func *pFuncState = 0;` |
|   151775 | 2316 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|   151775 | 2317 | `		if( pFuncState ){` |
|        - | 2318 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|   151753 | 2319 | `			pFuncState->nLine = nKwLine;` |
|    75770 | 2320 | `		}` |
|        - | 2321 | `	}` |
|   151775 | 2322 | `	return rc;` |
|    75789 | 2323 | `}` |
|        - | 2324 |  |
