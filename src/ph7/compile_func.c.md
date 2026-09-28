# src/ph7/compile_func.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 959/1116 lines (85.93%)

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
|    86644 |   68 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|        5 |   69 | `{` |
|        - |   70 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |   71 | `	SySet *pInstrContainer;` |
|        - |   72 | `	sxi32 rc;` |
|        - |   73 | `	/* Swap token stream */` |
|    86649 |   74 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|    86649 |   75 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    86649 |   76 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|        - |   77 | `	/* Compile the expression holding the argument value. A parameter default is a` |
|        - |   78 | `	 * const-expression belonging to the current class (see iInMemberDefault) — so` |
|        - |   79 | `	 * __TRAIT__ in it reads pCurClass rather than walking into the enclosing method. */` |
|    86649 |   80 | `	pGen->iInMemberDefault++;` |
|    86649 |   81 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    86649 |   82 | `	pGen->iInMemberDefault--;` |
|        - |   83 | `	/* Emit the done instruction */` |
|    86649 |   84 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    86649 |   85 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    86649 |   86 | `	RE_SWAP_DELIMITER(pGen);` |
|    86649 |   87 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |   88 | `		return SXERR_ABORT;` |
|        - |   89 | `	}` |
|    86649 |   90 | `	return SXRET_OK;` |
|    43327 |   91 | `}` |
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
|   154454 |  129 | `PH7_PRIVATE sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|        5 |  130 | `{` |
|        - |  131 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|        - |  132 | `	SyToken *pIn;  /* Token stream */` |
|        - |  133 | `	SyBlob sSig;         /* Function signature */` |
|        - |  134 | `	char *zDup;          /* Copy of argument name */` |
|        - |  135 | `	sxi32 rc;` |
|        - |  136 |  |
|   154459 |  137 | `	pIn = pGen->pIn;` |
|   154459 |  138 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|        - |  139 | `	/* Process arguments one after one */` |
|   221846 |  140 | `	for(;;){` |
|   443697 |  141 | `		if( pIn >= pEnd ){` |
|        - |  142 | `			/* No more arguments to process */` |
|   154441 |  143 | `			break;` |
|        - |  144 | `		}` |
|   289261 |  145 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   289261 |  146 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   289261 |  147 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   289261 |  148 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   289261 |  149 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|        - |  150 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|        - |  151 | `		 * first token inside the main token stream */` |
|   289261 |  152 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  153 | `			return SXERR_ABORT;` |
|        - |  154 | `		}` |
|        - |  155 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|        - |  156 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|        - |  157 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|        - |  158 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|        - |  159 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|        - |  160 | `		{` |
|   289261 |  161 | `			int bReadonly = 0, bVisSeen = 0;` |
|   289261 |  162 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   289261 |  163 | `			sxi32 iSetVisFlag = 0;` |
|        - |  164 | `			int nSetTok;` |
|        - |  165 | `			sxi32 nSetVis;` |
|   289261 |  166 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        3 |  167 | `				bReadonly = 1;` |
|        3 |  168 | `				pIn++;` |
|        1 |  169 | `			}` |
|   289261 |  170 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   289261 |  171 | `			if( nSetVis ){` |
|        - |  172 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|        3 |  173 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|        3 |  174 | `				bVisSeen = 1;` |
|        3 |  175 | `				pIn += nSetTok;` |
|        3 |  176 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|      ! 0 |  177 | `					bReadonly = 1;` |
|      ! 0 |  178 | `					pIn++;` |
|        1 |  179 | `				}` |
|   289260 |  180 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|    46905 |  181 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|    46905 |  182 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      133 |  183 | `					bVisSeen = 1;` |
|      133 |  184 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      174 |  185 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|       55 |  186 | `						: PH7_CLASS_PROT_PUBLIC;` |
|      133 |  187 | `					pIn++;` |
|      133 |  188 | `					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|      133 |  189 | `					if( nSetVis ){` |
|        - |  190 | ``						/* `public private(set) T $x` promoted form */`` |
|        3 |  191 | `						iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|        3 |  192 | `						pIn += nSetTok;` |
|        1 |  193 | `					}` |
|      133 |  194 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       26 |  195 | `						bReadonly = 1;` |
|       26 |  196 | `						pIn++;` |
|       11 |  197 | `					}` |
|       64 |  198 | `				}` |
|    23450 |  199 | `			}` |
|   289261 |  200 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|        5 |  201 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   289259 |  202 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|      ! 0 |  203 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|      ! 0 |  204 | `			}` |
|   289261 |  205 | `			if( bVisSeen \|\| bReadonly ){` |
|      137 |  206 | `				if( !bCtorCtx ){` |
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
|      133 |  219 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      133 |  220 | `				sArg.iPromoteVis = iVis;` |
|      133 |  221 | `				if( bReadonly ){` |
|       28 |  222 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|       12 |  223 | `				}` |
|       64 |  224 | `			}` |
|        - |  225 | `		}` |
|        - |  226 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   289252 |  227 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   177170 |  228 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|    59179 |  229 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    47389 |  230 | `			sxu32 nLineLocal = pIn->nLine;` |
|    47389 |  231 | `			sxi32 iTFlags = 0;` |
|    47389 |  232 | `			pGen->pIn = pIn;` |
|    47389 |  233 | `			rc = GenStateParseUnionTypeDecl(` |
|    23692 |  234 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|    23692 |  235 | `				&iTFlags, &sArg.sTypeName,` |
|        - |  236 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|        - |  237 | `				/* bAllowVoid */ 0,` |
|    23692 |  238 | `						nLineLocal);` |
|    47389 |  239 | `			pIn = pGen->pIn;` |
|    47389 |  240 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  241 | `				return SXERR_ABORT;` |
|    47389 |  242 | `			}else if( rc == SXERR_CORRUPT ){` |
|        - |  243 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|        3 |  244 | `				return SXERR_SYNTAX;` |
|    47387 |  245 | `			}else if( rc == SXERR_SYNTAX ){` |
|       11 |  246 | `				if( pIn < pEnd ){` |
|       15 |  247 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|        - |  248 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|        4 |  249 | `						&pIn->sData);` |
|        7 |  250 | `				}else{` |
|      ! 0 |  251 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|        - |  252 | `						"syntax error, unexpected end of file");` |
|        - |  253 | `				}` |
|       11 |  254 | `				return SXERR_SYNTAX;` |
|        - |  255 | `			}` |
|    47379 |  256 | `			sArg.iFlags \|= iTFlags;` |
|    23687 |  257 | `		}` |
|   289247 |  258 | `		if( pIn >= pEnd ){` |
|      ! 0 |  259 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|      ! 0 |  260 | `			return rc;` |
|        - |  261 | `		}` |
|   289247 |  262 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|        - |  263 | `			/* Pass by reference,record that */` |
|    11861 |  264 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    11861 |  265 | `			pIn++;` |
|     5928 |  266 | `		}` |
|   289247 |  267 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|        - |  268 | `			/* Variadic parameter: ...$args */` |
|     5999 |  269 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|     5999 |  270 | `			pIn++;` |
|     2997 |  271 | `		}` |
|   289247 |  272 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  273 | `			/* Invalid argument */` |
|      ! 0 |  274 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|      ! 0 |  275 | `			return rc;` |
|        - |  276 | `		}` |
|   289247 |  277 | `		pIn++; /* Jump the dollar sign */` |
|        - |  278 | `		/* Copy argument name */` |
|   289247 |  279 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   289247 |  280 | `		if( zDup == 0 ){` |
|      ! 0 |  281 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  282 | `			return SXERR_ABORT;` |
|        - |  283 | `		}` |
|   289247 |  284 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   289247 |  285 | `		pIn++;` |
|   289247 |  286 | `		if( pIn < pEnd ){` |
|   192567 |  287 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|        - |  288 | `				SyToken *pDefend;` |
|    86651 |  289 | `				sxi32 iNest = 0;` |
|    86651 |  290 | `				pIn++; /* Jump the equal sign */` |
|    86651 |  291 | `				pDefend = pIn;` |
|        - |  292 | `				/* Process the default value associated with this argument */` |
|   184897 |  293 | `				while( pDefend < pEnd ){` |
|   127145 |  294 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    28899 |  295 | `						break;` |
|        - |  296 | `					}` |
|    98251 |  297 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|        - |  298 | `						/* Increment nesting level */` |
|       35 |  299 | `						iNest++;` |
|    98236 |  300 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|        - |  301 | `						/* Decrement nesting level */` |
|       35 |  302 | `						iNest--;` |
|       15 |  303 | `					}` |
|    98251 |  304 | `					pDefend++;` |
|        5 |  305 | `				}` |
|    86651 |  306 | `				if( pIn >= pDefend ){` |
|        3 |  307 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|        3 |  308 | `					return rc;` |
|        - |  309 | `				}` |
|        - |  310 | `				/* Process default value */` |
|    86649 |  311 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|    86649 |  312 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  313 | `					return rc;` |
|        - |  314 | `				}` |
|        - |  315 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|        - |  316 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|        - |  317 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|        - |  318 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|        - |  319 | `				 * arg-type check lets null through. */` |
|    86644 |  320 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|    49110 |  321 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|    49104 |  322 | `					&& &pIn[1] == pDefend` |
|    11560 |  323 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|     8652 |  324 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|     2881 |  325 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
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
|    86647 |  348 | `				pIn = pDefend;` |
|    43321 |  349 | `			}` |
|   192563 |  350 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 |  351 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|      ! 0 |  352 | `				return rc;` |
|        - |  353 | `			}` |
|   192563 |  354 | `			pIn++; /* Jump the trailing comma */` |
|    96279 |  355 | `		}` |
|        - |  356 | `		/* Append argument signature */` |
|   289243 |  357 | `		if( sArg.nType > 0 ){` |
|    47241 |  358 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|        - |  359 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|      421 |  360 | `				int marker = 'o';` |
|      421 |  361 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|      421 |  362 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|      213 |  363 | `			}else{` |
|        - |  364 | `				int c;` |
|    46825 |  365 | `				c = 'n'; /* cc warning */` |
|        - |  366 | `				/* Type leading character */` |
|    46825 |  367 | `				switch(sArg.nType){` |
|       58 |  368 | `				case MEMOBJ_HASHMAP:` |
|        - |  369 | `					/* Hashmap aka 'array' */` |
|      121 |  370 | `					c = 'h';` |
|      121 |  371 | `					break;` |
|     5951 |  372 | `				case MEMOBJ_INT:` |
|        - |  373 | `					/* Integer */` |
|    11907 |  374 | `					c = 'i';` |
|    11907 |  375 | `					break;` |
|       17 |  376 | `				case MEMOBJ_BOOL:` |
|        - |  377 | `					/* Bool */` |
|       37 |  378 | `					c = 'b';` |
|       37 |  379 | `					break;` |
|     5752 |  380 | `				case MEMOBJ_REAL:` |
|        - |  381 | `					/* Float */` |
|    11509 |  382 | `					c = 'f';` |
|    11509 |  383 | `					break;` |
|    11618 |  384 | `				case MEMOBJ_STRING:` |
|        - |  385 | `					/* String */` |
|    23241 |  386 | `					c = 's';` |
|    23241 |  387 | `					break;` |
|       13 |  388 | `				case MEMOBJ_OBJ:` |
|        - |  389 | `					/* Object */` |
|       30 |  390 | `					c = 'o';` |
|       26 |  391 | `					break;` |
|        1 |  392 | `				default:` |
|        2 |  393 | `					break;` |
|        - |  394 | `				}` |
|    46825 |  395 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|        - |  396 | `			}` |
|    23623 |  397 | `		}else{` |
|        - |  398 | `			/* No type is associated with this parameter which mean` |
|        - |  399 | `			 * that this function is not condidate for overloading.` |
|        - |  400 | `			 */` |
|   242007 |  401 | `			SyBlobRelease(&sSig);` |
|        - |  402 | `		}` |
|        - |  403 | `		/* php's attribute placement rules, once the promotion modifiers are read:` |
|        - |  404 | `` 		 * a PROMOTED parameter is a property as well, so `#[\Override] public $p` `` |
|        - |  405 | `		 * in a constructor signature is accepted here and judged as the property` |
|        - |  406 | `		 * claim it is -- while php still NAMES the target "parameter". */` |
|   433857 |  407 | `		if( GenStateCheckAttrPlacement(&(*pGen),&sArg.aAttrs,32,` |
|   433862 |  408 | `				(sArg.iFlags & VM_FUNC_ARG_PROMOTED) ? (32\|8) : 32,0,0) == SXERR_ABORT ){` |
|      ! 0 |  409 | `			return SXERR_ABORT;` |
|        - |  410 | `		}` |
|        - |  411 | `		/* Save in the argument set */` |
|   289243 |  412 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|        5 |  413 | `	}` |
|   154441 |  414 | `	if( SyBlobLength(&sSig) > 0 ){` |
|        - |  415 | `		/* Save function signature */` |
|    18211 |  416 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|     9103 |  417 | `	}` |
|   154441 |  418 | `	return SXRET_OK;` |
|    77232 |  419 | `}` |
|        - |  420 | `/*` |
|        - |  421 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|        - |  422 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|        - |  423 | ` * the enclosing function. Returns the token just past the nested construct.` |
|        - |  424 | ` */` |
|      266 |  425 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|        5 |  426 | `{` |
|      271 |  427 | `	sxi32 iParen = 0;` |
|      271 |  428 | `	pIn++; /* past 'function'/'fn' */` |
|        - |  429 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|        - |  430 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|        - |  431 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|     1839 |  432 | `	while( pIn < pEnd ){` |
|     1839 |  433 | `		sxu32 t = pIn->nType;` |
|     1839 |  434 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|     1501 |  435 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|     1163 |  436 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|      897 |  437 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|     1573 |  438 | `		pIn++;` |
|        5 |  439 | `	}` |
|      271 |  440 | `	if( pIn >= pEnd ){ return pIn; }` |
|        - |  441 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|        - |  442 | `	{` |
|      271 |  443 | `		sxi32 d = 0;` |
|     2785 |  444 | `		while( pIn < pEnd ){` |
|     2785 |  445 | `			sxu32 t = pIn->nType;` |
|     2785 |  446 | `			if( t & PH7_TK_OCB ){ d++; }` |
|     2505 |  447 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|     2519 |  448 | `			pIn++;` |
|        5 |  449 | `		}` |
|        - |  450 | `	}` |
|      271 |  451 | `	return pIn;` |
|      138 |  452 | `}` |
|        - |  453 | `/*` |
|        - |  454 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|        - |  455 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|        - |  456 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|        - |  457 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|        - |  458 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|        - |  459 | ` * detached-mini-program path untouched.` |
|        - |  460 | ` */` |
|        - |  461 | `/*` |
|        - |  462 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|        - |  463 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|        - |  464 | ` * mixed, object.` |
|        - |  465 | ` */` |
|       28 |  466 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|        4 |  467 | `{` |
|        - |  468 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|        - |  469 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|        - |  470 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|        - |  471 | `	};` |
|        - |  472 | `	sxu32 i;` |
|       32 |  473 | `	if( nName > 0 && zName[0] == '\\' ){` |
|      ! 0 |  474 | `		zName++;` |
|      ! 0 |  475 | `		nName--;` |
|      ! 0 |  476 | `	}` |
|       46 |  477 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|       46 |  478 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|       32 |  479 | `			return 1;` |
|        - |  480 | `		}` |
|        8 |  481 | `	}` |
|      ! 0 |  482 | `	return 0;` |
|       18 |  483 | `}` |
|        - |  484 | `/*` |
|        - |  485 | ` * One atom of a generator's declared return type: is it a supertype of` |
|        - |  486 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|        - |  487 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|        - |  488 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|        - |  489 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|        - |  490 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|        - |  491 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|        - |  492 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|        - |  493 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|        - |  494 | ` * both rather than fatal on valid code (a recorded divergence).` |
|        - |  495 | ` */` |
|       30 |  496 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|        4 |  497 | `{` |
|       34 |  498 | `	if( nType == MEMOBJ_OBJ ){` |
|      ! 0 |  499 | ``		return 1; /* bare `object` */`` |
|        - |  500 | `	}` |
|       34 |  501 | `	if( nType != SXU32_HIGH ){` |
|        3 |  502 | `		return 0; /* scalar/array/void/never/null/... */` |
|        - |  503 | `	}` |
|       32 |  504 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|       32 |  505 | `		return 1;` |
|        - |  506 | `	}` |
|        - |  507 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|        - |  508 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|        - |  509 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|        - |  510 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|        - |  511 | `	{` |
|        - |  512 | `		SyBlob sFQN;` |
|        - |  513 | `		int bOk;` |
|      ! 0 |  514 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      ! 0 |  515 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|      ! 0 |  516 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|      ! 0 |  517 | `		SyBlobRelease(&sFQN);` |
|      ! 0 |  518 | `		return bOk;` |
|        - |  519 | `	}` |
|       19 |  520 | `}` |
|        - |  521 | `/*` |
|        - |  522 | ` * php 8: a generator function may only declare a return type that is a` |
|        - |  523 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|        - |  524 | ` * group qualifies only if every member does. Anything else is php's exact` |
|        - |  525 | ` * compile-time fatal "Generator return type must be a supertype of` |
|        - |  526 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|        - |  527 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|        - |  528 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|        - |  529 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|        - |  530 | ` */` |
|      360 |  531 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|        5 |  532 | `{` |
|      365 |  533 | `	int bOk = 0;` |
|        - |  534 | `	sxu32 nLine;` |
|        - |  535 | `	sxi32 rc;` |
|      365 |  536 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|      335 |  537 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|        - |  538 | `	}` |
|       34 |  539 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|      ! 0 |  540 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  541 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|        - |  542 | `		sxu32 i,j;` |
|      ! 0 |  543 | `		for( i = 0; i < n && !bOk; i++ ){` |
|        - |  544 | `			int bGroupOk;` |
|      ! 0 |  545 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|      ! 0 |  546 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|        - |  547 | `			}` |
|      ! 0 |  548 | `			bGroupOk = 1;` |
|      ! 0 |  549 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|      ! 0 |  550 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|      ! 0 |  551 | `					bGroupOk = 0;` |
|      ! 0 |  552 | `					break;` |
|        - |  553 | `				}` |
|      ! 0 |  554 | `			}` |
|      ! 0 |  555 | `			bOk = bGroupOk;` |
|      ! 0 |  556 | `		}` |
|      ! 0 |  557 | `	}else{` |
|       34 |  558 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|        - |  559 | `	}` |
|       34 |  560 | `	if( bOk ){` |
|       32 |  561 | `		return SXRET_OK;` |
|        - |  562 | `	}` |
|        - |  563 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|        - |  564 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|        - |  565 | `	 * token of this stream — its line is the function's closing brace. php` |
|        - |  566 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|        - |  567 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|        3 |  568 | `	nLine = pGen->pIn[-1].nLine;` |
|        - |  569 | `	{` |
|        3 |  570 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|        3 |  571 | `		if( sGiven.nByte < 1 ){` |
|      ! 0 |  572 | `			sGiven = pFunc->sReturnClass;` |
|      ! 0 |  573 | `		}` |
|        3 |  574 | `		if( sGiven.nByte < 1 ){` |
|        - |  575 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|        - |  576 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|        - |  577 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|      ! 0 |  578 | `			const char *zScalar =` |
|      ! 0 |  579 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|      ! 0 |  580 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|      ! 0 |  581 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|      ! 0 |  582 | `		}` |
|        3 |  583 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  584 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|        - |  585 | `	}` |
|        3 |  586 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      185 |  587 | `}` |
|   166768 |  588 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|        5 |  589 | `{` |
|   166773 |  590 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|   166773 |  591 | `	SyToken *pEnd = pGen->pEnd;` |
|   166773 |  592 | `	sxi32 iDepth = 0;` |
|   166773 |  593 | `	int bStarted = 0;` |
| 19032147 |  594 | `	while( pIn < pEnd ){` |
| 19032147 |  595 | `		sxu32 t = pIn->nType;` |
| 19032147 |  596 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 18235481 |  597 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 17439313 |  598 | `		if( t & PH7_TK_KEYWORD ){` |
|  1512013 |  599 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  1512013 |  600 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  1511653 |  601 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|        - |  602 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|   755691 |  603 | `		}` |
| 17438687 |  604 | `		pIn++;` |
|        5 |  605 | `	}` |
|   166413 |  606 | `	return FALSE;` |
|    83389 |  607 | `}` |
|        - |  608 | `/*` |
|        - |  609 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|        - |  610 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  611 | ` * and this routine takes care of generating the appropriate error message.` |
|        - |  612 | ` */` |
|   166768 |  613 | `PH7_PRIVATE sxi32 GenStateCompileFuncBody(` |
|        - |  614 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |  615 | `	ph7_vm_func *pFunc    /* Function state */` |
|        - |  616 | `	)` |
|        5 |  617 | `{` |
|        - |  618 | `	SySet *pInstrContainer; /* Instruction container */` |
|        - |  619 | `	GenBlock *pBlock;` |
|        - |  620 | `	sxu32 nGotoOfft;` |
|        - |  621 | `	sxi32 rc;` |
|        - |  622 | `	/* Attach the new function */` |
|   166773 |  623 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   166773 |  624 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  625 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|        - |  626 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  627 | `		return SXERR_ABORT;` |
|        - |  628 | `	}` |
|   166773 |  629 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|        - |  630 | `	/* Swap bytecode containers */` |
|   166773 |  631 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   166773 |  632 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|        - |  633 | `	/* Emit constructor property promotion prologue:` |
|        - |  634 | `	 *   $this->NAME = $NAME;` |
|        - |  635 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|        - |  636 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|        - |  637 | `	{` |
|   166773 |  638 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|        - |  639 | `		sxu32 i;` |
|   455329 |  640 | `		for( i = 0; i < nArg; i++ ){` |
|   288561 |  641 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|        - |  642 | `			char *zSrc;` |
|        - |  643 | `			sxu32 nSrc,nName;` |
|        - |  644 | `			SySet sToken;` |
|        - |  645 | `			SyToken *pTmpIn,*pTmpEnd;` |
|        - |  646 | `			sxi32 rcPromote;` |
|   288561 |  647 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   288443 |  648 | `				continue;` |
|        - |  649 | `			}` |
|        - |  650 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|        - |  651 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|        - |  652 | `			 * copied), so it must outlive the function — never free it. The` |
|        - |  653 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|        - |  654 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      123 |  655 | `			nName = SyStringLength(&pArg->sName);` |
|      123 |  656 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      123 |  657 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      123 |  658 | `			if( zSrc == 0 ){` |
|      ! 0 |  659 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  660 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  661 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  662 | `				return SXERR_ABORT;` |
|        - |  663 | `			}` |
|        - |  664 | `			{` |
|      123 |  665 | `				char *z = zSrc;` |
|      123 |  666 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      123 |  667 | `				z += sizeof("$this->")-1;` |
|      123 |  668 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      123 |  669 | `				z += nName;` |
|      123 |  670 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      123 |  671 | `				z += sizeof(" = $")-1;` |
|      123 |  672 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      123 |  673 | `				z += nName;` |
|      123 |  674 | `				*z = 0;` |
|        - |  675 | `			}` |
|      123 |  676 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      123 |  677 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|      123 |  678 | `			pTmpIn = pGen->pIn;` |
|      123 |  679 | `			pTmpEnd = pGen->pEnd;` |
|      123 |  680 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      123 |  681 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      123 |  682 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      123 |  683 | `			pGen->pIn = pTmpIn;` |
|      123 |  684 | `			pGen->pEnd = pTmpEnd;` |
|      123 |  685 | `			SySetRelease(&sToken);` |
|      123 |  686 | `			if( rcPromote == SXERR_ABORT ){` |
|      ! 0 |  687 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  688 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  689 | `				return SXERR_ABORT;` |
|        - |  690 | `			}` |
|        - |  691 | `			/* Discard the assignment result — this is a statement expression. */` |
|      123 |  692 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       64 |  693 | `		}` |
|        - |  694 | `	}` |
|        - |  695 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|        - |  696 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|        - |  697 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|        - |  698 | `	 * generator — and vice versa — is classified independently. */` |
|        - |  699 | `	{` |
|   166773 |  700 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|   166773 |  701 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|        - |  702 | `		/* Compile the body */` |
|   166773 |  703 | `		PH7_CompileBlock(&(*pGen),0);` |
|   166773 |  704 | `		pGen->bInGenerator = bSavedGen;` |
|        - |  705 | `	}` |
|        - |  706 | `	/* Fix exception jumps now the destination is resolved */` |
|   166773 |  707 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - |  708 | `	/* Emit the final return if not yet done */` |
|   166773 |  709 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - |  710 | `	/* Fix gotos jumps now the destination is resolved */` |
|   166773 |  711 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|      ! 0 |  712 | `		rc = SXERR_ABORT;` |
|      ! 0 |  713 | `	}` |
|   166773 |  714 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|        - |  715 | `	/* Restore the default container */` |
|   166773 |  716 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - |  717 | `	/* Leave function block */` |
|   166773 |  718 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   166773 |  719 | `	if( rc == SXERR_ABORT ){` |
|        - |  720 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  721 | `		return SXERR_ABORT;` |
|        - |  722 | `	}` |
|        - |  723 | `	/* Scan for yield opcodes to detect generator functions */` |
|        - |  724 | `	{` |
|   166773 |  725 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|        - |  726 | `		sxu32 i;` |
| 12139763 |  727 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 11973355 |  728 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|      365 |  729 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      365 |  730 | `				break;` |
|        - |  731 | `			}` |
|  5986500 |  732 | `		}` |
|        - |  733 | `	}` |
|   166773 |  734 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  735 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|      365 |  736 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|      ! 0 |  737 | `			return SXERR_ABORT;` |
|        - |  738 | `		}` |
|      180 |  739 | `	}` |
|        - |  740 | `	/* All done, function body compiled */` |
|   166773 |  741 | `	return SXRET_OK;` |
|    83389 |  742 | `}` |
|        - |  743 | `/*` |
|        - |  744 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|        - |  745 | ` * According to the PHP language reference manual.` |
|        - |  746 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|        - |  747 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|        - |  748 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|        - |  749 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - |  750 | ` *  Functions need not be defined before they are referenced.` |
|        - |  751 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|        - |  752 | ` *  a function even if they were defined inside and vice versa.` |
|        - |  753 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|        - |  754 | ` *  calls with over 32-64 recursion levels.` |
|        - |  755 | ` *` |
|        - |  756 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|        - |  757 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|        - |  758 | ` * on these extension.` |
|        - |  759 | ` */` |
|        - |  760 | `/*` |
|        - |  761 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|        - |  762 | ` */` |
|     1366 |  763 | `PH7_PRIVATE int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|        5 |  764 | `{` |
|        - |  765 | `	sxu32 i;` |
|     3663 |  766 | `	for( i = 0; i < n; i++ ){` |
|     3125 |  767 | `		int a = zA[i], b = zB[i];` |
|     3125 |  768 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     3125 |  769 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     3125 |  770 | `		if( a != b ) return a - b;` |
|     1151 |  771 | `	}` |
|      543 |  772 | `	return 0;` |
|      688 |  773 | `}` |
|        - |  774 | `/*` |
|        - |  775 | ` * Internal type-atom kinds used during union type parsing.` |
|        - |  776 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|        - |  777 | ` * (which are positive bit values stored in sxu32).` |
|        - |  778 | ` */` |
|        - |  779 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|        - |  780 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|        - |  781 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|        - |  782 |  |
|        - |  783 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|        - |  784 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|        - |  785 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|        - |  786 |  |
|        - |  787 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|        - |  788 | `struct PhlTypeAtom {` |
|        - |  789 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|        - |  790 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|        - |  791 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|        - |  792 | `	sxu32 nCanon;` |
|        - |  793 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|        - |  794 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|        - |  795 | `};` |
|        - |  796 |  |
|        - |  797 | `/*` |
|        - |  798 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|        - |  799 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|        - |  800 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|        - |  801 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|        - |  802 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|        - |  803 | ` * already be consumed by the caller.` |
|        - |  804 | ` */` |
|        - |  805 | `/*` |
|        - |  806 | ` * TRUE if pName is a reserved PHP type keyword (never a class name), so a type` |
|        - |  807 | ` * hint that spells it must not be namespace-qualified. Only the words that can` |
|        - |  808 | ` * reach GenStateParseOneTypeAtom's identifier branch as a bare name matter here` |
|        - |  809 | ` * (bool/int/float/string/array/object/self/static/parent arrive as keywords, and` |
|        - |  810 | ` * null/void/never are matched before the class path), but the full set is listed` |
|        - |  811 | ` * so the guard is robust to lexer changes.` |
|        - |  812 | ` */` |
|      872 |  813 | `static int GenStateIsReservedTypeWord(const SyString *pName)` |
|        5 |  814 | `{` |
|        - |  815 | `	static const char *azWords[] = {` |
|        - |  816 | `		"false","true","mixed","iterable","callable","null","void","never",` |
|        - |  817 | `		"bool","boolean","int","integer","float","double","string","array",` |
|        - |  818 | `		"object","self","static","parent"` |
|        - |  819 | `	};` |
|        - |  820 | `	sxu32 i;` |
|    10311 |  821 | `	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){` |
|     9905 |  822 | `		sxu32 n = (sxu32)SyStrlen(azWords[i]);` |
|     9905 |  823 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azWords[i],n) == 0 ){` |
|      471 |  824 | `			return 1;` |
|        - |  825 | `		}` |
|     4722 |  826 | `	}` |
|      411 |  827 | `	return 0;` |
|      441 |  828 | `}` |
|    56020 |  829 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|        5 |  830 | `{` |
|    56025 |  831 | `	SyToken *pIn = pGen->pIn;` |
|    56025 |  832 | `	int bAbsolute = 0;` |
|    56025 |  833 | `	SyZero(pOut, sizeof(*pOut));` |
|    56025 |  834 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    56025 |  835 | `	if( pIn >= pGen->pEnd ){` |
|      ! 0 |  836 | `		return SXERR_SYNTAX;` |
|        - |  837 | `	}` |
|        - |  838 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    56025 |  839 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       13 |  840 | `		bAbsolute = 1; /* fully-qualified: never prefix the current namespace */` |
|       13 |  841 | `		pIn++;` |
|       13 |  842 | `		if( pIn >= pGen->pEnd ){` |
|      ! 0 |  843 | `			return SXERR_SYNTAX;` |
|        - |  844 | `		}` |
|        5 |  845 | `	}` |
|        - |  846 | ``	/* `namespace\X` type hint: the CURRENT namespace spelled out, fully qualified`` |
|        - |  847 | `` 	 * from there. Collected here rather than below because the leading `namespace` `` |
|        - |  848 | `	 * is a KEYWORD token, which the atom parser would otherwise reject outright. */` |
|    56025 |  849 | `	if( !bAbsolute ){` |
|        - |  850 | `		SyBlob sRel;` |
|    56015 |  851 | `		SyBlobInit(&sRel,&pGen->pVm->sAllocator);` |
|    56015 |  852 | `		if( GenStateNsRelPrefix(pGen,&pIn,pGen->pEnd,&sRel) ){` |
|        - |  853 | `			char *zDup;` |
|        7 |  854 | `			SyBlobAppend(&sRel,pIn->sData.zString,pIn->sData.nByte);` |
|        7 |  855 | `			pIn++;` |
|        9 |  856 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|        7 |  857 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|      ! 0 |  858 | `				SyBlobAppend(&sRel,"\\",1);` |
|      ! 0 |  859 | `				SyBlobAppend(&sRel,pIn[1].sData.zString,pIn[1].sData.nByte);` |
|      ! 0 |  860 | `				pIn += 2;` |
|      ! 0 |  861 | `			}` |
|       10 |  862 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        6 |  863 | `				(const char *)SyBlobData(&sRel),SyBlobLength(&sRel));` |
|        7 |  864 | `			if( zDup == 0 ){` |
|      ! 0 |  865 | `				SyBlobRelease(&sRel);` |
|      ! 0 |  866 | `				return SXERR_ABORT;` |
|        - |  867 | `			}` |
|        7 |  868 | `			pOut->nType = SXU32_HIGH;` |
|        7 |  869 | `			SyStringInitFromBuf(&pOut->sClass,zDup,SyBlobLength(&sRel));` |
|        7 |  870 | `			SyBlobRelease(&sRel);` |
|        7 |  871 | `			pGen->pIn = pIn;` |
|        7 |  872 | `			return SXRET_OK;` |
|        - |  873 | `		}` |
|    56009 |  874 | `		SyBlobRelease(&sRel);` |
|    28002 |  875 | `	}` |
|    56019 |  876 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  877 | `		return SXERR_SYNTAX;` |
|        - |  878 | `	}` |
|    56019 |  879 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|    54603 |  880 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|    54603 |  881 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      265 |  882 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|    54473 |  883 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      245 |  884 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|    54223 |  885 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|    12847 |  886 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|    47682 |  887 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|    23819 |  888 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|    29354 |  889 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|    17297 |  890 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|     8801 |  891 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|       45 |  892 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      135 |  893 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       48 |  894 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|      112 |  895 | `			pOut->nType = SXU32_HIGH;` |
|      112 |  896 | `			pOut->sClass = pIn->sData;` |
|       58 |  897 | `		}else{` |
|        3 |  898 | `			return SXERR_SYNTAX;` |
|        - |  899 | `		}` |
|    54601 |  900 | `		pIn++;` |
|    27303 |  901 | `	}else{` |
|        - |  902 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|        - |  903 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|     1421 |  904 | `		SyString *pT = &pIn->sData;` |
|     1421 |  905 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|       36 |  906 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|       36 |  907 | `			pIn++;` |
|     1405 |  908 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|      473 |  909 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|      473 |  910 | `			pIn++;` |
|     1155 |  911 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       37 |  912 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       37 |  913 | `			pIn++;` |
|       21 |  914 | `		}else{` |
|        - |  915 | `			/* Class / interface name; consume namespace path a\b\c */` |
|      889 |  916 | `			SyToken *pFirst = pIn;` |
|      889 |  917 | `			SyToken *pLast = pIn;` |
|      889 |  918 | `			pOut->nType = SXU32_HIGH;` |
|      889 |  919 | `			pOut->sClass = pIn->sData;` |
|      889 |  920 | `			pIn++;` |
|     1332 |  921 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|      895 |  922 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|        5 |  923 | `				pLast = &pIn[1];` |
|        5 |  924 | `				pIn += 2;` |
|        1 |  925 | `			}` |
|      889 |  926 | `			if( pLast != pFirst ){` |
|        5 |  927 | `				const char *zFirst = pFirst->sData.zString;` |
|        5 |  928 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|        5 |  929 | `				pOut->sClass.zString = zFirst;` |
|        5 |  930 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|        2 |  931 | `			}` |
|        - |  932 | `			/* Namespace-qualify a bare (single-segment, non-absolute) class type so` |
|        - |  933 | ``			 * a `: Base` / `Base $x` hint in namespace N resolves to N\Base (or a`` |
|        - |  934 | ``			 * `use` alias) at type-check time instead of the global \Base — mirrors`` |
|        - |  935 | `			 * the NEW/CALL/instanceof qualification. Absolute (\Base) and already-` |
|        - |  936 | `			 * qualified (A\B) names are left as written, matching GenStateNsQualifyName. */` |
|        - |  937 | `			/* Reserved type words that reach this identifier branch (false, true,` |
|        - |  938 | `			 * mixed, iterable, callable) are NOT classes and must not be qualified` |
|        - |  939 | ``			 * (else `false\|string` becomes `Ns\false\|string`). */`` |
|      889 |  940 | `			if( !bAbsolute && pLast == pFirst && !GenStateIsReservedTypeWord(&pOut->sClass) ){` |
|        - |  941 | `				SyBlob sFqn;` |
|      411 |  942 | `				SyBlobInit(&sFqn,&pGen->pVm->sAllocator);` |
|      411 |  943 | `				GenStateResolveName(pGen,&pOut->sClass,&sFqn);` |
|      406 |  944 | `				if( SyBlobLength(&sFqn) != pOut->sClass.nByte` |
|      404 |  945 | `				 \|\| SyMemcmp(SyBlobData(&sFqn),(const void *)pOut->sClass.zString,pOut->sClass.nByte) != 0 ){` |
|       24 |  946 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       14 |  947 | `						(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));` |
|       17 |  948 | `					if( zDup ){` |
|       17 |  949 | `						SyStringInitFromBuf(&pOut->sClass,zDup,SyBlobLength(&sFqn));` |
|        7 |  950 | `					}` |
|        7 |  951 | `				}` |
|      411 |  952 | `				SyBlobRelease(&sFqn);` |
|      203 |  953 | `			}` |
|        - |  954 | `		}` |
|        - |  955 | `	}` |
|    56017 |  956 | `	pGen->pIn = pIn;` |
|    56017 |  957 | `	return SXRET_OK;` |
|    28015 |  958 | `}` |
|        - |  959 |  |
|        - |  960 | `/* Class-name ATOMS that php files with the BUILT-IN types rather than the` |
|        - |  961 | `` * classes. `callable`, `false` and `true` are type-mask bits in zend, so they`` |
|        - |  962 | `` * print AFTER every class name however they were written (`false\|A1` reads`` |
|        - |  963 | `` * `A1\|false`). `iterable` is two types at once: the class `Traversable`, which`` |
|        - |  964 | `` * keeps the atom's declaration position among the classes, plus `array`, which`` |
|        - |  965 | `` * prints with the builtins (`iterable\|A1` reads `Traversable\|A1\|array`). PH7`` |
|        - |  966 | ` * parses all four as class-name atoms, which is why they used to sort with the` |
|        - |  967 | ` * classes. */` |
|        - |  968 | `#define GEN_ATOM_PLAIN    0` |
|        - |  969 | `#define GEN_ATOM_CALLABLE 1` |
|        - |  970 | `#define GEN_ATOM_FALSE    2` |
|        - |  971 | `#define GEN_ATOM_TRUE     3` |
|        - |  972 | `#define GEN_ATOM_ITERABLE 4` |
|   223562 |  973 | `static int GenAtomMaskKind(const PhlTypeAtom *pAtom)` |
|        5 |  974 | `{` |
|   223567 |  975 | `	const SyString *p = &pAtom->sClass;` |
|   223567 |  976 | `	if( pAtom->nType != SXU32_HIGH \|\| p->zString == 0 ){` |
|   219059 |  977 | `		return GEN_ATOM_PLAIN;` |
|        - |  978 | `	}` |
|     4513 |  979 | `	if( p->nByte == 8 && SyStrnicmp(p->zString,"callable",8) == 0 ) return GEN_ATOM_CALLABLE;` |
|     3495 |  980 | `	if( p->nByte == 5 && SyStrnicmp(p->zString,"false",5) == 0 )    return GEN_ATOM_FALSE;` |
|     3333 |  981 | `	if( p->nByte == 4 && SyStrnicmp(p->zString,"true",4) == 0 )     return GEN_ATOM_TRUE;` |
|     3273 |  982 | `	if( p->nByte == 8 && SyStrnicmp(p->zString,"iterable",8) == 0 ) return GEN_ATOM_ITERABLE;` |
|     3049 |  983 | `	return GEN_ATOM_PLAIN;` |
|   111786 |  984 | `}` |
|        - |  985 | ``/* Emit one class-like atom: when *bExpandIterable*, `iterable` contributes only`` |
|        - |  986 | `` * its Traversable half here (the `array` half rides the built-in pass). php`` |
|        - |  987 | `` * expands it only in a COMPOUND type — a standalone `iterable`/`?iterable` keeps`` |
|        - |  988 | ` * its name in the canonical text (which is what Reflection prints; the TypeError` |
|        - |  989 | ` * for a standalone hint is rendered separately, by VmClassHintTypeName). */` |
|      678 |  990 | `static void GenAppendClassAtom(SyBlob *pBlob, const PhlTypeAtom *pAtom, int bExpandIterable)` |
|        5 |  991 | `{` |
|      683 |  992 | `	if( bExpandIterable && GenAtomMaskKind(pAtom) == GEN_ATOM_ITERABLE ){` |
|       15 |  993 | `		SyBlobAppend(pBlob, "Traversable", sizeof("Traversable")-1);` |
|       15 |  994 | `		return;` |
|        - |  995 | `	}` |
|      669 |  996 | `	SyBlobAppend(pBlob, pAtom->sClass.zString, pAtom->sClass.nByte);` |
|      344 |  997 | `}` |
|        - |  998 | `/*` |
|        - |  999 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|        - | 1000 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|        - | 1001 | ` *   classes (in declaration order)` |
|        - | 1002 | ` *   \| callable \| object \| array \| string \| int \| float \| bool \| false \| true` |
|        - | 1003 | ` *   [\| null]` |
|        - | 1004 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|        - | 1005 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|        - | 1006 | ` */` |
|    55710 | 1007 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|        5 | 1008 | `{` |
|        - | 1009 | `	int i;` |
|    55715 | 1010 | `	int nNonNull = 0;` |
|    55715 | 1011 | `	int bAnyIntersection = 0;` |
|        - | 1012 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    55715 | 1013 | `	sxu32 nMaxGroup = 0;` |
|  1838435 | 1014 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   111709 | 1015 | `	for( i = 0; i < nAtoms; i++ ){` |
|    55999 | 1016 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    55967 | 1017 | `			nNonNull++;` |
|    55967 | 1018 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    55967 | 1019 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    55967 | 1020 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|    27981 | 1021 | `			}` |
|    27981 | 1022 | `		}` |
|    28002 | 1023 | `	}` |
|   111637 | 1024 | `	for( i = 0; i < nAtoms; i++ ){` |
|    55959 | 1025 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       37 | 1026 | `			bAnyIntersection = 1;` |
|       37 | 1027 | `			break;` |
|        - | 1028 | `		}` |
|    27966 | 1029 | `	}` |
|    55715 | 1030 | `	if( bAnyIntersection ){` |
|        - | 1031 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|        - | 1032 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|        - | 1033 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|       37 | 1034 | `		sxu32 g, nGroups = 0;` |
|       37 | 1035 | `		int bFirstGroup = 1;` |
|       79 | 1036 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|       79 | 1037 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|       47 | 1038 | `			int bFirstMember = 1;` |
|        - | 1039 | `			int bWrap;` |
|       47 | 1040 | `			if( aGroupCount[g] == 0 ) continue;` |
|        - | 1041 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|        - | 1042 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|        - | 1043 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|        - | 1044 | `			 * parens, matching PHP's canonical text. */` |
|       63 | 1045 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|       47 | 1046 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|       47 | 1047 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      151 | 1048 | `			for( i = 0; i < nAtoms; i++ ){` |
|      109 | 1049 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|       79 | 1050 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|       79 | 1051 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       75 | 1052 | `					GenAppendClassAtom(pBlob, &aAtoms[i], 1);` |
|       40 | 1053 | `				}else{` |
|        6 | 1054 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - | 1055 | `				}` |
|       79 | 1056 | `				bFirstMember = 0;` |
|       42 | 1057 | `			}` |
|       47 | 1058 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|       47 | 1059 | `			bFirstGroup = 0;` |
|       26 | 1060 | `		}` |
|        - | 1061 | ``		/* `iterable` printed its Traversable half in place; its `array` half goes`` |
|        - | 1062 | ``		 * last, as php does (`(I1&I2)\|iterable` reads `(I1&I2)\|Traversable\|array`). */`` |
|      109 | 1063 | `		for( i = 0; i < nAtoms; i++ ){` |
|       79 | 1064 | `			if( GenAtomMaskKind(&aAtoms[i]) == GEN_ATOM_ITERABLE ){` |
|        3 | 1065 | `				SyBlobAppend(pBlob, "\|", 1);` |
|        3 | 1066 | `				SyBlobAppend(pBlob, "array", sizeof("array")-1);` |
|        3 | 1067 | `				break;` |
|        - | 1068 | `			}` |
|       41 | 1069 | `		}` |
|       37 | 1070 | `		if( bNullable ){` |
|      ! 0 | 1071 | `			SyBlobAppend(pBlob, "\|", 1);` |
|      ! 0 | 1072 | `			SyBlobAppend(pBlob, "null", 4);` |
|      ! 0 | 1073 | `		}` |
|      146 | 1074 | `		return;` |
|        - | 1075 | `	}` |
|    55683 | 1076 | `	if( nNonNull == 1 && bNullable ){` |
|        - | 1077 | `		/* Shorthand: ?T */` |
|      223 | 1078 | `		for( i = 0; i < nAtoms; i++ ){` |
|      223 | 1079 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      223 | 1080 | `			SyBlobAppend(pBlob, "?", 1);` |
|      223 | 1081 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|       77 | 1082 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       41 | 1083 | `			}else{` |
|      150 | 1084 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - | 1085 | `			}` |
|      223 | 1086 | `			return;` |
|      ! 0 | 1087 | `		}` |
|      ! 0 | 1088 | `	}` |
|        - | 1089 | `	{` |
|    55465 | 1090 | `		int bFirst = 1;` |
|        - | 1091 | `		/* 1) Classes in declaration order — minus the class-name atoms php counts` |
|        - | 1092 | ``		 * as built-in types (see GenAtomMaskKind); `iterable` leaves Traversable. */`` |
|   111153 | 1093 | `		for( i = 0; i < nAtoms; i++ ){` |
|        - | 1094 | `			int nKind;` |
|    55693 | 1095 | `			if( aAtoms[i].nType != SXU32_HIGH ) continue;` |
|      861 | 1096 | `			nKind = GenAtomMaskKind(&aAtoms[i]);` |
|      861 | 1097 | `			if( nKind == GEN_ATOM_CALLABLE \|\| nKind == GEN_ATOM_FALSE \|\| nKind == GEN_ATOM_TRUE ){` |
|      253 | 1098 | `				continue;` |
|        - | 1099 | `			}` |
|      613 | 1100 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|      613 | 1101 | `			GenAppendClassAtom(pBlob, &aAtoms[i], nNonNull > 1);` |
|      613 | 1102 | `			bFirst = 0;` |
|      309 | 1103 | `		}` |
|        - | 1104 | `		/* 2) Built-ins in php's canonical order. A slot is filled either by a` |
|        - | 1105 | `		 * plain atom of that MEMOBJ_* type or by a class-name atom of the matching` |
|        - | 1106 | ``		 * kind; the `array` slot also takes `iterable`'s second half. */`` |
|        - | 1107 | `		{` |
|        - | 1108 | `			static const struct {` |
|        - | 1109 | `				sxu32 nType;        /* plain atom type, 0 when kind-only */` |
|        - | 1110 | `				int nKind;          /* GEN_ATOM_* atom, GEN_ATOM_PLAIN when type-only */` |
|        - | 1111 | `				const char *zText;` |
|        - | 1112 | `				sxu32 nText;` |
|        - | 1113 | `			} aOrder[] = {` |
|        - | 1114 | `				{ 0,               GEN_ATOM_CALLABLE, "callable", sizeof("callable")-1 },` |
|        - | 1115 | `				{ MEMOBJ_OBJ,      GEN_ATOM_PLAIN,    "object",   sizeof("object")-1 },` |
|        - | 1116 | `				{ MEMOBJ_HASHMAP,  GEN_ATOM_ITERABLE, "array",    sizeof("array")-1 },` |
|        - | 1117 | `				{ MEMOBJ_STRING,   GEN_ATOM_PLAIN,    "string",   sizeof("string")-1 },` |
|        - | 1118 | `				{ MEMOBJ_INT,      GEN_ATOM_PLAIN,    "int",      sizeof("int")-1 },` |
|        - | 1119 | `				{ MEMOBJ_REAL,     GEN_ATOM_PLAIN,    "float",    sizeof("float")-1 },` |
|        - | 1120 | `				{ MEMOBJ_BOOL,     GEN_ATOM_PLAIN,    "bool",     sizeof("bool")-1 },` |
|        - | 1121 | `				{ 0,               GEN_ATOM_FALSE,    "false",    sizeof("false")-1 },` |
|        - | 1122 | `				{ 0,               GEN_ATOM_TRUE,     "true",     sizeof("true")-1 }` |
|        - | 1123 | `			};` |
|        - | 1124 | `			int k;` |
|   554605 | 1125 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|   945547 | 1126 | `				for( i = 0; i < nAtoms; i++ ){` |
|   640799 | 1127 | `					int bHit = (aOrder[k].nType != 0 && aAtoms[i].nType == aOrder[k].nType)` |
|   890419 | 1128 | `						\|\| (aOrder[k].nKind != GEN_ATOM_PLAIN` |
|   334563 | 1129 | `						    && GenAtomMaskKind(&aAtoms[i]) == aOrder[k].nKind` |
|   111377 | 1130 | `						    && (aOrder[k].nKind != GEN_ATOM_ITERABLE \|\| nNonNull > 1));` |
|   500989 | 1131 | `					if( !bHit ) continue;` |
|    54587 | 1132 | `					if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    54587 | 1133 | `					SyBlobAppend(pBlob, aOrder[k].zText, aOrder[k].nText);` |
|    54587 | 1134 | `					bFirst = 0;` |
|    54587 | 1135 | `					break;` |
|      ! 0 | 1136 | `				}` |
|   249575 | 1137 | `			}` |
|        - | 1138 | `		}` |
|        - | 1139 | `		/* 3) null suffix */` |
|    55465 | 1140 | `		if( bNullable ){` |
|       22 | 1141 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|       22 | 1142 | `			SyBlobAppend(pBlob, "null", 4);` |
|        9 | 1143 | `		}` |
|        - | 1144 | `	}` |
|    27860 | 1145 | `}` |
|        - | 1146 |  |
|        - | 1147 | `/*` |
|        - | 1148 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|        - | 1149 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|        - | 1150 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|        - | 1151 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|        - | 1152 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|        - | 1153 | ` * whether it was parenthesized.` |
|        - | 1154 | ` *` |
|        - | 1155 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|        - | 1156 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|        - | 1157 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|        - | 1158 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|        - | 1159 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|        - | 1160 | ` */` |
|    55986 | 1161 | `static sxi32 GenStateParsePart(` |
|        - | 1162 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|        - | 1163 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|        5 | 1164 | `{` |
|        - | 1165 | `	sxi32 rc;` |
|    55991 | 1166 | `	int nMembers = 0;` |
|    55991 | 1167 | `	int bParen = 0;` |
|    55991 | 1168 | `	*pnMembers = 0;` |
|    55991 | 1169 | `	*pbParen = 0;` |
|    55991 | 1170 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       13 | 1171 | `		bParen = 1;` |
|       13 | 1172 | `		pGen->pIn++; /* skip '(' */` |
|        5 | 1173 | `	}` |
|    27993 | 1174 | `	for(;;){` |
|    56025 | 1175 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|      ! 0 | 1176 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1177 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|      ! 0 | 1178 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1179 | `		}` |
|    56025 | 1180 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    56025 | 1181 | `		if( rc != SXRET_OK ){` |
|        3 | 1182 | `			return rc;` |
|        - | 1183 | `		}` |
|    56023 | 1184 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    56023 | 1185 | `		(*pnAtoms)++;` |
|    56023 | 1186 | `		nMembers++;` |
|        - | 1187 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    56023 | 1188 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       67 | 1189 | `			SyToken *pNext = &pGen->pIn[1];` |
|       62 | 1190 | `			if( pNext < pGen->pEnd` |
|       67 | 1191 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       39 | 1192 | `				pGen->pIn++; /* skip '&' */` |
|       39 | 1193 | `				continue;` |
|        - | 1194 | `			}` |
|       14 | 1195 | `		}` |
|    55989 | 1196 | `		break;` |
|      ! 0 | 1197 | `	}` |
|    55989 | 1198 | `	if( bParen ){` |
|       13 | 1199 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 1200 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1201 | `				"Malformed DNF type: expecting ')'");` |
|      ! 0 | 1202 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1203 | `		}` |
|       13 | 1204 | `		pGen->pIn++; /* skip ')' */` |
|       13 | 1205 | `		if( nMembers < 2 ){` |
|      ! 0 | 1206 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1207 | `				"Parenthesized type must be an intersection of at least two types");` |
|      ! 0 | 1208 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1209 | `		}` |
|        5 | 1210 | `	}` |
|    55989 | 1211 | `	*pnMembers = nMembers;` |
|    55989 | 1212 | `	*pbParen = bParen;` |
|    55989 | 1213 | `	return SXRET_OK;` |
|    27998 | 1214 | `}` |
|        - | 1215 |  |
|        - | 1216 | `/*` |
|        - | 1217 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|        - | 1218 | ` *` |
|        - | 1219 | ` * Outputs:` |
|        - | 1220 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|        - | 1221 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|        - | 1222 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|        - | 1223 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|        - | 1224 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|        - | 1225 | ` *     already be initialized by the caller (allocator set, etc).` |
|        - | 1226 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|        - | 1227 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|        - | 1228 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|        - | 1229 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|        - | 1230 | ` *` |
|        - | 1231 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|        - | 1232 | ` * SXERR_ABORT on fatal compile errors.` |
|        - | 1233 | ` */` |
|    55726 | 1234 | `PH7_PRIVATE sxi32 GenStateParseUnionTypeDecl(` |
|        - | 1235 | `	ph7_gen_state *pGen,` |
|        - | 1236 | `	sxu32 *pnType,` |
|        - | 1237 | `	SyString *pClass,` |
|        - | 1238 | `	SySet *pAlts,` |
|        - | 1239 | `	sxi32 *piTypeFlags,` |
|        - | 1240 | `	SyString *pTypeText,` |
|        - | 1241 | `	int iNullableFlag,` |
|        - | 1242 | `	int iUnionFlag,` |
|        - | 1243 | `	int bAllowVoid,` |
|        - | 1244 | `	sxu32 nLine` |
|        5 | 1245 | `){` |
|        - | 1246 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    55731 | 1247 | `	int nAtoms = 0;` |
|    55731 | 1248 | `	int bShortNullable = 0;` |
|    55731 | 1249 | `	int bExplicitNull = 0;` |
|        - | 1250 | `	sxi32 rc;` |
|    55731 | 1251 | `	*pnType = 0;` |
|    55731 | 1252 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    55731 | 1253 | `	*piTypeFlags = 0;` |
|    55731 | 1254 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|        - | 1255 |  |
|    55731 | 1256 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 1257 | `		return SXRET_OK;` |
|        - | 1258 | `	}` |
|        - | 1259 | ``	/* Optional `?` shorthand prefix */`` |
|    55726 | 1260 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      211 | 1261 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      211 | 1262 | `		bShortNullable = 1;` |
|      211 | 1263 | `		pGen->pIn++;` |
|      211 | 1264 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 1265 | `			return SXERR_SYNTAX;` |
|        - | 1266 | `		}` |
|      103 | 1267 | `	}` |
|        - | 1268 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|        - | 1269 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|        - | 1270 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|        - | 1271 | `	{` |
|        - | 1272 | `		int nMembers, bParen;` |
|    55731 | 1273 | `		sxu32 iGroup = 0;` |
|    55731 | 1274 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    55731 | 1275 | `		if( rc != SXRET_OK ){` |
|        4 | 1276 | `			return rc;` |
|        - | 1277 | `		}` |
|        - | 1278 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|        - | 1279 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|        - | 1280 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|        - | 1281 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|        - | 1282 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|    83991 | 1283 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    56134 | 1284 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      267 | 1285 | `			if( bShortNullable ){` |
|        - | 1286 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|        - | 1287 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|        - | 1288 | `				 * already reported" so callers skip their own error emission. */` |
|        3 | 1289 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - | 1290 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|        3 | 1291 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|        - | 1292 | `			}` |
|      265 | 1293 | `			if( nMembers >= 2 && !bParen ){` |
|      ! 0 | 1294 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|        - | 1295 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 | 1296 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1297 | `			}` |
|      265 | 1298 | ``			pGen->pIn++; /* skip `\|` */`` |
|      265 | 1299 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|      265 | 1300 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1301 | `				return rc;` |
|        - | 1302 | `			}` |
|        5 | 1303 | `		}` |
|    55727 | 1304 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|      ! 0 | 1305 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1306 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 | 1307 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1308 | `		}` |
|        - | 1309 | `	}` |
|        - | 1310 | `	/* Validation pass.` |
|        - | 1311 | `	 *` |
|        - | 1312 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|        - | 1313 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|        - | 1314 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|        - | 1315 | `	 */` |
|        - | 1316 | `	{` |
|        - | 1317 | `		int i, j;` |
|    55727 | 1318 | `		int bHasNonNull = 0;` |
|    55727 | 1319 | `		int bAnyIntersection = 0;` |
|        - | 1320 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|        - | 1321 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|        - | 1322 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|  1838831 | 1323 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   111743 | 1324 | `		for( i = 0; i < nAtoms; i++ ){` |
|    56021 | 1325 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|    28013 | 1326 | `		}` |
|   111667 | 1327 | `		for( i = 0; i < nAtoms; i++ ){` |
|    55979 | 1328 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|    27975 | 1329 | `		}` |
|        - | 1330 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|        - | 1331 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    55727 | 1332 | `		if( bShortNullable && bAnyIntersection ){` |
|      ! 0 | 1333 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1334 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|      ! 0 | 1335 | `			return SXERR_SYNTAX;` |
|        - | 1336 | `		}` |
|   111729 | 1337 | `		for( i = 0; i < nAtoms; i++ ){` |
|        - | 1338 | `			/* Intersection members must be class/interface types (PHP rejects` |
|        - | 1339 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|        - | 1340 | ``			 * `true`/`false` in an intersection). */`` |
|    56019 | 1341 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       71 | 1342 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|       71 | 1343 | `				if( bClassLike ){` |
|       69 | 1344 | `					SyString *pC = &aAtoms[i].sClass;` |
|       64 | 1345 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|       64 | 1346 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|       64 | 1347 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|       69 | 1348 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|      ! 0 | 1349 | `						bClassLike = 0;` |
|      ! 0 | 1350 | `					}` |
|       32 | 1351 | `				}` |
|       71 | 1352 | `				if( !bClassLike ){` |
|        - | 1353 | `					const char *zName; sxu32 nName;` |
|        3 | 1354 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 | 1355 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|      ! 0 | 1356 | `					}else{` |
|        3 | 1357 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|        - | 1358 | `					}` |
|        4 | 1359 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1360 | `						"Type %.*s cannot be part of an intersection type",` |
|        1 | 1361 | `						(int)nName, zName);` |
|        3 | 1362 | `					return SXERR_SYNTAX;` |
|        - | 1363 | `				}` |
|       32 | 1364 | `			}` |
|    56017 | 1365 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|      473 | 1366 | `				if( nAtoms > 1 ){` |
|        3 | 1367 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1368 | `						"Void can only be used as a standalone type");` |
|        3 | 1369 | `					return SXERR_SYNTAX;` |
|        - | 1370 | `				}` |
|      471 | 1371 | `				if( !bAllowVoid ){` |
|      ! 0 | 1372 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1373 | `						"void cannot be used here");` |
|      ! 0 | 1374 | `					return SXERR_SYNTAX;` |
|        - | 1375 | `				}` |
|      471 | 1376 | `				if( bShortNullable ){` |
|      ! 0 | 1377 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1378 | `						"Void type cannot be nullable");` |
|      ! 0 | 1379 | `					return SXERR_SYNTAX;` |
|        - | 1380 | `				}` |
|      233 | 1381 | `			}` |
|    56015 | 1382 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|        - | 1383 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|        - | 1384 | `				 * type (never = the function does not return). Mirrors the void` |
|        - | 1385 | `				 * validation above; accepted here and enforced at compile time` |
|        - | 1386 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|       37 | 1387 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|        - | 1388 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|        - | 1389 | `					 * same as any other non-standalone use. */` |
|        6 | 1390 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1391 | `						"never can only be used as a standalone type");` |
|        6 | 1392 | `					return SXERR_SYNTAX;` |
|        - | 1393 | `				}` |
|       33 | 1394 | `				if( !bAllowVoid ){` |
|        - | 1395 | `					/* Return-only: params call with bAllowVoid=0. */` |
|        3 | 1396 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1397 | `						"never cannot be used as a parameter type");` |
|        3 | 1398 | `					return SXERR_SYNTAX;` |
|        - | 1399 | `				}` |
|       13 | 1400 | `			}` |
|    56009 | 1401 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|       36 | 1402 | `				bExplicitNull = 1;` |
|       20 | 1403 | `			}else{` |
|    55977 | 1404 | `				bHasNonNull = 1;` |
|        - | 1405 | `			}` |
|        - | 1406 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|        - | 1407 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|        - | 1408 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|        - | 1409 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|        - | 1410 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    56359 | 1411 | `			for( j = 0; j < i; j++ ){` |
|      357 | 1412 | `				int bDup = 0;` |
|      357 | 1413 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|      691 | 1414 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|      352 | 1415 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|      357 | 1416 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|      337 | 1417 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|       91 | 1418 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       84 | 1419 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|       69 | 1420 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       22 | 1421 | `								aAtoms[j].sClass.zString,` |
|       44 | 1422 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|      ! 0 | 1423 | `							bDup = 1;` |
|      ! 0 | 1424 | `						}` |
|       47 | 1425 | `					}else{` |
|        3 | 1426 | `						bDup = 1;` |
|        - | 1427 | `					}` |
|       43 | 1428 | `				}` |
|      337 | 1429 | `				if( bDup ){` |
|        - | 1430 | `					const char *zName;` |
|        - | 1431 | `					sxu32 nName;` |
|        3 | 1432 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 | 1433 | `						zName = aAtoms[i].sClass.zString;` |
|      ! 0 | 1434 | `						nName = aAtoms[i].sClass.nByte;` |
|      ! 0 | 1435 | `					}else{` |
|        3 | 1436 | `						zName = aAtoms[i].zCanon;` |
|        3 | 1437 | `						nName = aAtoms[i].nCanon;` |
|        - | 1438 | `					}` |
|        4 | 1439 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        1 | 1440 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|        3 | 1441 | `					return SXERR_SYNTAX;` |
|        - | 1442 | `				}` |
|      170 | 1443 | `			}` |
|    28006 | 1444 | `		}` |
|    55715 | 1445 | `		if( !bHasNonNull && bExplicitNull ){` |
|        7 | 1446 | `			if( bShortNullable ){` |
|        - | 1447 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|      ! 0 | 1448 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1449 | `					"Null can not be used as a standalone type");` |
|      ! 0 | 1450 | `				return SXERR_SYNTAX;` |
|        - | 1451 | `			}` |
|        - | 1452 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|        - | 1453 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|        - | 1454 | `			 * path below leaves *pnType untouched when there is no non-null` |
|        - | 1455 | `			 * atom, so set it here. */` |
|        7 | 1456 | `			*pnType = MEMOBJ_NULL;` |
|        3 | 1457 | `		}` |
|        - | 1458 | `	}` |
|        - | 1459 | `	/* Compute nullability flag */` |
|    55715 | 1460 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      241 | 1461 | `		*piTypeFlags \|= iNullableFlag;` |
|      118 | 1462 | `	}` |
|        - | 1463 | `	/* Build canonical type text */` |
|    55715 | 1464 | `	if( pTypeText ){` |
|        - | 1465 | `		SyBlob sBlob;` |
|    55715 | 1466 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|    83468 | 1467 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|    27855 | 1468 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    55715 | 1469 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|    82832 | 1470 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    55218 | 1471 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    55223 | 1472 | `			if( zDup ){` |
|    55223 | 1473 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|    27609 | 1474 | `			}` |
|    27609 | 1475 | `		}` |
|    55715 | 1476 | `		SyBlobRelease(&sBlob);` |
|    27855 | 1477 | `	}` |
|        - | 1478 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|        - | 1479 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|        - | 1480 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|        - | 1481 | `	{` |
|    55715 | 1482 | `		int nNonNull = 0;` |
|    55715 | 1483 | `		int iNonNullIdx = -1;` |
|        - | 1484 | `		int i;` |
|   111709 | 1485 | `		for( i = 0; i < nAtoms; i++ ){` |
|    55999 | 1486 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    55967 | 1487 | `				nNonNull++;` |
|    55967 | 1488 | `				iNonNullIdx = i;` |
|    27981 | 1489 | `			}` |
|    28002 | 1490 | `		}` |
|    55715 | 1491 | `		if( nNonNull <= 1 ){` |
|        - | 1492 | `			/* Fast path: store as single type. */` |
|    55495 | 1493 | `			if( iNonNullIdx >= 0 ){` |
|    55489 | 1494 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    55489 | 1495 | `				if( pA->nType == SXU32_HIGH ){` |
|     1172 | 1496 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      389 | 1497 | `						pA->sClass.zString, pA->sClass.nByte);` |
|      783 | 1498 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      783 | 1499 | `					*pnType = SXU32_HIGH;` |
|      783 | 1500 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    55100 | 1501 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|      471 | 1502 | `					*pnType = MEMOBJ_VOID;` |
|    54478 | 1503 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|       30 | 1504 | `					*pnType = MEMOBJ_NEVER;` |
|       17 | 1505 | `				}else{` |
|    54219 | 1506 | `					*pnType = pA->nType;` |
|        - | 1507 | `				}` |
|    27742 | 1508 | `			}` |
|    27750 | 1509 | `		}else{` |
|        - | 1510 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      225 | 1511 | `			*piTypeFlags \|= iUnionFlag;` |
|      715 | 1512 | `			for( i = 0; i < nAtoms; i++ ){` |
|        - | 1513 | `				ph7_type_alt sAlt;` |
|      495 | 1514 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      483 | 1515 | `				SyZero(&sAlt, sizeof(sAlt));` |
|      483 | 1516 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|      483 | 1517 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      335 | 1518 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      110 | 1519 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      225 | 1520 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      225 | 1521 | `					sAlt.nType = SXU32_HIGH;` |
|      225 | 1522 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      115 | 1523 | `				}else{` |
|      263 | 1524 | `					sAlt.nType = aAtoms[i].nType;` |
|      263 | 1525 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|        - | 1526 | `				}` |
|      483 | 1527 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      244 | 1528 | `			}` |
|        - | 1529 | `		}` |
|        - | 1530 | `	}` |
|    55715 | 1531 | `	return SXRET_OK;` |
|    27868 | 1532 | `}` |
|        - | 1533 |  |
|        - | 1534 | `/*` |
|        - | 1535 | `` * php 8.5's `#[\NoDiscard]`, decided where the declaration is WRITTEN.`` |
|        - | 1536 | ` *` |
|        - | 1537 | ` * The attribute says a caller must do something with the answer, so php refuses` |
|        - | 1538 | `` * it on a declaration that HAS no answer -- a `void` or `never` return type --`` |
|        - | 1539 | ` * and on a constructor, which is called for its object rather than its return.` |
|        - | 1540 | ` * The nouns are php's: a "function" everywhere but a class body, where the same` |
|        - | 1541 | ` * sentence says "method". Run once the return type is parsed, since that is` |
|        - | 1542 | ` * what it judges; a declaration that already failed says nothing more.` |
|        - | 1543 | ` */` |
|   177356 | 1544 | `PH7_PRIVATE sxi32 GenStateApplyNoDiscard(ph7_gen_state *pGen,ph7_vm_func *pFunc,` |
|        - | 1545 | `	ph7_class *pClass,int bCtor)` |
|        5 | 1546 | `{` |
|   177361 | 1547 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(&pFunc->aAttrs);` |
|   177361 | 1548 | `	const char *zKind = pClass ? "method" : "function";` |
|        - | 1549 | `	sxu32 n;` |
|   177567 | 1550 | `	for( n = 0 ; n < SySetUsed(&pFunc->aAttrs) ; ++n ){` |
|      382 | 1551 | `		if( SyStringLength(&aAttr[n].sName) != sizeof("NoDiscard")-1` |
|      285 | 1552 | `		 \|\| SyStrnicmp(SyStringData(&aAttr[n].sName),"NoDiscard",sizeof("NoDiscard")-1) != 0 ){` |
|      211 | 1553 | `			continue;` |
|        - | 1554 | `		}` |
|      177 | 1555 | `		if( bCtor ){` |
|        4 | 1556 | `			return PH7_GenCompileError(&(*pGen),E_ERROR,pFunc->nLine,` |
|        1 | 1557 | `				"Method %z::%z cannot be #[\\NoDiscard]",&pClass->sName,&pFunc->sName);` |
|        - | 1558 | `		}` |
|      175 | 1559 | `		if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|        7 | 1560 | `			return PH7_GenCompileError(&(*pGen),E_ERROR,pFunc->nLine,` |
|        - | 1561 | `				"A void %s does not return a value, but #[\\NoDiscard] requires a return value",` |
|        2 | 1562 | `				zKind);` |
|        - | 1563 | `		}` |
|      171 | 1564 | `		if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|        7 | 1565 | `			return PH7_GenCompileError(&(*pGen),E_ERROR,pFunc->nLine,` |
|        - | 1566 | `				"A never returning %s does not return a value, but #[\\NoDiscard] requires a return value",` |
|        2 | 1567 | `				zKind);` |
|        - | 1568 | `		}` |
|      167 | 1569 | `		pFunc->iFlags \|= VM_FUNC_NODISCARD;` |
|      167 | 1570 | `		return SXRET_OK;` |
|      ! 0 | 1571 | `	}` |
|   177185 | 1572 | `	return SXRET_OK;` |
|    88683 | 1573 | `}` |
|        - | 1574 | `/*` |
|        - | 1575 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|        - | 1576 | `` * pGen->pIn should point to the token after `)`.`` |
|        - | 1577 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|        - | 1578 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|        - | 1579 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|        - | 1580 | `` *          and union types `: T\|U`.`` |
|        - | 1581 | ` */` |
|   172620 | 1582 | `PH7_PRIVATE sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|        5 | 1583 | `{` |
|   172625 | 1584 | `	sxi32 iFlags = 0;` |
|        - | 1585 | `	sxi32 rc;` |
|        - | 1586 | `	sxu32 nLine;` |
|   172625 | 1587 | `	pFunc->nReturnType = 0;` |
|   172625 | 1588 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|   172625 | 1589 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|        - | 1590 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|        - | 1591 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|        - | 1592 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|        - | 1593 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|        - | 1594 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|   172625 | 1595 | `	SySetReset(&pFunc->aReturnUnion);` |
|   172625 | 1596 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|   172625 | 1597 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|   165019 | 1598 | `		return SXRET_OK;` |
|        - | 1599 | `	}` |
|     7611 | 1600 | `	pGen->pIn++; /* Skip ':' */` |
|     7611 | 1601 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 1602 | `		return SXRET_OK;` |
|        - | 1603 | `	}` |
|     7611 | 1604 | `	nLine = pGen->pIn->nLine;` |
|     7611 | 1605 | `	rc = GenStateParseUnionTypeDecl(` |
|     3803 | 1606 | `		pGen,` |
|     3803 | 1607 | `		&pFunc->nReturnType,` |
|     3803 | 1608 | `		&pFunc->sReturnClass,` |
|     3803 | 1609 | `		&pFunc->aReturnUnion,` |
|        - | 1610 | `		&iFlags,` |
|     3803 | 1611 | `		&pFunc->sReturnTypeName,` |
|        - | 1612 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|        - | 1613 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|        - | 1614 | `		/* iUnionFlag */ 0,` |
|        - | 1615 | `		/* bAllowVoid */ 1,` |
|     3803 | 1616 | `		nLine);` |
|     7611 | 1617 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1618 | `		return SXERR_ABORT;` |
|        - | 1619 | `	}` |
|     7611 | 1620 | `	if( rc == SXERR_CORRUPT ){` |
|        - | 1621 | `		/* Error already reported */` |
|      ! 0 | 1622 | `		return SXERR_SYNTAX;` |
|        - | 1623 | `	}` |
|     7611 | 1624 | `	if( rc == SXERR_SYNTAX ){` |
|        9 | 1625 | `		if( pGen->pIn < pGen->pEnd ){` |
|       12 | 1626 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - | 1627 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|        6 | 1628 | `				&pGen->pIn->sData);` |
|        6 | 1629 | `		}else{` |
|      ! 0 | 1630 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|        - | 1631 | `				"syntax error, unexpected end of file in return type declaration");` |
|        - | 1632 | `		}` |
|        9 | 1633 | `		return SXERR_SYNTAX;` |
|        - | 1634 | `	}` |
|     7605 | 1635 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     7605 | 1636 | `	return SXRET_OK;` |
|    86315 | 1637 | `}` |
|        - | 1638 |  |
|   162900 | 1639 | `PH7_PRIVATE sxi32 GenStateCompileFunc(` |
|        - | 1640 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 1641 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|        - | 1642 | `	sxi32 iFlags,        /* Control flags */` |
|        - | 1643 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|        - | 1644 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|        - | 1645 | `	)` |
|        5 | 1646 | `{` |
|        - | 1647 | `	ph7_vm_func *pFunc;` |
|        - | 1648 | `	SyToken *pEnd;` |
|        - | 1649 | `	sxu32 nLine;` |
|        - | 1650 | `	char *zName;` |
|        - | 1651 | `	sxi32 rc;` |
|        - | 1652 | `	/* Extract line number */` |
|   162905 | 1653 | `	nLine = pGen->pIn->nLine;` |
|        - | 1654 | `	/* Jump the left parenthesis '(' */` |
|   162905 | 1655 | `	pGen->pIn++;` |
|        - | 1656 | `	/* Delimit the function signature */` |
|   162905 | 1657 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   162905 | 1658 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 1659 | `		/* Syntax error */` |
|       12 | 1660 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable");` |
|        4 | 1661 | `		(void)pName;` |
|       12 | 1662 | `		if( rc == SXERR_ABORT ){` |
|        - | 1663 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1664 | `			return SXERR_ABORT;` |
|        - | 1665 | `		}` |
|       12 | 1666 | `		pGen->pIn = pGen->pEnd;` |
|       12 | 1667 | `		return SXRET_OK;` |
|        - | 1668 | `	}` |
|        - | 1669 | `	/* Create the function state */` |
|   162897 | 1670 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   162897 | 1671 | `	if( pFunc == 0 ){` |
|      ! 0 | 1672 | `		goto OutOfMem;` |
|        - | 1673 | `	}` |
|        - | 1674 | `	/* Build the function name, prepending namespace if active.` |
|        - | 1675 | `	 * A NAMED function (never a closure) also answers to php's import rules: its` |
|        - | 1676 | ``	 * short name must not already be a local `use function` import, and the name it`` |
|        - | 1677 | ``	 * takes is remembered so a later `use function` in this unit sees it. */`` |
|   162923 | 1678 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|        - | 1679 | `		SyBlob sFQN;` |
|        - | 1680 | `		sxu32 nLen;` |
|       57 | 1681 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       57 | 1682 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       57 | 1683 | `		SyBlobAppend(&sFQN,"\\",1);` |
|       57 | 1684 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|       57 | 1685 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|       57 | 1686 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|       57 | 1687 | `		SyBlobRelease(&sFQN);` |
|       57 | 1688 | `		if( zName == 0 ){` |
|      ! 0 | 1689 | `			goto OutOfMem;` |
|        - | 1690 | `		}` |
|       57 | 1691 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       31 | 1692 | `	}else{` |
|   162845 | 1693 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   162845 | 1694 | `		if( zName == 0 ){` |
|      ! 0 | 1695 | `			goto OutOfMem;` |
|        - | 1696 | `		}` |
|   162845 | 1697 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|        - | 1698 | `	}` |
|   162897 | 1699 | `	if( !bHandleClosure ){` |
|   158153 | 1700 | `		if( GenStateGuardImportRedeclare(pGen,1,pName,&pFunc->sName,nLine) == SXERR_ABORT ){` |
|      ! 0 | 1701 | `			return SXERR_ABORT;` |
|        - | 1702 | `		}` |
|   158153 | 1703 | `		GenStateRecordDeclaredName(pGen,1,&pFunc->sName);` |
|    79074 | 1704 | `	}` |
|        - | 1705 | ``	/* Take php's `{closure:SCOPE:LINE}` name the caller built for this closure. It has to`` |
|        - | 1706 | `	 * land here, ahead of the body, because a __FUNCTION__ inside the body reads it at` |
|        - | 1707 | `	 * compile time — and it must be cleared, since a NESTED declaration reaches this same` |
|        - | 1708 | `	 * point and would otherwise inherit its parent's. */` |
|   162897 | 1709 | `	if( SyStringLength(&pGen->sPendingClosureName) > 0 ){` |
|     4749 | 1710 | `		if( bHandleClosure ){` |
|     4749 | 1711 | `			pFunc->sClosureName = pGen->sPendingClosureName;` |
|     2372 | 1712 | `		}` |
|     4749 | 1713 | `		SyStringInitFromBuf(&pGen->sPendingClosureName,0,0);` |
|     2372 | 1714 | `	}` |
|        - | 1715 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|        - | 1716 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|   162897 | 1717 | `	pFunc->nLine = nLine;` |
|   162897 | 1718 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|   162897 | 1719 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 1720 | `		return SXERR_ABORT;` |
|        - | 1721 | `	}` |
|   162897 | 1722 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pFunc->aAttrs,2,2,0,0) == SXERR_ABORT ){` |
|      ! 0 | 1723 | `		return SXERR_ABORT;` |
|        - | 1724 | `	}` |
|   162897 | 1725 | `	if( pGen->pIn < pEnd ){` |
|        - | 1726 | `		/* Collect function arguments */` |
|   152405 | 1727 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   152405 | 1728 | `		if( rc == SXERR_ABORT ){` |
|        - | 1729 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|        3 | 1730 | `			return SXERR_ABORT;` |
|        - | 1731 | `		}` |
|    76199 | 1732 | `	}` |
|        - | 1733 | `	/* Point past ')' and parse optional return type ': type' */` |
|   162895 | 1734 | `	pGen->pIn = &pEnd[1];` |
|        - | 1735 | `	{` |
|   162895 | 1736 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   162895 | 1737 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 | 1738 | `			return SXERR_ABORT;` |
|   162895 | 1739 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|        9 | 1740 | `			return SXERR_SYNTAX;` |
|        - | 1741 | `		}` |
|        - | 1742 | `	}` |
|        - | 1743 | `	/* php's #[\NoDiscard] declaration rules, which want the return type. A` |
|        - | 1744 | ``	 * closure's second (post-`use`) return-type parse re-runs below; the flag is`` |
|        - | 1745 | `	 * idempotent and the refusals are the same either way. */` |
|   162889 | 1746 | `	if( GenStateApplyNoDiscard(&(*pGen),pFunc,0,0) == SXERR_ABORT ){` |
|      ! 0 | 1747 | `		return SXERR_ABORT;` |
|        - | 1748 | `	}` |
|   162889 | 1749 | `	if( bHandleClosure ){` |
|        - | 1750 | `		ph7_vm_func_closure_env sEnv;` |
|     4749 | 1751 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     4744 | 1752 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     2996 | 1753 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|     1243 | 1754 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - | 1755 | `				/* Closure,record environment variable */` |
|     1243 | 1756 | `				pGen->pIn++;` |
|     1243 | 1757 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 1758 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|      ! 0 | 1759 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1760 | `						return SXERR_ABORT;` |
|        - | 1761 | `					}` |
|      ! 0 | 1762 | `				}` |
|     1243 | 1763 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|        - | 1764 | `				/* Compile until we hit the first closing parenthesis */` |
|     2631 | 1765 | `				while( pGen->pIn < pGen->pEnd ){` |
|     2631 | 1766 | `					int iFlagsLocal = 0;` |
|     2631 | 1767 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|     1241 | 1768 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|     1241 | 1769 | `						break;` |
|        - | 1770 | `					}` |
|     1395 | 1771 | `					nLineLocal = pGen->pIn->nLine;` |
|     1395 | 1772 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - | 1773 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|        - | 1774 | `						 * to the variable's memory slot instead of copying its value. */` |
|      287 | 1775 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|      287 | 1776 | `						pGen->pIn++;` |
|      141 | 1777 | `					}` |
|     1390 | 1778 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|     1395 | 1779 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 1780 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - | 1781 | `								"Closure: Unexpected token. Expecting a variable name");` |
|      ! 0 | 1782 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 1783 | `								return SXERR_ABORT;` |
|        - | 1784 | `							}` |
|        - | 1785 | `							/* Find the closing parenthesis */` |
|      ! 0 | 1786 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 1787 | `								pGen->pIn++;` |
|      ! 0 | 1788 | `							}` |
|      ! 0 | 1789 | `							if(pGen->pIn < pGen->pEnd){` |
|      ! 0 | 1790 | `								pGen->pIn++;` |
|      ! 0 | 1791 | `							}` |
|      ! 0 | 1792 | `							break;` |
|        - | 1793 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|      ! 0 | 1794 | `					}else{` |
|        - | 1795 | `						SyString *pNameLocal;` |
|        - | 1796 | `						char *zDup;` |
|        - | 1797 | `						/* Duplicate variable name */` |
|     1395 | 1798 | `						pNameLocal = &pGen->pIn[1].sData;` |
|     1395 | 1799 | `						if( PH7_VmIsAutoGlobal(pNameLocal->zString,pNameLocal->nByte) ){` |
|        - | 1800 | `							/* php's compile fatal. It is a real protection, not a` |
|        - | 1801 | `							 * style rule: the import resolves through hSuper, so` |
|        - | 1802 | `							 * installing the captured value would overwrite the` |
|        - | 1803 | ``							 * superglobal's own slot — `use ($GLOBALS)` replaced the`` |
|        - | 1804 | `							 * live symbol-table view with a snapshot and every later` |
|        - | 1805 | `							 * global went missing program-wide. */` |
|        3 | 1806 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - | 1807 | `								"Cannot use auto-global as lexical variable");` |
|        3 | 1808 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 1809 | `								return SXERR_ABORT;` |
|        - | 1810 | `							}` |
|        3 | 1811 | `							return SXERR_SYNTAX;` |
|        - | 1812 | `						}` |
|     1393 | 1813 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|     1393 | 1814 | `						if( zDup ){` |
|        - | 1815 | `							/* Zero the structure */` |
|     1393 | 1816 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     1393 | 1817 | `							sEnv.iFlags = iFlagsLocal;` |
|     1393 | 1818 | `							sEnv.nLine = nLineLocal; /* the capture's own source line (php warns here) */` |
|     1393 | 1819 | `							sEnv.nIdx = SXU32_HIGH;` |
|     1393 | 1820 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     1393 | 1821 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|     1497 | 1822 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|      208 | 1823 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|      ! 0 | 1824 | `									got_this = 1;` |
|      ! 0 | 1825 | `							}` |
|        - | 1826 | `							/* Save imported variable */` |
|     1393 | 1827 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      699 | 1828 | `						}else{` |
|      ! 0 | 1829 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1830 | `							 return SXERR_ABORT;` |
|        - | 1831 | `						}` |
|        - | 1832 | `					}` |
|     1393 | 1833 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|     1547 | 1834 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - | 1835 | `						/* Ignore trailing commas */` |
|      158 | 1836 | `						pGen->pIn++;` |
|        4 | 1837 | `					}` |
|        5 | 1838 | `				}` |
|        - | 1839 | `				/* php 7.1+: the return type follows the use clause —` |
|        - | 1840 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|        - | 1841 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|        - | 1842 | `				 * so an unconditional call would wipe a type parsed at the` |
|        - | 1843 | `				 * legacy pre-use position. */` |
|     1241 | 1844 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|       16 | 1845 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|       16 | 1846 | `					if( rcRt2 == SXERR_ABORT ){` |
|      ! 0 | 1847 | `						return SXERR_ABORT;` |
|       16 | 1848 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|      ! 0 | 1849 | `						return SXERR_SYNTAX;` |
|        - | 1850 | `					}` |
|        - | 1851 | `					/* The type this closure really declared is only known now. */` |
|       16 | 1852 | `					if( GenStateApplyNoDiscard(&(*pGen),pFunc,0,0) == SXERR_ABORT ){` |
|      ! 0 | 1853 | `						return SXERR_ABORT;` |
|        - | 1854 | `					}` |
|        7 | 1855 | `				}` |
|      618 | 1856 | `		}` |
|     4747 | 1857 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|        - | 1858 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|        - | 1859 | `			 * available to the closure environment — for EVERY non-static` |
|        - | 1860 | `			 * anonymous function, use list or not (php binds $this to any` |
|        - | 1861 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|        - | 1862 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|        - | 1863 | `			 * a global-scope closure is silently dropped at install. A static` |
|        - | 1864 | `			 * closure never binds $this (php). */` |
|     4433 | 1865 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     4433 | 1866 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|     4433 | 1867 | `			sEnv.nIdx = SXU32_HIGH;` |
|     4433 | 1868 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     4433 | 1869 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|     4433 | 1870 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|     2214 | 1871 | `		}` |
|     4747 | 1872 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|        - | 1873 | `			/* Mark as closure */` |
|     4509 | 1874 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|     2252 | 1875 | `		}` |
|     2371 | 1876 | `	}` |
|        - | 1877 | `	/* Compile the body */` |
|   162887 | 1878 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   162887 | 1879 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1880 | `		return SXERR_ABORT;` |
|        - | 1881 | `	}` |
|        - | 1882 | `	/* The cursor sits just past the body's closing brace */` |
|   162887 | 1883 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|   162887 | 1884 | `	if( ppFunc ){` |
|   162887 | 1885 | `		*ppFunc = pFunc;` |
|    81441 | 1886 | `	}` |
|   162887 | 1887 | `	rc = SXRET_OK;` |
|   162887 | 1888 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|        - | 1889 | `		/* Reject a php-fatal redeclaration before hoisting the function */` |
|   158383 | 1890 | `		if( GenStateGuardFuncRedeclaration(pGen,pFunc) == SXERR_ABORT ){` |
|       11 | 1891 | `			return SXERR_ABORT;` |
|        - | 1892 | `		}` |
|        - | 1893 | `		/* Finally register the function */` |
|   158375 | 1894 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    79185 | 1895 | `	}` |
|   162879 | 1896 | `	if( rc == SXRET_OK ){` |
|   162879 | 1897 | `		return SXRET_OK;` |
|        - | 1898 | `	}` |
|        - | 1899 | `	/* Fall through if something goes wrong */` |
|      ! 0 | 1900 | `OutOfMem:` |
|        - | 1901 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - | 1902 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|        - | 1903 | `	 */` |
|      ! 0 | 1904 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 | 1905 | `	return SXERR_ABORT;` |
|    81455 | 1906 | `}` |
|        - | 1907 | `/*` |
|        - | 1908 | ` * Compile a standard PHP function.` |
|        - | 1909 | ` *  Refer to the block-comment above for more information.` |
|        - | 1910 | ` */` |
|   158164 | 1911 | `PH7_PRIVATE sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|        5 | 1912 | `{` |
|        - | 1913 | `	SyString *pName;` |
|        - | 1914 | `	sxi32 iFlags;` |
|        - | 1915 | `	sxu32 nKwLine;` |
|        - | 1916 | `	sxu32 nLine;` |
|        - | 1917 | `	sxi32 rc;` |
|        - | 1918 |  |
|   158169 | 1919 | `	nLine = pGen->pIn->nLine;` |
|   158169 | 1920 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|   158169 | 1921 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   158169 | 1922 | `	iFlags = 0;` |
|   158169 | 1923 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - | 1924 | `		/* Return by reference,remember that */` |
|       21 | 1925 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|        - | 1926 | `		/* Jump the '&' token */` |
|       21 | 1927 | `		pGen->pIn++;` |
|        9 | 1928 | `	}` |
|   158169 | 1929 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 1930 | `		/* Invalid function name */` |
|        8 | 1931 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|        8 | 1932 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1933 | `			return SXERR_ABORT;` |
|        - | 1934 | `		}` |
|        - | 1935 | `		/* Sychronize with the next semi-colon or braces*/` |
|       22 | 1936 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       16 | 1937 | `			pGen->pIn++;` |
|        2 | 1938 | `		}` |
|        8 | 1939 | `		return SXRET_OK;` |
|        - | 1940 | `	}` |
|   158163 | 1941 | `	pName = &pGen->pIn->sData;` |
|   158163 | 1942 | `	nLine = pGen->pIn->nLine;` |
|        - | 1943 | `	/* Jump the function name */` |
|   158163 | 1944 | `	pGen->pIn++;` |
|   158163 | 1945 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1946 | `		/* Syntax error */` |
|        3 | 1947 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|        3 | 1948 | `		if( rc == SXERR_ABORT ){` |
|        - | 1949 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1950 | `			return SXERR_ABORT;` |
|        - | 1951 | `		}` |
|        - | 1952 | `		/* Sychronize with the next semi-colon or '{' */` |
|        3 | 1953 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 1954 | `			pGen->pIn++;` |
|      ! 0 | 1955 | `		}` |
|        3 | 1956 | `		return SXRET_OK;` |
|        - | 1957 | `	}` |
|        - | 1958 | `	/* Compile function body */` |
|        - | 1959 | `	{` |
|   158161 | 1960 | `		ph7_vm_func *pFuncState = 0;` |
|   158161 | 1961 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|   158161 | 1962 | `		if( pFuncState ){` |
|        - | 1963 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|   158145 | 1964 | `			pFuncState->nLine = nKwLine;` |
|    79070 | 1965 | `		}` |
|        - | 1966 | `	}` |
|   158161 | 1967 | `	return rc;` |
|    79087 | 1968 | `}` |
|        - | 1969 |  |
