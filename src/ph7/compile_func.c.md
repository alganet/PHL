# src/ph7/compile_func.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 933/1085 lines (85.99%)

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
|    79754 |   68 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|        5 |   69 | `{` |
|        - |   70 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |   71 | `	SySet *pInstrContainer;` |
|        - |   72 | `	sxi32 rc;` |
|        - |   73 | `	/* Swap token stream */` |
|    79759 |   74 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|    79759 |   75 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    79759 |   76 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|        - |   77 | `	/* Compile the expression holding the argument value. A parameter default is a` |
|        - |   78 | `	 * const-expression belonging to the current class (see iInMemberDefault) — so` |
|        - |   79 | `	 * __TRAIT__ in it reads pCurClass rather than walking into the enclosing method. */` |
|    79759 |   80 | `	pGen->iInMemberDefault++;` |
|    79759 |   81 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    79759 |   82 | `	pGen->iInMemberDefault--;` |
|        - |   83 | `	/* Emit the done instruction */` |
|    79759 |   84 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    79759 |   85 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    79759 |   86 | `	RE_SWAP_DELIMITER(pGen);` |
|    79759 |   87 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |   88 | `		return SXERR_ABORT;` |
|        - |   89 | `	}` |
|    79759 |   90 | `	return SXRET_OK;` |
|    39882 |   91 | `}` |
|        - |   92 | `/*` |
|        - |   93 | ` * Collect function arguments one after one.` |
|        - |   94 | ` * According to the PHP language reference manual.` |
|        - |   95 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|        - |   96 | ` * list of expressions.` |
|        - |   97 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|        - |   98 | ` * and default argument values. Variable-length argument lists are also supported,` |
|        - |   99 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|        - |  100 | ` * for more information.` |
|        - |  101 | ` * Example #1 Passing arrays to functions` |
|        - |  102 | ` * <?php` |
|        - |  103 | ` * function takes_array($input)` |
|        - |  104 | ` * {` |
|        - |  105 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|        - |  106 | ` * }` |
|        - |  107 | ` * ?>` |
|        - |  108 | ` * Making arguments be passed by reference` |
|        - |  109 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|        - |  110 | ` * within the function is changed, it does not get changed outside of the function).` |
|        - |  111 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|        - |  112 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|        - |  113 | ` * to the argument name in the function definition:` |
|        - |  114 | ` * Example #2 Passing function parameters by reference` |
|        - |  115 | ` * <?php` |
|        - |  116 | ` * function add_some_extra(&$string)` |
|        - |  117 | ` * {` |
|        - |  118 | ` *   $string .= 'and something extra.';` |
|        - |  119 | ` * }` |
|        - |  120 | ` * $str = 'This is a string, ';` |
|        - |  121 | ` * add_some_extra($str);` |
|        - |  122 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|        - |  123 | ` * ?>` |
|        - |  124 | ` *` |
|        - |  125 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|        - |  126 | ` * complex agrument values.Please refer to the official documentation for more information` |
|        - |  127 | ` * on these extension.` |
|        - |  128 | ` */` |
|   138960 |  129 | `PH7_PRIVATE sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|        5 |  130 | `{` |
|        - |  131 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|        - |  132 | `	SyToken *pIn;  /* Token stream */` |
|        - |  133 | `	SyBlob sSig;         /* Function signature */` |
|        - |  134 | `	char *zDup;          /* Copy of argument name */` |
|        - |  135 | `	sxi32 rc;` |
|        - |  136 |  |
|   138965 |  137 | `	pIn = pGen->pIn;` |
|   138965 |  138 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|        - |  139 | `	/* Process arguments one after one */` |
|   202742 |  140 | `	for(;;){` |
|   405489 |  141 | `		if( pIn >= pEnd ){` |
|        - |  142 | `			/* No more arguments to process */` |
|   138947 |  143 | `			break;` |
|        - |  144 | `		}` |
|   266547 |  145 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   266547 |  146 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   266547 |  147 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   266547 |  148 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   266547 |  149 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|        - |  150 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|        - |  151 | `		 * first token inside the main token stream */` |
|   266547 |  152 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  153 | `			return SXERR_ABORT;` |
|        - |  154 | `		}` |
|        - |  155 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|        - |  156 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|        - |  157 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|        - |  158 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|        - |  159 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|        - |  160 | `		{` |
|   266547 |  161 | `			int bReadonly = 0, bVisSeen = 0;` |
|   266547 |  162 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   266547 |  163 | `			sxi32 iSetVisFlag = 0;` |
|        - |  164 | `			int nSetTok;` |
|        - |  165 | `			sxi32 nSetVis;` |
|   266547 |  166 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        3 |  167 | `				bReadonly = 1;` |
|        3 |  168 | `				pIn++;` |
|        1 |  169 | `			}` |
|   266547 |  170 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   266547 |  171 | `			if( nSetVis ){` |
|        - |  172 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|        3 |  173 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|        3 |  174 | `				bVisSeen = 1;` |
|        3 |  175 | `				pIn += nSetTok;` |
|        3 |  176 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|      ! 0 |  177 | `					bReadonly = 1;` |
|      ! 0 |  178 | `					pIn++;` |
|        1 |  179 | `				}` |
|   266546 |  180 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|    38069 |  181 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|    38069 |  182 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      109 |  183 | `					bVisSeen = 1;` |
|      109 |  184 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      144 |  185 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|       46 |  186 | `						: PH7_CLASS_PROT_PUBLIC;` |
|      109 |  187 | `					pIn++;` |
|      109 |  188 | `					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|      109 |  189 | `					if( nSetVis ){` |
|        - |  190 | ``						/* `public private(set) T $x` promoted form */`` |
|        3 |  191 | `						iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|        3 |  192 | `						pIn += nSetTok;` |
|        1 |  193 | `					}` |
|      109 |  194 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       20 |  195 | `						bReadonly = 1;` |
|       20 |  196 | `						pIn++;` |
|        8 |  197 | `					}` |
|       52 |  198 | `				}` |
|    19032 |  199 | `			}` |
|   266547 |  200 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|        5 |  201 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   266545 |  202 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|      ! 0 |  203 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|      ! 0 |  204 | `			}` |
|   266547 |  205 | `			if( bVisSeen \|\| bReadonly ){` |
|      113 |  206 | `				if( !bCtorCtx ){` |
|        6 |  207 | `					if( bAbstractCtx ){` |
|        3 |  208 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - |  209 | `							"Cannot declare promoted property in an abstract constructor");` |
|        2 |  210 | `					}else{` |
|        3 |  211 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - |  212 | `							"Cannot declare promoted property outside a constructor");` |
|        - |  213 | `					}` |
|        6 |  214 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  215 | `						return SXERR_ABORT;` |
|        - |  216 | `					}` |
|        6 |  217 | `					return SXERR_SYNTAX;` |
|        - |  218 | `				}` |
|      109 |  219 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      109 |  220 | `				sArg.iPromoteVis = iVis;` |
|      109 |  221 | `				if( bReadonly ){` |
|       22 |  222 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|        9 |  223 | `				}` |
|       52 |  224 | `			}` |
|        - |  225 | `		}` |
|        - |  226 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   266538 |  227 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   159651 |  228 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|    47992 |  229 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    38445 |  230 | `			sxu32 nLineLocal = pIn->nLine;` |
|    38445 |  231 | `			sxi32 iTFlags = 0;` |
|    38445 |  232 | `			pGen->pIn = pIn;` |
|    38445 |  233 | `			rc = GenStateParseUnionTypeDecl(` |
|    19220 |  234 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|    19220 |  235 | `				&iTFlags, &sArg.sTypeName,` |
|        - |  236 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|        - |  237 | `				/* bAllowVoid */ 0,` |
|    19220 |  238 | `						nLineLocal);` |
|    38445 |  239 | `			pIn = pGen->pIn;` |
|    38445 |  240 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  241 | `				return SXERR_ABORT;` |
|    38445 |  242 | `			}else if( rc == SXERR_CORRUPT ){` |
|        - |  243 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|        3 |  244 | `				return SXERR_SYNTAX;` |
|    38443 |  245 | `			}else if( rc == SXERR_SYNTAX ){` |
|       10 |  246 | `				if( pIn < pEnd ){` |
|       14 |  247 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|        - |  248 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|        4 |  249 | `						&pIn->sData);` |
|        6 |  250 | `				}else{` |
|      ! 0 |  251 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|        - |  252 | `						"syntax error, unexpected end of file");` |
|        - |  253 | `				}` |
|       10 |  254 | `				return SXERR_SYNTAX;` |
|        - |  255 | `			}` |
|    38435 |  256 | `			sArg.iFlags \|= iTFlags;` |
|    19215 |  257 | `		}` |
|   266533 |  258 | `		if( pIn >= pEnd ){` |
|      ! 0 |  259 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|      ! 0 |  260 | `			return rc;` |
|        - |  261 | `		}` |
|   266533 |  262 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|        - |  263 | `			/* Pass by reference,record that */` |
|     9587 |  264 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     9587 |  265 | `			pIn++;` |
|     4791 |  266 | `		}` |
|   266533 |  267 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|        - |  268 | `			/* Variadic parameter: ...$args */` |
|     4893 |  269 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|     4893 |  270 | `			pIn++;` |
|     2444 |  271 | `		}` |
|   266533 |  272 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  273 | `			/* Invalid argument */` |
|      ! 0 |  274 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|      ! 0 |  275 | `			return rc;` |
|        - |  276 | `		}` |
|   266533 |  277 | `		pIn++; /* Jump the dollar sign */` |
|        - |  278 | `		/* Copy argument name */` |
|   266533 |  279 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   266533 |  280 | `		if( zDup == 0 ){` |
|      ! 0 |  281 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  282 | `			return SXERR_ABORT;` |
|        - |  283 | `		}` |
|   266533 |  284 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   266533 |  285 | `		pIn++;` |
|   266533 |  286 | `		if( pIn < pEnd ){` |
|   183855 |  287 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|        - |  288 | `				SyToken *pDefend;` |
|    79761 |  289 | `				sxi32 iNest = 0;` |
|    79761 |  290 | `				pIn++; /* Jump the equal sign */` |
|    79761 |  291 | `				pDefend = pIn;` |
|        - |  292 | `				/* Process the default value associated with this argument */` |
|   168971 |  293 | `				while( pDefend < pEnd ){` |
|   112709 |  294 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    23499 |  295 | `						break;` |
|        - |  296 | `					}` |
|    89215 |  297 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|        - |  298 | `						/* Increment nesting level */` |
|       33 |  299 | `						iNest++;` |
|    89201 |  300 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|        - |  301 | `						/* Decrement nesting level */` |
|       33 |  302 | `						iNest--;` |
|       14 |  303 | `					}` |
|    89215 |  304 | `					pDefend++;` |
|        5 |  305 | `				}` |
|    79761 |  306 | `				if( pIn >= pDefend ){` |
|        3 |  307 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|        3 |  308 | `					return rc;` |
|        - |  309 | `				}` |
|        - |  310 | `				/* Process default value */` |
|    79759 |  311 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|    79759 |  312 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  313 | `					return rc;` |
|        - |  314 | `				}` |
|        - |  315 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|        - |  316 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|        - |  317 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|        - |  318 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|        - |  319 | `				 * arg-type check lets null through. */` |
|    79754 |  320 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|    44588 |  321 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|    44582 |  322 | `					&& &pIn[1] == pDefend` |
|     9406 |  323 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|     7040 |  324 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|     2346 |  325 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|        - |  326 | `` 					/* php 8.4 DEPRECATED the implicit-nullable form (`int $x = null` `` |
|        - |  327 | ``					 * without the `?`). PHL targets php's *non-deprecated* surface and`` |
|        - |  328 | ``					 * rejects it outright — the explicit `?int` must be written.`` |
|        - |  329 | ``					 * `mixed $x = null` is fine: mixed already includes null (explicit`` |
|        - |  330 | `					 * ?T / T\|null are already excluded via VM_FUNC_ARG_NULLABLE above). */` |
|        4 |  331 | `					if( sArg.sClass.nByte == sizeof("mixed")-1` |
|        5 |  332 | `						&& SyStrnicmp(SyStringData(&sArg.sClass),"mixed",sizeof("mixed")-1) == 0 ){` |
|        3 |  333 | `						sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|        2 |  334 | `					}else{` |
|        3 |  335 | `						const char *zSep = "";` |
|        3 |  336 | `						SyString sCls = { "", 0 };` |
|        3 |  337 | `						if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|      ! 0 |  338 | `							sCls = ((ph7_class *)pFunc->pUserData)->sName;` |
|      ! 0 |  339 | `							zSep = "::";` |
|      ! 0 |  340 | `						}` |
|        4 |  341 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,` |
|        - |  342 | `							"%z%s%z(): Cannot use null as the default for non-nullable parameter $%z; write the explicit ?T type instead",` |
|        1 |  343 | `							&sCls,zSep,&pFunc->sName,&sArg.sName);` |
|        3 |  344 | `						return SXERR_ABORT;` |
|        - |  345 | `					}` |
|        1 |  346 | `				}` |
|        - |  347 | `				/* Point beyond the default value */` |
|    79757 |  348 | `				pIn = pDefend;` |
|    39876 |  349 | `			}` |
|   183851 |  350 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 |  351 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|      ! 0 |  352 | `				return rc;` |
|        - |  353 | `			}` |
|   183851 |  354 | `			pIn++; /* Jump the trailing comma */` |
|    91923 |  355 | `		}` |
|        - |  356 | `		/* Append argument signature */` |
|   266529 |  357 | `		if( sArg.nType > 0 ){` |
|    38297 |  358 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|        - |  359 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|      309 |  360 | `				int marker = 'o';` |
|      309 |  361 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|      309 |  362 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|      157 |  363 | `			}else{` |
|        - |  364 | `				int c;` |
|    37993 |  365 | `				c = 'n'; /* cc warning */` |
|        - |  366 | `				/* Type leading character */` |
|    37993 |  367 | `				switch(sArg.nType){` |
|       32 |  368 | `				case MEMOBJ_HASHMAP:` |
|        - |  369 | `					/* Hashmap aka 'array' */` |
|       69 |  370 | `					c = 'h';` |
|       69 |  371 | `					break;` |
|     4852 |  372 | `				case MEMOBJ_INT:` |
|        - |  373 | `					/* Integer */` |
|     9709 |  374 | `					c = 'i';` |
|     9709 |  375 | `					break;` |
|        5 |  376 | `				case MEMOBJ_BOOL:` |
|        - |  377 | `					/* Bool */` |
|       13 |  378 | `					c = 'b';` |
|       13 |  379 | `					break;` |
|     4680 |  380 | `				case MEMOBJ_REAL:` |
|        - |  381 | `					/* Float */` |
|     9365 |  382 | `					c = 'f';` |
|     9365 |  383 | `					break;` |
|     9412 |  384 | `				case MEMOBJ_STRING:` |
|        - |  385 | `					/* String */` |
|    18829 |  386 | `					c = 's';` |
|    18829 |  387 | `					break;` |
|       12 |  388 | `				case MEMOBJ_OBJ:` |
|        - |  389 | `					/* Object */` |
|       28 |  390 | `					c = 'o';` |
|       24 |  391 | `					break;` |
|        1 |  392 | `				default:` |
|        2 |  393 | `					break;` |
|        - |  394 | `				}` |
|    37993 |  395 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|        - |  396 | `			}` |
|    19151 |  397 | `		}else{` |
|        - |  398 | `			/* No type is associated with this parameter which mean` |
|        - |  399 | `			 * that this function is not condidate for overloading.` |
|        - |  400 | `			 */` |
|   228237 |  401 | `			SyBlobRelease(&sSig);` |
|        - |  402 | `		}` |
|        - |  403 | `		/* Save in the argument set */` |
|   266529 |  404 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|        5 |  405 | `	}` |
|   138947 |  406 | `	if( SyBlobLength(&sSig) > 0 ){` |
|        - |  407 | `		/* Save function signature */` |
|    14761 |  408 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|     7378 |  409 | `	}` |
|   138947 |  410 | `	return SXRET_OK;` |
|    69485 |  411 | `}` |
|        - |  412 | `/*` |
|        - |  413 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|        - |  414 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|        - |  415 | ` * the enclosing function. Returns the token just past the nested construct.` |
|        - |  416 | ` */` |
|      172 |  417 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|        5 |  418 | `{` |
|      177 |  419 | `	sxi32 iParen = 0;` |
|      177 |  420 | `	pIn++; /* past 'function'/'fn' */` |
|        - |  421 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|        - |  422 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|        - |  423 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|      959 |  424 | `	while( pIn < pEnd ){` |
|      959 |  425 | `		sxu32 t = pIn->nType;` |
|      959 |  426 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|      753 |  427 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|      547 |  428 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|      375 |  429 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|      787 |  430 | `		pIn++;` |
|        5 |  431 | `	}` |
|      177 |  432 | `	if( pIn >= pEnd ){ return pIn; }` |
|        - |  433 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|        - |  434 | `	{` |
|      177 |  435 | `		sxi32 d = 0;` |
|     1727 |  436 | `		while( pIn < pEnd ){` |
|     1727 |  437 | `			sxu32 t = pIn->nType;` |
|     1727 |  438 | `			if( t & PH7_TK_OCB ){ d++; }` |
|     1543 |  439 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|     1555 |  440 | `			pIn++;` |
|        5 |  441 | `		}` |
|        - |  442 | `	}` |
|      177 |  443 | `	return pIn;` |
|       91 |  444 | `}` |
|        - |  445 | `/*` |
|        - |  446 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|        - |  447 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|        - |  448 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|        - |  449 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|        - |  450 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|        - |  451 | ` * detached-mini-program path untouched.` |
|        - |  452 | ` */` |
|        - |  453 | `/*` |
|        - |  454 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|        - |  455 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|        - |  456 | ` * mixed, object.` |
|        - |  457 | ` */` |
|       28 |  458 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|        3 |  459 | `{` |
|        - |  460 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|        - |  461 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|        - |  462 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|        - |  463 | `	};` |
|        - |  464 | `	sxu32 i;` |
|       31 |  465 | `	if( nName > 0 && zName[0] == '\\' ){` |
|      ! 0 |  466 | `		zName++;` |
|      ! 0 |  467 | `		nName--;` |
|      ! 0 |  468 | `	}` |
|       45 |  469 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|       45 |  470 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|       31 |  471 | `			return 1;` |
|        - |  472 | `		}` |
|        8 |  473 | `	}` |
|      ! 0 |  474 | `	return 0;` |
|       17 |  475 | `}` |
|        - |  476 | `/*` |
|        - |  477 | ` * One atom of a generator's declared return type: is it a supertype of` |
|        - |  478 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|        - |  479 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|        - |  480 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|        - |  481 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|        - |  482 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|        - |  483 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|        - |  484 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|        - |  485 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|        - |  486 | ` * both rather than fatal on valid code (a recorded divergence).` |
|        - |  487 | ` */` |
|       30 |  488 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|        4 |  489 | `{` |
|       34 |  490 | `	if( nType == MEMOBJ_OBJ ){` |
|      ! 0 |  491 | ``		return 1; /* bare `object` */`` |
|        - |  492 | `	}` |
|       34 |  493 | `	if( nType != SXU32_HIGH ){` |
|        3 |  494 | `		return 0; /* scalar/array/void/never/null/... */` |
|        - |  495 | `	}` |
|       31 |  496 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|       31 |  497 | `		return 1;` |
|        - |  498 | `	}` |
|        - |  499 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|        - |  500 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|        - |  501 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|        - |  502 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|        - |  503 | `	{` |
|        - |  504 | `		SyBlob sFQN;` |
|        - |  505 | `		int bOk;` |
|      ! 0 |  506 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      ! 0 |  507 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|      ! 0 |  508 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|      ! 0 |  509 | `		SyBlobRelease(&sFQN);` |
|      ! 0 |  510 | `		return bOk;` |
|        - |  511 | `	}` |
|       19 |  512 | `}` |
|        - |  513 | `/*` |
|        - |  514 | ` * php 8: a generator function may only declare a return type that is a` |
|        - |  515 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|        - |  516 | ` * group qualifies only if every member does. Anything else is php's exact` |
|        - |  517 | ` * compile-time fatal "Generator return type must be a supertype of` |
|        - |  518 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|        - |  519 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|        - |  520 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|        - |  521 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|        - |  522 | ` */` |
|      330 |  523 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|        5 |  524 | `{` |
|      335 |  525 | `	int bOk = 0;` |
|        - |  526 | `	sxu32 nLine;` |
|        - |  527 | `	sxi32 rc;` |
|      335 |  528 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|      305 |  529 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|        - |  530 | `	}` |
|       34 |  531 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|      ! 0 |  532 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  533 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|        - |  534 | `		sxu32 i,j;` |
|      ! 0 |  535 | `		for( i = 0; i < n && !bOk; i++ ){` |
|        - |  536 | `			int bGroupOk;` |
|      ! 0 |  537 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|      ! 0 |  538 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|        - |  539 | `			}` |
|      ! 0 |  540 | `			bGroupOk = 1;` |
|      ! 0 |  541 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|      ! 0 |  542 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|      ! 0 |  543 | `					bGroupOk = 0;` |
|      ! 0 |  544 | `					break;` |
|        - |  545 | `				}` |
|      ! 0 |  546 | `			}` |
|      ! 0 |  547 | `			bOk = bGroupOk;` |
|      ! 0 |  548 | `		}` |
|      ! 0 |  549 | `	}else{` |
|       34 |  550 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|        - |  551 | `	}` |
|       34 |  552 | `	if( bOk ){` |
|       31 |  553 | `		return SXRET_OK;` |
|        - |  554 | `	}` |
|        - |  555 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|        - |  556 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|        - |  557 | `	 * token of this stream — its line is the function's closing brace. php` |
|        - |  558 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|        - |  559 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|        3 |  560 | `	nLine = pGen->pIn[-1].nLine;` |
|        - |  561 | `	{` |
|        3 |  562 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|        3 |  563 | `		if( sGiven.nByte < 1 ){` |
|      ! 0 |  564 | `			sGiven = pFunc->sReturnClass;` |
|      ! 0 |  565 | `		}` |
|        3 |  566 | `		if( sGiven.nByte < 1 ){` |
|        - |  567 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|        - |  568 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|        - |  569 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|      ! 0 |  570 | `			const char *zScalar =` |
|      ! 0 |  571 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|      ! 0 |  572 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|      ! 0 |  573 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|      ! 0 |  574 | `		}` |
|        3 |  575 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  576 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|        - |  577 | `	}` |
|        3 |  578 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      170 |  579 | `}` |
|   148312 |  580 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|        5 |  581 | `{` |
|   148317 |  582 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|   148317 |  583 | `	SyToken *pEnd = pGen->pEnd;` |
|   148317 |  584 | `	sxi32 iDepth = 0;` |
|   148317 |  585 | `	int bStarted = 0;` |
| 14477467 |  586 | `	while( pIn < pEnd ){` |
| 14477467 |  587 | `		sxu32 t = pIn->nType;` |
| 14477467 |  588 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 13869265 |  589 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 13261527 |  590 | `		if( t & PH7_TK_KEYWORD ){` |
|  1207533 |  591 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  1207533 |  592 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  1207203 |  593 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|        - |  594 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|   603513 |  595 | `		}` |
| 13261025 |  596 | `		pIn++;` |
|        5 |  597 | `	}` |
|   147987 |  598 | `	return FALSE;` |
|    74161 |  599 | `}` |
|        - |  600 | `/*` |
|        - |  601 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|        - |  602 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  603 | ` * and this routine takes care of generating the appropriate error message.` |
|        - |  604 | ` */` |
|   148312 |  605 | `PH7_PRIVATE sxi32 GenStateCompileFuncBody(` |
|        - |  606 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |  607 | `	ph7_vm_func *pFunc    /* Function state */` |
|        - |  608 | `	)` |
|        5 |  609 | `{` |
|        - |  610 | `	SySet *pInstrContainer; /* Instruction container */` |
|        - |  611 | `	GenBlock *pBlock;` |
|        - |  612 | `	sxu32 nGotoOfft;` |
|        - |  613 | `	sxi32 rc;` |
|        - |  614 | `	/* Attach the new function */` |
|   148317 |  615 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   148317 |  616 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  617 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|        - |  618 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  619 | `		return SXERR_ABORT;` |
|        - |  620 | `	}` |
|   148317 |  621 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|        - |  622 | `	/* Swap bytecode containers */` |
|   148317 |  623 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   148317 |  624 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|        - |  625 | `	/* Emit constructor property promotion prologue:` |
|        - |  626 | `	 *   $this->NAME = $NAME;` |
|        - |  627 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|        - |  628 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|        - |  629 | `	{` |
|   148317 |  630 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|        - |  631 | `		sxu32 i;` |
|   414435 |  632 | `		for( i = 0; i < nArg; i++ ){` |
|   266123 |  633 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|        - |  634 | `			char *zSrc;` |
|        - |  635 | `			sxu32 nSrc,nName;` |
|        - |  636 | `			SySet sToken;` |
|        - |  637 | `			SyToken *pTmpIn,*pTmpEnd;` |
|        - |  638 | `			sxi32 rcPromote;` |
|   266123 |  639 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   266029 |  640 | `				continue;` |
|        - |  641 | `			}` |
|        - |  642 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|        - |  643 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|        - |  644 | `			 * copied), so it must outlive the function — never free it. The` |
|        - |  645 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|        - |  646 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|       99 |  647 | `			nName = SyStringLength(&pArg->sName);` |
|       99 |  648 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|       99 |  649 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|       99 |  650 | `			if( zSrc == 0 ){` |
|      ! 0 |  651 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  652 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  653 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  654 | `				return SXERR_ABORT;` |
|        - |  655 | `			}` |
|        - |  656 | `			{` |
|       99 |  657 | `				char *z = zSrc;` |
|       99 |  658 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|       99 |  659 | `				z += sizeof("$this->")-1;` |
|       99 |  660 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|       99 |  661 | `				z += nName;` |
|       99 |  662 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|       99 |  663 | `				z += sizeof(" = $")-1;` |
|       99 |  664 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|       99 |  665 | `				z += nName;` |
|       99 |  666 | `				*z = 0;` |
|        - |  667 | `			}` |
|       99 |  668 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       99 |  669 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|       99 |  670 | `			pTmpIn = pGen->pIn;` |
|       99 |  671 | `			pTmpEnd = pGen->pEnd;` |
|       99 |  672 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|       99 |  673 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       99 |  674 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|       99 |  675 | `			pGen->pIn = pTmpIn;` |
|       99 |  676 | `			pGen->pEnd = pTmpEnd;` |
|       99 |  677 | `			SySetRelease(&sToken);` |
|       99 |  678 | `			if( rcPromote == SXERR_ABORT ){` |
|      ! 0 |  679 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  680 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  681 | `				return SXERR_ABORT;` |
|        - |  682 | `			}` |
|        - |  683 | `			/* Discard the assignment result — this is a statement expression. */` |
|       99 |  684 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       52 |  685 | `		}` |
|        - |  686 | `	}` |
|        - |  687 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|        - |  688 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|        - |  689 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|        - |  690 | `	 * generator — and vice versa — is classified independently. */` |
|        - |  691 | `	{` |
|   148317 |  692 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|   148317 |  693 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|        - |  694 | `		/* Compile the body */` |
|   148317 |  695 | `		PH7_CompileBlock(&(*pGen),0);` |
|   148317 |  696 | `		pGen->bInGenerator = bSavedGen;` |
|        - |  697 | `	}` |
|        - |  698 | `	/* Fix exception jumps now the destination is resolved */` |
|   148317 |  699 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - |  700 | `	/* Emit the final return if not yet done */` |
|   148317 |  701 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - |  702 | `	/* Fix gotos jumps now the destination is resolved */` |
|   148317 |  703 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|      ! 0 |  704 | `		rc = SXERR_ABORT;` |
|      ! 0 |  705 | `	}` |
|   148317 |  706 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|        - |  707 | `	/* Restore the default container */` |
|   148317 |  708 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - |  709 | `	/* Leave function block */` |
|   148317 |  710 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   148317 |  711 | `	if( rc == SXERR_ABORT ){` |
|        - |  712 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  713 | `		return SXERR_ABORT;` |
|        - |  714 | `	}` |
|        - |  715 | `	/* Scan for yield opcodes to detect generator functions */` |
|        - |  716 | `	{` |
|   148317 |  717 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|        - |  718 | `		sxu32 i;` |
|  9283515 |  719 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
|  9135533 |  720 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|      335 |  721 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      335 |  722 | `				break;` |
|        - |  723 | `			}` |
|  4567604 |  724 | `		}` |
|        - |  725 | `	}` |
|   148317 |  726 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  727 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|      335 |  728 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|      ! 0 |  729 | `			return SXERR_ABORT;` |
|        - |  730 | `		}` |
|      165 |  731 | `	}` |
|        - |  732 | `	/* All done, function body compiled */` |
|   148317 |  733 | `	return SXRET_OK;` |
|    74161 |  734 | `}` |
|        - |  735 | `/*` |
|        - |  736 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|        - |  737 | ` * According to the PHP language reference manual.` |
|        - |  738 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|        - |  739 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|        - |  740 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|        - |  741 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - |  742 | ` *  Functions need not be defined before they are referenced.` |
|        - |  743 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|        - |  744 | ` *  a function even if they were defined inside and vice versa.` |
|        - |  745 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|        - |  746 | ` *  calls with over 32-64 recursion levels.` |
|        - |  747 | ` *` |
|        - |  748 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|        - |  749 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|        - |  750 | ` * on these extension.` |
|        - |  751 | ` */` |
|        - |  752 | `/*` |
|        - |  753 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|        - |  754 | ` */` |
|      968 |  755 | `PH7_PRIVATE int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|        5 |  756 | `{` |
|        - |  757 | `	sxu32 i;` |
|     2587 |  758 | `	for( i = 0; i < n; i++ ){` |
|     2215 |  759 | `		int a = zA[i], b = zB[i];` |
|     2215 |  760 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     2215 |  761 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     2215 |  762 | `		if( a != b ) return a - b;` |
|      812 |  763 | `	}` |
|      377 |  764 | `	return 0;` |
|      489 |  765 | `}` |
|        - |  766 | `/*` |
|        - |  767 | ` * Internal type-atom kinds used during union type parsing.` |
|        - |  768 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|        - |  769 | ` * (which are positive bit values stored in sxu32).` |
|        - |  770 | ` */` |
|        - |  771 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|        - |  772 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|        - |  773 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|        - |  774 |  |
|        - |  775 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|        - |  776 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|        - |  777 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|        - |  778 |  |
|        - |  779 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|        - |  780 | `struct PhlTypeAtom {` |
|        - |  781 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|        - |  782 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|        - |  783 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|        - |  784 | `	sxu32 nCanon;` |
|        - |  785 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|        - |  786 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|        - |  787 | `};` |
|        - |  788 |  |
|        - |  789 | `/*` |
|        - |  790 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|        - |  791 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|        - |  792 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|        - |  793 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|        - |  794 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|        - |  795 | ` * already be consumed by the caller.` |
|        - |  796 | ` */` |
|        - |  797 | `/*` |
|        - |  798 | ` * TRUE if pName is a reserved PHP type keyword (never a class name), so a type` |
|        - |  799 | ` * hint that spells it must not be namespace-qualified. Only the words that can` |
|        - |  800 | ` * reach GenStateParseOneTypeAtom's identifier branch as a bare name matter here` |
|        - |  801 | ` * (bool/int/float/string/array/object/self/static/parent arrive as keywords, and` |
|        - |  802 | ` * null/void/never are matched before the class path), but the full set is listed` |
|        - |  803 | ` * so the guard is robust to lexer changes.` |
|        - |  804 | ` */` |
|      704 |  805 | `static int GenStateIsReservedTypeWord(const SyString *pName)` |
|        5 |  806 | `{` |
|        - |  807 | `	static const char *azWords[] = {` |
|        - |  808 | `		"false","true","mixed","iterable","callable","null","void","never",` |
|        - |  809 | `		"bool","boolean","int","integer","float","double","string","array",` |
|        - |  810 | `		"object","self","static","parent"` |
|        - |  811 | `	};` |
|        - |  812 | `	sxu32 i;` |
|     8797 |  813 | `	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){` |
|     8439 |  814 | `		sxu32 n = (sxu32)SyStrlen(azWords[i]);` |
|     8439 |  815 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azWords[i],n) == 0 ){` |
|      351 |  816 | `			return 1;` |
|        - |  817 | `		}` |
|     4049 |  818 | `	}` |
|      363 |  819 | `	return 0;` |
|      357 |  820 | `}` |
|    45324 |  821 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|        5 |  822 | `{` |
|    45329 |  823 | `	SyToken *pIn = pGen->pIn;` |
|    45329 |  824 | `	int bAbsolute = 0;` |
|    45329 |  825 | `	SyZero(pOut, sizeof(*pOut));` |
|    45329 |  826 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    45329 |  827 | `	if( pIn >= pGen->pEnd ){` |
|      ! 0 |  828 | `		return SXERR_SYNTAX;` |
|        - |  829 | `	}` |
|        - |  830 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    45329 |  831 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       12 |  832 | `		bAbsolute = 1; /* fully-qualified: never prefix the current namespace */` |
|       12 |  833 | `		pIn++;` |
|       12 |  834 | `		if( pIn >= pGen->pEnd ){` |
|      ! 0 |  835 | `			return SXERR_SYNTAX;` |
|        - |  836 | `		}` |
|        5 |  837 | `	}` |
|        - |  838 | ``	/* `namespace\X` type hint: the CURRENT namespace spelled out, fully qualified`` |
|        - |  839 | `` 	 * from there. Collected here rather than below because the leading `namespace` `` |
|        - |  840 | `	 * is a KEYWORD token, which the atom parser would otherwise reject outright. */` |
|    45329 |  841 | `	if( !bAbsolute ){` |
|        - |  842 | `		SyBlob sRel;` |
|    45319 |  843 | `		SyBlobInit(&sRel,&pGen->pVm->sAllocator);` |
|    45319 |  844 | `		if( GenStateNsRelPrefix(pGen,&pIn,pGen->pEnd,&sRel) ){` |
|        - |  845 | `			char *zDup;` |
|        7 |  846 | `			SyBlobAppend(&sRel,pIn->sData.zString,pIn->sData.nByte);` |
|        7 |  847 | `			pIn++;` |
|        9 |  848 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|        7 |  849 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  850 | `				SyBlobAppend(&sRel,"\\",1);` |
|      ! 0 |  851 | `				SyBlobAppend(&sRel,pIn[1].sData.zString,pIn[1].sData.nByte);` |
|      ! 0 |  852 | `				pIn += 2;` |
|      ! 0 |  853 | `			}` |
|       10 |  854 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        6 |  855 | `				(const char *)SyBlobData(&sRel),SyBlobLength(&sRel));` |
|        7 |  856 | `			if( zDup == 0 ){` |
|      ! 0 |  857 | `				SyBlobRelease(&sRel);` |
|      ! 0 |  858 | `				return SXERR_ABORT;` |
|        - |  859 | `			}` |
|        7 |  860 | `			pOut->nType = SXU32_HIGH;` |
|        7 |  861 | `			SyStringInitFromBuf(&pOut->sClass,zDup,SyBlobLength(&sRel));` |
|        7 |  862 | `			SyBlobRelease(&sRel);` |
|        7 |  863 | `			pGen->pIn = pIn;` |
|        7 |  864 | `			return SXRET_OK;` |
|        - |  865 | `		}` |
|    45313 |  866 | `		SyBlobRelease(&sRel);` |
|    22654 |  867 | `	}` |
|    45323 |  868 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  869 | `		return SXERR_SYNTAX;` |
|        - |  870 | `	}` |
|    45323 |  871 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|    44243 |  872 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|    44243 |  873 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      195 |  874 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|    44148 |  875 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      135 |  876 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|    43988 |  877 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|    10383 |  878 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|    38734 |  879 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|    19325 |  880 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|    23885 |  881 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|    14079 |  882 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|     7188 |  883 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|       43 |  884 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      132 |  885 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       48 |  886 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|      110 |  887 | `			pOut->nType = SXU32_HIGH;` |
|      110 |  888 | `			pOut->sClass = pIn->sData;` |
|       57 |  889 | `		}else{` |
|        3 |  890 | `			return SXERR_SYNTAX;` |
|        - |  891 | `		}` |
|    44241 |  892 | `		pIn++;` |
|    22123 |  893 | `	}else{` |
|        - |  894 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|        - |  895 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|     1085 |  896 | `		SyString *pT = &pIn->sData;` |
|     1085 |  897 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|       36 |  898 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|       36 |  899 | `			pIn++;` |
|     1069 |  900 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|      311 |  901 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|      311 |  902 | `			pIn++;` |
|      900 |  903 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       33 |  904 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       33 |  905 | `			pIn++;` |
|       19 |  906 | `		}else{` |
|        - |  907 | `			/* Class / interface name; consume namespace path a\b\c */` |
|      719 |  908 | `			SyToken *pFirst = pIn;` |
|      719 |  909 | `			SyToken *pLast = pIn;` |
|      719 |  910 | `			pOut->nType = SXU32_HIGH;` |
|      719 |  911 | `			pOut->sClass = pIn->sData;` |
|      719 |  912 | `			pIn++;` |
|     1074 |  913 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|      722 |  914 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|        3 |  915 | `				pLast = &pIn[1];` |
|        3 |  916 | `				pIn += 2;` |
|        1 |  917 | `			}` |
|      719 |  918 | `			if( pLast != pFirst ){` |
|        3 |  919 | `				const char *zFirst = pFirst->sData.zString;` |
|        3 |  920 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|        3 |  921 | `				pOut->sClass.zString = zFirst;` |
|        3 |  922 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|        1 |  923 | `			}` |
|        - |  924 | `			/* Namespace-qualify a bare (single-segment, non-absolute) class type so` |
|        - |  925 | ``			 * a `: Base` / `Base $x` hint in namespace N resolves to N\Base (or a`` |
|        - |  926 | ``			 * `use` alias) at type-check time instead of the global \Base — mirrors`` |
|        - |  927 | `			 * the NEW/CALL/instanceof qualification. Absolute (\Base) and already-` |
|        - |  928 | `			 * qualified (A\B) names are left as written, matching GenStateNsQualifyName. */` |
|        - |  929 | `			/* Reserved type words that reach this identifier branch (false, true,` |
|        - |  930 | `			 * mixed, iterable, callable) are NOT classes and must not be qualified` |
|        - |  931 | ``			 * (else `false\|string` becomes `Ns\false\|string`). */`` |
|      719 |  932 | `			if( !bAbsolute && pLast == pFirst && !GenStateIsReservedTypeWord(&pOut->sClass) ){` |
|        - |  933 | `				SyBlob sFqn;` |
|      363 |  934 | `				SyBlobInit(&sFqn,&pGen->pVm->sAllocator);` |
|      363 |  935 | `				GenStateResolveName(pGen,&pOut->sClass,&sFqn);` |
|      358 |  936 | `				if( SyBlobLength(&sFqn) != pOut->sClass.nByte` |
|      356 |  937 | `				 \|\| SyMemcmp(SyBlobData(&sFqn),(const void *)pOut->sClass.zString,pOut->sClass.nByte) != 0 ){` |
|       24 |  938 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       14 |  939 | `						(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));` |
|       17 |  940 | `					if( zDup ){` |
|       17 |  941 | `						SyStringInitFromBuf(&pOut->sClass,zDup,SyBlobLength(&sFqn));` |
|        7 |  942 | `					}` |
|        7 |  943 | `				}` |
|      363 |  944 | `				SyBlobRelease(&sFqn);` |
|      179 |  945 | `			}` |
|        - |  946 | `		}` |
|        - |  947 | `	}` |
|    45321 |  948 | `	pGen->pIn = pIn;` |
|    45321 |  949 | `	return SXRET_OK;` |
|    22667 |  950 | `}` |
|        - |  951 |  |
|        - |  952 | `/* Class-name ATOMS that php files with the BUILT-IN types rather than the` |
|        - |  953 | `` * classes. `callable`, `false` and `true` are type-mask bits in zend, so they`` |
|        - |  954 | `` * print AFTER every class name however they were written (`false\|A1` reads`` |
|        - |  955 | `` * `A1\|false`). `iterable` is two types at once: the class `Traversable`, which`` |
|        - |  956 | `` * keeps the atom's declaration position among the classes, plus `array`, which`` |
|        - |  957 | `` * prints with the builtins (`iterable\|A1` reads `Traversable\|A1\|array`). PH7`` |
|        - |  958 | ` * parses all four as class-name atoms, which is why they used to sort with the` |
|        - |  959 | ` * classes. */` |
|        - |  960 | `#define GEN_ATOM_PLAIN    0` |
|        - |  961 | `#define GEN_ATOM_CALLABLE 1` |
|        - |  962 | `#define GEN_ATOM_FALSE    2` |
|        - |  963 | `#define GEN_ATOM_TRUE     3` |
|        - |  964 | `#define GEN_ATOM_ITERABLE 4` |
|   180766 |  965 | `static int GenAtomMaskKind(const PhlTypeAtom *pAtom)` |
|        5 |  966 | `{` |
|   180771 |  967 | `	const SyString *p = &pAtom->sClass;` |
|   180771 |  968 | `	if( pAtom->nType != SXU32_HIGH \|\| p->zString == 0 ){` |
|   177093 |  969 | `		return GEN_ATOM_PLAIN;` |
|        - |  970 | `	}` |
|     3683 |  971 | `	if( p->nByte == 8 && SyStrnicmp(p->zString,"callable",8) == 0 ) return GEN_ATOM_CALLABLE;` |
|     2995 |  972 | `	if( p->nByte == 5 && SyStrnicmp(p->zString,"false",5) == 0 )    return GEN_ATOM_FALSE;` |
|     2833 |  973 | `	if( p->nByte == 4 && SyStrnicmp(p->zString,"true",4) == 0 )     return GEN_ATOM_TRUE;` |
|     2773 |  974 | `	if( p->nByte == 8 && SyStrnicmp(p->zString,"iterable",8) == 0 ) return GEN_ATOM_ITERABLE;` |
|     2601 |  975 | `	return GEN_ATOM_PLAIN;` |
|    90388 |  976 | `}` |
|        - |  977 | ``/* Emit one class-like atom: when *bExpandIterable*, `iterable` contributes only`` |
|        - |  978 | `` * its Traversable half here (the `array` half rides the built-in pass). php`` |
|        - |  979 | `` * expands it only in a COMPOUND type — a standalone `iterable`/`?iterable` keeps`` |
|        - |  980 | ` * its name in the canonical text (which is what Reflection prints; the TypeError` |
|        - |  981 | ` * for a standalone hint is rendered separately, by VmClassHintTypeName). */` |
|      576 |  982 | `static void GenAppendClassAtom(SyBlob *pBlob, const PhlTypeAtom *pAtom, int bExpandIterable)` |
|        5 |  983 | `{` |
|      581 |  984 | `	if( bExpandIterable && GenAtomMaskKind(pAtom) == GEN_ATOM_ITERABLE ){` |
|       13 |  985 | `		SyBlobAppend(pBlob, "Traversable", sizeof("Traversable")-1);` |
|       13 |  986 | `		return;` |
|        - |  987 | `	}` |
|      569 |  988 | `	SyBlobAppend(pBlob, pAtom->sClass.zString, pAtom->sClass.nByte);` |
|      293 |  989 | `}` |
|        - |  990 | `/*` |
|        - |  991 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|        - |  992 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|        - |  993 | ` *   classes (in declaration order)` |
|        - |  994 | ` *   \| callable \| object \| array \| string \| int \| float \| bool \| false \| true` |
|        - |  995 | ` *   [\| null]` |
|        - |  996 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|        - |  997 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|        - |  998 | ` */` |
|    45020 |  999 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|        5 | 1000 | `{` |
|        - | 1001 | `	int i;` |
|    45025 | 1002 | `	int nNonNull = 0;` |
|    45025 | 1003 | `	int bAnyIntersection = 0;` |
|        - | 1004 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    45025 | 1005 | `	sxu32 nMaxGroup = 0;` |
|  1485665 | 1006 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    90323 | 1007 | `	for( i = 0; i < nAtoms; i++ ){` |
|    45303 | 1008 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    45271 | 1009 | `			nNonNull++;` |
|    45271 | 1010 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    45271 | 1011 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    45271 | 1012 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|    22633 | 1013 | `			}` |
|    22633 | 1014 | `		}` |
|    22654 | 1015 | `	}` |
|    90255 | 1016 | `	for( i = 0; i < nAtoms; i++ ){` |
|    45265 | 1017 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       35 | 1018 | `			bAnyIntersection = 1;` |
|       35 | 1019 | `			break;` |
|        - | 1020 | `		}` |
|    22620 | 1021 | `	}` |
|    45025 | 1022 | `	if( bAnyIntersection ){` |
|        - | 1023 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|        - | 1024 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|        - | 1025 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|       35 | 1026 | `		sxu32 g, nGroups = 0;` |
|       35 | 1027 | `		int bFirstGroup = 1;` |
|       75 | 1028 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|       75 | 1029 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|       45 | 1030 | `			int bFirstMember = 1;` |
|        - | 1031 | `			int bWrap;` |
|       45 | 1032 | `			if( aGroupCount[g] == 0 ) continue;` |
|        - | 1033 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|        - | 1034 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|        - | 1035 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|        - | 1036 | `			 * parens, matching PHP's canonical text. */` |
|       60 | 1037 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|       45 | 1038 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|       45 | 1039 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      145 | 1040 | `			for( i = 0; i < nAtoms; i++ ){` |
|      105 | 1041 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|       75 | 1042 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|       75 | 1043 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       71 | 1044 | `					GenAppendClassAtom(pBlob, &aAtoms[i], 1);` |
|       38 | 1045 | `				}else{` |
|        6 | 1046 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - | 1047 | `				}` |
|       75 | 1048 | `				bFirstMember = 0;` |
|       40 | 1049 | `			}` |
|       45 | 1050 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|       45 | 1051 | `			bFirstGroup = 0;` |
|       25 | 1052 | `		}` |
|        - | 1053 | ``		/* `iterable` printed its Traversable half in place; its `array` half goes`` |
|        - | 1054 | ``		 * last, as php does (`(I1&I2)\|iterable` reads `(I1&I2)\|Traversable\|array`). */`` |
|      103 | 1055 | `		for( i = 0; i < nAtoms; i++ ){` |
|       75 | 1056 | `			if( GenAtomMaskKind(&aAtoms[i]) == GEN_ATOM_ITERABLE ){` |
|        3 | 1057 | `				SyBlobAppend(pBlob, "\|", 1);` |
|        3 | 1058 | `				SyBlobAppend(pBlob, "array", sizeof("array")-1);` |
|        3 | 1059 | `				break;` |
|        - | 1060 | `			}` |
|       39 | 1061 | `		}` |
|       35 | 1062 | `		if( bNullable ){` |
|      ! 0 | 1063 | `			SyBlobAppend(pBlob, "\|", 1);` |
|      ! 0 | 1064 | `			SyBlobAppend(pBlob, "null", 4);` |
|      ! 0 | 1065 | `		}` |
|      135 | 1066 | `		return;` |
|        - | 1067 | `	}` |
|    44995 | 1068 | `	if( nNonNull == 1 && bNullable ){` |
|        - | 1069 | `		/* Shorthand: ?T */` |
|      205 | 1070 | `		for( i = 0; i < nAtoms; i++ ){` |
|      205 | 1071 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      205 | 1072 | `			SyBlobAppend(pBlob, "?", 1);` |
|      205 | 1073 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|       73 | 1074 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       39 | 1075 | `			}else{` |
|      137 | 1076 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - | 1077 | `			}` |
|      205 | 1078 | `			return;` |
|      ! 0 | 1079 | `		}` |
|      ! 0 | 1080 | `	}` |
|        - | 1081 | `	{` |
|    44795 | 1082 | `		int bFirst = 1;` |
|        - | 1083 | `		/* 1) Classes in declaration order — minus the class-name atoms php counts` |
|        - | 1084 | ``		 * as built-in types (see GenAtomMaskKind); `iterable` leaves Traversable. */`` |
|    89809 | 1085 | `		for( i = 0; i < nAtoms; i++ ){` |
|        - | 1086 | `			int nKind;` |
|    45019 | 1087 | `			if( aAtoms[i].nType != SXU32_HIGH ) continue;` |
|      697 | 1088 | `			nKind = GenAtomMaskKind(&aAtoms[i]);` |
|      697 | 1089 | `			if( nKind == GEN_ATOM_CALLABLE \|\| nKind == GEN_ATOM_FALSE \|\| nKind == GEN_ATOM_TRUE ){` |
|      187 | 1090 | `				continue;` |
|        - | 1091 | `			}` |
|      515 | 1092 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|      515 | 1093 | `			GenAppendClassAtom(pBlob, &aAtoms[i], nNonNull > 1);` |
|      515 | 1094 | `			bFirst = 0;` |
|      260 | 1095 | `		}` |
|        - | 1096 | `		/* 2) Built-ins in php's canonical order. A slot is filled either by a` |
|        - | 1097 | `		 * plain atom of that MEMOBJ_* type or by a class-name atom of the matching` |
|        - | 1098 | ``		 * kind; the `array` slot also takes `iterable`'s second half. */`` |
|        - | 1099 | `		{` |
|        - | 1100 | `			static const struct {` |
|        - | 1101 | `				sxu32 nType;        /* plain atom type, 0 when kind-only */` |
|        - | 1102 | `				int nKind;          /* GEN_ATOM_* atom, GEN_ATOM_PLAIN when type-only */` |
|        - | 1103 | `				const char *zText;` |
|        - | 1104 | `				sxu32 nText;` |
|        - | 1105 | `			} aOrder[] = {` |
|        - | 1106 | `				{ 0,               GEN_ATOM_CALLABLE, "callable", sizeof("callable")-1 },` |
|        - | 1107 | `				{ MEMOBJ_OBJ,      GEN_ATOM_PLAIN,    "object",   sizeof("object")-1 },` |
|        - | 1108 | `				{ MEMOBJ_HASHMAP,  GEN_ATOM_ITERABLE, "array",    sizeof("array")-1 },` |
|        - | 1109 | `				{ MEMOBJ_STRING,   GEN_ATOM_PLAIN,    "string",   sizeof("string")-1 },` |
|        - | 1110 | `				{ MEMOBJ_INT,      GEN_ATOM_PLAIN,    "int",      sizeof("int")-1 },` |
|        - | 1111 | `				{ MEMOBJ_REAL,     GEN_ATOM_PLAIN,    "float",    sizeof("float")-1 },` |
|        - | 1112 | `				{ MEMOBJ_BOOL,     GEN_ATOM_PLAIN,    "bool",     sizeof("bool")-1 },` |
|        - | 1113 | `				{ 0,               GEN_ATOM_FALSE,    "false",    sizeof("false")-1 },` |
|        - | 1114 | `				{ 0,               GEN_ATOM_TRUE,     "true",     sizeof("true")-1 }` |
|        - | 1115 | `			};` |
|        - | 1116 | `			int k;` |
|   447905 | 1117 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|   763867 | 1118 | `				for( i = 0; i < nAtoms; i++ ){` |
|   517889 | 1119 | `					int bHit = (aOrder[k].nType != 0 && aAtoms[i].nType == aOrder[k].nType)` |
|   719715 | 1120 | `						\|\| (aOrder[k].nKind != GEN_ATOM_PLAIN` |
|   270393 | 1121 | `						    && GenAtomMaskKind(&aAtoms[i]) == aOrder[k].nKind` |
|    90028 | 1122 | `						    && (aOrder[k].nKind != GEN_ATOM_ITERABLE \|\| nNonNull > 1));` |
|   404927 | 1123 | `					if( !bHit ) continue;` |
|    44175 | 1124 | `					if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    44175 | 1125 | `					SyBlobAppend(pBlob, aOrder[k].zText, aOrder[k].nText);` |
|    44175 | 1126 | `					bFirst = 0;` |
|    44175 | 1127 | `					break;` |
|      ! 0 | 1128 | `				}` |
|   201560 | 1129 | `			}` |
|        - | 1130 | `		}` |
|        - | 1131 | `		/* 3) null suffix */` |
|    44795 | 1132 | `		if( bNullable ){` |
|       22 | 1133 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|       22 | 1134 | `			SyBlobAppend(pBlob, "null", 4);` |
|        9 | 1135 | `		}` |
|        - | 1136 | `	}` |
|    22515 | 1137 | `}` |
|        - | 1138 |  |
|        - | 1139 | `/*` |
|        - | 1140 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|        - | 1141 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|        - | 1142 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|        - | 1143 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|        - | 1144 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|        - | 1145 | ` * whether it was parenthesized.` |
|        - | 1146 | ` *` |
|        - | 1147 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|        - | 1148 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|        - | 1149 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|        - | 1150 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|        - | 1151 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|        - | 1152 | ` */` |
|    45292 | 1153 | `static sxi32 GenStateParsePart(` |
|        - | 1154 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|        - | 1155 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|        5 | 1156 | `{` |
|        - | 1157 | `	sxi32 rc;` |
|    45297 | 1158 | `	int nMembers = 0;` |
|    45297 | 1159 | `	int bParen = 0;` |
|    45297 | 1160 | `	*pnMembers = 0;` |
|    45297 | 1161 | `	*pbParen = 0;` |
|    45297 | 1162 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       14 | 1163 | `		bParen = 1;` |
|       14 | 1164 | `		pGen->pIn++; /* skip '(' */` |
|        5 | 1165 | `	}` |
|    22646 | 1166 | `	for(;;){` |
|    45329 | 1167 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|      ! 0 | 1168 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1169 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|      ! 0 | 1170 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1171 | `		}` |
|    45329 | 1172 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    45329 | 1173 | `		if( rc != SXRET_OK ){` |
|        3 | 1174 | `			return rc;` |
|        - | 1175 | `		}` |
|    45327 | 1176 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    45327 | 1177 | `		(*pnAtoms)++;` |
|    45327 | 1178 | `		nMembers++;` |
|        - | 1179 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    45327 | 1180 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       65 | 1181 | `			SyToken *pNext = &pGen->pIn[1];` |
|       60 | 1182 | `			if( pNext < pGen->pEnd` |
|       65 | 1183 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       37 | 1184 | `				pGen->pIn++; /* skip '&' */` |
|       37 | 1185 | `				continue;` |
|        - | 1186 | `			}` |
|       14 | 1187 | `		}` |
|    45295 | 1188 | `		break;` |
|      ! 0 | 1189 | `	}` |
|    45295 | 1190 | `	if( bParen ){` |
|       14 | 1191 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 1192 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1193 | `				"Malformed DNF type: expecting ')'");` |
|      ! 0 | 1194 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1195 | `		}` |
|       14 | 1196 | `		pGen->pIn++; /* skip ')' */` |
|       14 | 1197 | `		if( nMembers < 2 ){` |
|      ! 0 | 1198 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1199 | `				"Parenthesized type must be an intersection of at least two types");` |
|      ! 0 | 1200 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1201 | `		}` |
|        5 | 1202 | `	}` |
|    45295 | 1203 | `	*pnMembers = nMembers;` |
|    45295 | 1204 | `	*pbParen = bParen;` |
|    45295 | 1205 | `	return SXRET_OK;` |
|    22651 | 1206 | `}` |
|        - | 1207 |  |
|        - | 1208 | `/*` |
|        - | 1209 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|        - | 1210 | ` *` |
|        - | 1211 | ` * Outputs:` |
|        - | 1212 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|        - | 1213 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|        - | 1214 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|        - | 1215 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|        - | 1216 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|        - | 1217 | ` *     already be initialized by the caller (allocator set, etc).` |
|        - | 1218 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|        - | 1219 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|        - | 1220 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|        - | 1221 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|        - | 1222 | ` *` |
|        - | 1223 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|        - | 1224 | ` * SXERR_ABORT on fatal compile errors.` |
|        - | 1225 | ` */` |
|    45036 | 1226 | `PH7_PRIVATE sxi32 GenStateParseUnionTypeDecl(` |
|        - | 1227 | `	ph7_gen_state *pGen,` |
|        - | 1228 | `	sxu32 *pnType,` |
|        - | 1229 | `	SyString *pClass,` |
|        - | 1230 | `	SySet *pAlts,` |
|        - | 1231 | `	sxi32 *piTypeFlags,` |
|        - | 1232 | `	SyString *pTypeText,` |
|        - | 1233 | `	int iNullableFlag,` |
|        - | 1234 | `	int iUnionFlag,` |
|        - | 1235 | `	int bAllowVoid,` |
|        - | 1236 | `	sxu32 nLine` |
|        5 | 1237 | `){` |
|        - | 1238 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    45041 | 1239 | `	int nAtoms = 0;` |
|    45041 | 1240 | `	int bShortNullable = 0;` |
|    45041 | 1241 | `	int bExplicitNull = 0;` |
|        - | 1242 | `	sxi32 rc;` |
|    45041 | 1243 | `	*pnType = 0;` |
|    45041 | 1244 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    45041 | 1245 | `	*piTypeFlags = 0;` |
|    45041 | 1246 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|        - | 1247 |  |
|    45041 | 1248 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 1249 | `		return SXRET_OK;` |
|        - | 1250 | `	}` |
|        - | 1251 | ``	/* Optional `?` shorthand prefix */`` |
|    45036 | 1252 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      193 | 1253 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      193 | 1254 | `		bShortNullable = 1;` |
|      193 | 1255 | `		pGen->pIn++;` |
|      193 | 1256 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 1257 | `			return SXERR_SYNTAX;` |
|        - | 1258 | `		}` |
|       94 | 1259 | `	}` |
|        - | 1260 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|        - | 1261 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|        - | 1262 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|        - | 1263 | `	{` |
|        - | 1264 | `		int nMembers, bParen;` |
|    45041 | 1265 | `		sxu32 iGroup = 0;` |
|    45041 | 1266 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    45041 | 1267 | `		if( rc != SXRET_OK ){` |
|        4 | 1268 | `			return rc;` |
|        - | 1269 | `		}` |
|        - | 1270 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|        - | 1271 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|        - | 1272 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|        - | 1273 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|        - | 1274 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|    67950 | 1275 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    45438 | 1276 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      263 | 1277 | `			if( bShortNullable ){` |
|        - | 1278 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|        - | 1279 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|        - | 1280 | `				 * already reported" so callers skip their own error emission. */` |
|        3 | 1281 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - | 1282 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|        3 | 1283 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|        - | 1284 | `			}` |
|      261 | 1285 | `			if( nMembers >= 2 && !bParen ){` |
|      ! 0 | 1286 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|        - | 1287 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 | 1288 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1289 | `			}` |
|      261 | 1290 | ``			pGen->pIn++; /* skip `\|` */`` |
|      261 | 1291 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|      261 | 1292 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1293 | `				return rc;` |
|        - | 1294 | `			}` |
|        5 | 1295 | `		}` |
|    45037 | 1296 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|      ! 0 | 1297 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1298 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 | 1299 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1300 | `		}` |
|        - | 1301 | `	}` |
|        - | 1302 | `	/* Validation pass.` |
|        - | 1303 | `	 *` |
|        - | 1304 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|        - | 1305 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|        - | 1306 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|        - | 1307 | `	 */` |
|        - | 1308 | `	{` |
|        - | 1309 | `		int i, j;` |
|    45037 | 1310 | `		int bHasNonNull = 0;` |
|    45037 | 1311 | `		int bAnyIntersection = 0;` |
|        - | 1312 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|        - | 1313 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|        - | 1314 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|  1486061 | 1315 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    90357 | 1316 | `		for( i = 0; i < nAtoms; i++ ){` |
|    45325 | 1317 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|    22665 | 1318 | `		}` |
|    90285 | 1319 | `		for( i = 0; i < nAtoms; i++ ){` |
|    45285 | 1320 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|    22629 | 1321 | `		}` |
|        - | 1322 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|        - | 1323 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    45037 | 1324 | `		if( bShortNullable && bAnyIntersection ){` |
|      ! 0 | 1325 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1326 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|      ! 0 | 1327 | `			return SXERR_SYNTAX;` |
|        - | 1328 | `		}` |
|    90343 | 1329 | `		for( i = 0; i < nAtoms; i++ ){` |
|        - | 1330 | `			/* Intersection members must be class/interface types (PHP rejects` |
|        - | 1331 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|        - | 1332 | ``			 * `true`/`false` in an intersection). */`` |
|    45323 | 1333 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       67 | 1334 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|       67 | 1335 | `				if( bClassLike ){` |
|       65 | 1336 | `					SyString *pC = &aAtoms[i].sClass;` |
|       60 | 1337 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|       60 | 1338 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|       60 | 1339 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|       65 | 1340 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|      ! 0 | 1341 | `						bClassLike = 0;` |
|      ! 0 | 1342 | `					}` |
|       30 | 1343 | `				}` |
|       67 | 1344 | `				if( !bClassLike ){` |
|        - | 1345 | `					const char *zName; sxu32 nName;` |
|        3 | 1346 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 | 1347 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|      ! 0 | 1348 | `					}else{` |
|        3 | 1349 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|        - | 1350 | `					}` |
|        4 | 1351 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1352 | `						"Type %.*s cannot be part of an intersection type",` |
|        1 | 1353 | `						(int)nName, zName);` |
|        3 | 1354 | `					return SXERR_SYNTAX;` |
|        - | 1355 | `				}` |
|       30 | 1356 | `			}` |
|    45321 | 1357 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|      311 | 1358 | `				if( nAtoms > 1 ){` |
|        3 | 1359 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1360 | `						"Void can only be used as a standalone type");` |
|        3 | 1361 | `					return SXERR_SYNTAX;` |
|        - | 1362 | `				}` |
|      309 | 1363 | `				if( !bAllowVoid ){` |
|      ! 0 | 1364 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1365 | `						"void cannot be used here");` |
|      ! 0 | 1366 | `					return SXERR_SYNTAX;` |
|        - | 1367 | `				}` |
|      309 | 1368 | `				if( bShortNullable ){` |
|      ! 0 | 1369 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1370 | `						"Void type cannot be nullable");` |
|      ! 0 | 1371 | `					return SXERR_SYNTAX;` |
|        - | 1372 | `				}` |
|      152 | 1373 | `			}` |
|    45319 | 1374 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|        - | 1375 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|        - | 1376 | `				 * type (never = the function does not return). Mirrors the void` |
|        - | 1377 | `				 * validation above; accepted here and enforced at compile time` |
|        - | 1378 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|       33 | 1379 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|        - | 1380 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|        - | 1381 | `					 * same as any other non-standalone use. */` |
|        6 | 1382 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1383 | `						"never can only be used as a standalone type");` |
|        6 | 1384 | `					return SXERR_SYNTAX;` |
|        - | 1385 | `				}` |
|       28 | 1386 | `				if( !bAllowVoid ){` |
|        - | 1387 | `					/* Return-only: params call with bAllowVoid=0. */` |
|        3 | 1388 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1389 | `						"never cannot be used as a parameter type");` |
|        3 | 1390 | `					return SXERR_SYNTAX;` |
|        - | 1391 | `				}` |
|       11 | 1392 | `			}` |
|    45313 | 1393 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|       36 | 1394 | `				bExplicitNull = 1;` |
|       20 | 1395 | `			}else{` |
|    45281 | 1396 | `				bHasNonNull = 1;` |
|        - | 1397 | `			}` |
|        - | 1398 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|        - | 1399 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|        - | 1400 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|        - | 1401 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|        - | 1402 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    45657 | 1403 | `			for( j = 0; j < i; j++ ){` |
|      351 | 1404 | `				int bDup = 0;` |
|      351 | 1405 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|      680 | 1406 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|      346 | 1407 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|      351 | 1408 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|      331 | 1409 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|       89 | 1410 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       82 | 1411 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|       67 | 1412 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       21 | 1413 | `								aAtoms[j].sClass.zString,` |
|       42 | 1414 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|      ! 0 | 1415 | `							bDup = 1;` |
|      ! 0 | 1416 | `						}` |
|       46 | 1417 | `					}else{` |
|        3 | 1418 | `						bDup = 1;` |
|        - | 1419 | `					}` |
|       42 | 1420 | `				}` |
|      331 | 1421 | `				if( bDup ){` |
|        - | 1422 | `					const char *zName;` |
|        - | 1423 | `					sxu32 nName;` |
|        3 | 1424 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 | 1425 | `						zName = aAtoms[i].sClass.zString;` |
|      ! 0 | 1426 | `						nName = aAtoms[i].sClass.nByte;` |
|      ! 0 | 1427 | `					}else{` |
|        3 | 1428 | `						zName = aAtoms[i].zCanon;` |
|        3 | 1429 | `						nName = aAtoms[i].nCanon;` |
|        - | 1430 | `					}` |
|        4 | 1431 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        1 | 1432 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|        3 | 1433 | `					return SXERR_SYNTAX;` |
|        - | 1434 | `				}` |
|      167 | 1435 | `			}` |
|    22658 | 1436 | `		}` |
|    45025 | 1437 | `		if( !bHasNonNull && bExplicitNull ){` |
|        7 | 1438 | `			if( bShortNullable ){` |
|        - | 1439 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|      ! 0 | 1440 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1441 | `					"Null can not be used as a standalone type");` |
|      ! 0 | 1442 | `				return SXERR_SYNTAX;` |
|        - | 1443 | `			}` |
|        - | 1444 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|        - | 1445 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|        - | 1446 | `			 * path below leaves *pnType untouched when there is no non-null` |
|        - | 1447 | `			 * atom, so set it here. */` |
|        7 | 1448 | `			*pnType = MEMOBJ_NULL;` |
|        3 | 1449 | `		}` |
|        - | 1450 | `	}` |
|        - | 1451 | `	/* Compute nullability flag */` |
|    45025 | 1452 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      223 | 1453 | `		*piTypeFlags \|= iNullableFlag;` |
|      109 | 1454 | `	}` |
|        - | 1455 | `	/* Build canonical type text */` |
|    45025 | 1456 | `	if( pTypeText ){` |
|        - | 1457 | `		SyBlob sBlob;` |
|    45025 | 1458 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|    67442 | 1459 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|    22510 | 1460 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    45025 | 1461 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|    67046 | 1462 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    44694 | 1463 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    44699 | 1464 | `			if( zDup ){` |
|    44699 | 1465 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|    22347 | 1466 | `			}` |
|    22347 | 1467 | `		}` |
|    45025 | 1468 | `		SyBlobRelease(&sBlob);` |
|    22510 | 1469 | `	}` |
|        - | 1470 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|        - | 1471 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|        - | 1472 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|        - | 1473 | `	{` |
|    45025 | 1474 | `		int nNonNull = 0;` |
|    45025 | 1475 | `		int iNonNullIdx = -1;` |
|        - | 1476 | `		int i;` |
|    90323 | 1477 | `		for( i = 0; i < nAtoms; i++ ){` |
|    45303 | 1478 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    45271 | 1479 | `				nNonNull++;` |
|    45271 | 1480 | `				iNonNullIdx = i;` |
|    22633 | 1481 | `			}` |
|    22654 | 1482 | `		}` |
|    45025 | 1483 | `		if( nNonNull <= 1 ){` |
|        - | 1484 | `			/* Fast path: store as single type. */` |
|    44811 | 1485 | `			if( iNonNullIdx >= 0 ){` |
|    44805 | 1486 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    44805 | 1487 | `				if( pA->nType == SXU32_HIGH ){` |
|      923 | 1488 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      306 | 1489 | `						pA->sClass.zString, pA->sClass.nByte);` |
|      617 | 1490 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      617 | 1491 | `					*pnType = SXU32_HIGH;` |
|      617 | 1492 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    44499 | 1493 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|      309 | 1494 | `					*pnType = MEMOBJ_VOID;` |
|    44041 | 1495 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|       25 | 1496 | `					*pnType = MEMOBJ_NEVER;` |
|       14 | 1497 | `				}else{` |
|    43867 | 1498 | `					*pnType = pA->nType;` |
|        - | 1499 | `				}` |
|    22400 | 1500 | `			}` |
|    22408 | 1501 | `		}else{` |
|        - | 1502 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      219 | 1503 | `			*piTypeFlags \|= iUnionFlag;` |
|      697 | 1504 | `			for( i = 0; i < nAtoms; i++ ){` |
|        - | 1505 | `				ph7_type_alt sAlt;` |
|      483 | 1506 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      471 | 1507 | `				SyZero(&sAlt, sizeof(sAlt));` |
|      471 | 1508 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|      471 | 1509 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      326 | 1510 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      107 | 1511 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      219 | 1512 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      219 | 1513 | `					sAlt.nType = SXU32_HIGH;` |
|      219 | 1514 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      112 | 1515 | `				}else{` |
|      257 | 1516 | `					sAlt.nType = aAtoms[i].nType;` |
|      257 | 1517 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|        - | 1518 | `				}` |
|      471 | 1519 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      238 | 1520 | `			}` |
|        - | 1521 | `		}` |
|        - | 1522 | `	}` |
|    45025 | 1523 | `	return SXRET_OK;` |
|    22523 | 1524 | `}` |
|        - | 1525 |  |
|        - | 1526 | `/*` |
|        - | 1527 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|        - | 1528 | `` * pGen->pIn should point to the token after `)`.`` |
|        - | 1529 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|        - | 1530 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|        - | 1531 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|        - | 1532 | `` *          and union types `: T\|U`.`` |
|        - | 1533 | ` */` |
|   151840 | 1534 | `PH7_PRIVATE sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|        5 | 1535 | `{` |
|   151845 | 1536 | `	sxi32 iFlags = 0;` |
|        - | 1537 | `	sxi32 rc;` |
|        - | 1538 | `	sxu32 nLine;` |
|   151845 | 1539 | `	pFunc->nReturnType = 0;` |
|   151845 | 1540 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|   151845 | 1541 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|        - | 1542 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|        - | 1543 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|        - | 1544 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|        - | 1545 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|        - | 1546 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|   151845 | 1547 | `	SySetReset(&pFunc->aReturnUnion);` |
|   151845 | 1548 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|   151845 | 1549 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|   145877 | 1550 | `		return SXRET_OK;` |
|        - | 1551 | `	}` |
|     5973 | 1552 | `	pGen->pIn++; /* Skip ':' */` |
|     5973 | 1553 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 1554 | `		return SXRET_OK;` |
|        - | 1555 | `	}` |
|     5973 | 1556 | `	nLine = pGen->pIn->nLine;` |
|     5973 | 1557 | `	rc = GenStateParseUnionTypeDecl(` |
|     2984 | 1558 | `		pGen,` |
|     2984 | 1559 | `		&pFunc->nReturnType,` |
|     2984 | 1560 | `		&pFunc->sReturnClass,` |
|     2984 | 1561 | `		&pFunc->aReturnUnion,` |
|        - | 1562 | `		&iFlags,` |
|     2984 | 1563 | `		&pFunc->sReturnTypeName,` |
|        - | 1564 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|        - | 1565 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|        - | 1566 | `		/* iUnionFlag */ 0,` |
|        - | 1567 | `		/* bAllowVoid */ 1,` |
|     2984 | 1568 | `		nLine);` |
|     5973 | 1569 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1570 | `		return SXERR_ABORT;` |
|        - | 1571 | `	}` |
|     5973 | 1572 | `	if( rc == SXERR_CORRUPT ){` |
|        - | 1573 | `		/* Error already reported */` |
|      ! 0 | 1574 | `		return SXERR_SYNTAX;` |
|        - | 1575 | `	}` |
|     5973 | 1576 | `	if( rc == SXERR_SYNTAX ){` |
|        8 | 1577 | `		if( pGen->pIn < pGen->pEnd ){` |
|       11 | 1578 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - | 1579 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|        6 | 1580 | `				&pGen->pIn->sData);` |
|        5 | 1581 | `		}else{` |
|      ! 0 | 1582 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|        - | 1583 | `				"syntax error, unexpected end of file in return type declaration");` |
|        - | 1584 | `		}` |
|        8 | 1585 | `		return SXERR_SYNTAX;` |
|        - | 1586 | `	}` |
|     5967 | 1587 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     5967 | 1588 | `	return SXRET_OK;` |
|    75925 | 1589 | `}` |
|        - | 1590 |  |
|   145092 | 1591 | `PH7_PRIVATE sxi32 GenStateCompileFunc(` |
|        - | 1592 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 1593 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|        - | 1594 | `	sxi32 iFlags,        /* Control flags */` |
|        - | 1595 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|        - | 1596 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|        - | 1597 | `	)` |
|        5 | 1598 | `{` |
|        - | 1599 | `	ph7_vm_func *pFunc;` |
|        - | 1600 | `	SyToken *pEnd;` |
|        - | 1601 | `	sxu32 nLine;` |
|        - | 1602 | `	char *zName;` |
|        - | 1603 | `	sxi32 rc;` |
|        - | 1604 | `	/* Extract line number */` |
|   145097 | 1605 | `	nLine = pGen->pIn->nLine;` |
|        - | 1606 | `	/* Jump the left parenthesis '(' */` |
|   145097 | 1607 | `	pGen->pIn++;` |
|        - | 1608 | `	/* Delimit the function signature */` |
|   145097 | 1609 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   145097 | 1610 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 1611 | `		/* Syntax error */` |
|       12 | 1612 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable");` |
|        4 | 1613 | `		(void)pName;` |
|       12 | 1614 | `		if( rc == SXERR_ABORT ){` |
|        - | 1615 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1616 | `			return SXERR_ABORT;` |
|        - | 1617 | `		}` |
|       12 | 1618 | `		pGen->pIn = pGen->pEnd;` |
|       12 | 1619 | `		return SXRET_OK;` |
|        - | 1620 | `	}` |
|        - | 1621 | `	/* Create the function state */` |
|   145089 | 1622 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   145089 | 1623 | `	if( pFunc == 0 ){` |
|      ! 0 | 1624 | `		goto OutOfMem;` |
|        - | 1625 | `	}` |
|        - | 1626 | `	/* Build the function name, prepending namespace if active.` |
|        - | 1627 | `	 * A NAMED function (never a closure) also answers to php's import rules: its` |
|        - | 1628 | ``	 * short name must not already be a local `use function` import, and the name it`` |
|        - | 1629 | ``	 * takes is remembered so a later `use function` in this unit sees it. */`` |
|   145113 | 1630 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|        - | 1631 | `		SyBlob sFQN;` |
|        - | 1632 | `		sxu32 nLen;` |
|       53 | 1633 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       53 | 1634 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       53 | 1635 | `		SyBlobAppend(&sFQN,"\\",1);` |
|       53 | 1636 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|       53 | 1637 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|       53 | 1638 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|       53 | 1639 | `		SyBlobRelease(&sFQN);` |
|       53 | 1640 | `		if( zName == 0 ){` |
|      ! 0 | 1641 | `			goto OutOfMem;` |
|        - | 1642 | `		}` |
|       53 | 1643 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       29 | 1644 | `	}else{` |
|   145041 | 1645 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   145041 | 1646 | `		if( zName == 0 ){` |
|      ! 0 | 1647 | `			goto OutOfMem;` |
|        - | 1648 | `		}` |
|   145041 | 1649 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|        - | 1650 | `	}` |
|   145089 | 1651 | `	if( !bHandleClosure ){` |
|   142487 | 1652 | `		if( GenStateGuardImportRedeclare(pGen,1,pName,&pFunc->sName,nLine) == SXERR_ABORT ){` |
|      ! 0 | 1653 | `			return SXERR_ABORT;` |
|        - | 1654 | `		}` |
|   142487 | 1655 | `		GenStateRecordDeclaredName(pGen,1,&pFunc->sName);` |
|    71241 | 1656 | `	}` |
|        - | 1657 | ``	/* Take php's `{closure:SCOPE:LINE}` name the caller built for this closure. It has to`` |
|        - | 1658 | `	 * land here, ahead of the body, because a __FUNCTION__ inside the body reads it at` |
|        - | 1659 | `	 * compile time — and it must be cleared, since a NESTED declaration reaches this same` |
|        - | 1660 | `	 * point and would otherwise inherit its parent's. */` |
|   145089 | 1661 | `	if( SyStringLength(&pGen->sPendingClosureName) > 0 ){` |
|     2607 | 1662 | `		if( bHandleClosure ){` |
|     2607 | 1663 | `			pFunc->sClosureName = pGen->sPendingClosureName;` |
|     1301 | 1664 | `		}` |
|     2607 | 1665 | `		SyStringInitFromBuf(&pGen->sPendingClosureName,0,0);` |
|     1301 | 1666 | `	}` |
|        - | 1667 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|        - | 1668 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|   145089 | 1669 | `	pFunc->nLine = nLine;` |
|   145089 | 1670 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|   145089 | 1671 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 1672 | `		return SXERR_ABORT;` |
|        - | 1673 | `	}` |
|   145089 | 1674 | `	if( pGen->pIn < pEnd ){` |
|        - | 1675 | `		/* Collect function arguments */` |
|   137363 | 1676 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   137363 | 1677 | `		if( rc == SXERR_ABORT ){` |
|        - | 1678 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|        3 | 1679 | `			return SXERR_ABORT;` |
|        - | 1680 | `		}` |
|    68678 | 1681 | `	}` |
|        - | 1682 | `	/* Point past ')' and parse optional return type ': type' */` |
|   145087 | 1683 | `	pGen->pIn = &pEnd[1];` |
|        - | 1684 | `	{` |
|   145087 | 1685 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   145087 | 1686 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 | 1687 | `			return SXERR_ABORT;` |
|   145087 | 1688 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|        8 | 1689 | `			return SXERR_SYNTAX;` |
|        - | 1690 | `		}` |
|        - | 1691 | `	}` |
|   145081 | 1692 | `	if( bHandleClosure ){` |
|        - | 1693 | `		ph7_vm_func_closure_env sEnv;` |
|     2607 | 1694 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     2602 | 1695 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     1495 | 1696 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      383 | 1697 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - | 1698 | `				/* Closure,record environment variable */` |
|      383 | 1699 | `				pGen->pIn++;` |
|      383 | 1700 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 1701 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|      ! 0 | 1702 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1703 | `						return SXERR_ABORT;` |
|        - | 1704 | `					}` |
|      ! 0 | 1705 | `				}` |
|      383 | 1706 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|        - | 1707 | `				/* Compile until we hit the first closing parenthesis */` |
|      805 | 1708 | `				while( pGen->pIn < pGen->pEnd ){` |
|      805 | 1709 | `					int iFlagsLocal = 0;` |
|      805 | 1710 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      381 | 1711 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      381 | 1712 | `						break;` |
|        - | 1713 | `					}` |
|      429 | 1714 | `					nLineLocal = pGen->pIn->nLine;` |
|      429 | 1715 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - | 1716 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|        - | 1717 | `						 * to the variable's memory slot instead of copying its value. */` |
|      128 | 1718 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|      128 | 1719 | `						pGen->pIn++;` |
|       62 | 1720 | `					}` |
|      424 | 1721 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      429 | 1722 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 1723 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - | 1724 | `								"Closure: Unexpected token. Expecting a variable name");` |
|      ! 0 | 1725 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 1726 | `								return SXERR_ABORT;` |
|        - | 1727 | `							}` |
|        - | 1728 | `							/* Find the closing parenthesis */` |
|      ! 0 | 1729 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 1730 | `								pGen->pIn++;` |
|      ! 0 | 1731 | `							}` |
|      ! 0 | 1732 | `							if(pGen->pIn < pGen->pEnd){` |
|      ! 0 | 1733 | `								pGen->pIn++;` |
|      ! 0 | 1734 | `							}` |
|      ! 0 | 1735 | `							break;` |
|        - | 1736 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|      ! 0 | 1737 | `					}else{` |
|        - | 1738 | `						SyString *pNameLocal;` |
|        - | 1739 | `						char *zDup;` |
|        - | 1740 | `						/* Duplicate variable name */` |
|      429 | 1741 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      429 | 1742 | `						if( PH7_VmIsAutoGlobal(pNameLocal->zString,pNameLocal->nByte) ){` |
|        - | 1743 | `							/* php's compile fatal. It is a real protection, not a` |
|        - | 1744 | `							 * style rule: the import resolves through hSuper, so` |
|        - | 1745 | `							 * installing the captured value would overwrite the` |
|        - | 1746 | ``							 * superglobal's own slot — `use ($GLOBALS)` replaced the`` |
|        - | 1747 | `							 * live symbol-table view with a snapshot and every later` |
|        - | 1748 | `							 * global went missing program-wide. */` |
|        3 | 1749 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - | 1750 | `								"Cannot use auto-global as lexical variable");` |
|        3 | 1751 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 1752 | `								return SXERR_ABORT;` |
|        - | 1753 | `							}` |
|        3 | 1754 | `							return SXERR_SYNTAX;` |
|        - | 1755 | `						}` |
|      427 | 1756 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      427 | 1757 | `						if( zDup ){` |
|        - | 1758 | `							/* Zero the structure */` |
|      427 | 1759 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      427 | 1760 | `							sEnv.iFlags = iFlagsLocal;` |
|      427 | 1761 | `							sEnv.nLine = nLineLocal; /* the capture's own source line (php warns here) */` |
|      427 | 1762 | `							sEnv.nIdx = SXU32_HIGH;` |
|      427 | 1763 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      427 | 1764 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      476 | 1765 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|       98 | 1766 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|      ! 0 | 1767 | `									got_this = 1;` |
|      ! 0 | 1768 | `							}` |
|        - | 1769 | `							/* Save imported variable */` |
|      427 | 1770 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      216 | 1771 | `						}else{` |
|      ! 0 | 1772 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1773 | `							 return SXERR_ABORT;` |
|        - | 1774 | `						}` |
|        - | 1775 | `					}` |
|      427 | 1776 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      475 | 1777 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - | 1778 | `						/* Ignore trailing commas */` |
|       52 | 1779 | `						pGen->pIn++;` |
|        4 | 1780 | `					}` |
|        5 | 1781 | `				}` |
|        - | 1782 | `				/* php 7.1+: the return type follows the use clause —` |
|        - | 1783 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|        - | 1784 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|        - | 1785 | `				 * so an unconditional call would wipe a type parsed at the` |
|        - | 1786 | `				 * legacy pre-use position. */` |
|      381 | 1787 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|        7 | 1788 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|        7 | 1789 | `					if( rcRt2 == SXERR_ABORT ){` |
|      ! 0 | 1790 | `						return SXERR_ABORT;` |
|        7 | 1791 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|      ! 0 | 1792 | `						return SXERR_SYNTAX;` |
|        - | 1793 | `					}` |
|        3 | 1794 | `				}` |
|      188 | 1795 | `		}` |
|     2605 | 1796 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|        - | 1797 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|        - | 1798 | `			 * available to the closure environment — for EVERY non-static` |
|        - | 1799 | `			 * anonymous function, use list or not (php binds $this to any` |
|        - | 1800 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|        - | 1801 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|        - | 1802 | `			 * a global-scope closure is silently dropped at install. A static` |
|        - | 1803 | `			 * closure never binds $this (php). */` |
|     2559 | 1804 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     2559 | 1805 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|     2559 | 1806 | `			sEnv.nIdx = SXU32_HIGH;` |
|     2559 | 1807 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     2559 | 1808 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|     2559 | 1809 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|     1277 | 1810 | `		}` |
|     2605 | 1811 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|        - | 1812 | `			/* Mark as closure */` |
|     2563 | 1813 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|     1279 | 1814 | `		}` |
|     1300 | 1815 | `	}` |
|        - | 1816 | `	/* Compile the body */` |
|   145079 | 1817 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   145079 | 1818 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1819 | `		return SXERR_ABORT;` |
|        - | 1820 | `	}` |
|        - | 1821 | `	/* The cursor sits just past the body's closing brace */` |
|   145079 | 1822 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|   145079 | 1823 | `	if( ppFunc ){` |
|   145079 | 1824 | `		*ppFunc = pFunc;` |
|    72537 | 1825 | `	}` |
|   145079 | 1826 | `	rc = SXRET_OK;` |
|   145079 | 1827 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|        - | 1828 | `		/* Reject a php-fatal redeclaration before hoisting the function */` |
|   142521 | 1829 | `		if( GenStateGuardFuncRedeclaration(pGen,pFunc) == SXERR_ABORT ){` |
|       11 | 1830 | `			return SXERR_ABORT;` |
|        - | 1831 | `		}` |
|        - | 1832 | `		/* Finally register the function */` |
|   142513 | 1833 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    71254 | 1834 | `	}` |
|   145071 | 1835 | `	if( rc == SXRET_OK ){` |
|   145071 | 1836 | `		return SXRET_OK;` |
|        - | 1837 | `	}` |
|        - | 1838 | `	/* Fall through if something goes wrong */` |
|      ! 0 | 1839 | `OutOfMem:` |
|        - | 1840 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - | 1841 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|        - | 1842 | `	 */` |
|      ! 0 | 1843 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 | 1844 | `	return SXERR_ABORT;` |
|    72551 | 1845 | `}` |
|        - | 1846 | `/*` |
|        - | 1847 | ` * Compile a standard PHP function.` |
|        - | 1848 | ` *  Refer to the block-comment above for more information.` |
|        - | 1849 | ` */` |
|   142498 | 1850 | `PH7_PRIVATE sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|        5 | 1851 | `{` |
|        - | 1852 | `	SyString *pName;` |
|        - | 1853 | `	sxi32 iFlags;` |
|        - | 1854 | `	sxu32 nKwLine;` |
|        - | 1855 | `	sxu32 nLine;` |
|        - | 1856 | `	sxi32 rc;` |
|        - | 1857 |  |
|   142503 | 1858 | `	nLine = pGen->pIn->nLine;` |
|   142503 | 1859 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|   142503 | 1860 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   142503 | 1861 | `	iFlags = 0;` |
|   142503 | 1862 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - | 1863 | `		/* Return by reference,remember that */` |
|       19 | 1864 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|        - | 1865 | `		/* Jump the '&' token */` |
|       19 | 1866 | `		pGen->pIn++;` |
|        8 | 1867 | `	}` |
|   142503 | 1868 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 1869 | `		/* Invalid function name */` |
|        8 | 1870 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|        8 | 1871 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1872 | `			return SXERR_ABORT;` |
|        - | 1873 | `		}` |
|        - | 1874 | `		/* Sychronize with the next semi-colon or braces*/` |
|       22 | 1875 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       16 | 1876 | `			pGen->pIn++;` |
|        2 | 1877 | `		}` |
|        8 | 1878 | `		return SXRET_OK;` |
|        - | 1879 | `	}` |
|   142497 | 1880 | `	pName = &pGen->pIn->sData;` |
|   142497 | 1881 | `	nLine = pGen->pIn->nLine;` |
|        - | 1882 | `	/* Jump the function name */` |
|   142497 | 1883 | `	pGen->pIn++;` |
|   142497 | 1884 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1885 | `		/* Syntax error */` |
|        3 | 1886 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|        3 | 1887 | `		if( rc == SXERR_ABORT ){` |
|        - | 1888 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1889 | `			return SXERR_ABORT;` |
|        - | 1890 | `		}` |
|        - | 1891 | `		/* Sychronize with the next semi-colon or '{' */` |
|        3 | 1892 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 1893 | `			pGen->pIn++;` |
|      ! 0 | 1894 | `		}` |
|        3 | 1895 | `		return SXRET_OK;` |
|        - | 1896 | `	}` |
|        - | 1897 | `	/* Compile function body */` |
|        - | 1898 | `	{` |
|   142495 | 1899 | `		ph7_vm_func *pFuncState = 0;` |
|   142495 | 1900 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|   142495 | 1901 | `		if( pFuncState ){` |
|        - | 1902 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|   142479 | 1903 | `			pFuncState->nLine = nKwLine;` |
|    71237 | 1904 | `		}` |
|        - | 1905 | `	}` |
|   142495 | 1906 | `	return rc;` |
|    71254 | 1907 | `}` |
|        - | 1908 |  |
