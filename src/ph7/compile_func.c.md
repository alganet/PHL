# src/ph7/compile_func.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 870/1013 lines (85.88%)

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
|    705960 |   68 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|         5 |   69 | `{` |
|         - |   70 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |   71 | `	SySet *pInstrContainer;` |
|         - |   72 | `	sxi32 rc;` |
|         - |   73 | `	/* Swap token stream */` |
|    705965 |   74 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|    705965 |   75 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    705965 |   76 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|         - |   77 | `	/* Compile the expression holding the argument value */` |
|    705965 |   78 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |   79 | `	/* Emit the done instruction */` |
|    705965 |   80 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    705965 |   81 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    705965 |   82 | `	RE_SWAP_DELIMITER(pGen);` |
|    705965 |   83 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |   84 | `		return SXERR_ABORT;` |
|         - |   85 | `	}` |
|    705965 |   86 | `	return SXRET_OK;` |
|    352985 |   87 | `}` |
|         - |   88 | `/*` |
|         - |   89 | ` * Collect function arguments one after one.` |
|         - |   90 | ` * According to the PHP language reference manual.` |
|         - |   91 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|         - |   92 | ` * list of expressions.` |
|         - |   93 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|         - |   94 | ` * and default argument values. Variable-length argument lists are also supported,` |
|         - |   95 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|         - |   96 | ` * for more information.` |
|         - |   97 | ` * Example #1 Passing arrays to functions` |
|         - |   98 | ` * <?php` |
|         - |   99 | ` * function takes_array($input)` |
|         - |  100 | ` * {` |
|         - |  101 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|         - |  102 | ` * }` |
|         - |  103 | ` * ?>` |
|         - |  104 | ` * Making arguments be passed by reference` |
|         - |  105 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|         - |  106 | ` * within the function is changed, it does not get changed outside of the function).` |
|         - |  107 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|         - |  108 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|         - |  109 | ` * to the argument name in the function definition:` |
|         - |  110 | ` * Example #2 Passing function parameters by reference` |
|         - |  111 | ` * <?php` |
|         - |  112 | ` * function add_some_extra(&$string)` |
|         - |  113 | ` * {` |
|         - |  114 | ` *   $string .= 'and something extra.';` |
|         - |  115 | ` * }` |
|         - |  116 | ` * $str = 'This is a string, ';` |
|         - |  117 | ` * add_some_extra($str);` |
|         - |  118 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|         - |  119 | ` * ?>` |
|         - |  120 | ` *` |
|         - |  121 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|         - |  122 | ` * complex agrument values.Please refer to the official documentation for more information` |
|         - |  123 | ` * on these extension.` |
|         - |  124 | ` */` |
|   1556950 |  125 | `PH7_PRIVATE sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|         5 |  126 | `{` |
|         - |  127 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|         - |  128 | `	SyToken *pIn;  /* Token stream */` |
|         - |  129 | `	SyBlob sSig;         /* Function signature */` |
|         - |  130 | `	char *zDup;          /* Copy of argument name */` |
|         - |  131 | `	sxi32 rc;` |
|         - |  132 |  |
|   1556955 |  133 | `	pIn = pGen->pIn;` |
|   1556955 |  134 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|         - |  135 | `	/* Process arguments one after one */` |
|   2005220 |  136 | `	for(;;){` |
|   4010445 |  137 | `		if( pIn >= pEnd ){` |
|         - |  138 | `			/* No more arguments to process */` |
|   1556937 |  139 | `			break;` |
|         - |  140 | `		}` |
|   2453513 |  141 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   2453513 |  142 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   2453513 |  143 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   2453513 |  144 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   2453513 |  145 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|         - |  146 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|         - |  147 | `		 * first token inside the main token stream */` |
|   2453513 |  148 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  149 | `			return SXERR_ABORT;` |
|         - |  150 | `		}` |
|         - |  151 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|         - |  152 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|         - |  153 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|         - |  154 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|         - |  155 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|         - |  156 | `		{` |
|   2453513 |  157 | `			int bReadonly = 0, bVisSeen = 0;` |
|   2453513 |  158 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   2453513 |  159 | `			sxi32 iSetVisFlag = 0;` |
|         - |  160 | `			int nSetTok;` |
|         - |  161 | `			sxi32 nSetVis;` |
|   2453513 |  162 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|         3 |  163 | `				bReadonly = 1;` |
|         3 |  164 | `				pIn++;` |
|         1 |  165 | `			}` |
|   2453513 |  166 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   2453513 |  167 | `			if( nSetVis ){` |
|         - |  168 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|         3 |  169 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  170 | `				bVisSeen = 1;` |
|         3 |  171 | `				pIn += nSetTok;` |
|         3 |  172 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       ! 0 |  173 | `					bReadonly = 1;` |
|       ! 0 |  174 | `					pIn++;` |
|         1 |  175 | `				}` |
|   2453512 |  176 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|    109021 |  177 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|    109021 |  178 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|        91 |  179 | `					bVisSeen = 1;` |
|        91 |  180 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|       121 |  181 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|        39 |  182 | `						: PH7_CLASS_PROT_PUBLIC;` |
|        91 |  183 | `					pIn++;` |
|        91 |  184 | `					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|        91 |  185 | `					if( nSetVis ){` |
|         - |  186 | ``						/* `public private(set) T $x` promoted form */`` |
|         3 |  187 | `						iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  188 | `						pIn += nSetTok;` |
|         1 |  189 | `					}` |
|        91 |  190 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        18 |  191 | `						bReadonly = 1;` |
|        18 |  192 | `						pIn++;` |
|         7 |  193 | `					}` |
|        43 |  194 | `				}` |
|     54508 |  195 | `			}` |
|   2453513 |  196 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|         5 |  197 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   2453511 |  198 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|       ! 0 |  199 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|       ! 0 |  200 | `			}` |
|   2453513 |  201 | `			if( bVisSeen \|\| bReadonly ){` |
|        95 |  202 | `				if( !bCtorCtx ){` |
|         6 |  203 | `					if( bAbstractCtx ){` |
|         3 |  204 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  205 | `							"Cannot declare promoted property in an abstract constructor");` |
|         2 |  206 | `					}else{` |
|         3 |  207 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  208 | `							"Cannot declare promoted property outside a constructor");` |
|         - |  209 | `					}` |
|         6 |  210 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  211 | `						return SXERR_ABORT;` |
|         - |  212 | `					}` |
|         6 |  213 | `					return SXERR_SYNTAX;` |
|         - |  214 | `				}` |
|        91 |  215 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|        91 |  216 | `				sArg.iPromoteVis = iVis;` |
|        91 |  217 | `				if( bReadonly ){` |
|        20 |  218 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|         8 |  219 | `				}` |
|        43 |  220 | `			}` |
|         - |  221 | `		}` |
|         - |  222 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   2453504 |  223 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   1322119 |  224 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|    179081 |  225 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    144067 |  226 | `			sxu32 nLineLocal = pIn->nLine;` |
|    144067 |  227 | `			sxi32 iTFlags = 0;` |
|    144067 |  228 | `			pGen->pIn = pIn;` |
|    144067 |  229 | `			rc = GenStateParseUnionTypeDecl(` |
|     72031 |  230 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|     72031 |  231 | `				&iTFlags, &sArg.sTypeName,` |
|         - |  232 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|         - |  233 | `				/* bAllowVoid */ 0,` |
|     72031 |  234 | `						nLineLocal);` |
|    144067 |  235 | `			pIn = pGen->pIn;` |
|    144067 |  236 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  237 | `				return SXERR_ABORT;` |
|    144067 |  238 | `			}else if( rc == SXERR_CORRUPT ){` |
|         - |  239 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|         3 |  240 | `				return SXERR_SYNTAX;` |
|    144065 |  241 | `			}else if( rc == SXERR_SYNTAX ){` |
|        11 |  242 | `				if( pIn < pEnd ){` |
|        15 |  243 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|         - |  244 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|         4 |  245 | `						&pIn->sData);` |
|         7 |  246 | `				}else{` |
|       ! 0 |  247 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|         - |  248 | `						"syntax error, unexpected end of file");` |
|         - |  249 | `				}` |
|        11 |  250 | `				return SXERR_SYNTAX;` |
|         - |  251 | `			}` |
|    144057 |  252 | `			sArg.iFlags \|= iTFlags;` |
|     72026 |  253 | `		}` |
|   2453499 |  254 | `		if( pIn >= pEnd ){` |
|       ! 0 |  255 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|       ! 0 |  256 | `			return rc;` |
|         - |  257 | `		}` |
|   2453499 |  258 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|         - |  259 | `			/* Pass by reference,record that */` |
|     23329 |  260 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     23329 |  261 | `			pIn++;` |
|     11662 |  262 | `		}` |
|   2453499 |  263 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|         - |  264 | `			/* Variadic parameter: ...$args */` |
|     23385 |  265 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|     23385 |  266 | `			pIn++;` |
|     11690 |  267 | `		}` |
|   2453499 |  268 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  269 | `			/* Invalid argument */` |
|       ! 0 |  270 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|       ! 0 |  271 | `			return rc;` |
|         - |  272 | `		}` |
|   2453499 |  273 | `		pIn++; /* Jump the dollar sign */` |
|         - |  274 | `		/* Copy argument name */` |
|   2453499 |  275 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   2453499 |  276 | `		if( zDup == 0 ){` |
|       ! 0 |  277 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  278 | `			return SXERR_ABORT;` |
|         - |  279 | `		}` |
|   2453499 |  280 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   2453499 |  281 | `		pIn++;` |
|   2453499 |  282 | `		if( pIn < pEnd ){` |
|   1358163 |  283 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|         - |  284 | `				SyToken *pDefend;` |
|    705967 |  285 | `				sxi32 iNest = 0;` |
|    705967 |  286 | `				pIn++; /* Jump the equal sign */` |
|    705967 |  287 | `				pDefend = pIn;` |
|         - |  288 | `				/* Process the default value associated with this argument */` |
|   1477889 |  289 | `				while( pDefend < pEnd ){` |
|   1016295 |  290 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    244373 |  291 | `						break;` |
|         - |  292 | `					}` |
|    771927 |  293 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|         - |  294 | `						/* Increment nesting level */` |
|     27163 |  295 | `						iNest++;` |
|    758348 |  296 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|         - |  297 | `						/* Decrement nesting level */` |
|     27163 |  298 | `						iNest--;` |
|     13579 |  299 | `					}` |
|    771927 |  300 | `					pDefend++;` |
|         5 |  301 | `				}` |
|    705967 |  302 | `				if( pIn >= pDefend ){` |
|         3 |  303 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|         3 |  304 | `					return rc;` |
|         - |  305 | `				}` |
|         - |  306 | `				/* Process default value */` |
|    705965 |  307 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|    705965 |  308 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  309 | `					return rc;` |
|         - |  310 | `				}` |
|         - |  311 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|         - |  312 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|         - |  313 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|         - |  314 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|         - |  315 | `				 * arg-type check lets null through. */` |
|    705960 |  316 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|    384027 |  317 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|    376268 |  318 | `					&& &pIn[1] == pDefend` |
|     38819 |  319 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|     21350 |  320 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|      5826 |  321 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|         - |  322 | `` 					/* php 8.4 DEPRECATED the implicit-nullable form (`int $x = null` `` |
|         - |  323 | ``					 * without the `?`). PHL targets php's *non-deprecated* surface and`` |
|         - |  324 | ``					 * rejects it outright — the explicit `?int` must be written.`` |
|         - |  325 | ``					 * `mixed $x = null` is fine: mixed already includes null (explicit`` |
|         - |  326 | `					 * ?T / T\|null are already excluded via VM_FUNC_ARG_NULLABLE above). */` |
|         4 |  327 | `					if( sArg.sClass.nByte == sizeof("mixed")-1` |
|         5 |  328 | `						&& SyStrnicmp(SyStringData(&sArg.sClass),"mixed",sizeof("mixed")-1) == 0 ){` |
|         3 |  329 | `						sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|         2 |  330 | `					}else{` |
|         3 |  331 | `						const char *zSep = "";` |
|         3 |  332 | `						SyString sCls = { "", 0 };` |
|         3 |  333 | `						if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|       ! 0 |  334 | `							sCls = ((ph7_class *)pFunc->pUserData)->sName;` |
|       ! 0 |  335 | `							zSep = "::";` |
|       ! 0 |  336 | `						}` |
|         4 |  337 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,` |
|         - |  338 | `							"%z%s%z(): Cannot use null as the default for non-nullable parameter $%z; write the explicit ?T type instead",` |
|         1 |  339 | `							&sCls,zSep,&pFunc->sName,&sArg.sName);` |
|         3 |  340 | `						return SXERR_ABORT;` |
|         - |  341 | `					}` |
|         1 |  342 | `				}` |
|         - |  343 | `				/* Point beyond the default value */` |
|    705963 |  344 | `				pIn = pDefend;` |
|    352979 |  345 | `			}` |
|   1358159 |  346 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  347 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|       ! 0 |  348 | `				return rc;` |
|         - |  349 | `			}` |
|   1358159 |  350 | `			pIn++; /* Jump the trailing comma */` |
|    679077 |  351 | `		}` |
|         - |  352 | `		/* Append argument signature */` |
|   2453495 |  353 | `		if( sArg.nType > 0 ){` |
|    143995 |  354 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|         - |  355 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|     31139 |  356 | `				int marker = 'o';` |
|     31139 |  357 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|     31139 |  358 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     15572 |  359 | `			}else{` |
|         - |  360 | `				int c;` |
|    112861 |  361 | `				c = 'n'; /* cc warning */` |
|         - |  362 | `				/* Type leading character */` |
|    112861 |  363 | `				switch(sArg.nType){` |
|      5825 |  364 | `				case MEMOBJ_HASHMAP:` |
|         - |  365 | `					/* Hashmap aka 'array' */` |
|     11655 |  366 | `					c = 'h';` |
|     11655 |  367 | `					break;` |
|     17582 |  368 | `				case MEMOBJ_INT:` |
|         - |  369 | `					/* Integer */` |
|     35169 |  370 | `					c = 'i';` |
|     35169 |  371 | `					break;` |
|         2 |  372 | `				case MEMOBJ_BOOL:` |
|         - |  373 | `					/* Bool */` |
|         5 |  374 | `					c = 'b';` |
|         5 |  375 | `					break;` |
|         6 |  376 | `				case MEMOBJ_REAL:` |
|         - |  377 | `					/* Float */` |
|        14 |  378 | `					c = 'f';` |
|        14 |  379 | `					break;` |
|     33006 |  380 | `				case MEMOBJ_STRING:` |
|         - |  381 | `					/* String */` |
|     66017 |  382 | `					c = 's';` |
|     66017 |  383 | `					break;` |
|         6 |  384 | `				case MEMOBJ_OBJ:` |
|         - |  385 | `					/* Object */` |
|        14 |  386 | `					c = 'o';` |
|        12 |  387 | `					break;` |
|         1 |  388 | `				default:` |
|         2 |  389 | `					break;` |
|         - |  390 | `				}` |
|    112861 |  391 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|         - |  392 | `			}` |
|     72000 |  393 | `		}else{` |
|         - |  394 | `			/* No type is associated with this parameter which mean` |
|         - |  395 | `			 * that this function is not condidate for overloading.` |
|         - |  396 | `			 */` |
|   2309505 |  397 | `			SyBlobRelease(&sSig);` |
|         - |  398 | `		}` |
|         - |  399 | `		/* Save in the argument set */` |
|   2453495 |  400 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|         5 |  401 | `	}` |
|   1556937 |  402 | `	if( SyBlobLength(&sSig) > 0 ){` |
|         - |  403 | `		/* Save function signature */` |
|     97369 |  404 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|     48682 |  405 | `	}` |
|   1556937 |  406 | `	return SXRET_OK;` |
|    778480 |  407 | `}` |
|         - |  408 | `/*` |
|         - |  409 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|         - |  410 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|         - |  411 | ` * the enclosing function. Returns the token just past the nested construct.` |
|         - |  412 | ` */` |
|     34960 |  413 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|         5 |  414 | `{` |
|     34965 |  415 | `	sxi32 iParen = 0;` |
|     34965 |  416 | `	pIn++; /* past 'function'/'fn' */` |
|         - |  417 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|         - |  418 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|         - |  419 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|    155417 |  420 | `	while( pIn < pEnd ){` |
|    155417 |  421 | `		sxu32 t = pIn->nType;` |
|    155417 |  422 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|    151469 |  423 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|    104863 |  424 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|     85415 |  425 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|    120457 |  426 | `		pIn++;` |
|         5 |  427 | `	}` |
|     19453 |  428 | `	if( pIn >= pEnd ){ return pIn; }` |
|         - |  429 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|         - |  430 | `	{` |
|     19453 |  431 | `		sxi32 d = 0;` |
|    772385 |  432 | `		while( pIn < pEnd ){` |
|    772385 |  433 | `			sxu32 t = pIn->nType;` |
|    772385 |  434 | `			if( t & PH7_TK_OCB ){ d++; }` |
|    741295 |  435 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|    752937 |  436 | `			pIn++;` |
|         5 |  437 | `		}` |
|         - |  438 | `	}` |
|     19453 |  439 | `	return pIn;` |
|     17485 |  440 | `}` |
|         - |  441 | `/*` |
|         - |  442 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|         - |  443 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|         - |  444 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|         - |  445 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|         - |  446 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|         - |  447 | ` * detached-mini-program path untouched.` |
|         - |  448 | ` */` |
|         - |  449 | `/*` |
|         - |  450 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|         - |  451 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|         - |  452 | ` * mixed, object.` |
|         - |  453 | ` */` |
|     11658 |  454 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|         5 |  455 | `{` |
|         - |  456 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|         - |  457 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|         - |  458 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|         - |  459 | `	};` |
|         - |  460 | `	sxu32 i;` |
|     11663 |  461 | `	if( nName > 0 && zName[0] == '\\' ){` |
|       ! 0 |  462 | `		zName++;` |
|       ! 0 |  463 | `		nName--;` |
|       ! 0 |  464 | `	}` |
|     11671 |  465 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|     11671 |  466 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|     11663 |  467 | `			return 1;` |
|         - |  468 | `		}` |
|         5 |  469 | `	}` |
|       ! 0 |  470 | `	return 0;` |
|      5834 |  471 | `}` |
|         - |  472 | `/*` |
|         - |  473 | ` * One atom of a generator's declared return type: is it a supertype of` |
|         - |  474 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|         - |  475 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|         - |  476 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|         - |  477 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|         - |  478 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|         - |  479 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|         - |  480 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|         - |  481 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|         - |  482 | ` * both rather than fatal on valid code (a recorded divergence).` |
|         - |  483 | ` */` |
|     11660 |  484 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|         5 |  485 | `{` |
|     11665 |  486 | `	if( nType == MEMOBJ_OBJ ){` |
|       ! 0 |  487 | ``		return 1; /* bare `object` */`` |
|         - |  488 | `	}` |
|     11665 |  489 | `	if( nType != SXU32_HIGH ){` |
|         3 |  490 | `		return 0; /* scalar/array/void/never/null/... */` |
|         - |  491 | `	}` |
|     11663 |  492 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|     11663 |  493 | `		return 1;` |
|         - |  494 | `	}` |
|         - |  495 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|         - |  496 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|         - |  497 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|         - |  498 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|         - |  499 | `	{` |
|         - |  500 | `		SyBlob sFQN;` |
|         - |  501 | `		int bOk;` |
|       ! 0 |  502 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       ! 0 |  503 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|       ! 0 |  504 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|       ! 0 |  505 | `		SyBlobRelease(&sFQN);` |
|       ! 0 |  506 | `		return bOk;` |
|         - |  507 | `	}` |
|      5835 |  508 | `}` |
|         - |  509 | `/*` |
|         - |  510 | ` * php 8: a generator function may only declare a return type that is a` |
|         - |  511 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|         - |  512 | ` * group qualifies only if every member does. Anything else is php's exact` |
|         - |  513 | ` * compile-time fatal "Generator return type must be a supertype of` |
|         - |  514 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|         - |  515 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|         - |  516 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|         - |  517 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|         - |  518 | ` */` |
|     11900 |  519 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  520 | `{` |
|     11905 |  521 | `	int bOk = 0;` |
|         - |  522 | `	sxu32 nLine;` |
|         - |  523 | `	sxi32 rc;` |
|     11905 |  524 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|       245 |  525 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|         - |  526 | `	}` |
|     11665 |  527 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|       ! 0 |  528 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|       ! 0 |  529 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|         - |  530 | `		sxu32 i,j;` |
|       ! 0 |  531 | `		for( i = 0; i < n && !bOk; i++ ){` |
|         - |  532 | `			int bGroupOk;` |
|       ! 0 |  533 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|       ! 0 |  534 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|         - |  535 | `			}` |
|       ! 0 |  536 | `			bGroupOk = 1;` |
|       ! 0 |  537 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|       ! 0 |  538 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|       ! 0 |  539 | `					bGroupOk = 0;` |
|       ! 0 |  540 | `					break;` |
|         - |  541 | `				}` |
|       ! 0 |  542 | `			}` |
|       ! 0 |  543 | `			bOk = bGroupOk;` |
|       ! 0 |  544 | `		}` |
|       ! 0 |  545 | `	}else{` |
|     11665 |  546 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|         - |  547 | `	}` |
|     11665 |  548 | `	if( bOk ){` |
|     11663 |  549 | `		return SXRET_OK;` |
|         - |  550 | `	}` |
|         - |  551 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|         - |  552 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|         - |  553 | `	 * token of this stream — its line is the function's closing brace. php` |
|         - |  554 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|         - |  555 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|         3 |  556 | `	nLine = pGen->pIn[-1].nLine;` |
|         - |  557 | `	{` |
|         3 |  558 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|         3 |  559 | `		if( sGiven.nByte < 1 ){` |
|       ! 0 |  560 | `			sGiven = pFunc->sReturnClass;` |
|       ! 0 |  561 | `		}` |
|         3 |  562 | `		if( sGiven.nByte < 1 ){` |
|         - |  563 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|         - |  564 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|         - |  565 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|       ! 0 |  566 | `			const char *zScalar =` |
|       ! 0 |  567 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|       ! 0 |  568 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|       ! 0 |  569 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|       ! 0 |  570 | `		}` |
|         3 |  571 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  572 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|         - |  573 | `	}` |
|         3 |  574 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      5955 |  575 | `}` |
|   3276844 |  576 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|         5 |  577 | `{` |
|   3276849 |  578 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|   3276849 |  579 | `	SyToken *pEnd = pGen->pEnd;` |
|   3276849 |  580 | `	sxi32 iDepth = 0;` |
|   3276849 |  581 | `	int bStarted = 0;` |
| 150105191 |  582 | `	while( pIn < pEnd ){` |
| 150105191 |  583 | `		sxu32 t = pIn->nType;` |
| 150105191 |  584 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 143344889 |  585 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 136619871 |  586 | `		if( t & PH7_TK_KEYWORD ){` |
|  10204877 |  587 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  10204877 |  588 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  10192977 |  589 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|         - |  590 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|   5079006 |  591 | `		}` |
| 136573011 |  592 | `		pIn++;` |
|         5 |  593 | `	}` |
|   3264949 |  594 | `	return FALSE;` |
|   1638427 |  595 | `}` |
|         - |  596 | `/*` |
|         - |  597 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|         - |  598 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  599 | ` * and this routine takes care of generating the appropriate error message.` |
|         - |  600 | ` */` |
|   3276844 |  601 | `PH7_PRIVATE sxi32 GenStateCompileFuncBody(` |
|         - |  602 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  603 | `	ph7_vm_func *pFunc    /* Function state */` |
|         - |  604 | `	)` |
|         5 |  605 | `{` |
|         - |  606 | `	SySet *pInstrContainer; /* Instruction container */` |
|         - |  607 | `	GenBlock *pBlock;` |
|         - |  608 | `	sxu32 nGotoOfft;` |
|         - |  609 | `	sxi32 rc;` |
|         - |  610 | `	/* Attach the new function */` |
|   3276849 |  611 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   3276849 |  612 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  613 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|         - |  614 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  615 | `		return SXERR_ABORT;` |
|         - |  616 | `	}` |
|   3276849 |  617 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|         - |  618 | `	/* Swap bytecode containers */` |
|   3276849 |  619 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   3276849 |  620 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|         - |  621 | `	/* Emit constructor property promotion prologue:` |
|         - |  622 | `	 *   $this->NAME = $NAME;` |
|         - |  623 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|         - |  624 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|         - |  625 | `	{` |
|   3276849 |  626 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|         - |  627 | `		sxu32 i;` |
|   5675873 |  628 | `		for( i = 0; i < nArg; i++ ){` |
|   2399029 |  629 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|         - |  630 | `			char *zSrc;` |
|         - |  631 | `			sxu32 nSrc,nName;` |
|         - |  632 | `			SySet sToken;` |
|         - |  633 | `			SyToken *pTmpIn,*pTmpEnd;` |
|         - |  634 | `			sxi32 rcPromote;` |
|   2399029 |  635 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   2398953 |  636 | `				continue;` |
|         - |  637 | `			}` |
|         - |  638 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|         - |  639 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|         - |  640 | `			 * copied), so it must outlive the function — never free it. The` |
|         - |  641 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|         - |  642 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|        81 |  643 | `			nName = SyStringLength(&pArg->sName);` |
|        81 |  644 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|        81 |  645 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|        81 |  646 | `			if( zSrc == 0 ){` |
|       ! 0 |  647 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  648 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  649 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  650 | `				return SXERR_ABORT;` |
|         - |  651 | `			}` |
|         - |  652 | `			{` |
|        81 |  653 | `				char *z = zSrc;` |
|        81 |  654 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|        81 |  655 | `				z += sizeof("$this->")-1;` |
|        81 |  656 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        81 |  657 | `				z += nName;` |
|        81 |  658 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|        81 |  659 | `				z += sizeof(" = $")-1;` |
|        81 |  660 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        81 |  661 | `				z += nName;` |
|        81 |  662 | `				*z = 0;` |
|         - |  663 | `			}` |
|        81 |  664 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        81 |  665 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|        81 |  666 | `			pTmpIn = pGen->pIn;` |
|        81 |  667 | `			pTmpEnd = pGen->pEnd;` |
|        81 |  668 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|        81 |  669 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        81 |  670 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|        81 |  671 | `			pGen->pIn = pTmpIn;` |
|        81 |  672 | `			pGen->pEnd = pTmpEnd;` |
|        81 |  673 | `			SySetRelease(&sToken);` |
|        81 |  674 | `			if( rcPromote == SXERR_ABORT ){` |
|       ! 0 |  675 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  676 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  677 | `				return SXERR_ABORT;` |
|         - |  678 | `			}` |
|         - |  679 | `			/* Discard the assignment result — this is a statement expression. */` |
|        81 |  680 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        43 |  681 | `		}` |
|         - |  682 | `	}` |
|         - |  683 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|         - |  684 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|         - |  685 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|         - |  686 | `	 * generator — and vice versa — is classified independently. */` |
|         - |  687 | `	{` |
|   3276849 |  688 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|   3276849 |  689 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|         - |  690 | `		/* Compile the body */` |
|   3276849 |  691 | `		PH7_CompileBlock(&(*pGen),0);` |
|   3276849 |  692 | `		pGen->bInGenerator = bSavedGen;` |
|         - |  693 | `	}` |
|         - |  694 | `	/* Fix exception jumps now the destination is resolved */` |
|   3276849 |  695 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - |  696 | `	/* Emit the final return if not yet done */` |
|   3276849 |  697 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - |  698 | `	/* Fix gotos jumps now the destination is resolved */` |
|   3276849 |  699 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|       ! 0 |  700 | `		rc = SXERR_ABORT;` |
|       ! 0 |  701 | `	}` |
|   3276849 |  702 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|         - |  703 | `	/* Restore the default container */` |
|   3276849 |  704 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - |  705 | `	/* Leave function block */` |
|   3276849 |  706 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   3276849 |  707 | `	if( rc == SXERR_ABORT ){` |
|         - |  708 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  709 | `		return SXERR_ABORT;` |
|         - |  710 | `	}` |
|         - |  711 | `	/* Scan for yield opcodes to detect generator functions */` |
|         - |  712 | `	{` |
|   3276849 |  713 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|         - |  714 | `		sxu32 i;` |
|  91940649 |  715 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
|  88675705 |  716 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     11905 |  717 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     11905 |  718 | `				break;` |
|         - |  719 | `			}` |
|  44331905 |  720 | `		}` |
|         - |  721 | `	}` |
|   3276849 |  722 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|         - |  723 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|     11905 |  724 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|       ! 0 |  725 | `			return SXERR_ABORT;` |
|         - |  726 | `		}` |
|      5950 |  727 | `	}` |
|         - |  728 | `	/* All done, function body compiled */` |
|   3276849 |  729 | `	return SXRET_OK;` |
|   1638427 |  730 | `}` |
|         - |  731 | `/*` |
|         - |  732 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|         - |  733 | ` * According to the PHP language reference manual.` |
|         - |  734 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|         - |  735 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|         - |  736 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|         - |  737 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  738 | ` *  Functions need not be defined before they are referenced.` |
|         - |  739 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|         - |  740 | ` *  a function even if they were defined inside and vice versa.` |
|         - |  741 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|         - |  742 | ` *  calls with over 32-64 recursion levels.` |
|         - |  743 | ` *` |
|         - |  744 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|         - |  745 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|         - |  746 | ` * on these extension.` |
|         - |  747 | ` */` |
|         - |  748 | `/*` |
|         - |  749 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|         - |  750 | ` */` |
|       592 |  751 | `PH7_PRIVATE int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|         5 |  752 | `{` |
|         - |  753 | `	sxu32 i;` |
|      1649 |  754 | `	for( i = 0; i < n; i++ ){` |
|      1415 |  755 | `		int a = zA[i], b = zB[i];` |
|      1415 |  756 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      1415 |  757 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      1415 |  758 | `		if( a != b ) return a - b;` |
|       531 |  759 | `	}` |
|       239 |  760 | `	return 0;` |
|       301 |  761 | `}` |
|         - |  762 | `/*` |
|         - |  763 | ` * Internal type-atom kinds used during union type parsing.` |
|         - |  764 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|         - |  765 | ` * (which are positive bit values stored in sxu32).` |
|         - |  766 | ` */` |
|         - |  767 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|         - |  768 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|         - |  769 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|         - |  770 |  |
|         - |  771 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|         - |  772 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|         - |  773 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|         - |  774 |  |
|         - |  775 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|         - |  776 | `struct PhlTypeAtom {` |
|         - |  777 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|         - |  778 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|         - |  779 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|         - |  780 | `	sxu32 nCanon;` |
|         - |  781 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|         - |  782 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|         - |  783 | `};` |
|         - |  784 |  |
|         - |  785 | `/*` |
|         - |  786 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|         - |  787 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|         - |  788 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|         - |  789 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|         - |  790 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|         - |  791 | ` * already be consumed by the caller.` |
|         - |  792 | ` */` |
|         - |  793 | `/*` |
|         - |  794 | ` * TRUE if pName is a reserved PHP type keyword (never a class name), so a type` |
|         - |  795 | ` * hint that spells it must not be namespace-qualified. Only the words that can` |
|         - |  796 | ` * reach GenStateParseOneTypeAtom's identifier branch as a bare name matter here` |
|         - |  797 | ` * (bool/int/float/string/array/object/self/static/parent arrive as keywords, and` |
|         - |  798 | ` * null/void/never are matched before the class path), but the full set is listed` |
|         - |  799 | ` * so the guard is robust to lexer changes.` |
|         - |  800 | ` */` |
|     43006 |  801 | `static int GenStateIsReservedTypeWord(const SyString *pName)` |
|         5 |  802 | `{` |
|         - |  803 | `	static const char *azWords[] = {` |
|         - |  804 | `		"false","true","mixed","iterable","callable","null","void","never",` |
|         - |  805 | `		"bool","boolean","int","integer","float","double","string","array",` |
|         - |  806 | `		"object","self","static","parent"` |
|         - |  807 | `	};` |
|         - |  808 | `	sxu32 i;` |
|    901053 |  809 | `	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){` |
|    858165 |  810 | `		sxu32 n = (sxu32)SyStrlen(azWords[i]);` |
|    858165 |  811 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azWords[i],n) == 0 ){` |
|       123 |  812 | `			return 1;` |
|         - |  813 | `		}` |
|    429026 |  814 | `	}` |
|     42893 |  815 | `	return 0;` |
|     21508 |  816 | `}` |
|    191990 |  817 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|         5 |  818 | `{` |
|    191995 |  819 | `	SyToken *pIn = pGen->pIn;` |
|    191995 |  820 | `	int bAbsolute = 0;` |
|    191995 |  821 | `	SyZero(pOut, sizeof(*pOut));` |
|    191995 |  822 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    191995 |  823 | `	if( pIn >= pGen->pEnd ){` |
|       ! 0 |  824 | `		return SXERR_SYNTAX;` |
|         - |  825 | `	}` |
|         - |  826 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    191995 |  827 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|        10 |  828 | `		bAbsolute = 1; /* fully-qualified: never prefix the current namespace */` |
|        10 |  829 | `		pIn++;` |
|        10 |  830 | `		if( pIn >= pGen->pEnd ){` |
|       ! 0 |  831 | `			return SXERR_SYNTAX;` |
|         - |  832 | `		}` |
|         4 |  833 | `	}` |
|    191995 |  834 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  835 | `		return SXERR_SYNTAX;` |
|         - |  836 | `	}` |
|    191995 |  837 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|    148753 |  838 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|    148753 |  839 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|     15601 |  840 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|    140955 |  841 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      7843 |  842 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|    129238 |  843 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|     47237 |  844 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|    101703 |  845 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|     77991 |  846 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|     39094 |  847 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|        45 |  848 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|        81 |  849 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|        25 |  850 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|        48 |  851 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|        17 |  852 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|        35 |  853 | `			pOut->nType = SXU32_HIGH;` |
|        35 |  854 | `			pOut->sClass = pIn->sData;` |
|        19 |  855 | `		}else{` |
|         3 |  856 | `			return SXERR_SYNTAX;` |
|         - |  857 | `		}` |
|    148751 |  858 | `		pIn++;` |
|     74378 |  859 | `	}else{` |
|         - |  860 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|         - |  861 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|     43247 |  862 | `		SyString *pT = &pIn->sData;` |
|     43247 |  863 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|        34 |  864 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|        34 |  865 | `			pIn++;` |
|     43232 |  866 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|       181 |  867 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|       181 |  868 | `			pIn++;` |
|     43129 |  869 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|        26 |  870 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|        26 |  871 | `			pIn++;` |
|        15 |  872 | `		}else{` |
|         - |  873 | `			/* Class / interface name; consume namespace path a\b\c */` |
|     43019 |  874 | `			SyToken *pFirst = pIn;` |
|     43019 |  875 | `			SyToken *pLast = pIn;` |
|     43019 |  876 | `			pOut->nType = SXU32_HIGH;` |
|     43019 |  877 | `			pOut->sClass = pIn->sData;` |
|     43019 |  878 | `			pIn++;` |
|     64524 |  879 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|     43022 |  880 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|         3 |  881 | `				pLast = &pIn[1];` |
|         3 |  882 | `				pIn += 2;` |
|         1 |  883 | `			}` |
|     43019 |  884 | `			if( pLast != pFirst ){` |
|         3 |  885 | `				const char *zFirst = pFirst->sData.zString;` |
|         3 |  886 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|         3 |  887 | `				pOut->sClass.zString = zFirst;` |
|         3 |  888 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|         1 |  889 | `			}` |
|         - |  890 | `			/* Namespace-qualify a bare (single-segment, non-absolute) class type so` |
|         - |  891 | ``			 * a `: Base` / `Base $x` hint in namespace N resolves to N\Base (or a`` |
|         - |  892 | ``			 * `use` alias) at type-check time instead of the global \Base — mirrors`` |
|         - |  893 | `			 * the NEW/CALL/instanceof qualification. Absolute (\Base) and already-` |
|         - |  894 | `			 * qualified (A\B) names are left as written, matching GenStateNsQualifyName. */` |
|         - |  895 | `			/* Reserved type words that reach this identifier branch (false, true,` |
|         - |  896 | `			 * mixed, iterable, callable) are NOT classes and must not be qualified` |
|         - |  897 | ``			 * (else `false\|string` becomes `Ns\false\|string`). */`` |
|     43019 |  898 | `			if( !bAbsolute && pLast == pFirst && !GenStateIsReservedTypeWord(&pOut->sClass) ){` |
|         - |  899 | `				SyBlob sFqn;` |
|     42893 |  900 | `				SyBlobInit(&sFqn,&pGen->pVm->sAllocator);` |
|     42893 |  901 | `				GenStateResolveName(pGen,&pOut->sClass,&sFqn);` |
|     42888 |  902 | `				if( SyBlobLength(&sFqn) != pOut->sClass.nByte` |
|     42888 |  903 | `				 \|\| SyMemcmp(SyBlobData(&sFqn),(const void *)pOut->sClass.zString,pOut->sClass.nByte) != 0 ){` |
|        17 |  904 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 |  905 | `						(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));` |
|        12 |  906 | `					if( zDup ){` |
|        12 |  907 | `						SyStringInitFromBuf(&pOut->sClass,zDup,SyBlobLength(&sFqn));` |
|         5 |  908 | `					}` |
|         5 |  909 | `				}` |
|     42893 |  910 | `				SyBlobRelease(&sFqn);` |
|     21444 |  911 | `			}` |
|         - |  912 | `		}` |
|         - |  913 | `	}` |
|    191993 |  914 | `	pGen->pIn = pIn;` |
|    191993 |  915 | `	return SXRET_OK;` |
|     96000 |  916 | `}` |
|         - |  917 |  |
|         - |  918 | `/*` |
|         - |  919 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|         - |  920 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|         - |  921 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|         - |  922 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|         - |  923 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|         - |  924 | ` */` |
|    191814 |  925 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|         5 |  926 | `{` |
|         - |  927 | `	int i;` |
|    191819 |  928 | `	int nNonNull = 0;` |
|    191819 |  929 | `	int bAnyIntersection = 0;` |
|         - |  930 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    191819 |  931 | `	sxu32 nMaxGroup = 0;` |
|   6329867 |  932 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    383783 |  933 | `	for( i = 0; i < nAtoms; i++ ){` |
|    191969 |  934 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    191939 |  935 | `			nNonNull++;` |
|    191939 |  936 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    191939 |  937 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    191939 |  938 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|     95967 |  939 | `			}` |
|     95967 |  940 | `		}` |
|     95987 |  941 | `	}` |
|    383731 |  942 | `	for( i = 0; i < nAtoms; i++ ){` |
|    191941 |  943 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        29 |  944 | `			bAnyIntersection = 1;` |
|        29 |  945 | `			break;` |
|         - |  946 | `		}` |
|     95961 |  947 | `	}` |
|    191819 |  948 | `	if( bAnyIntersection ){` |
|         - |  949 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|         - |  950 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|         - |  951 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|        29 |  952 | `		sxu32 g, nGroups = 0;` |
|        29 |  953 | `		int bFirstGroup = 1;` |
|        59 |  954 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|        59 |  955 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|        35 |  956 | `			int bFirstMember = 1;` |
|         - |  957 | `			int bWrap;` |
|        35 |  958 | `			if( aGroupCount[g] == 0 ) continue;` |
|         - |  959 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|         - |  960 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|         - |  961 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|         - |  962 | `			 * parens, matching PHP's canonical text. */` |
|        47 |  963 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|        35 |  964 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|        35 |  965 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|       107 |  966 | `			for( i = 0; i < nAtoms; i++ ){` |
|        77 |  967 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|        59 |  968 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|        59 |  969 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|        55 |  970 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        30 |  971 | `				}else{` |
|         6 |  972 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  973 | `				}` |
|        59 |  974 | `				bFirstMember = 0;` |
|        32 |  975 | `			}` |
|        35 |  976 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|        35 |  977 | `			bFirstGroup = 0;` |
|        20 |  978 | `		}` |
|        29 |  979 | `		if( bNullable ){` |
|       ! 0 |  980 | `			SyBlobAppend(pBlob, "\|", 1);` |
|       ! 0 |  981 | `			SyBlobAppend(pBlob, "null", 4);` |
|       ! 0 |  982 | `		}` |
|      9782 |  983 | `		return;` |
|         - |  984 | `	}` |
|    191795 |  985 | `	if( nNonNull == 1 && bNullable ){` |
|         - |  986 | `		/* Shorthand: ?T */` |
|     19511 |  987 | `		for( i = 0; i < nAtoms; i++ ){` |
|     19511 |  988 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     19511 |  989 | `			SyBlobAppend(pBlob, "?", 1);` |
|     19511 |  990 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|     11665 |  991 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      5835 |  992 | `			}else{` |
|      7851 |  993 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  994 | `			}` |
|     19511 |  995 | `			return;` |
|       ! 0 |  996 | `		}` |
|       ! 0 |  997 | `	}` |
|         - |  998 | `	{` |
|    172289 |  999 | `		int bFirst = 1;` |
|         - | 1000 | `		/* 1) Classes in declaration order */` |
|    344679 | 1001 | `		for( i = 0; i < nAtoms; i++ ){` |
|    172395 | 1002 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|     31341 | 1003 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     31341 | 1004 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|     31341 | 1005 | `				bFirst = 0;` |
|     15668 | 1006 | `			}` |
|     86200 | 1007 | `		}` |
|         - | 1008 | `		/* 2) Built-ins in canonical order */` |
|         - | 1009 | `		{` |
|         - | 1010 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|         - | 1011 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|         - | 1012 | `			int k;` |
|   1205993 | 1013 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|   1927089 | 1014 | `				for( i = 0; i < nAtoms; i++ ){` |
|   1034233 | 1015 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|    140853 | 1016 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    140853 | 1017 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|    140853 | 1018 | `						bFirst = 0;` |
|    140853 | 1019 | `						break;` |
|         - | 1020 | `					}` |
|    446695 | 1021 | `				}` |
|    516857 | 1022 | `			}` |
|         - | 1023 | `		}` |
|         - | 1024 | `		/* 3) null suffix */` |
|    172289 | 1025 | `		if( bNullable ){` |
|        20 | 1026 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|        20 | 1027 | `			SyBlobAppend(pBlob, "null", 4);` |
|         8 | 1028 | `		}` |
|         - | 1029 | `	}` |
|     95912 | 1030 | `}` |
|         - | 1031 |  |
|         - | 1032 | `/*` |
|         - | 1033 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|         - | 1034 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|         - | 1035 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|         - | 1036 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|         - | 1037 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|         - | 1038 | ` * whether it was parenthesized.` |
|         - | 1039 | ` *` |
|         - | 1040 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|         - | 1041 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|         - | 1042 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|         - | 1043 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|         - | 1044 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|         - | 1045 | ` */` |
|    191964 | 1046 | `static sxi32 GenStateParsePart(` |
|         - | 1047 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|         - | 1048 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|         5 | 1049 | `{` |
|         - | 1050 | `	sxi32 rc;` |
|    191969 | 1051 | `	int nMembers = 0;` |
|    191969 | 1052 | `	int bParen = 0;` |
|    191969 | 1053 | `	*pnMembers = 0;` |
|    191969 | 1054 | `	*pbParen = 0;` |
|    191969 | 1055 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         9 | 1056 | `		bParen = 1;` |
|         9 | 1057 | `		pGen->pIn++; /* skip '(' */` |
|         3 | 1058 | `	}` |
|     95982 | 1059 | `	for(;;){` |
|    191995 | 1060 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|       ! 0 | 1061 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1062 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|       ! 0 | 1063 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 1064 | `		}` |
|    191995 | 1065 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    191995 | 1066 | `		if( rc != SXRET_OK ){` |
|         3 | 1067 | `			return rc;` |
|         - | 1068 | `		}` |
|    191993 | 1069 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    191993 | 1070 | `		(*pnAtoms)++;` |
|    191993 | 1071 | `		nMembers++;` |
|         - | 1072 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    191993 | 1073 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        39 | 1074 | `			SyToken *pNext = &pGen->pIn[1];` |
|        34 | 1075 | `			if( pNext < pGen->pEnd` |
|        39 | 1076 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        31 | 1077 | `				pGen->pIn++; /* skip '&' */` |
|        31 | 1078 | `				continue;` |
|         - | 1079 | `			}` |
|         4 | 1080 | `		}` |
|    191967 | 1081 | `		break;` |
|       ! 0 | 1082 | `	}` |
|    191967 | 1083 | `	if( bParen ){` |
|         9 | 1084 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 1085 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1086 | `				"Malformed DNF type: expecting ')'");` |
|       ! 0 | 1087 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 1088 | `		}` |
|         9 | 1089 | `		pGen->pIn++; /* skip ')' */` |
|         9 | 1090 | `		if( nMembers < 2 ){` |
|       ! 0 | 1091 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1092 | `				"Parenthesized type must be an intersection of at least two types");` |
|       ! 0 | 1093 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 1094 | `		}` |
|         3 | 1095 | `	}` |
|    191967 | 1096 | `	*pnMembers = nMembers;` |
|    191967 | 1097 | `	*pbParen = bParen;` |
|    191967 | 1098 | `	return SXRET_OK;` |
|     95987 | 1099 | `}` |
|         - | 1100 |  |
|         - | 1101 | `/*` |
|         - | 1102 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|         - | 1103 | ` *` |
|         - | 1104 | ` * Outputs:` |
|         - | 1105 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|         - | 1106 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|         - | 1107 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|         - | 1108 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|         - | 1109 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|         - | 1110 | ` *     already be initialized by the caller (allocator set, etc).` |
|         - | 1111 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|         - | 1112 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|         - | 1113 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|         - | 1114 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|         - | 1115 | ` *` |
|         - | 1116 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|         - | 1117 | ` * SXERR_ABORT on fatal compile errors.` |
|         - | 1118 | ` */` |
|    191830 | 1119 | `PH7_PRIVATE sxi32 GenStateParseUnionTypeDecl(` |
|         - | 1120 | `	ph7_gen_state *pGen,` |
|         - | 1121 | `	sxu32 *pnType,` |
|         - | 1122 | `	SyString *pClass,` |
|         - | 1123 | `	SySet *pAlts,` |
|         - | 1124 | `	sxi32 *piTypeFlags,` |
|         - | 1125 | `	SyString *pTypeText,` |
|         - | 1126 | `	int iNullableFlag,` |
|         - | 1127 | `	int iUnionFlag,` |
|         - | 1128 | `	int bAllowVoid,` |
|         - | 1129 | `	sxu32 nLine` |
|         5 | 1130 | `){` |
|         - | 1131 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    191835 | 1132 | `	int nAtoms = 0;` |
|    191835 | 1133 | `	int bShortNullable = 0;` |
|    191835 | 1134 | `	int bExplicitNull = 0;` |
|         - | 1135 | `	sxi32 rc;` |
|    191835 | 1136 | `	*pnType = 0;` |
|    191835 | 1137 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    191835 | 1138 | `	*piTypeFlags = 0;` |
|    191835 | 1139 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|         - | 1140 |  |
|    191835 | 1141 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 1142 | `		return SXRET_OK;` |
|         - | 1143 | `	}` |
|         - | 1144 | ``	/* Optional `?` shorthand prefix */`` |
|    191830 | 1145 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|     19499 | 1146 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|     19499 | 1147 | `		bShortNullable = 1;` |
|     19499 | 1148 | `		pGen->pIn++;` |
|     19499 | 1149 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 1150 | `			return SXERR_SYNTAX;` |
|         - | 1151 | `		}` |
|      9747 | 1152 | `	}` |
|         - | 1153 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|         - | 1154 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|         - | 1155 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|         - | 1156 | `	{` |
|         - | 1157 | `		int nMembers, bParen;` |
|    191835 | 1158 | `		sxu32 iGroup = 0;` |
|    191835 | 1159 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    191835 | 1160 | `		if( rc != SXRET_OK ){` |
|         4 | 1161 | `			return rc;` |
|         - | 1162 | `		}` |
|         - | 1163 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|         - | 1164 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|         - | 1165 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|         - | 1166 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|         - | 1167 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|    287948 | 1168 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    192039 | 1169 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       141 | 1170 | `			if( bShortNullable ){` |
|         - | 1171 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|         - | 1172 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|         - | 1173 | `				 * already reported" so callers skip their own error emission. */` |
|         3 | 1174 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - | 1175 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|         3 | 1176 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|         - | 1177 | `			}` |
|       139 | 1178 | `			if( nMembers >= 2 && !bParen ){` |
|       ! 0 | 1179 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|         - | 1180 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 | 1181 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 1182 | `			}` |
|       139 | 1183 | ``			pGen->pIn++; /* skip `\|` */`` |
|       139 | 1184 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|       139 | 1185 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1186 | `				return rc;` |
|         - | 1187 | `			}` |
|         5 | 1188 | `		}` |
|    191831 | 1189 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|       ! 0 | 1190 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1191 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 | 1192 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 1193 | `		}` |
|         - | 1194 | `	}` |
|         - | 1195 | `	/* Validation pass.` |
|         - | 1196 | `	 *` |
|         - | 1197 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|         - | 1198 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|         - | 1199 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|         - | 1200 | `	 */` |
|         - | 1201 | `	{` |
|         - | 1202 | `		int i, j;` |
|    191831 | 1203 | `		int bHasNonNull = 0;` |
|    191831 | 1204 | `		int bAnyIntersection = 0;` |
|         - | 1205 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|         - | 1206 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|         - | 1207 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|   6330263 | 1208 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    383817 | 1209 | `		for( i = 0; i < nAtoms; i++ ){` |
|    191991 | 1210 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|     95998 | 1211 | `		}` |
|    383761 | 1212 | `		for( i = 0; i < nAtoms; i++ ){` |
|    191961 | 1213 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|     95970 | 1214 | `		}` |
|         - | 1215 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|         - | 1216 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    191831 | 1217 | `		if( bShortNullable && bAnyIntersection ){` |
|       ! 0 | 1218 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1219 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|       ! 0 | 1220 | `			return SXERR_SYNTAX;` |
|         - | 1221 | `		}` |
|    383803 | 1222 | `		for( i = 0; i < nAtoms; i++ ){` |
|         - | 1223 | `			/* Intersection members must be class/interface types (PHP rejects` |
|         - | 1224 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|         - | 1225 | ``			 * `true`/`false` in an intersection). */`` |
|    191989 | 1226 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        55 | 1227 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|        55 | 1228 | `				if( bClassLike ){` |
|        53 | 1229 | `					SyString *pC = &aAtoms[i].sClass;` |
|        48 | 1230 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|        48 | 1231 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|        48 | 1232 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|        53 | 1233 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|       ! 0 | 1234 | `						bClassLike = 0;` |
|       ! 0 | 1235 | `					}` |
|        24 | 1236 | `				}` |
|        55 | 1237 | `				if( !bClassLike ){` |
|         - | 1238 | `					const char *zName; sxu32 nName;` |
|         3 | 1239 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 | 1240 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|       ! 0 | 1241 | `					}else{` |
|         3 | 1242 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|         - | 1243 | `					}` |
|         4 | 1244 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1245 | `						"Type %.*s cannot be part of an intersection type",` |
|         1 | 1246 | `						(int)nName, zName);` |
|         3 | 1247 | `					return SXERR_SYNTAX;` |
|         - | 1248 | `				}` |
|        24 | 1249 | `			}` |
|    191987 | 1250 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|       181 | 1251 | `				if( nAtoms > 1 ){` |
|         3 | 1252 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1253 | `						"Void can only be used as a standalone type");` |
|         3 | 1254 | `					return SXERR_SYNTAX;` |
|         - | 1255 | `				}` |
|       179 | 1256 | `				if( !bAllowVoid ){` |
|       ! 0 | 1257 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1258 | `						"void cannot be used here");` |
|       ! 0 | 1259 | `					return SXERR_SYNTAX;` |
|         - | 1260 | `				}` |
|       179 | 1261 | `				if( bShortNullable ){` |
|       ! 0 | 1262 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1263 | `						"Void type cannot be nullable");` |
|       ! 0 | 1264 | `					return SXERR_SYNTAX;` |
|         - | 1265 | `				}` |
|        87 | 1266 | `			}` |
|    191985 | 1267 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|         - | 1268 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|         - | 1269 | `				 * type (never = the function does not return). Mirrors the void` |
|         - | 1270 | `				 * validation above; accepted here and enforced at compile time` |
|         - | 1271 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|        26 | 1272 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|         - | 1273 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|         - | 1274 | `					 * same as any other non-standalone use. */` |
|         5 | 1275 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1276 | `						"never can only be used as a standalone type");` |
|         5 | 1277 | `					return SXERR_SYNTAX;` |
|         - | 1278 | `				}` |
|        21 | 1279 | `				if( !bAllowVoid ){` |
|         - | 1280 | `					/* Return-only: params call with bAllowVoid=0. */` |
|         3 | 1281 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1282 | `						"never cannot be used as a parameter type");` |
|         3 | 1283 | `					return SXERR_SYNTAX;` |
|         - | 1284 | `				}` |
|         8 | 1285 | `			}` |
|    191979 | 1286 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|        34 | 1287 | `				bExplicitNull = 1;` |
|        19 | 1288 | `			}else{` |
|    191949 | 1289 | `				bHasNonNull = 1;` |
|         - | 1290 | `			}` |
|         - | 1291 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|         - | 1292 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|         - | 1293 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|         - | 1294 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|         - | 1295 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    192177 | 1296 | `			for( j = 0; j < i; j++ ){` |
|       205 | 1297 | `				int bDup = 0;` |
|       205 | 1298 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|       391 | 1299 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|       200 | 1300 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|       205 | 1301 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|       193 | 1302 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|        49 | 1303 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|        42 | 1304 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|        42 | 1305 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|        16 | 1306 | `								aAtoms[j].sClass.zString,` |
|        32 | 1307 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|       ! 0 | 1308 | `							bDup = 1;` |
|       ! 0 | 1309 | `						}` |
|        26 | 1310 | `					}else{` |
|         3 | 1311 | `						bDup = 1;` |
|         - | 1312 | `					}` |
|        22 | 1313 | `				}` |
|       193 | 1314 | `				if( bDup ){` |
|         - | 1315 | `					const char *zName;` |
|         - | 1316 | `					sxu32 nName;` |
|         3 | 1317 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 | 1318 | `						zName = aAtoms[i].sClass.zString;` |
|       ! 0 | 1319 | `						nName = aAtoms[i].sClass.nByte;` |
|       ! 0 | 1320 | `					}else{` |
|         3 | 1321 | `						zName = aAtoms[i].zCanon;` |
|         3 | 1322 | `						nName = aAtoms[i].nCanon;` |
|         - | 1323 | `					}` |
|         4 | 1324 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         1 | 1325 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|         3 | 1326 | `					return SXERR_SYNTAX;` |
|         - | 1327 | `				}` |
|        98 | 1328 | `			}` |
|     95991 | 1329 | `		}` |
|    191819 | 1330 | `		if( !bHasNonNull && bExplicitNull ){` |
|         7 | 1331 | `			if( bShortNullable ){` |
|         - | 1332 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|       ! 0 | 1333 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1334 | `					"Null can not be used as a standalone type");` |
|       ! 0 | 1335 | `				return SXERR_SYNTAX;` |
|         - | 1336 | `			}` |
|         - | 1337 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|         - | 1338 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|         - | 1339 | `			 * path below leaves *pnType untouched when there is no non-null` |
|         - | 1340 | `			 * atom, so set it here. */` |
|         7 | 1341 | `			*pnType = MEMOBJ_NULL;` |
|         3 | 1342 | `		}` |
|         - | 1343 | `	}` |
|         - | 1344 | `	/* Compute nullability flag */` |
|    191819 | 1345 | `	if( bShortNullable \|\| bExplicitNull ){` |
|     19527 | 1346 | `		*piTypeFlags \|= iNullableFlag;` |
|      9761 | 1347 | `	}` |
|         - | 1348 | `	/* Build canonical type text */` |
|    191819 | 1349 | `	if( pTypeText ){` |
|         - | 1350 | `		SyBlob sBlob;` |
|    191819 | 1351 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|    277980 | 1352 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|     95907 | 1353 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    191819 | 1354 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|    287441 | 1355 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    191624 | 1356 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    191629 | 1357 | `			if( zDup ){` |
|    191629 | 1358 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|     95812 | 1359 | `			}` |
|     95812 | 1360 | `		}` |
|    191819 | 1361 | `		SyBlobRelease(&sBlob);` |
|     95907 | 1362 | `	}` |
|         - | 1363 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|         - | 1364 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|         - | 1365 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|         - | 1366 | `	{` |
|    191819 | 1367 | `		int nNonNull = 0;` |
|    191819 | 1368 | `		int iNonNullIdx = -1;` |
|         - | 1369 | `		int i;` |
|    383783 | 1370 | `		for( i = 0; i < nAtoms; i++ ){` |
|    191969 | 1371 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    191939 | 1372 | `				nNonNull++;` |
|    191939 | 1373 | `				iNonNullIdx = i;` |
|     95967 | 1374 | `			}` |
|     95987 | 1375 | `		}` |
|    191819 | 1376 | `		if( nNonNull <= 1 ){` |
|         - | 1377 | `			/* Fast path: store as single type. */` |
|    191715 | 1378 | `			if( iNonNullIdx >= 0 ){` |
|    191709 | 1379 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    191709 | 1380 | `				if( pA->nType == SXU32_HIGH ){` |
|     64439 | 1381 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     21478 | 1382 | `						pA->sClass.zString, pA->sClass.nByte);` |
|     42961 | 1383 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|     42961 | 1384 | `					*pnType = SXU32_HIGH;` |
|     42961 | 1385 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    170231 | 1386 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|       179 | 1387 | `					*pnType = MEMOBJ_VOID;` |
|    148666 | 1388 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|        18 | 1389 | `					*pnType = MEMOBJ_NEVER;` |
|        10 | 1390 | `				}else{` |
|    148563 | 1391 | `					*pnType = pA->nType;` |
|         - | 1392 | `				}` |
|     95852 | 1393 | `			}` |
|     95860 | 1394 | `		}else{` |
|         - | 1395 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|       109 | 1396 | `			*piTypeFlags \|= iUnionFlag;` |
|       349 | 1397 | `			for( i = 0; i < nAtoms; i++ ){` |
|         - | 1398 | `				ph7_type_alt sAlt;` |
|       245 | 1399 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       235 | 1400 | `				SyZero(&sAlt, sizeof(sAlt));` |
|       235 | 1401 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|       235 | 1402 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       140 | 1403 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        45 | 1404 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        95 | 1405 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|        95 | 1406 | `					sAlt.nType = SXU32_HIGH;` |
|        95 | 1407 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|        50 | 1408 | `				}else{` |
|       145 | 1409 | `					sAlt.nType = aAtoms[i].nType;` |
|       145 | 1410 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|         - | 1411 | `				}` |
|       235 | 1412 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|       120 | 1413 | `			}` |
|         - | 1414 | `		}` |
|         - | 1415 | `	}` |
|    191819 | 1416 | `	return SXRET_OK;` |
|     95920 | 1417 | `}` |
|         - | 1418 |  |
|         - | 1419 | `/*` |
|         - | 1420 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|         - | 1421 | `` * pGen->pIn should point to the token after `)`.`` |
|         - | 1422 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|         - | 1423 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|         - | 1424 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|         - | 1425 | `` *          and union types `: T\|U`.`` |
|         - | 1426 | ` */` |
|   3417022 | 1427 | `PH7_PRIVATE sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|         5 | 1428 | `{` |
|   3417027 | 1429 | `	sxi32 iFlags = 0;` |
|         - | 1430 | `	sxi32 rc;` |
|         - | 1431 | `	sxu32 nLine;` |
|   3417027 | 1432 | `	pFunc->nReturnType = 0;` |
|   3417027 | 1433 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|   3417027 | 1434 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|         - | 1435 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|         - | 1436 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|         - | 1437 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|         - | 1438 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|         - | 1439 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|   3417027 | 1440 | `	SySetReset(&pFunc->aReturnUnion);` |
|   3417027 | 1441 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|   3417027 | 1442 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|   3385175 | 1443 | `		return SXRET_OK;` |
|         - | 1444 | `	}` |
|     31857 | 1445 | `	pGen->pIn++; /* Skip ':' */` |
|     31857 | 1446 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 1447 | `		return SXRET_OK;` |
|         - | 1448 | `	}` |
|     31857 | 1449 | `	nLine = pGen->pIn->nLine;` |
|     31857 | 1450 | `	rc = GenStateParseUnionTypeDecl(` |
|     15926 | 1451 | `		pGen,` |
|     15926 | 1452 | `		&pFunc->nReturnType,` |
|     15926 | 1453 | `		&pFunc->sReturnClass,` |
|     15926 | 1454 | `		&pFunc->aReturnUnion,` |
|         - | 1455 | `		&iFlags,` |
|     15926 | 1456 | `		&pFunc->sReturnTypeName,` |
|         - | 1457 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|         - | 1458 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|         - | 1459 | `		/* iUnionFlag */ 0,` |
|         - | 1460 | `		/* bAllowVoid */ 1,` |
|     15926 | 1461 | `		nLine);` |
|     31857 | 1462 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 1463 | `		return SXERR_ABORT;` |
|         - | 1464 | `	}` |
|     31857 | 1465 | `	if( rc == SXERR_CORRUPT ){` |
|         - | 1466 | `		/* Error already reported */` |
|       ! 0 | 1467 | `		return SXERR_SYNTAX;` |
|         - | 1468 | `	}` |
|     31857 | 1469 | `	if( rc == SXERR_SYNTAX ){` |
|         8 | 1470 | `		if( pGen->pIn < pGen->pEnd ){` |
|        11 | 1471 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - | 1472 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|         6 | 1473 | `				&pGen->pIn->sData);` |
|         5 | 1474 | `		}else{` |
|       ! 0 | 1475 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|         - | 1476 | `				"syntax error, unexpected end of file in return type declaration");` |
|         - | 1477 | `		}` |
|         8 | 1478 | `		return SXERR_SYNTAX;` |
|         - | 1479 | `	}` |
|     31851 | 1480 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     31851 | 1481 | `	return SXRET_OK;` |
|   1708516 | 1482 | `}` |
|         - | 1483 |  |
|    521620 | 1484 | `PH7_PRIVATE sxi32 GenStateCompileFunc(` |
|         - | 1485 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 1486 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|         - | 1487 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 1488 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|         - | 1489 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|         - | 1490 | `	)` |
|         5 | 1491 | `{` |
|         - | 1492 | `	ph7_vm_func *pFunc;` |
|         - | 1493 | `	SyToken *pEnd;` |
|         - | 1494 | `	sxu32 nLine;` |
|         - | 1495 | `	char *zName;` |
|         - | 1496 | `	sxi32 rc;` |
|         - | 1497 | `	/* Extract line number */` |
|    521625 | 1498 | `	nLine = pGen->pIn->nLine;` |
|         - | 1499 | `	/* Jump the left parenthesis '(' */` |
|    521625 | 1500 | `	pGen->pIn++;` |
|         - | 1501 | `	/* Delimit the function signature */` |
|    521625 | 1502 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    521625 | 1503 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 1504 | `		/* Syntax error */` |
|         9 | 1505 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable");` |
|         3 | 1506 | `		(void)pName;` |
|         9 | 1507 | `		if( rc == SXERR_ABORT ){` |
|         - | 1508 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 1509 | `			return SXERR_ABORT;` |
|         - | 1510 | `		}` |
|         9 | 1511 | `		pGen->pIn = pGen->pEnd;` |
|         9 | 1512 | `		return SXRET_OK;` |
|         - | 1513 | `	}` |
|         - | 1514 | `	/* Create the function state */` |
|    521619 | 1515 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|    521619 | 1516 | `	if( pFunc == 0 ){` |
|       ! 0 | 1517 | `		goto OutOfMem;` |
|         - | 1518 | `	}` |
|         - | 1519 | `	/* Build the function name, prepending namespace if active */` |
|    521627 | 1520 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|         - | 1521 | `		SyBlob sFQN;` |
|         - | 1522 | `		sxu32 nLen;` |
|        18 | 1523 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        18 | 1524 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        18 | 1525 | `		SyBlobAppend(&sFQN,"\\",1);` |
|        18 | 1526 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|        18 | 1527 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|        18 | 1528 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|        18 | 1529 | `		SyBlobRelease(&sFQN);` |
|        18 | 1530 | `		if( zName == 0 ){` |
|       ! 0 | 1531 | `			goto OutOfMem;` |
|         - | 1532 | `		}` |
|        18 | 1533 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|        10 | 1534 | `	}else{` |
|    521603 | 1535 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    521603 | 1536 | `		if( zName == 0 ){` |
|       ! 0 | 1537 | `			goto OutOfMem;` |
|         - | 1538 | `		}` |
|    521603 | 1539 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|         - | 1540 | `	}` |
|         - | 1541 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|         - | 1542 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|    521619 | 1543 | `	pFunc->nLine = nLine;` |
|    521619 | 1544 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|    521619 | 1545 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 1546 | `		return SXERR_ABORT;` |
|         - | 1547 | `	}` |
|    521619 | 1548 | `	if( pGen->pIn < pEnd ){` |
|         - | 1549 | `		/* Collect function arguments */` |
|    443065 | 1550 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|    443065 | 1551 | `		if( rc == SXERR_ABORT ){` |
|         - | 1552 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|         3 | 1553 | `			return SXERR_ABORT;` |
|         - | 1554 | `		}` |
|    221529 | 1555 | `	}` |
|         - | 1556 | `	/* Point past ')' and parse optional return type ': type' */` |
|    521617 | 1557 | `	pGen->pIn = &pEnd[1];` |
|         - | 1558 | `	{` |
|    521617 | 1559 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|    521617 | 1560 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 | 1561 | `			return SXERR_ABORT;` |
|    521617 | 1562 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|         8 | 1563 | `			return SXERR_SYNTAX;` |
|         - | 1564 | `		}` |
|         - | 1565 | `	}` |
|    521611 | 1566 | `	if( bHandleClosure ){` |
|         - | 1567 | `		ph7_vm_func_closure_env sEnv;` |
|       583 | 1568 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|       578 | 1569 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       340 | 1570 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|        97 | 1571 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - | 1572 | `				/* Closure,record environment variable */` |
|        97 | 1573 | `				pGen->pIn++;` |
|        97 | 1574 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 | 1575 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|       ! 0 | 1576 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 1577 | `						return SXERR_ABORT;` |
|         - | 1578 | `					}` |
|       ! 0 | 1579 | `				}` |
|        97 | 1580 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|         - | 1581 | `				/* Compile until we hit the first closing parenthesis */` |
|       199 | 1582 | `				while( pGen->pIn < pGen->pEnd ){` |
|       199 | 1583 | `					int iFlagsLocal = 0;` |
|       199 | 1584 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|        97 | 1585 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|        97 | 1586 | `						break;` |
|         - | 1587 | `					}` |
|       107 | 1588 | `					nLineLocal = pGen->pIn->nLine;` |
|       107 | 1589 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - | 1590 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|         - | 1591 | `						 * to the variable's memory slot instead of copying its value. */` |
|        60 | 1592 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|        60 | 1593 | `						pGen->pIn++;` |
|        29 | 1594 | `					}` |
|       102 | 1595 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       107 | 1596 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 1597 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|         - | 1598 | `								"Closure: Unexpected token. Expecting a variable name");` |
|       ! 0 | 1599 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 1600 | `								return SXERR_ABORT;` |
|         - | 1601 | `							}` |
|         - | 1602 | `							/* Find the closing parenthesis */` |
|       ! 0 | 1603 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 1604 | `								pGen->pIn++;` |
|       ! 0 | 1605 | `							}` |
|       ! 0 | 1606 | `							if(pGen->pIn < pGen->pEnd){` |
|       ! 0 | 1607 | `								pGen->pIn++;` |
|       ! 0 | 1608 | `							}` |
|       ! 0 | 1609 | `							break;` |
|         - | 1610 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|       ! 0 | 1611 | `					}else{` |
|         - | 1612 | `						SyString *pNameLocal;` |
|         - | 1613 | `						char *zDup;` |
|         - | 1614 | `						/* Duplicate variable name */` |
|       107 | 1615 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       107 | 1616 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       107 | 1617 | `						if( zDup ){` |
|         - | 1618 | `							/* Zero the structure */` |
|       107 | 1619 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       107 | 1620 | `							sEnv.iFlags = iFlagsLocal;` |
|       107 | 1621 | `							sEnv.nIdx = SXU32_HIGH;` |
|       107 | 1622 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       107 | 1623 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|       122 | 1624 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|        30 | 1625 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|       ! 0 | 1626 | `									got_this = 1;` |
|       ! 0 | 1627 | `							}` |
|         - | 1628 | `							/* Save imported variable */` |
|       107 | 1629 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        56 | 1630 | `						}else{` |
|       ! 0 | 1631 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1632 | `							 return SXERR_ABORT;` |
|         - | 1633 | `						}` |
|         - | 1634 | `					}` |
|       107 | 1635 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|       119 | 1636 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - | 1637 | `						/* Ignore trailing commas */` |
|        13 | 1638 | `						pGen->pIn++;` |
|         1 | 1639 | `					}` |
|         5 | 1640 | `				}` |
|         - | 1641 | `				/* php 7.1+: the return type follows the use clause —` |
|         - | 1642 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|         - | 1643 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|         - | 1644 | `				 * so an unconditional call would wipe a type parsed at the` |
|         - | 1645 | `				 * legacy pre-use position. */` |
|        97 | 1646 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|         7 | 1647 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|         7 | 1648 | `					if( rcRt2 == SXERR_ABORT ){` |
|       ! 0 | 1649 | `						return SXERR_ABORT;` |
|         7 | 1650 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|       ! 0 | 1651 | `						return SXERR_SYNTAX;` |
|         - | 1652 | `					}` |
|         3 | 1653 | `				}` |
|        46 | 1654 | `		}` |
|       583 | 1655 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|         - | 1656 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|         - | 1657 | `			 * available to the closure environment — for EVERY non-static` |
|         - | 1658 | `			 * anonymous function, use list or not (php binds $this to any` |
|         - | 1659 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|         - | 1660 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|         - | 1661 | `			 * a global-scope closure is silently dropped at install. A static` |
|         - | 1662 | `			 * closure never binds $this (php). */` |
|       561 | 1663 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       561 | 1664 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|       561 | 1665 | `			sEnv.nIdx = SXU32_HIGH;` |
|       561 | 1666 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       561 | 1667 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|       561 | 1668 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       278 | 1669 | `		}` |
|       583 | 1670 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|         - | 1671 | `			/* Mark as closure */` |
|       563 | 1672 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       279 | 1673 | `		}` |
|       289 | 1674 | `	}` |
|         - | 1675 | `	/* Compile the body */` |
|    521611 | 1676 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|    521611 | 1677 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 1678 | `		return SXERR_ABORT;` |
|         - | 1679 | `	}` |
|         - | 1680 | `	/* The cursor sits just past the body's closing brace */` |
|    521611 | 1681 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|    521611 | 1682 | `	if( ppFunc ){` |
|    521611 | 1683 | `		*ppFunc = pFunc;` |
|    260803 | 1684 | `	}` |
|    521611 | 1685 | `	rc = SXRET_OK;` |
|    521611 | 1686 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|         - | 1687 | `		/* Reject a php-fatal redeclaration before hoisting the function */` |
|    521053 | 1688 | `		if( GenStateGuardFuncRedeclaration(pGen,pFunc) == SXERR_ABORT ){` |
|         6 | 1689 | `			return SXERR_ABORT;` |
|         - | 1690 | `		}` |
|         - | 1691 | `		/* Finally register the function */` |
|    521049 | 1692 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    260522 | 1693 | `	}` |
|    521607 | 1694 | `	if( rc == SXRET_OK ){` |
|    521607 | 1695 | `		return SXRET_OK;` |
|         - | 1696 | `	}` |
|         - | 1697 | `	/* Fall through if something goes wrong */` |
|       ! 0 | 1698 | `OutOfMem:` |
|         - | 1699 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - | 1700 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|         - | 1701 | `	 */` |
|       ! 0 | 1702 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 | 1703 | `	return SXERR_ABORT;` |
|    260815 | 1704 | `}` |
|         - | 1705 | `/*` |
|         - | 1706 | ` * Compile a standard PHP function.` |
|         - | 1707 | ` *  Refer to the block-comment above for more information.` |
|         - | 1708 | ` */` |
|    521050 | 1709 | `PH7_PRIVATE sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|         5 | 1710 | `{` |
|         - | 1711 | `	SyString *pName;` |
|         - | 1712 | `	sxi32 iFlags;` |
|         - | 1713 | `	sxu32 nKwLine;` |
|         - | 1714 | `	sxu32 nLine;` |
|         - | 1715 | `	sxi32 rc;` |
|         - | 1716 |  |
|    521055 | 1717 | `	nLine = pGen->pIn->nLine;` |
|    521055 | 1718 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    521055 | 1719 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|    521055 | 1720 | `	iFlags = 0;` |
|    521055 | 1721 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - | 1722 | `		/* Return by reference,remember that */` |
|        12 | 1723 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|         - | 1724 | `		/* Jump the '&' token */` |
|        12 | 1725 | `		pGen->pIn++;` |
|         5 | 1726 | `	}` |
|    521055 | 1727 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - | 1728 | `		/* Invalid function name */` |
|         7 | 1729 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         7 | 1730 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1731 | `			return SXERR_ABORT;` |
|         - | 1732 | `		}` |
|         - | 1733 | `		/* Sychronize with the next semi-colon or braces*/` |
|        21 | 1734 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        15 | 1735 | `			pGen->pIn++;` |
|         1 | 1736 | `		}` |
|         7 | 1737 | `		return SXRET_OK;` |
|         - | 1738 | `	}` |
|    521049 | 1739 | `	pName = &pGen->pIn->sData;` |
|    521049 | 1740 | `	nLine = pGen->pIn->nLine;` |
|         - | 1741 | `	/* Jump the function name */` |
|    521049 | 1742 | `	pGen->pIn++;` |
|    521049 | 1743 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 1744 | `		/* Syntax error */` |
|         3 | 1745 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         3 | 1746 | `		if( rc == SXERR_ABORT ){` |
|         - | 1747 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 1748 | `			return SXERR_ABORT;` |
|         - | 1749 | `		}` |
|         - | 1750 | `		/* Sychronize with the next semi-colon or '{' */` |
|         3 | 1751 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 | 1752 | `			pGen->pIn++;` |
|       ! 0 | 1753 | `		}` |
|         3 | 1754 | `		return SXRET_OK;` |
|         - | 1755 | `	}` |
|         - | 1756 | `	/* Compile function body */` |
|         - | 1757 | `	{` |
|    521047 | 1758 | `		ph7_vm_func *pFuncState = 0;` |
|    521047 | 1759 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|    521047 | 1760 | `		if( pFuncState ){` |
|         - | 1761 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|    521033 | 1762 | `			pFuncState->nLine = nKwLine;` |
|    260514 | 1763 | `		}` |
|         - | 1764 | `	}` |
|    521047 | 1765 | `	return rc;` |
|    260530 | 1766 | `}` |
|         - | 1767 |  |
