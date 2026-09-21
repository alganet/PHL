# src/ph7/compile_func.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 873/1016 lines (85.93%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|         - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    5 | ` */` |
|         - |    6 | `#include "ph7int.h"` |
|         - |    7 | `#include "compile_int.h"` |
|         - |    8 | `/*` |
|         - |    9 | ` * Section:` |
|         - |   10 | ` *    Function compilation: argument collection and default values, the` |
|         - |   11 | ` *    union/return type-declaration parsers, function bodies and the` |
|         - |   12 | ` *    'function' statement itself.` |
|         - |   13 | ` * Status:` |
|         - |   14 | ` *    Stable.` |
|         - |   15 | ` */` |
|         - |   16 | `/*` |
|         - |   17 | ` * Process default argument values. That is,a function may define C++-style default value` |
|         - |   18 | ` * as follows:` |
|         - |   19 | ` * function makecoffee($type = "cappuccino")` |
|         - |   20 | ` * {` |
|         - |   21 | ` *   return "Making a cup of $type.\n";` |
|         - |   22 | ` * }` |
|         - |   23 | ` * Symisc eXtension.` |
|         - |   24 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|         - |   25 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|         - |   26 | ` *      Example: Work only with PH7,generate error under zend` |
|         - |   27 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|         - |   28 | ` *      {` |
|         - |   29 | ` *       var_dump($a);` |
|         - |   30 | ` *      }` |
|         - |   31 | ` *     //call test without args` |
|         - |   32 | ` *      test();` |
|         - |   33 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|         - |   34 | ` *      Example:` |
|         - |   35 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|         - |   36 | ` * 3 -) Function overloading!!` |
|         - |   37 | ` *      Example:` |
|         - |   38 | ` *      function foo($a) {` |
|         - |   39 | ` *   	  return $a.PHP_EOL;` |
|         - |   40 | ` *	    }` |
|         - |   41 | ` *	    function foo($a, $b) {` |
|         - |   42 | ` *   	  return $a + $b;` |
|         - |   43 | ` *	    }` |
|         - |   44 | ` *	    echo foo(5); // Prints "5"` |
|         - |   45 | ` *	    echo foo(5, 2); // Prints "7"` |
|         - |   46 | ` *      // Same arg` |
|         - |   47 | ` *	   function foo(string $a)` |
|         - |   48 | ` *	   {` |
|         - |   49 | ` *	     echo "a is a string\n";` |
|         - |   50 | ` *	     var_dump($a);` |
|         - |   51 | ` *	   }` |
|         - |   52 | ` *	  function foo(int $a)` |
|         - |   53 | ` *	  {` |
|         - |   54 | ` *	    echo "a is integer\n";` |
|         - |   55 | ` *	    var_dump($a);` |
|         - |   56 | ` *	  }` |
|         - |   57 | ` *	  function foo(array $a)` |
|         - |   58 | ` *	  {` |
|         - |   59 | ` * 	    echo "a is an array\n";` |
|         - |   60 | ` * 	    var_dump($a);` |
|         - |   61 | ` *	  }` |
|         - |   62 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|         - |   63 | ` *	  foo(52); // a is integer [second foo]` |
|         - |   64 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|         - |   65 | ` * Please refer to the official documentation for more information on the powerful extension` |
|         - |   66 | ` * introduced by the PH7 engine.` |
|         - |   67 | ` */` |
|    753686 |   68 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|         5 |   69 | `{` |
|         - |   70 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |   71 | `	SySet *pInstrContainer;` |
|         - |   72 | `	sxi32 rc;` |
|         - |   73 | `	/* Swap token stream */` |
|    753691 |   74 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|    753691 |   75 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    753691 |   76 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|         - |   77 | `	/* Compile the expression holding the argument value. A parameter default is a` |
|         - |   78 | `	 * const-expression belonging to the current class (see iInMemberDefault) — so` |
|         - |   79 | `	 * __TRAIT__ in it reads pCurClass rather than walking into the enclosing method. */` |
|    753691 |   80 | `	pGen->iInMemberDefault++;` |
|    753691 |   81 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    753691 |   82 | `	pGen->iInMemberDefault--;` |
|         - |   83 | `	/* Emit the done instruction */` |
|    753691 |   84 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    753691 |   85 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    753691 |   86 | `	RE_SWAP_DELIMITER(pGen);` |
|    753691 |   87 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |   88 | `		return SXERR_ABORT;` |
|         - |   89 | `	}` |
|    753691 |   90 | `	return SXRET_OK;` |
|    376848 |   91 | `}` |
|         - |   92 | `/*` |
|         - |   93 | ` * Collect function arguments one after one.` |
|         - |   94 | ` * According to the PHP language reference manual.` |
|         - |   95 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|         - |   96 | ` * list of expressions.` |
|         - |   97 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|         - |   98 | ` * and default argument values. Variable-length argument lists are also supported,` |
|         - |   99 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|         - |  100 | ` * for more information.` |
|         - |  101 | ` * Example #1 Passing arrays to functions` |
|         - |  102 | ` * <?php` |
|         - |  103 | ` * function takes_array($input)` |
|         - |  104 | ` * {` |
|         - |  105 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|         - |  106 | ` * }` |
|         - |  107 | ` * ?>` |
|         - |  108 | ` * Making arguments be passed by reference` |
|         - |  109 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|         - |  110 | ` * within the function is changed, it does not get changed outside of the function).` |
|         - |  111 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|         - |  112 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|         - |  113 | ` * to the argument name in the function definition:` |
|         - |  114 | ` * Example #2 Passing function parameters by reference` |
|         - |  115 | ` * <?php` |
|         - |  116 | ` * function add_some_extra(&$string)` |
|         - |  117 | ` * {` |
|         - |  118 | ` *   $string .= 'and something extra.';` |
|         - |  119 | ` * }` |
|         - |  120 | ` * $str = 'This is a string, ';` |
|         - |  121 | ` * add_some_extra($str);` |
|         - |  122 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|         - |  123 | ` * ?>` |
|         - |  124 | ` *` |
|         - |  125 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|         - |  126 | ` * complex agrument values.Please refer to the official documentation for more information` |
|         - |  127 | ` * on these extension.` |
|         - |  128 | ` */` |
|   1662340 |  129 | `PH7_PRIVATE sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|         5 |  130 | `{` |
|         - |  131 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|         - |  132 | `	SyToken *pIn;  /* Token stream */` |
|         - |  133 | `	SyBlob sSig;         /* Function signature */` |
|         - |  134 | `	char *zDup;          /* Copy of argument name */` |
|         - |  135 | `	sxi32 rc;` |
|         - |  136 |  |
|   1662345 |  137 | `	pIn = pGen->pIn;` |
|   1662345 |  138 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|         - |  139 | `	/* Process arguments one after one */` |
|   2140925 |  140 | `	for(;;){` |
|   4281855 |  141 | `		if( pIn >= pEnd ){` |
|         - |  142 | `			/* No more arguments to process */` |
|   1662327 |  143 | `			break;` |
|         - |  144 | `		}` |
|   2619533 |  145 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   2619533 |  146 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   2619533 |  147 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   2619533 |  148 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   2619533 |  149 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|         - |  150 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|         - |  151 | `		 * first token inside the main token stream */` |
|   2619533 |  152 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  153 | `			return SXERR_ABORT;` |
|         - |  154 | `		}` |
|         - |  155 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|         - |  156 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|         - |  157 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|         - |  158 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|         - |  159 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|         - |  160 | `		{` |
|   2619533 |  161 | `			int bReadonly = 0, bVisSeen = 0;` |
|   2619533 |  162 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   2619533 |  163 | `			sxi32 iSetVisFlag = 0;` |
|         - |  164 | `			int nSetTok;` |
|         - |  165 | `			sxi32 nSetVis;` |
|   2619533 |  166 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|         3 |  167 | `				bReadonly = 1;` |
|         3 |  168 | `				pIn++;` |
|         1 |  169 | `			}` |
|   2619533 |  170 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   2619533 |  171 | `			if( nSetVis ){` |
|         - |  172 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|         3 |  173 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  174 | `				bVisSeen = 1;` |
|         3 |  175 | `				pIn += nSetTok;` |
|         3 |  176 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       ! 0 |  177 | `					bReadonly = 1;` |
|       ! 0 |  178 | `					pIn++;` |
|         1 |  179 | `				}` |
|   2619532 |  180 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|    112341 |  181 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|    112341 |  182 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|        93 |  183 | `					bVisSeen = 1;` |
|        93 |  184 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|       124 |  185 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|        40 |  186 | `						: PH7_CLASS_PROT_PUBLIC;` |
|        93 |  187 | `					pIn++;` |
|        93 |  188 | `					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|        93 |  189 | `					if( nSetVis ){` |
|         - |  190 | ``						/* `public private(set) T $x` promoted form */`` |
|         3 |  191 | `						iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  192 | `						pIn += nSetTok;` |
|         1 |  193 | `					}` |
|        93 |  194 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        18 |  195 | `						bReadonly = 1;` |
|        18 |  196 | `						pIn++;` |
|         7 |  197 | `					}` |
|        44 |  198 | `				}` |
|     56168 |  199 | `			}` |
|   2619533 |  200 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|         5 |  201 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   2619531 |  202 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|       ! 0 |  203 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|       ! 0 |  204 | `			}` |
|   2619533 |  205 | `			if( bVisSeen \|\| bReadonly ){` |
|        97 |  206 | `				if( !bCtorCtx ){` |
|         6 |  207 | `					if( bAbstractCtx ){` |
|         3 |  208 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  209 | `							"Cannot declare promoted property in an abstract constructor");` |
|         2 |  210 | `					}else{` |
|         3 |  211 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  212 | `							"Cannot declare promoted property outside a constructor");` |
|         - |  213 | `					}` |
|         6 |  214 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  215 | `						return SXERR_ABORT;` |
|         - |  216 | `					}` |
|         6 |  217 | `					return SXERR_SYNTAX;` |
|         - |  218 | `				}` |
|        93 |  219 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|        93 |  220 | `				sArg.iPromoteVis = iVis;` |
|        93 |  221 | `				if( bReadonly ){` |
|        20 |  222 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|         8 |  223 | `				}` |
|        44 |  224 | `			}` |
|         - |  225 | `		}` |
|         - |  226 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   2619524 |  227 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   1409583 |  228 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|    187178 |  229 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    149781 |  230 | `			sxu32 nLineLocal = pIn->nLine;` |
|    149781 |  231 | `			sxi32 iTFlags = 0;` |
|    149781 |  232 | `			pGen->pIn = pIn;` |
|    149781 |  233 | `			rc = GenStateParseUnionTypeDecl(` |
|     74888 |  234 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|     74888 |  235 | `				&iTFlags, &sArg.sTypeName,` |
|         - |  236 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|         - |  237 | `				/* bAllowVoid */ 0,` |
|     74888 |  238 | `						nLineLocal);` |
|    149781 |  239 | `			pIn = pGen->pIn;` |
|    149781 |  240 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  241 | `				return SXERR_ABORT;` |
|    149781 |  242 | `			}else if( rc == SXERR_CORRUPT ){` |
|         - |  243 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|         3 |  244 | `				return SXERR_SYNTAX;` |
|    149779 |  245 | `			}else if( rc == SXERR_SYNTAX ){` |
|        11 |  246 | `				if( pIn < pEnd ){` |
|        15 |  247 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|         - |  248 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|         4 |  249 | `						&pIn->sData);` |
|         7 |  250 | `				}else{` |
|       ! 0 |  251 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|         - |  252 | `						"syntax error, unexpected end of file");` |
|         - |  253 | `				}` |
|        11 |  254 | `				return SXERR_SYNTAX;` |
|         - |  255 | `			}` |
|    149771 |  256 | `			sArg.iFlags \|= iTFlags;` |
|     74883 |  257 | `		}` |
|   2619519 |  258 | `		if( pIn >= pEnd ){` |
|       ! 0 |  259 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|       ! 0 |  260 | `			return rc;` |
|         - |  261 | `		}` |
|   2619519 |  262 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|         - |  263 | `			/* Pass by reference,record that */` |
|     24951 |  264 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     24951 |  265 | `			pIn++;` |
|     12473 |  266 | `		}` |
|   2619519 |  267 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|         - |  268 | `			/* Variadic parameter: ...$args */` |
|     25007 |  269 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|     25007 |  270 | `			pIn++;` |
|     12501 |  271 | `		}` |
|   2619519 |  272 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  273 | `			/* Invalid argument */` |
|       ! 0 |  274 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|       ! 0 |  275 | `			return rc;` |
|         - |  276 | `		}` |
|   2619519 |  277 | `		pIn++; /* Jump the dollar sign */` |
|         - |  278 | `		/* Copy argument name */` |
|   2619519 |  279 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   2619519 |  280 | `		if( zDup == 0 ){` |
|       ! 0 |  281 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  282 | `			return SXERR_ABORT;` |
|         - |  283 | `		}` |
|   2619519 |  284 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   2619519 |  285 | `		pIn++;` |
|   2619519 |  286 | `		if( pIn < pEnd ){` |
|   1449991 |  287 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|         - |  288 | `				SyToken *pDefend;` |
|    753693 |  289 | `				sxi32 iNest = 0;` |
|    753693 |  290 | `				pIn++; /* Jump the equal sign */` |
|    753693 |  291 | `				pDefend = pIn;` |
|         - |  292 | `				/* Process the default value associated with this argument */` |
|   1577807 |  293 | `				while( pDefend < pEnd ){` |
|   1085015 |  294 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    260901 |  295 | `						break;` |
|         - |  296 | `					}` |
|    824119 |  297 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|         - |  298 | `						/* Increment nesting level */` |
|     28999 |  299 | `						iNest++;` |
|    809622 |  300 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|         - |  301 | `						/* Decrement nesting level */` |
|     28999 |  302 | `						iNest--;` |
|     14497 |  303 | `					}` |
|    824119 |  304 | `					pDefend++;` |
|         5 |  305 | `				}` |
|    753693 |  306 | `				if( pIn >= pDefend ){` |
|         3 |  307 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|         3 |  308 | `					return rc;` |
|         - |  309 | `				}` |
|         - |  310 | `				/* Process default value */` |
|    753691 |  311 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|    753691 |  312 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  313 | `					return rc;` |
|         - |  314 | `				}` |
|         - |  315 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|         - |  316 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|         - |  317 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|         - |  318 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|         - |  319 | `				 * arg-type check lets null through. */` |
|    753686 |  320 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|    409991 |  321 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|    401707 |  322 | `					&& &pIn[1] == pDefend` |
|     41447 |  323 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|     22797 |  324 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|      6221 |  325 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|         - |  326 | `` 					/* php 8.4 DEPRECATED the implicit-nullable form (`int $x = null` `` |
|         - |  327 | ``					 * without the `?`). PHL targets php's *non-deprecated* surface and`` |
|         - |  328 | ``					 * rejects it outright — the explicit `?int` must be written.`` |
|         - |  329 | ``					 * `mixed $x = null` is fine: mixed already includes null (explicit`` |
|         - |  330 | `					 * ?T / T\|null are already excluded via VM_FUNC_ARG_NULLABLE above). */` |
|         4 |  331 | `					if( sArg.sClass.nByte == sizeof("mixed")-1` |
|         5 |  332 | `						&& SyStrnicmp(SyStringData(&sArg.sClass),"mixed",sizeof("mixed")-1) == 0 ){` |
|         3 |  333 | `						sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|         2 |  334 | `					}else{` |
|         3 |  335 | `						const char *zSep = "";` |
|         3 |  336 | `						SyString sCls = { "", 0 };` |
|         3 |  337 | `						if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|       ! 0 |  338 | `							sCls = ((ph7_class *)pFunc->pUserData)->sName;` |
|       ! 0 |  339 | `							zSep = "::";` |
|       ! 0 |  340 | `						}` |
|         4 |  341 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,` |
|         - |  342 | `							"%z%s%z(): Cannot use null as the default for non-nullable parameter $%z; write the explicit ?T type instead",` |
|         1 |  343 | `							&sCls,zSep,&pFunc->sName,&sArg.sName);` |
|         3 |  344 | `						return SXERR_ABORT;` |
|         - |  345 | `					}` |
|         1 |  346 | `				}` |
|         - |  347 | `				/* Point beyond the default value */` |
|    753689 |  348 | `				pIn = pDefend;` |
|    376842 |  349 | `			}` |
|   1449987 |  350 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  351 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|       ! 0 |  352 | `				return rc;` |
|         - |  353 | `			}` |
|   1449987 |  354 | `			pIn++; /* Jump the trailing comma */` |
|    724991 |  355 | `		}` |
|         - |  356 | `		/* Append argument signature */` |
|   2619515 |  357 | `		if( sArg.nType > 0 ){` |
|    149701 |  358 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|         - |  359 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|     33269 |  360 | `				int marker = 'o';` |
|     33269 |  361 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|     33269 |  362 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     16637 |  363 | `			}else{` |
|         - |  364 | `				int c;` |
|    116437 |  365 | `				c = 'n'; /* cc warning */` |
|         - |  366 | `				/* Type leading character */` |
|    116437 |  367 | `				switch(sArg.nType){` |
|      6225 |  368 | `				case MEMOBJ_HASHMAP:` |
|         - |  369 | `					/* Hashmap aka 'array' */` |
|     12455 |  370 | `					c = 'h';` |
|     12455 |  371 | `					break;` |
|     18792 |  372 | `				case MEMOBJ_INT:` |
|         - |  373 | `					/* Integer */` |
|     37589 |  374 | `					c = 'i';` |
|     37589 |  375 | `					break;` |
|         4 |  376 | `				case MEMOBJ_BOOL:` |
|         - |  377 | `					/* Bool */` |
|        11 |  378 | `					c = 'b';` |
|        11 |  379 | `					break;` |
|         8 |  380 | `				case MEMOBJ_REAL:` |
|         - |  381 | `					/* Float */` |
|        19 |  382 | `					c = 'f';` |
|        19 |  383 | `					break;` |
|     33175 |  384 | `				case MEMOBJ_STRING:` |
|         - |  385 | `					/* String */` |
|     66355 |  386 | `					c = 's';` |
|     66355 |  387 | `					break;` |
|        11 |  388 | `				case MEMOBJ_OBJ:` |
|         - |  389 | `					/* Object */` |
|        26 |  390 | `					c = 'o';` |
|        22 |  391 | `					break;` |
|         1 |  392 | `				default:` |
|         2 |  393 | `					break;` |
|         - |  394 | `				}` |
|    116437 |  395 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|         - |  396 | `			}` |
|     74853 |  397 | `		}else{` |
|         - |  398 | `			/* No type is associated with this parameter which mean` |
|         - |  399 | `			 * that this function is not condidate for overloading.` |
|         - |  400 | `			 */` |
|   2469819 |  401 | `			SyBlobRelease(&sSig);` |
|         - |  402 | `		}` |
|         - |  403 | `		/* Save in the argument set */` |
|   2619515 |  404 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|         5 |  405 | `	}` |
|   1662327 |  406 | `	if( SyBlobLength(&sSig) > 0 ){` |
|         - |  407 | `		/* Save function signature */` |
|     99913 |  408 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|     49954 |  409 | `	}` |
|   1662327 |  410 | `	return SXRET_OK;` |
|    831175 |  411 | `}` |
|         - |  412 | `/*` |
|         - |  413 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|         - |  414 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|         - |  415 | ` * the enclosing function. Returns the token just past the nested construct.` |
|         - |  416 | ` */` |
|     37336 |  417 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|         5 |  418 | `{` |
|     37341 |  419 | `	sxi32 iParen = 0;` |
|     37341 |  420 | `	pIn++; /* past 'function'/'fn' */` |
|         - |  421 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|         - |  422 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|         - |  423 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|    165967 |  424 | `	while( pIn < pEnd ){` |
|    165967 |  425 | `		sxu32 t = pIn->nType;` |
|    165967 |  426 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|    161739 |  427 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|    111971 |  428 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|     91195 |  429 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|    128631 |  430 | `		pIn++;` |
|         5 |  431 | `	}` |
|     20781 |  432 | `	if( pIn >= pEnd ){ return pIn; }` |
|         - |  433 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|         - |  434 | `	{` |
|     20781 |  435 | `		sxi32 d = 0;` |
|    824675 |  436 | `		while( pIn < pEnd ){` |
|    824675 |  437 | `			sxu32 t = pIn->nType;` |
|    824675 |  438 | `			if( t & PH7_TK_OCB ){ d++; }` |
|    791471 |  439 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|    803899 |  440 | `			pIn++;` |
|         5 |  441 | `		}` |
|         - |  442 | `	}` |
|     20781 |  443 | `	return pIn;` |
|     18673 |  444 | `}` |
|         - |  445 | `/*` |
|         - |  446 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|         - |  447 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|         - |  448 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|         - |  449 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|         - |  450 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|         - |  451 | ` * detached-mini-program path untouched.` |
|         - |  452 | ` */` |
|         - |  453 | `/*` |
|         - |  454 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|         - |  455 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|         - |  456 | ` * mixed, object.` |
|         - |  457 | ` */` |
|     12444 |  458 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|         5 |  459 | `{` |
|         - |  460 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|         - |  461 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|         - |  462 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|         - |  463 | `	};` |
|         - |  464 | `	sxu32 i;` |
|     12449 |  465 | `	if( nName > 0 && zName[0] == '\\' ){` |
|       ! 0 |  466 | `		zName++;` |
|       ! 0 |  467 | `		nName--;` |
|       ! 0 |  468 | `	}` |
|     12457 |  469 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|     12457 |  470 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|     12449 |  471 | `			return 1;` |
|         - |  472 | `		}` |
|         5 |  473 | `	}` |
|       ! 0 |  474 | `	return 0;` |
|      6227 |  475 | `}` |
|         - |  476 | `/*` |
|         - |  477 | ` * One atom of a generator's declared return type: is it a supertype of` |
|         - |  478 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|         - |  479 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|         - |  480 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|         - |  481 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|         - |  482 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|         - |  483 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|         - |  484 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|         - |  485 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|         - |  486 | ` * both rather than fatal on valid code (a recorded divergence).` |
|         - |  487 | ` */` |
|     12446 |  488 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|         5 |  489 | `{` |
|     12451 |  490 | `	if( nType == MEMOBJ_OBJ ){` |
|       ! 0 |  491 | ``		return 1; /* bare `object` */`` |
|         - |  492 | `	}` |
|     12451 |  493 | `	if( nType != SXU32_HIGH ){` |
|         3 |  494 | `		return 0; /* scalar/array/void/never/null/... */` |
|         - |  495 | `	}` |
|     12449 |  496 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|     12449 |  497 | `		return 1;` |
|         - |  498 | `	}` |
|         - |  499 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|         - |  500 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|         - |  501 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|         - |  502 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|         - |  503 | `	{` |
|         - |  504 | `		SyBlob sFQN;` |
|         - |  505 | `		int bOk;` |
|       ! 0 |  506 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       ! 0 |  507 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|       ! 0 |  508 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|       ! 0 |  509 | `		SyBlobRelease(&sFQN);` |
|       ! 0 |  510 | `		return bOk;` |
|         - |  511 | `	}` |
|      6228 |  512 | `}` |
|         - |  513 | `/*` |
|         - |  514 | ` * php 8: a generator function may only declare a return type that is a` |
|         - |  515 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|         - |  516 | ` * group qualifies only if every member does. Anything else is php's exact` |
|         - |  517 | ` * compile-time fatal "Generator return type must be a supertype of` |
|         - |  518 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|         - |  519 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|         - |  520 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|         - |  521 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|         - |  522 | ` */` |
|     12712 |  523 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  524 | `{` |
|     12717 |  525 | `	int bOk = 0;` |
|         - |  526 | `	sxu32 nLine;` |
|         - |  527 | `	sxi32 rc;` |
|     12717 |  528 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|       271 |  529 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|         - |  530 | `	}` |
|     12451 |  531 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|       ! 0 |  532 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|       ! 0 |  533 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|         - |  534 | `		sxu32 i,j;` |
|       ! 0 |  535 | `		for( i = 0; i < n && !bOk; i++ ){` |
|         - |  536 | `			int bGroupOk;` |
|       ! 0 |  537 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|       ! 0 |  538 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|         - |  539 | `			}` |
|       ! 0 |  540 | `			bGroupOk = 1;` |
|       ! 0 |  541 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|       ! 0 |  542 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|       ! 0 |  543 | `					bGroupOk = 0;` |
|       ! 0 |  544 | `					break;` |
|         - |  545 | `				}` |
|       ! 0 |  546 | `			}` |
|       ! 0 |  547 | `			bOk = bGroupOk;` |
|       ! 0 |  548 | `		}` |
|       ! 0 |  549 | `	}else{` |
|     12451 |  550 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|         - |  551 | `	}` |
|     12451 |  552 | `	if( bOk ){` |
|     12449 |  553 | `		return SXRET_OK;` |
|         - |  554 | `	}` |
|         - |  555 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|         - |  556 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|         - |  557 | `	 * token of this stream — its line is the function's closing brace. php` |
|         - |  558 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|         - |  559 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|         3 |  560 | `	nLine = pGen->pIn[-1].nLine;` |
|         - |  561 | `	{` |
|         3 |  562 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|         3 |  563 | `		if( sGiven.nByte < 1 ){` |
|       ! 0 |  564 | `			sGiven = pFunc->sReturnClass;` |
|       ! 0 |  565 | `		}` |
|         3 |  566 | `		if( sGiven.nByte < 1 ){` |
|         - |  567 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|         - |  568 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|         - |  569 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|       ! 0 |  570 | `			const char *zScalar =` |
|       ! 0 |  571 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|       ! 0 |  572 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|       ! 0 |  573 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|       ! 0 |  574 | `		}` |
|         3 |  575 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  576 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|         - |  577 | `	}` |
|         3 |  578 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      6361 |  579 | `}` |
|   3498574 |  580 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|         5 |  581 | `{` |
|   3498579 |  582 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|   3498579 |  583 | `	SyToken *pEnd = pGen->pEnd;` |
|   3498579 |  584 | `	sxi32 iDepth = 0;` |
|   3498579 |  585 | `	int bStarted = 0;` |
| 161117129 |  586 | `	while( pIn < pEnd ){` |
| 161117129 |  587 | `		sxu32 t = pIn->nType;` |
| 161117129 |  588 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 153883003 |  589 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 146686549 |  590 | `		if( t & PH7_TK_KEYWORD ){` |
|  10944589 |  591 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  10944589 |  592 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  10931877 |  593 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|         - |  594 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|   5447268 |  595 | `		}` |
| 146636501 |  596 | `		pIn++;` |
|         5 |  597 | `	}` |
|   3485867 |  598 | `	return FALSE;` |
|   1749292 |  599 | `}` |
|         - |  600 | `/*` |
|         - |  601 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|         - |  602 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  603 | ` * and this routine takes care of generating the appropriate error message.` |
|         - |  604 | ` */` |
|   3498574 |  605 | `PH7_PRIVATE sxi32 GenStateCompileFuncBody(` |
|         - |  606 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  607 | `	ph7_vm_func *pFunc    /* Function state */` |
|         - |  608 | `	)` |
|         5 |  609 | `{` |
|         - |  610 | `	SySet *pInstrContainer; /* Instruction container */` |
|         - |  611 | `	GenBlock *pBlock;` |
|         - |  612 | `	sxu32 nGotoOfft;` |
|         - |  613 | `	sxi32 rc;` |
|         - |  614 | `	/* Attach the new function */` |
|   3498579 |  615 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   3498579 |  616 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  617 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|         - |  618 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  619 | `		return SXERR_ABORT;` |
|         - |  620 | `	}` |
|   3498579 |  621 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|         - |  622 | `	/* Swap bytecode containers */` |
|   3498579 |  623 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   3498579 |  624 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|         - |  625 | `	/* Emit constructor property promotion prologue:` |
|         - |  626 | `	 *   $this->NAME = $NAME;` |
|         - |  627 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|         - |  628 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|         - |  629 | `	{` |
|   3498579 |  630 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|         - |  631 | `		sxu32 i;` |
|   6059897 |  632 | `		for( i = 0; i < nArg; i++ ){` |
|   2561323 |  633 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|         - |  634 | `			char *zSrc;` |
|         - |  635 | `			sxu32 nSrc,nName;` |
|         - |  636 | `			SySet sToken;` |
|         - |  637 | `			SyToken *pTmpIn,*pTmpEnd;` |
|         - |  638 | `			sxi32 rcPromote;` |
|   2561323 |  639 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   2561245 |  640 | `				continue;` |
|         - |  641 | `			}` |
|         - |  642 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|         - |  643 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|         - |  644 | `			 * copied), so it must outlive the function — never free it. The` |
|         - |  645 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|         - |  646 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|        83 |  647 | `			nName = SyStringLength(&pArg->sName);` |
|        83 |  648 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|        83 |  649 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|        83 |  650 | `			if( zSrc == 0 ){` |
|       ! 0 |  651 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  652 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  653 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  654 | `				return SXERR_ABORT;` |
|         - |  655 | `			}` |
|         - |  656 | `			{` |
|        83 |  657 | `				char *z = zSrc;` |
|        83 |  658 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|        83 |  659 | `				z += sizeof("$this->")-1;` |
|        83 |  660 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        83 |  661 | `				z += nName;` |
|        83 |  662 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|        83 |  663 | `				z += sizeof(" = $")-1;` |
|        83 |  664 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        83 |  665 | `				z += nName;` |
|        83 |  666 | `				*z = 0;` |
|         - |  667 | `			}` |
|        83 |  668 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        83 |  669 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|        83 |  670 | `			pTmpIn = pGen->pIn;` |
|        83 |  671 | `			pTmpEnd = pGen->pEnd;` |
|        83 |  672 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|        83 |  673 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        83 |  674 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|        83 |  675 | `			pGen->pIn = pTmpIn;` |
|        83 |  676 | `			pGen->pEnd = pTmpEnd;` |
|        83 |  677 | `			SySetRelease(&sToken);` |
|        83 |  678 | `			if( rcPromote == SXERR_ABORT ){` |
|       ! 0 |  679 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  680 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  681 | `				return SXERR_ABORT;` |
|         - |  682 | `			}` |
|         - |  683 | `			/* Discard the assignment result — this is a statement expression. */` |
|        83 |  684 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        44 |  685 | `		}` |
|         - |  686 | `	}` |
|         - |  687 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|         - |  688 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|         - |  689 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|         - |  690 | `	 * generator — and vice versa — is classified independently. */` |
|         - |  691 | `	{` |
|   3498579 |  692 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|   3498579 |  693 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|         - |  694 | `		/* Compile the body */` |
|   3498579 |  695 | `		PH7_CompileBlock(&(*pGen),0);` |
|   3498579 |  696 | `		pGen->bInGenerator = bSavedGen;` |
|         - |  697 | `	}` |
|         - |  698 | `	/* Fix exception jumps now the destination is resolved */` |
|   3498579 |  699 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - |  700 | `	/* Emit the final return if not yet done */` |
|   3498579 |  701 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - |  702 | `	/* Fix gotos jumps now the destination is resolved */` |
|   3498579 |  703 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|       ! 0 |  704 | `		rc = SXERR_ABORT;` |
|       ! 0 |  705 | `	}` |
|   3498579 |  706 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|         - |  707 | `	/* Restore the default container */` |
|   3498579 |  708 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - |  709 | `	/* Leave function block */` |
|   3498579 |  710 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   3498579 |  711 | `	if( rc == SXERR_ABORT ){` |
|         - |  712 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  713 | `		return SXERR_ABORT;` |
|         - |  714 | `	}` |
|         - |  715 | `	/* Scan for yield opcodes to detect generator functions */` |
|         - |  716 | `	{` |
|   3498579 |  717 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|         - |  718 | `		sxu32 i;` |
|  98772205 |  719 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
|  95286343 |  720 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     12717 |  721 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     12717 |  722 | `				break;` |
|         - |  723 | `			}` |
|  47636818 |  724 | `		}` |
|         - |  725 | `	}` |
|   3498579 |  726 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|         - |  727 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|     12717 |  728 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|       ! 0 |  729 | `			return SXERR_ABORT;` |
|         - |  730 | `		}` |
|      6356 |  731 | `	}` |
|         - |  732 | `	/* All done, function body compiled */` |
|   3498579 |  733 | `	return SXRET_OK;` |
|   1749292 |  734 | `}` |
|         - |  735 | `/*` |
|         - |  736 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|         - |  737 | ` * According to the PHP language reference manual.` |
|         - |  738 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|         - |  739 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|         - |  740 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|         - |  741 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  742 | ` *  Functions need not be defined before they are referenced.` |
|         - |  743 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|         - |  744 | ` *  a function even if they were defined inside and vice versa.` |
|         - |  745 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|         - |  746 | ` *  calls with over 32-64 recursion levels.` |
|         - |  747 | ` *` |
|         - |  748 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|         - |  749 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|         - |  750 | ` * on these extension.` |
|         - |  751 | ` */` |
|         - |  752 | `/*` |
|         - |  753 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|         - |  754 | ` */` |
|       596 |  755 | `PH7_PRIVATE int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|         5 |  756 | `{` |
|         - |  757 | `	sxu32 i;` |
|      1653 |  758 | `	for( i = 0; i < n; i++ ){` |
|      1419 |  759 | `		int a = zA[i], b = zB[i];` |
|      1419 |  760 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      1419 |  761 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      1419 |  762 | `		if( a != b ) return a - b;` |
|       531 |  763 | `	}` |
|       239 |  764 | `	return 0;` |
|       303 |  765 | `}` |
|         - |  766 | `/*` |
|         - |  767 | ` * Internal type-atom kinds used during union type parsing.` |
|         - |  768 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|         - |  769 | ` * (which are positive bit values stored in sxu32).` |
|         - |  770 | ` */` |
|         - |  771 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|         - |  772 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|         - |  773 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|         - |  774 |  |
|         - |  775 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|         - |  776 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|         - |  777 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|         - |  778 |  |
|         - |  779 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|         - |  780 | `struct PhlTypeAtom {` |
|         - |  781 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|         - |  782 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|         - |  783 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|         - |  784 | `	sxu32 nCanon;` |
|         - |  785 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|         - |  786 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|         - |  787 | `};` |
|         - |  788 |  |
|         - |  789 | `/*` |
|         - |  790 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|         - |  791 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|         - |  792 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|         - |  793 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|         - |  794 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|         - |  795 | ` * already be consumed by the caller.` |
|         - |  796 | ` */` |
|         - |  797 | `/*` |
|         - |  798 | ` * TRUE if pName is a reserved PHP type keyword (never a class name), so a type` |
|         - |  799 | ` * hint that spells it must not be namespace-qualified. Only the words that can` |
|         - |  800 | ` * reach GenStateParseOneTypeAtom's identifier branch as a bare name matter here` |
|         - |  801 | ` * (bool/int/float/string/array/object/self/static/parent arrive as keywords, and` |
|         - |  802 | ` * null/void/never are matched before the class path), but the full set is listed` |
|         - |  803 | ` * so the guard is robust to lexer changes.` |
|         - |  804 | ` */` |
|     45924 |  805 | `static int GenStateIsReservedTypeWord(const SyString *pName)` |
|         5 |  806 | `{` |
|         - |  807 | `	static const char *azWords[] = {` |
|         - |  808 | `		"false","true","mixed","iterable","callable","null","void","never",` |
|         - |  809 | `		"bool","boolean","int","integer","float","double","string","array",` |
|         - |  810 | `		"object","self","static","parent"` |
|         - |  811 | `	};` |
|         - |  812 | `	sxu32 i;` |
|    962195 |  813 | `	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){` |
|    916397 |  814 | `		sxu32 n = (sxu32)SyStrlen(azWords[i]);` |
|    916397 |  815 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azWords[i],n) == 0 ){` |
|       131 |  816 | `			return 1;` |
|         - |  817 | `		}` |
|    458138 |  818 | `	}` |
|     45803 |  819 | `	return 0;` |
|     22967 |  820 | `}` |
|    201016 |  821 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|         5 |  822 | `{` |
|    201021 |  823 | `	SyToken *pIn = pGen->pIn;` |
|    201021 |  824 | `	int bAbsolute = 0;` |
|    201021 |  825 | `	SyZero(pOut, sizeof(*pOut));` |
|    201021 |  826 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    201021 |  827 | `	if( pIn >= pGen->pEnd ){` |
|       ! 0 |  828 | `		return SXERR_SYNTAX;` |
|         - |  829 | `	}` |
|         - |  830 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    201021 |  831 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|        10 |  832 | `		bAbsolute = 1; /* fully-qualified: never prefix the current namespace */` |
|        10 |  833 | `		pIn++;` |
|        10 |  834 | `		if( pIn >= pGen->pEnd ){` |
|       ! 0 |  835 | `			return SXERR_SYNTAX;` |
|         - |  836 | `		}` |
|         4 |  837 | `	}` |
|    201021 |  838 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  839 | `		return SXERR_SYNTAX;` |
|         - |  840 | `	}` |
|    201021 |  841 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|    154861 |  842 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|    154861 |  843 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|     16669 |  844 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|    146529 |  845 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      8377 |  846 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|    134011 |  847 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|     50535 |  848 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|    104560 |  849 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|     79173 |  850 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|     39711 |  851 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|        57 |  852 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|       101 |  853 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|        37 |  854 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|        59 |  855 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|        20 |  856 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|        40 |  857 | `			pOut->nType = SXU32_HIGH;` |
|        40 |  858 | `			pOut->sClass = pIn->sData;` |
|        22 |  859 | `		}else{` |
|         3 |  860 | `			return SXERR_SYNTAX;` |
|         - |  861 | `		}` |
|    154859 |  862 | `		pIn++;` |
|     77432 |  863 | `	}else{` |
|         - |  864 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|         - |  865 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|     46165 |  866 | `		SyString *pT = &pIn->sData;` |
|     46165 |  867 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|        34 |  868 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|        34 |  869 | `			pIn++;` |
|     46150 |  870 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|       181 |  871 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|       181 |  872 | `			pIn++;` |
|     46047 |  873 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|        26 |  874 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|        26 |  875 | `			pIn++;` |
|        15 |  876 | `		}else{` |
|         - |  877 | `			/* Class / interface name; consume namespace path a\b\c */` |
|     45937 |  878 | `			SyToken *pFirst = pIn;` |
|     45937 |  879 | `			SyToken *pLast = pIn;` |
|     45937 |  880 | `			pOut->nType = SXU32_HIGH;` |
|     45937 |  881 | `			pOut->sClass = pIn->sData;` |
|     45937 |  882 | `			pIn++;` |
|     68901 |  883 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|     45940 |  884 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|         3 |  885 | `				pLast = &pIn[1];` |
|         3 |  886 | `				pIn += 2;` |
|         1 |  887 | `			}` |
|     45937 |  888 | `			if( pLast != pFirst ){` |
|         3 |  889 | `				const char *zFirst = pFirst->sData.zString;` |
|         3 |  890 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|         3 |  891 | `				pOut->sClass.zString = zFirst;` |
|         3 |  892 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|         1 |  893 | `			}` |
|         - |  894 | `			/* Namespace-qualify a bare (single-segment, non-absolute) class type so` |
|         - |  895 | ``			 * a `: Base` / `Base $x` hint in namespace N resolves to N\Base (or a`` |
|         - |  896 | ``			 * `use` alias) at type-check time instead of the global \Base — mirrors`` |
|         - |  897 | `			 * the NEW/CALL/instanceof qualification. Absolute (\Base) and already-` |
|         - |  898 | `			 * qualified (A\B) names are left as written, matching GenStateNsQualifyName. */` |
|         - |  899 | `			/* Reserved type words that reach this identifier branch (false, true,` |
|         - |  900 | `			 * mixed, iterable, callable) are NOT classes and must not be qualified` |
|         - |  901 | ``			 * (else `false\|string` becomes `Ns\false\|string`). */`` |
|     45937 |  902 | `			if( !bAbsolute && pLast == pFirst && !GenStateIsReservedTypeWord(&pOut->sClass) ){` |
|         - |  903 | `				SyBlob sFqn;` |
|     45803 |  904 | `				SyBlobInit(&sFqn,&pGen->pVm->sAllocator);` |
|     45803 |  905 | `				GenStateResolveName(pGen,&pOut->sClass,&sFqn);` |
|     45798 |  906 | `				if( SyBlobLength(&sFqn) != pOut->sClass.nByte` |
|     45798 |  907 | `				 \|\| SyMemcmp(SyBlobData(&sFqn),(const void *)pOut->sClass.zString,pOut->sClass.nByte) != 0 ){` |
|        17 |  908 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 |  909 | `						(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));` |
|        12 |  910 | `					if( zDup ){` |
|        12 |  911 | `						SyStringInitFromBuf(&pOut->sClass,zDup,SyBlobLength(&sFqn));` |
|         5 |  912 | `					}` |
|         5 |  913 | `				}` |
|     45803 |  914 | `				SyBlobRelease(&sFqn);` |
|     22899 |  915 | `			}` |
|         - |  916 | `		}` |
|         - |  917 | `	}` |
|    201019 |  918 | `	pGen->pIn = pIn;` |
|    201019 |  919 | `	return SXRET_OK;` |
|    100513 |  920 | `}` |
|         - |  921 |  |
|         - |  922 | `/*` |
|         - |  923 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|         - |  924 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|         - |  925 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|         - |  926 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|         - |  927 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|         - |  928 | ` */` |
|    200826 |  929 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|         5 |  930 | `{` |
|         - |  931 | `	int i;` |
|    200831 |  932 | `	int nNonNull = 0;` |
|    200831 |  933 | `	int bAnyIntersection = 0;` |
|         - |  934 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    200831 |  935 | `	sxu32 nMaxGroup = 0;` |
|   6627263 |  936 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    401821 |  937 | `	for( i = 0; i < nAtoms; i++ ){` |
|    200995 |  938 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    200965 |  939 | `			nNonNull++;` |
|    200965 |  940 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    200965 |  941 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    200965 |  942 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|    100480 |  943 | `			}` |
|    100480 |  944 | `		}` |
|    100500 |  945 | `	}` |
|    401769 |  946 | `	for( i = 0; i < nAtoms; i++ ){` |
|    200967 |  947 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        28 |  948 | `			bAnyIntersection = 1;` |
|        28 |  949 | `			break;` |
|         - |  950 | `		}` |
|    100474 |  951 | `	}` |
|    200831 |  952 | `	if( bAnyIntersection ){` |
|         - |  953 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|         - |  954 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|         - |  955 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|        28 |  956 | `		sxu32 g, nGroups = 0;` |
|        28 |  957 | `		int bFirstGroup = 1;` |
|        58 |  958 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|        58 |  959 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|        34 |  960 | `			int bFirstMember = 1;` |
|         - |  961 | `			int bWrap;` |
|        34 |  962 | `			if( aGroupCount[g] == 0 ) continue;` |
|         - |  963 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|         - |  964 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|         - |  965 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|         - |  966 | `			 * parens, matching PHP's canonical text. */` |
|        46 |  967 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|        34 |  968 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|        34 |  969 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|       106 |  970 | `			for( i = 0; i < nAtoms; i++ ){` |
|        76 |  971 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|        58 |  972 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|        58 |  973 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|        54 |  974 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        29 |  975 | `				}else{` |
|         6 |  976 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  977 | `				}` |
|        58 |  978 | `				bFirstMember = 0;` |
|        31 |  979 | `			}` |
|        34 |  980 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|        34 |  981 | `			bFirstGroup = 0;` |
|        19 |  982 | `		}` |
|        28 |  983 | `		if( bNullable ){` |
|       ! 0 |  984 | `			SyBlobAppend(pBlob, "\|", 1);` |
|       ! 0 |  985 | `			SyBlobAppend(pBlob, "null", 4);` |
|       ! 0 |  986 | `		}` |
|     10443 |  987 | `		return;` |
|         - |  988 | `	}` |
|    200807 |  989 | `	if( nNonNull == 1 && bNullable ){` |
|         - |  990 | `		/* Shorthand: ?T */` |
|     20835 |  991 | `		for( i = 0; i < nAtoms; i++ ){` |
|     20835 |  992 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     20835 |  993 | `			SyBlobAppend(pBlob, "?", 1);` |
|     20835 |  994 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|     12455 |  995 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      6230 |  996 | `			}else{` |
|      8385 |  997 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  998 | `			}` |
|     20835 |  999 | `			return;` |
|       ! 0 | 1000 | `		}` |
|       ! 0 | 1001 | `	}` |
|         - | 1002 | `	{` |
|    179977 | 1003 | `		int bFirst = 1;` |
|         - | 1004 | `		/* 1) Classes in declaration order */` |
|    360069 | 1005 | `		for( i = 0; i < nAtoms; i++ ){` |
|    180097 | 1006 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|     33473 | 1007 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     33473 | 1008 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|     33473 | 1009 | `				bFirst = 0;` |
|     16734 | 1010 | `			}` |
|     90051 | 1011 | `		}` |
|         - | 1012 | `		/* 2) Built-ins in canonical order */` |
|         - | 1013 | `		{` |
|         - | 1014 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|         - | 1015 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|         - | 1016 | `			int k;` |
|   1259809 | 1017 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|   2013845 | 1018 | `				for( i = 0; i < nAtoms; i++ ){` |
|   1080431 | 1019 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|    146423 | 1020 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    146423 | 1021 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|    146423 | 1022 | `						bFirst = 0;` |
|    146423 | 1023 | `						break;` |
|         - | 1024 | `					}` |
|    467009 | 1025 | `				}` |
|    539921 | 1026 | `			}` |
|         - | 1027 | `		}` |
|         - | 1028 | `		/* 3) null suffix */` |
|    179977 | 1029 | `		if( bNullable ){` |
|        20 | 1030 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|        20 | 1031 | `			SyBlobAppend(pBlob, "null", 4);` |
|         8 | 1032 | `		}` |
|         - | 1033 | `	}` |
|    100418 | 1034 | `}` |
|         - | 1035 |  |
|         - | 1036 | `/*` |
|         - | 1037 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|         - | 1038 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|         - | 1039 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|         - | 1040 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|         - | 1041 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|         - | 1042 | ` * whether it was parenthesized.` |
|         - | 1043 | ` *` |
|         - | 1044 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|         - | 1045 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|         - | 1046 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|         - | 1047 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|         - | 1048 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|         - | 1049 | ` */` |
|    200990 | 1050 | `static sxi32 GenStateParsePart(` |
|         - | 1051 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|         - | 1052 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|         5 | 1053 | `{` |
|         - | 1054 | `	sxi32 rc;` |
|    200995 | 1055 | `	int nMembers = 0;` |
|    200995 | 1056 | `	int bParen = 0;` |
|    200995 | 1057 | `	*pnMembers = 0;` |
|    200995 | 1058 | `	*pbParen = 0;` |
|    200995 | 1059 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         9 | 1060 | `		bParen = 1;` |
|         9 | 1061 | `		pGen->pIn++; /* skip '(' */` |
|         3 | 1062 | `	}` |
|    100495 | 1063 | `	for(;;){` |
|    201021 | 1064 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|       ! 0 | 1065 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1066 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|       ! 0 | 1067 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 1068 | `		}` |
|    201021 | 1069 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    201021 | 1070 | `		if( rc != SXRET_OK ){` |
|         3 | 1071 | `			return rc;` |
|         - | 1072 | `		}` |
|    201019 | 1073 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    201019 | 1074 | `		(*pnAtoms)++;` |
|    201019 | 1075 | `		nMembers++;` |
|         - | 1076 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    201019 | 1077 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        39 | 1078 | `			SyToken *pNext = &pGen->pIn[1];` |
|        34 | 1079 | `			if( pNext < pGen->pEnd` |
|        39 | 1080 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        31 | 1081 | `				pGen->pIn++; /* skip '&' */` |
|        31 | 1082 | `				continue;` |
|         - | 1083 | `			}` |
|         4 | 1084 | `		}` |
|    200993 | 1085 | `		break;` |
|       ! 0 | 1086 | `	}` |
|    200993 | 1087 | `	if( bParen ){` |
|         9 | 1088 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 1089 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1090 | `				"Malformed DNF type: expecting ')'");` |
|       ! 0 | 1091 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 1092 | `		}` |
|         9 | 1093 | `		pGen->pIn++; /* skip ')' */` |
|         9 | 1094 | `		if( nMembers < 2 ){` |
|       ! 0 | 1095 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1096 | `				"Parenthesized type must be an intersection of at least two types");` |
|       ! 0 | 1097 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 1098 | `		}` |
|         3 | 1099 | `	}` |
|    200993 | 1100 | `	*pnMembers = nMembers;` |
|    200993 | 1101 | `	*pbParen = bParen;` |
|    200993 | 1102 | `	return SXRET_OK;` |
|    100500 | 1103 | `}` |
|         - | 1104 |  |
|         - | 1105 | `/*` |
|         - | 1106 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|         - | 1107 | ` *` |
|         - | 1108 | ` * Outputs:` |
|         - | 1109 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|         - | 1110 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|         - | 1111 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|         - | 1112 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|         - | 1113 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|         - | 1114 | ` *     already be initialized by the caller (allocator set, etc).` |
|         - | 1115 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|         - | 1116 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|         - | 1117 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|         - | 1118 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|         - | 1119 | ` *` |
|         - | 1120 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|         - | 1121 | ` * SXERR_ABORT on fatal compile errors.` |
|         - | 1122 | ` */` |
|    200842 | 1123 | `PH7_PRIVATE sxi32 GenStateParseUnionTypeDecl(` |
|         - | 1124 | `	ph7_gen_state *pGen,` |
|         - | 1125 | `	sxu32 *pnType,` |
|         - | 1126 | `	SyString *pClass,` |
|         - | 1127 | `	SySet *pAlts,` |
|         - | 1128 | `	sxi32 *piTypeFlags,` |
|         - | 1129 | `	SyString *pTypeText,` |
|         - | 1130 | `	int iNullableFlag,` |
|         - | 1131 | `	int iUnionFlag,` |
|         - | 1132 | `	int bAllowVoid,` |
|         - | 1133 | `	sxu32 nLine` |
|         5 | 1134 | `){` |
|         - | 1135 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    200847 | 1136 | `	int nAtoms = 0;` |
|    200847 | 1137 | `	int bShortNullable = 0;` |
|    200847 | 1138 | `	int bExplicitNull = 0;` |
|         - | 1139 | `	sxi32 rc;` |
|    200847 | 1140 | `	*pnType = 0;` |
|    200847 | 1141 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    200847 | 1142 | `	*piTypeFlags = 0;` |
|    200847 | 1143 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|         - | 1144 |  |
|    200847 | 1145 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 1146 | `		return SXRET_OK;` |
|         - | 1147 | `	}` |
|         - | 1148 | ``	/* Optional `?` shorthand prefix */`` |
|    200842 | 1149 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|     20823 | 1150 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|     20823 | 1151 | `		bShortNullable = 1;` |
|     20823 | 1152 | `		pGen->pIn++;` |
|     20823 | 1153 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 1154 | `			return SXERR_SYNTAX;` |
|         - | 1155 | `		}` |
|     10409 | 1156 | `	}` |
|         - | 1157 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|         - | 1158 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|         - | 1159 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|         - | 1160 | `	{` |
|         - | 1161 | `		int nMembers, bParen;` |
|    200847 | 1162 | `		sxu32 iGroup = 0;` |
|    200847 | 1163 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    200847 | 1164 | `		if( rc != SXRET_OK ){` |
|         4 | 1165 | `			return rc;` |
|         - | 1166 | `		}` |
|         - | 1167 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|         - | 1168 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|         - | 1169 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|         - | 1170 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|         - | 1171 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|    301487 | 1172 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    201072 | 1173 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       155 | 1174 | `			if( bShortNullable ){` |
|         - | 1175 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|         - | 1176 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|         - | 1177 | `				 * already reported" so callers skip their own error emission. */` |
|         3 | 1178 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - | 1179 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|         3 | 1180 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|         - | 1181 | `			}` |
|       153 | 1182 | `			if( nMembers >= 2 && !bParen ){` |
|       ! 0 | 1183 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|         - | 1184 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 | 1185 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 1186 | `			}` |
|       153 | 1187 | ``			pGen->pIn++; /* skip `\|` */`` |
|       153 | 1188 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|       153 | 1189 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1190 | `				return rc;` |
|         - | 1191 | `			}` |
|         5 | 1192 | `		}` |
|    200843 | 1193 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|       ! 0 | 1194 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1195 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 | 1196 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 1197 | `		}` |
|         - | 1198 | `	}` |
|         - | 1199 | `	/* Validation pass.` |
|         - | 1200 | `	 *` |
|         - | 1201 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|         - | 1202 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|         - | 1203 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|         - | 1204 | `	 */` |
|         - | 1205 | `	{` |
|         - | 1206 | `		int i, j;` |
|    200843 | 1207 | `		int bHasNonNull = 0;` |
|    200843 | 1208 | `		int bAnyIntersection = 0;` |
|         - | 1209 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|         - | 1210 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|         - | 1211 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|   6627659 | 1212 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    401855 | 1213 | `		for( i = 0; i < nAtoms; i++ ){` |
|    201017 | 1214 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|    100511 | 1215 | `		}` |
|    401799 | 1216 | `		for( i = 0; i < nAtoms; i++ ){` |
|    200987 | 1217 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|    100483 | 1218 | `		}` |
|         - | 1219 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|         - | 1220 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    200843 | 1221 | `		if( bShortNullable && bAnyIntersection ){` |
|       ! 0 | 1222 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1223 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|       ! 0 | 1224 | `			return SXERR_SYNTAX;` |
|         - | 1225 | `		}` |
|    401841 | 1226 | `		for( i = 0; i < nAtoms; i++ ){` |
|         - | 1227 | `			/* Intersection members must be class/interface types (PHP rejects` |
|         - | 1228 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|         - | 1229 | ``			 * `true`/`false` in an intersection). */`` |
|    201015 | 1230 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        55 | 1231 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|        55 | 1232 | `				if( bClassLike ){` |
|        52 | 1233 | `					SyString *pC = &aAtoms[i].sClass;` |
|        48 | 1234 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|        48 | 1235 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|        48 | 1236 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|        52 | 1237 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|       ! 0 | 1238 | `						bClassLike = 0;` |
|       ! 0 | 1239 | `					}` |
|        24 | 1240 | `				}` |
|        55 | 1241 | `				if( !bClassLike ){` |
|         - | 1242 | `					const char *zName; sxu32 nName;` |
|         3 | 1243 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 | 1244 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|       ! 0 | 1245 | `					}else{` |
|         3 | 1246 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|         - | 1247 | `					}` |
|         4 | 1248 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1249 | `						"Type %.*s cannot be part of an intersection type",` |
|         1 | 1250 | `						(int)nName, zName);` |
|         3 | 1251 | `					return SXERR_SYNTAX;` |
|         - | 1252 | `				}` |
|        24 | 1253 | `			}` |
|    201013 | 1254 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|       181 | 1255 | `				if( nAtoms > 1 ){` |
|         3 | 1256 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1257 | `						"Void can only be used as a standalone type");` |
|         3 | 1258 | `					return SXERR_SYNTAX;` |
|         - | 1259 | `				}` |
|       179 | 1260 | `				if( !bAllowVoid ){` |
|       ! 0 | 1261 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1262 | `						"void cannot be used here");` |
|       ! 0 | 1263 | `					return SXERR_SYNTAX;` |
|         - | 1264 | `				}` |
|       179 | 1265 | `				if( bShortNullable ){` |
|       ! 0 | 1266 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1267 | `						"Void type cannot be nullable");` |
|       ! 0 | 1268 | `					return SXERR_SYNTAX;` |
|         - | 1269 | `				}` |
|        87 | 1270 | `			}` |
|    201011 | 1271 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|         - | 1272 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|         - | 1273 | `				 * type (never = the function does not return). Mirrors the void` |
|         - | 1274 | `				 * validation above; accepted here and enforced at compile time` |
|         - | 1275 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|        26 | 1276 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|         - | 1277 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|         - | 1278 | `					 * same as any other non-standalone use. */` |
|         6 | 1279 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1280 | `						"never can only be used as a standalone type");` |
|         6 | 1281 | `					return SXERR_SYNTAX;` |
|         - | 1282 | `				}` |
|        21 | 1283 | `				if( !bAllowVoid ){` |
|         - | 1284 | `					/* Return-only: params call with bAllowVoid=0. */` |
|         3 | 1285 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1286 | `						"never cannot be used as a parameter type");` |
|         3 | 1287 | `					return SXERR_SYNTAX;` |
|         - | 1288 | `				}` |
|         8 | 1289 | `			}` |
|    201005 | 1290 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|        34 | 1291 | `				bExplicitNull = 1;` |
|        19 | 1292 | `			}else{` |
|    200975 | 1293 | `				bHasNonNull = 1;` |
|         - | 1294 | `			}` |
|         - | 1295 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|         - | 1296 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|         - | 1297 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|         - | 1298 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|         - | 1299 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    201217 | 1300 | `			for( j = 0; j < i; j++ ){` |
|       219 | 1301 | `				int bDup = 0;` |
|       219 | 1302 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|       419 | 1303 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|       214 | 1304 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|       219 | 1305 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|       207 | 1306 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|        49 | 1307 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|        42 | 1308 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|        42 | 1309 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|        16 | 1310 | `								aAtoms[j].sClass.zString,` |
|        32 | 1311 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|       ! 0 | 1312 | `							bDup = 1;` |
|       ! 0 | 1313 | `						}` |
|        26 | 1314 | `					}else{` |
|         3 | 1315 | `						bDup = 1;` |
|         - | 1316 | `					}` |
|        22 | 1317 | `				}` |
|       207 | 1318 | `				if( bDup ){` |
|         - | 1319 | `					const char *zName;` |
|         - | 1320 | `					sxu32 nName;` |
|         3 | 1321 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 | 1322 | `						zName = aAtoms[i].sClass.zString;` |
|       ! 0 | 1323 | `						nName = aAtoms[i].sClass.nByte;` |
|       ! 0 | 1324 | `					}else{` |
|         3 | 1325 | `						zName = aAtoms[i].zCanon;` |
|         3 | 1326 | `						nName = aAtoms[i].nCanon;` |
|         - | 1327 | `					}` |
|         4 | 1328 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         1 | 1329 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|         3 | 1330 | `					return SXERR_SYNTAX;` |
|         - | 1331 | `				}` |
|       105 | 1332 | `			}` |
|    100504 | 1333 | `		}` |
|    200831 | 1334 | `		if( !bHasNonNull && bExplicitNull ){` |
|         7 | 1335 | `			if( bShortNullable ){` |
|         - | 1336 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|       ! 0 | 1337 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1338 | `					"Null can not be used as a standalone type");` |
|       ! 0 | 1339 | `				return SXERR_SYNTAX;` |
|         - | 1340 | `			}` |
|         - | 1341 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|         - | 1342 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|         - | 1343 | `			 * path below leaves *pnType untouched when there is no non-null` |
|         - | 1344 | `			 * atom, so set it here. */` |
|         7 | 1345 | `			*pnType = MEMOBJ_NULL;` |
|         3 | 1346 | `		}` |
|         - | 1347 | `	}` |
|         - | 1348 | `	/* Compute nullability flag */` |
|    200831 | 1349 | `	if( bShortNullable \|\| bExplicitNull ){` |
|     20851 | 1350 | `		*piTypeFlags \|= iNullableFlag;` |
|     10423 | 1351 | `	}` |
|         - | 1352 | `	/* Build canonical type text */` |
|    200831 | 1353 | `	if( pTypeText ){` |
|         - | 1354 | `		SyBlob sBlob;` |
|    200831 | 1355 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|    290836 | 1356 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|    100413 | 1357 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    200831 | 1358 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|    300959 | 1359 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    200636 | 1360 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    200641 | 1361 | `			if( zDup ){` |
|    200641 | 1362 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|    100318 | 1363 | `			}` |
|    100318 | 1364 | `		}` |
|    200831 | 1365 | `		SyBlobRelease(&sBlob);` |
|    100413 | 1366 | `	}` |
|         - | 1367 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|         - | 1368 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|         - | 1369 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|         - | 1370 | `	{` |
|    200831 | 1371 | `		int nNonNull = 0;` |
|    200831 | 1372 | `		int iNonNullIdx = -1;` |
|         - | 1373 | `		int i;` |
|    401821 | 1374 | `		for( i = 0; i < nAtoms; i++ ){` |
|    200995 | 1375 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    200965 | 1376 | `				nNonNull++;` |
|    200965 | 1377 | `				iNonNullIdx = i;` |
|    100480 | 1378 | `			}` |
|    100500 | 1379 | `		}` |
|    200831 | 1380 | `		if( nNonNull <= 1 ){` |
|         - | 1381 | `			/* Fast path: store as single type. */` |
|    200713 | 1382 | `			if( iNonNullIdx >= 0 ){` |
|    200707 | 1383 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    200707 | 1384 | `				if( pA->nType == SXU32_HIGH ){` |
|     68822 | 1385 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     22939 | 1386 | `						pA->sClass.zString, pA->sClass.nByte);` |
|     45883 | 1387 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|     45883 | 1388 | `					*pnType = SXU32_HIGH;` |
|     45883 | 1389 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    177768 | 1390 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|       179 | 1391 | `					*pnType = MEMOBJ_VOID;` |
|    154742 | 1392 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|        18 | 1393 | `					*pnType = MEMOBJ_NEVER;` |
|        10 | 1394 | `				}else{` |
|    154639 | 1395 | `					*pnType = pA->nType;` |
|         - | 1396 | `				}` |
|    100351 | 1397 | `			}` |
|    100359 | 1398 | `		}else{` |
|         - | 1399 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|       123 | 1400 | `			*piTypeFlags \|= iUnionFlag;` |
|       391 | 1401 | `			for( i = 0; i < nAtoms; i++ ){` |
|         - | 1402 | `				ph7_type_alt sAlt;` |
|       273 | 1403 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       263 | 1404 | `				SyZero(&sAlt, sizeof(sAlt));` |
|       263 | 1405 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|       263 | 1406 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       140 | 1407 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        45 | 1408 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        95 | 1409 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|        95 | 1410 | `					sAlt.nType = SXU32_HIGH;` |
|        95 | 1411 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|        50 | 1412 | `				}else{` |
|       173 | 1413 | `					sAlt.nType = aAtoms[i].nType;` |
|       173 | 1414 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|         - | 1415 | `				}` |
|       263 | 1416 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|       134 | 1417 | `			}` |
|         - | 1418 | `		}` |
|         - | 1419 | `	}` |
|    200831 | 1420 | `	return SXRET_OK;` |
|    100426 | 1421 | `}` |
|         - | 1422 |  |
|         - | 1423 | `/*` |
|         - | 1424 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|         - | 1425 | `` * pGen->pIn should point to the token after `)`.`` |
|         - | 1426 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|         - | 1427 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|         - | 1428 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|         - | 1429 | `` *          and union types `: T\|U`.`` |
|         - | 1430 | ` */` |
|   3648398 | 1431 | `PH7_PRIVATE sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|         5 | 1432 | `{` |
|   3648403 | 1433 | `	sxi32 iFlags = 0;` |
|         - | 1434 | `	sxi32 rc;` |
|         - | 1435 | `	sxu32 nLine;` |
|   3648403 | 1436 | `	pFunc->nReturnType = 0;` |
|   3648403 | 1437 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|   3648403 | 1438 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|         - | 1439 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|         - | 1440 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|         - | 1441 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|         - | 1442 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|         - | 1443 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|   3648403 | 1444 | `	SySetReset(&pFunc->aReturnUnion);` |
|   3648403 | 1445 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|   3648403 | 1446 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|   3614387 | 1447 | `		return SXRET_OK;` |
|         - | 1448 | `	}` |
|     34021 | 1449 | `	pGen->pIn++; /* Skip ':' */` |
|     34021 | 1450 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 1451 | `		return SXRET_OK;` |
|         - | 1452 | `	}` |
|     34021 | 1453 | `	nLine = pGen->pIn->nLine;` |
|     34021 | 1454 | `	rc = GenStateParseUnionTypeDecl(` |
|     17008 | 1455 | `		pGen,` |
|     17008 | 1456 | `		&pFunc->nReturnType,` |
|     17008 | 1457 | `		&pFunc->sReturnClass,` |
|     17008 | 1458 | `		&pFunc->aReturnUnion,` |
|         - | 1459 | `		&iFlags,` |
|     17008 | 1460 | `		&pFunc->sReturnTypeName,` |
|         - | 1461 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|         - | 1462 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|         - | 1463 | `		/* iUnionFlag */ 0,` |
|         - | 1464 | `		/* bAllowVoid */ 1,` |
|     17008 | 1465 | `		nLine);` |
|     34021 | 1466 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 1467 | `		return SXERR_ABORT;` |
|         - | 1468 | `	}` |
|     34021 | 1469 | `	if( rc == SXERR_CORRUPT ){` |
|         - | 1470 | `		/* Error already reported */` |
|       ! 0 | 1471 | `		return SXERR_SYNTAX;` |
|         - | 1472 | `	}` |
|     34021 | 1473 | `	if( rc == SXERR_SYNTAX ){` |
|         9 | 1474 | `		if( pGen->pIn < pGen->pEnd ){` |
|        12 | 1475 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - | 1476 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|         6 | 1477 | `				&pGen->pIn->sData);` |
|         6 | 1478 | `		}else{` |
|       ! 0 | 1479 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|         - | 1480 | `				"syntax error, unexpected end of file in return type declaration");` |
|         - | 1481 | `		}` |
|         9 | 1482 | `		return SXERR_SYNTAX;` |
|         - | 1483 | `	}` |
|     34015 | 1484 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     34015 | 1485 | `	return SXRET_OK;` |
|   1824204 | 1486 | `}` |
|         - | 1487 |  |
|    557114 | 1488 | `PH7_PRIVATE sxi32 GenStateCompileFunc(` |
|         - | 1489 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 1490 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|         - | 1491 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 1492 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|         - | 1493 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|         - | 1494 | `	)` |
|         5 | 1495 | `{` |
|         - | 1496 | `	ph7_vm_func *pFunc;` |
|         - | 1497 | `	SyToken *pEnd;` |
|         - | 1498 | `	sxu32 nLine;` |
|         - | 1499 | `	char *zName;` |
|         - | 1500 | `	sxi32 rc;` |
|         - | 1501 | `	/* Extract line number */` |
|    557119 | 1502 | `	nLine = pGen->pIn->nLine;` |
|         - | 1503 | `	/* Jump the left parenthesis '(' */` |
|    557119 | 1504 | `	pGen->pIn++;` |
|         - | 1505 | `	/* Delimit the function signature */` |
|    557119 | 1506 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    557119 | 1507 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 1508 | `		/* Syntax error */` |
|        11 | 1509 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable");` |
|         4 | 1510 | `		(void)pName;` |
|        11 | 1511 | `		if( rc == SXERR_ABORT ){` |
|         - | 1512 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 1513 | `			return SXERR_ABORT;` |
|         - | 1514 | `		}` |
|        11 | 1515 | `		pGen->pIn = pGen->pEnd;` |
|        11 | 1516 | `		return SXRET_OK;` |
|         - | 1517 | `	}` |
|         - | 1518 | `	/* Create the function state */` |
|    557111 | 1519 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|    557111 | 1520 | `	if( pFunc == 0 ){` |
|       ! 0 | 1521 | `		goto OutOfMem;` |
|         - | 1522 | `	}` |
|         - | 1523 | `	/* Build the function name, prepending namespace if active */` |
|    557119 | 1524 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|         - | 1525 | `		SyBlob sFQN;` |
|         - | 1526 | `		sxu32 nLen;` |
|        18 | 1527 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        18 | 1528 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        18 | 1529 | `		SyBlobAppend(&sFQN,"\\",1);` |
|        18 | 1530 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|        18 | 1531 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|        18 | 1532 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|        18 | 1533 | `		SyBlobRelease(&sFQN);` |
|        18 | 1534 | `		if( zName == 0 ){` |
|       ! 0 | 1535 | `			goto OutOfMem;` |
|         - | 1536 | `		}` |
|        18 | 1537 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|        10 | 1538 | `	}else{` |
|    557095 | 1539 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    557095 | 1540 | `		if( zName == 0 ){` |
|       ! 0 | 1541 | `			goto OutOfMem;` |
|         - | 1542 | `		}` |
|    557095 | 1543 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|         - | 1544 | `	}` |
|         - | 1545 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|         - | 1546 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|    557111 | 1547 | `	pFunc->nLine = nLine;` |
|    557111 | 1548 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|    557111 | 1549 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 1550 | `		return SXERR_ABORT;` |
|         - | 1551 | `	}` |
|    557111 | 1552 | `	if( pGen->pIn < pEnd ){` |
|         - | 1553 | `		/* Collect function arguments */` |
|    473149 | 1554 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|    473149 | 1555 | `		if( rc == SXERR_ABORT ){` |
|         - | 1556 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|         3 | 1557 | `			return SXERR_ABORT;` |
|         - | 1558 | `		}` |
|    236571 | 1559 | `	}` |
|         - | 1560 | `	/* Point past ')' and parse optional return type ': type' */` |
|    557109 | 1561 | `	pGen->pIn = &pEnd[1];` |
|         - | 1562 | `	{` |
|    557109 | 1563 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|    557109 | 1564 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 | 1565 | `			return SXERR_ABORT;` |
|    557109 | 1566 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|         9 | 1567 | `			return SXERR_SYNTAX;` |
|         - | 1568 | `		}` |
|         - | 1569 | `	}` |
|    557103 | 1570 | `	if( bHandleClosure ){` |
|         - | 1571 | `		ph7_vm_func_closure_env sEnv;` |
|       719 | 1572 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|       714 | 1573 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       429 | 1574 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|       139 | 1575 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - | 1576 | `				/* Closure,record environment variable */` |
|       139 | 1577 | `				pGen->pIn++;` |
|       139 | 1578 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 | 1579 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|       ! 0 | 1580 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 1581 | `						return SXERR_ABORT;` |
|         - | 1582 | `					}` |
|       ! 0 | 1583 | `				}` |
|       139 | 1584 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|         - | 1585 | `				/* Compile until we hit the first closing parenthesis */` |
|       305 | 1586 | `				while( pGen->pIn < pGen->pEnd ){` |
|       305 | 1587 | `					int iFlagsLocal = 0;` |
|       305 | 1588 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|       139 | 1589 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|       139 | 1590 | `						break;` |
|         - | 1591 | `					}` |
|       171 | 1592 | `					nLineLocal = pGen->pIn->nLine;` |
|       171 | 1593 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - | 1594 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|         - | 1595 | `						 * to the variable's memory slot instead of copying its value. */` |
|        87 | 1596 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|        87 | 1597 | `						pGen->pIn++;` |
|        42 | 1598 | `					}` |
|       166 | 1599 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       171 | 1600 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 1601 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|         - | 1602 | `								"Closure: Unexpected token. Expecting a variable name");` |
|       ! 0 | 1603 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 1604 | `								return SXERR_ABORT;` |
|         - | 1605 | `							}` |
|         - | 1606 | `							/* Find the closing parenthesis */` |
|       ! 0 | 1607 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 1608 | `								pGen->pIn++;` |
|       ! 0 | 1609 | `							}` |
|       ! 0 | 1610 | `							if(pGen->pIn < pGen->pEnd){` |
|       ! 0 | 1611 | `								pGen->pIn++;` |
|       ! 0 | 1612 | `							}` |
|       ! 0 | 1613 | `							break;` |
|         - | 1614 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|       ! 0 | 1615 | `					}else{` |
|         - | 1616 | `						SyString *pNameLocal;` |
|         - | 1617 | `						char *zDup;` |
|         - | 1618 | `						/* Duplicate variable name */` |
|       171 | 1619 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       171 | 1620 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       171 | 1621 | `						if( zDup ){` |
|         - | 1622 | `							/* Zero the structure */` |
|       171 | 1623 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       171 | 1624 | `							sEnv.iFlags = iFlagsLocal;` |
|       171 | 1625 | `							sEnv.nLine = nLineLocal; /* the capture's own source line (php warns here) */` |
|       171 | 1626 | `							sEnv.nIdx = SXU32_HIGH;` |
|       171 | 1627 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       171 | 1628 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|       189 | 1629 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|        36 | 1630 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|       ! 0 | 1631 | `									got_this = 1;` |
|       ! 0 | 1632 | `							}` |
|         - | 1633 | `							/* Save imported variable */` |
|       171 | 1634 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        88 | 1635 | `						}else{` |
|       ! 0 | 1636 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1637 | `							 return SXERR_ABORT;` |
|         - | 1638 | `						}` |
|         - | 1639 | `					}` |
|       171 | 1640 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|       205 | 1641 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - | 1642 | `						/* Ignore trailing commas */` |
|        36 | 1643 | `						pGen->pIn++;` |
|         2 | 1644 | `					}` |
|         5 | 1645 | `				}` |
|         - | 1646 | `				/* php 7.1+: the return type follows the use clause —` |
|         - | 1647 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|         - | 1648 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|         - | 1649 | `				 * so an unconditional call would wipe a type parsed at the` |
|         - | 1650 | `				 * legacy pre-use position. */` |
|       139 | 1651 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|         7 | 1652 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|         7 | 1653 | `					if( rcRt2 == SXERR_ABORT ){` |
|       ! 0 | 1654 | `						return SXERR_ABORT;` |
|         7 | 1655 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|       ! 0 | 1656 | `						return SXERR_SYNTAX;` |
|         - | 1657 | `					}` |
|         3 | 1658 | `				}` |
|        67 | 1659 | `		}` |
|       719 | 1660 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|         - | 1661 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|         - | 1662 | `			 * available to the closure environment — for EVERY non-static` |
|         - | 1663 | `			 * anonymous function, use list or not (php binds $this to any` |
|         - | 1664 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|         - | 1665 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|         - | 1666 | `			 * a global-scope closure is silently dropped at install. A static` |
|         - | 1667 | `			 * closure never binds $this (php). */` |
|       677 | 1668 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       677 | 1669 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|       677 | 1670 | `			sEnv.nIdx = SXU32_HIGH;` |
|       677 | 1671 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       677 | 1672 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|       677 | 1673 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       336 | 1674 | `		}` |
|       719 | 1675 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|         - | 1676 | `			/* Mark as closure */` |
|       681 | 1677 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       338 | 1678 | `		}` |
|       357 | 1679 | `	}` |
|         - | 1680 | `	/* Compile the body */` |
|    557103 | 1681 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|    557103 | 1682 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 1683 | `		return SXERR_ABORT;` |
|         - | 1684 | `	}` |
|         - | 1685 | `	/* The cursor sits just past the body's closing brace */` |
|    557103 | 1686 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|    557103 | 1687 | `	if( ppFunc ){` |
|    557103 | 1688 | `		*ppFunc = pFunc;` |
|    278549 | 1689 | `	}` |
|    557103 | 1690 | `	rc = SXRET_OK;` |
|    557103 | 1691 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|         - | 1692 | `		/* Reject a php-fatal redeclaration before hoisting the function */` |
|    556427 | 1693 | `		if( GenStateGuardFuncRedeclaration(pGen,pFunc) == SXERR_ABORT ){` |
|         9 | 1694 | `			return SXERR_ABORT;` |
|         - | 1695 | `		}` |
|         - | 1696 | `		/* Finally register the function */` |
|    556421 | 1697 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    278208 | 1698 | `	}` |
|    557097 | 1699 | `	if( rc == SXRET_OK ){` |
|    557097 | 1700 | `		return SXRET_OK;` |
|         - | 1701 | `	}` |
|         - | 1702 | `	/* Fall through if something goes wrong */` |
|       ! 0 | 1703 | `OutOfMem:` |
|         - | 1704 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - | 1705 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|         - | 1706 | `	 */` |
|       ! 0 | 1707 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 | 1708 | `	return SXERR_ABORT;` |
|    278562 | 1709 | `}` |
|         - | 1710 | `/*` |
|         - | 1711 | ` * Compile a standard PHP function.` |
|         - | 1712 | ` *  Refer to the block-comment above for more information.` |
|         - | 1713 | ` */` |
|    556408 | 1714 | `PH7_PRIVATE sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|         5 | 1715 | `{` |
|         - | 1716 | `	SyString *pName;` |
|         - | 1717 | `	sxi32 iFlags;` |
|         - | 1718 | `	sxu32 nKwLine;` |
|         - | 1719 | `	sxu32 nLine;` |
|         - | 1720 | `	sxi32 rc;` |
|         - | 1721 |  |
|    556413 | 1722 | `	nLine = pGen->pIn->nLine;` |
|    556413 | 1723 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    556413 | 1724 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|    556413 | 1725 | `	iFlags = 0;` |
|    556413 | 1726 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - | 1727 | `		/* Return by reference,remember that */` |
|        12 | 1728 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|         - | 1729 | `		/* Jump the '&' token */` |
|        12 | 1730 | `		pGen->pIn++;` |
|         5 | 1731 | `	}` |
|    556413 | 1732 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - | 1733 | `		/* Invalid function name */` |
|         7 | 1734 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         7 | 1735 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1736 | `			return SXERR_ABORT;` |
|         - | 1737 | `		}` |
|         - | 1738 | `		/* Sychronize with the next semi-colon or braces*/` |
|        21 | 1739 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        15 | 1740 | `			pGen->pIn++;` |
|         1 | 1741 | `		}` |
|         7 | 1742 | `		return SXRET_OK;` |
|         - | 1743 | `	}` |
|    556407 | 1744 | `	pName = &pGen->pIn->sData;` |
|    556407 | 1745 | `	nLine = pGen->pIn->nLine;` |
|         - | 1746 | `	/* Jump the function name */` |
|    556407 | 1747 | `	pGen->pIn++;` |
|    556407 | 1748 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 1749 | `		/* Syntax error */` |
|         3 | 1750 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         3 | 1751 | `		if( rc == SXERR_ABORT ){` |
|         - | 1752 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 1753 | `			return SXERR_ABORT;` |
|         - | 1754 | `		}` |
|         - | 1755 | `		/* Sychronize with the next semi-colon or '{' */` |
|         3 | 1756 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 | 1757 | `			pGen->pIn++;` |
|       ! 0 | 1758 | `		}` |
|         3 | 1759 | `		return SXRET_OK;` |
|         - | 1760 | `	}` |
|         - | 1761 | `	/* Compile function body */` |
|         - | 1762 | `	{` |
|    556405 | 1763 | `		ph7_vm_func *pFuncState = 0;` |
|    556405 | 1764 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|    556405 | 1765 | `		if( pFuncState ){` |
|         - | 1766 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|    556389 | 1767 | `			pFuncState->nLine = nKwLine;` |
|    278192 | 1768 | `		}` |
|         - | 1769 | `	}` |
|    556405 | 1770 | `	return rc;` |
|    278209 | 1771 | `}` |
|         - | 1772 |  |
