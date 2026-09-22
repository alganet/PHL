# src/ph7/compile_func.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 927/1079 lines (85.91%)

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
|    815264 |   68 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|         5 |   69 | `{` |
|         - |   70 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |   71 | `	SySet *pInstrContainer;` |
|         - |   72 | `	sxi32 rc;` |
|         - |   73 | `	/* Swap token stream */` |
|    815269 |   74 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|    815269 |   75 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    815269 |   76 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|         - |   77 | `	/* Compile the expression holding the argument value. A parameter default is a` |
|         - |   78 | `	 * const-expression belonging to the current class (see iInMemberDefault) — so` |
|         - |   79 | `	 * __TRAIT__ in it reads pCurClass rather than walking into the enclosing method. */` |
|    815269 |   80 | `	pGen->iInMemberDefault++;` |
|    815269 |   81 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    815269 |   82 | `	pGen->iInMemberDefault--;` |
|         - |   83 | `	/* Emit the done instruction */` |
|    815269 |   84 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    815269 |   85 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    815269 |   86 | `	RE_SWAP_DELIMITER(pGen);` |
|    815269 |   87 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |   88 | `		return SXERR_ABORT;` |
|         - |   89 | `	}` |
|    815269 |   90 | `	return SXRET_OK;` |
|    407637 |   91 | `}` |
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
|   1782144 |  129 | `PH7_PRIVATE sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|         5 |  130 | `{` |
|         - |  131 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|         - |  132 | `	SyToken *pIn;  /* Token stream */` |
|         - |  133 | `	SyBlob sSig;         /* Function signature */` |
|         - |  134 | `	char *zDup;          /* Copy of argument name */` |
|         - |  135 | `	sxi32 rc;` |
|         - |  136 |  |
|   1782149 |  137 | `	pIn = pGen->pIn;` |
|   1782149 |  138 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|         - |  139 | `	/* Process arguments one after one */` |
|   2314678 |  140 | `	for(;;){` |
|   4629361 |  141 | `		if( pIn >= pEnd ){` |
|         - |  142 | `			/* No more arguments to process */` |
|   1782131 |  143 | `			break;` |
|         - |  144 | `		}` |
|   2847235 |  145 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   2847235 |  146 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   2847235 |  147 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   2847235 |  148 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   2847235 |  149 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|         - |  150 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|         - |  151 | `		 * first token inside the main token stream */` |
|   2847235 |  152 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  153 | `			return SXERR_ABORT;` |
|         - |  154 | `		}` |
|         - |  155 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|         - |  156 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|         - |  157 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|         - |  158 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|         - |  159 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|         - |  160 | `		{` |
|   2847235 |  161 | `			int bReadonly = 0, bVisSeen = 0;` |
|   2847235 |  162 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   2847235 |  163 | `			sxi32 iSetVisFlag = 0;` |
|         - |  164 | `			int nSetTok;` |
|         - |  165 | `			sxi32 nSetVis;` |
|   2847235 |  166 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|         3 |  167 | `				bReadonly = 1;` |
|         3 |  168 | `				pIn++;` |
|         1 |  169 | `			}` |
|   2847235 |  170 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   2847235 |  171 | `			if( nSetVis ){` |
|         - |  172 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|         3 |  173 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  174 | `				bVisSeen = 1;` |
|         3 |  175 | `				pIn += nSetTok;` |
|         3 |  176 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       ! 0 |  177 | `					bReadonly = 1;` |
|       ! 0 |  178 | `					pIn++;` |
|         1 |  179 | `				}` |
|   2847234 |  180 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|    122921 |  181 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|    122921 |  182 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|        99 |  183 | `					bVisSeen = 1;` |
|        99 |  184 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|       131 |  185 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|        42 |  186 | `						: PH7_CLASS_PROT_PUBLIC;` |
|        99 |  187 | `					pIn++;` |
|        99 |  188 | `					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|        99 |  189 | `					if( nSetVis ){` |
|         - |  190 | ``						/* `public private(set) T $x` promoted form */`` |
|         3 |  191 | `						iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  192 | `						pIn += nSetTok;` |
|         1 |  193 | `					}` |
|        99 |  194 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        18 |  195 | `						bReadonly = 1;` |
|        18 |  196 | `						pIn++;` |
|         7 |  197 | `					}` |
|        47 |  198 | `				}` |
|     61458 |  199 | `			}` |
|   2847235 |  200 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|         5 |  201 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   2847233 |  202 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|       ! 0 |  203 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|       ! 0 |  204 | `			}` |
|   2847235 |  205 | `			if( bVisSeen \|\| bReadonly ){` |
|       103 |  206 | `				if( !bCtorCtx ){` |
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
|        99 |  219 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|        99 |  220 | `				sArg.iPromoteVis = iVis;` |
|        99 |  221 | `				if( bReadonly ){` |
|        20 |  222 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|         8 |  223 | `				}` |
|        47 |  224 | `			}` |
|         - |  225 | `		}` |
|         - |  226 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   2847226 |  227 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   1539656 |  228 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|    218452 |  229 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    159441 |  230 | `			sxu32 nLineLocal = pIn->nLine;` |
|    159441 |  231 | `			sxi32 iTFlags = 0;` |
|    159441 |  232 | `			pGen->pIn = pIn;` |
|    159441 |  233 | `			rc = GenStateParseUnionTypeDecl(` |
|     79718 |  234 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|     79718 |  235 | `				&iTFlags, &sArg.sTypeName,` |
|         - |  236 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|         - |  237 | `				/* bAllowVoid */ 0,` |
|     79718 |  238 | `						nLineLocal);` |
|    159441 |  239 | `			pIn = pGen->pIn;` |
|    159441 |  240 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  241 | `				return SXERR_ABORT;` |
|    159441 |  242 | `			}else if( rc == SXERR_CORRUPT ){` |
|         - |  243 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|         3 |  244 | `				return SXERR_SYNTAX;` |
|    159439 |  245 | `			}else if( rc == SXERR_SYNTAX ){` |
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
|    159431 |  256 | `			sArg.iFlags \|= iTFlags;` |
|     79713 |  257 | `		}` |
|   2847221 |  258 | `		if( pIn >= pEnd ){` |
|       ! 0 |  259 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|       ! 0 |  260 | `			return rc;` |
|         - |  261 | `		}` |
|   2847221 |  262 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|         - |  263 | `			/* Pass by reference,record that */` |
|     27291 |  264 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     27291 |  265 | `			pIn++;` |
|     13643 |  266 | `		}` |
|   2847221 |  267 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|         - |  268 | `			/* Variadic parameter: ...$args */` |
|     45461 |  269 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|     45461 |  270 | `			pIn++;` |
|     22728 |  271 | `		}` |
|   2847221 |  272 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  273 | `			/* Invalid argument */` |
|       ! 0 |  274 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|       ! 0 |  275 | `			return rc;` |
|         - |  276 | `		}` |
|   2847221 |  277 | `		pIn++; /* Jump the dollar sign */` |
|         - |  278 | `		/* Copy argument name */` |
|   2847221 |  279 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   2847221 |  280 | `		if( zDup == 0 ){` |
|       ! 0 |  281 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  282 | `			return SXERR_ABORT;` |
|         - |  283 | `		}` |
|   2847221 |  284 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   2847221 |  285 | `		pIn++;` |
|   2847221 |  286 | `		if( pIn < pEnd ){` |
|   1581437 |  287 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|         - |  288 | `				SyToken *pDefend;` |
|    815271 |  289 | `				sxi32 iNest = 0;` |
|    815271 |  290 | `				pIn++; /* Jump the equal sign */` |
|    815271 |  291 | `				pDefend = pIn;` |
|         - |  292 | `				/* Process the default value associated with this argument */` |
|   1698503 |  293 | `				while( pDefend < pEnd ){` |
|   1182163 |  294 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    298931 |  295 | `						break;` |
|         - |  296 | `					}` |
|    883237 |  297 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|         - |  298 | `						/* Increment nesting level */` |
|     27187 |  299 | `						iNest++;` |
|    869646 |  300 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|         - |  301 | `						/* Decrement nesting level */` |
|     27187 |  302 | `						iNest--;` |
|     13591 |  303 | `					}` |
|    883237 |  304 | `					pDefend++;` |
|         5 |  305 | `				}` |
|    815271 |  306 | `				if( pIn >= pDefend ){` |
|         3 |  307 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|         3 |  308 | `					return rc;` |
|         - |  309 | `				}` |
|         - |  310 | `				/* Process default value */` |
|    815269 |  311 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|    815269 |  312 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  313 | `					return rc;` |
|         - |  314 | `				}` |
|         - |  315 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|         - |  316 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|         - |  317 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|         - |  318 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|         - |  319 | `				 * arg-type check lets null through. */` |
|    815264 |  320 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|    439358 |  321 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|    430298 |  322 | `					&& &pIn[1] == pDefend` |
|     38539 |  323 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|     22669 |  324 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|      6803 |  325 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
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
|    815267 |  348 | `				pIn = pDefend;` |
|    407631 |  349 | `			}` |
|   1581433 |  350 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  351 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|       ! 0 |  352 | `				return rc;` |
|         - |  353 | `			}` |
|   1581433 |  354 | `			pIn++; /* Jump the trailing comma */` |
|    790714 |  355 | `		}` |
|         - |  356 | `		/* Append argument signature */` |
|   2847217 |  357 | `		if( sArg.nType > 0 ){` |
|    159303 |  358 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|         - |  359 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|     31937 |  360 | `				int marker = 'o';` |
|     31937 |  361 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|     31937 |  362 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     15971 |  363 | `			}else{` |
|         - |  364 | `				int c;` |
|    127371 |  365 | `				c = 'n'; /* cc warning */` |
|         - |  366 | `				/* Type leading character */` |
|    127371 |  367 | `				switch(sArg.nType){` |
|      6812 |  368 | `				case MEMOBJ_HASHMAP:` |
|         - |  369 | `					/* Hashmap aka 'array' */` |
|     13629 |  370 | `					c = 'h';` |
|     13629 |  371 | `					break;` |
|     20544 |  372 | `				case MEMOBJ_INT:` |
|         - |  373 | `					/* Integer */` |
|     41093 |  374 | `					c = 'i';` |
|     41093 |  375 | `					break;` |
|         4 |  376 | `				case MEMOBJ_BOOL:` |
|         - |  377 | `					/* Bool */` |
|        11 |  378 | `					c = 'b';` |
|        11 |  379 | `					break;` |
|         8 |  380 | `				case MEMOBJ_REAL:` |
|         - |  381 | `					/* Float */` |
|        20 |  382 | `					c = 'f';` |
|        20 |  383 | `					break;` |
|     36303 |  384 | `				case MEMOBJ_STRING:` |
|         - |  385 | `					/* String */` |
|     72611 |  386 | `					c = 's';` |
|     72611 |  387 | `					break;` |
|        11 |  388 | `				case MEMOBJ_OBJ:` |
|         - |  389 | `					/* Object */` |
|        25 |  390 | `					c = 'o';` |
|        22 |  391 | `					break;` |
|         1 |  392 | `				default:` |
|         2 |  393 | `					break;` |
|         - |  394 | `				}` |
|    127371 |  395 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|         - |  396 | `			}` |
|     79654 |  397 | `		}else{` |
|         - |  398 | `			/* No type is associated with this parameter which mean` |
|         - |  399 | `			 * that this function is not condidate for overloading.` |
|         - |  400 | `			 */` |
|   2687919 |  401 | `			SyBlobRelease(&sSig);` |
|         - |  402 | `		}` |
|         - |  403 | `		/* Save in the argument set */` |
|   2847217 |  404 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|         5 |  405 | `	}` |
|   1782131 |  406 | `	if( SyBlobLength(&sSig) > 0 ){` |
|         - |  407 | `		/* Save function signature */` |
|     95767 |  408 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|     47881 |  409 | `	}` |
|   1782131 |  410 | `	return SXRET_OK;` |
|    891077 |  411 | `}` |
|         - |  412 | `/*` |
|         - |  413 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|         - |  414 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|         - |  415 | ` * the enclosing function. Returns the token just past the nested construct.` |
|         - |  416 | ` */` |
|     40854 |  417 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|         5 |  418 | `{` |
|     40859 |  419 | `	sxi32 iParen = 0;` |
|     40859 |  420 | `	pIn++; /* past 'function'/'fn' */` |
|         - |  421 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|         - |  422 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|         - |  423 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|    181629 |  424 | `	while( pIn < pEnd ){` |
|    181629 |  425 | `		sxu32 t = pIn->nType;` |
|    181629 |  426 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|    176981 |  427 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|    122525 |  428 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|     99783 |  429 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|    140775 |  430 | `		pIn++;` |
|         5 |  431 | `	}` |
|     22747 |  432 | `	if( pIn >= pEnd ){ return pIn; }` |
|         - |  433 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|         - |  434 | `	{` |
|     22747 |  435 | `		sxi32 d = 0;` |
|    902053 |  436 | `		while( pIn < pEnd ){` |
|    902053 |  437 | `			sxu32 t = pIn->nType;` |
|    902053 |  438 | `			if( t & PH7_TK_OCB ){ d++; }` |
|    865719 |  439 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|    879311 |  440 | `			pIn++;` |
|         5 |  441 | `		}` |
|         - |  442 | `	}` |
|     22747 |  443 | `	return pIn;` |
|     20432 |  444 | `}` |
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
|     13612 |  458 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|         5 |  459 | `{` |
|         - |  460 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|         - |  461 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|         - |  462 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|         - |  463 | `	};` |
|         - |  464 | `	sxu32 i;` |
|     13617 |  465 | `	if( nName > 0 && zName[0] == '\\' ){` |
|       ! 0 |  466 | `		zName++;` |
|       ! 0 |  467 | `		nName--;` |
|       ! 0 |  468 | `	}` |
|     13631 |  469 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|     13631 |  470 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|     13617 |  471 | `			return 1;` |
|         - |  472 | `		}` |
|         9 |  473 | `	}` |
|       ! 0 |  474 | `	return 0;` |
|      6811 |  475 | `}` |
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
|     13614 |  488 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|         5 |  489 | `{` |
|     13619 |  490 | `	if( nType == MEMOBJ_OBJ ){` |
|       ! 0 |  491 | ``		return 1; /* bare `object` */`` |
|         - |  492 | `	}` |
|     13619 |  493 | `	if( nType != SXU32_HIGH ){` |
|         3 |  494 | `		return 0; /* scalar/array/void/never/null/... */` |
|         - |  495 | `	}` |
|     13617 |  496 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|     13617 |  497 | `		return 1;` |
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
|      6812 |  512 | `}` |
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
|     13894 |  523 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  524 | `{` |
|     13899 |  525 | `	int bOk = 0;` |
|         - |  526 | `	sxu32 nLine;` |
|         - |  527 | `	sxi32 rc;` |
|     13899 |  528 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|       285 |  529 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|         - |  530 | `	}` |
|     13619 |  531 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
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
|     13619 |  550 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|         - |  551 | `	}` |
|     13619 |  552 | `	if( bOk ){` |
|     13617 |  553 | `		return SXRET_OK;` |
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
|      6952 |  579 | `}` |
|   3714488 |  580 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|         5 |  581 | `{` |
|   3714493 |  582 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|   3714493 |  583 | `	SyToken *pEnd = pGen->pEnd;` |
|   3714493 |  584 | `	sxi32 iDepth = 0;` |
|   3714493 |  585 | `	int bStarted = 0;` |
| 175998161 |  586 | `	while( pIn < pEnd ){` |
| 175998161 |  587 | `		sxu32 t = pIn->nType;` |
| 175998161 |  588 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 168197793 |  589 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 160438621 |  590 | `		if( t & PH7_TK_KEYWORD ){` |
|  11904083 |  591 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  11904083 |  592 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  11890189 |  593 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|         - |  594 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|   5924665 |  595 | `		}` |
| 160383873 |  596 | `		pIn++;` |
|         5 |  597 | `	}` |
|   3700599 |  598 | `	return FALSE;` |
|   1857249 |  599 | `}` |
|         - |  600 | `/*` |
|         - |  601 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|         - |  602 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  603 | ` * and this routine takes care of generating the appropriate error message.` |
|         - |  604 | ` */` |
|   3714488 |  605 | `PH7_PRIVATE sxi32 GenStateCompileFuncBody(` |
|         - |  606 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  607 | `	ph7_vm_func *pFunc    /* Function state */` |
|         - |  608 | `	)` |
|         5 |  609 | `{` |
|         - |  610 | `	SySet *pInstrContainer; /* Instruction container */` |
|         - |  611 | `	GenBlock *pBlock;` |
|         - |  612 | `	sxu32 nGotoOfft;` |
|         - |  613 | `	sxi32 rc;` |
|         - |  614 | `	/* Attach the new function */` |
|   3714493 |  615 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   3714493 |  616 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  617 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|         - |  618 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  619 | `		return SXERR_ABORT;` |
|         - |  620 | `	}` |
|   3714493 |  621 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|         - |  622 | `	/* Swap bytecode containers */` |
|   3714493 |  623 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   3714493 |  624 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|         - |  625 | `	/* Emit constructor property promotion prologue:` |
|         - |  626 | `	 *   $this->NAME = $NAME;` |
|         - |  627 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|         - |  628 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|         - |  629 | `	{` |
|   3714493 |  630 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|         - |  631 | `		sxu32 i;` |
|   6498027 |  632 | `		for( i = 0; i < nArg; i++ ){` |
|   2783539 |  633 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|         - |  634 | `			char *zSrc;` |
|         - |  635 | `			sxu32 nSrc,nName;` |
|         - |  636 | `			SySet sToken;` |
|         - |  637 | `			SyToken *pTmpIn,*pTmpEnd;` |
|         - |  638 | `			sxi32 rcPromote;` |
|   2783539 |  639 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   2783455 |  640 | `				continue;` |
|         - |  641 | `			}` |
|         - |  642 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|         - |  643 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|         - |  644 | `			 * copied), so it must outlive the function — never free it. The` |
|         - |  645 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|         - |  646 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|        89 |  647 | `			nName = SyStringLength(&pArg->sName);` |
|        89 |  648 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|        89 |  649 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|        89 |  650 | `			if( zSrc == 0 ){` |
|       ! 0 |  651 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  652 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  653 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  654 | `				return SXERR_ABORT;` |
|         - |  655 | `			}` |
|         - |  656 | `			{` |
|        89 |  657 | `				char *z = zSrc;` |
|        89 |  658 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|        89 |  659 | `				z += sizeof("$this->")-1;` |
|        89 |  660 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        89 |  661 | `				z += nName;` |
|        89 |  662 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|        89 |  663 | `				z += sizeof(" = $")-1;` |
|        89 |  664 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        89 |  665 | `				z += nName;` |
|        89 |  666 | `				*z = 0;` |
|         - |  667 | `			}` |
|        89 |  668 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        89 |  669 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|        89 |  670 | `			pTmpIn = pGen->pIn;` |
|        89 |  671 | `			pTmpEnd = pGen->pEnd;` |
|        89 |  672 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|        89 |  673 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        89 |  674 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|        89 |  675 | `			pGen->pIn = pTmpIn;` |
|        89 |  676 | `			pGen->pEnd = pTmpEnd;` |
|        89 |  677 | `			SySetRelease(&sToken);` |
|        89 |  678 | `			if( rcPromote == SXERR_ABORT ){` |
|       ! 0 |  679 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  680 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  681 | `				return SXERR_ABORT;` |
|         - |  682 | `			}` |
|         - |  683 | `			/* Discard the assignment result — this is a statement expression. */` |
|        89 |  684 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        47 |  685 | `		}` |
|         - |  686 | `	}` |
|         - |  687 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|         - |  688 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|         - |  689 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|         - |  690 | `	 * generator — and vice versa — is classified independently. */` |
|         - |  691 | `	{` |
|   3714493 |  692 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|   3714493 |  693 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|         - |  694 | `		/* Compile the body */` |
|   3714493 |  695 | `		PH7_CompileBlock(&(*pGen),0);` |
|   3714493 |  696 | `		pGen->bInGenerator = bSavedGen;` |
|         - |  697 | `	}` |
|         - |  698 | `	/* Fix exception jumps now the destination is resolved */` |
|   3714493 |  699 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - |  700 | `	/* Emit the final return if not yet done */` |
|   3714493 |  701 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - |  702 | `	/* Fix gotos jumps now the destination is resolved */` |
|   3714493 |  703 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|       ! 0 |  704 | `		rc = SXERR_ABORT;` |
|       ! 0 |  705 | `	}` |
|   3714493 |  706 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|         - |  707 | `	/* Restore the default container */` |
|   3714493 |  708 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - |  709 | `	/* Leave function block */` |
|   3714493 |  710 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   3714493 |  711 | `	if( rc == SXERR_ABORT ){` |
|         - |  712 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  713 | `		return SXERR_ABORT;` |
|         - |  714 | `	}` |
|         - |  715 | `	/* Scan for yield opcodes to detect generator functions */` |
|         - |  716 | `	{` |
|   3714493 |  717 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|         - |  718 | `		sxu32 i;` |
| 107964589 |  719 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 104263995 |  720 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     13899 |  721 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     13899 |  722 | `				break;` |
|         - |  723 | `			}` |
|  52125053 |  724 | `		}` |
|         - |  725 | `	}` |
|   3714493 |  726 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|         - |  727 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|     13899 |  728 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|       ! 0 |  729 | `			return SXERR_ABORT;` |
|         - |  730 | `		}` |
|      6947 |  731 | `	}` |
|         - |  732 | `	/* All done, function body compiled */` |
|   3714493 |  733 | `	return SXRET_OK;` |
|   1857249 |  734 | `}` |
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
|       748 |  755 | `PH7_PRIVATE int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|         5 |  756 | `{` |
|         - |  757 | `	sxu32 i;` |
|      2031 |  758 | `	for( i = 0; i < n; i++ ){` |
|      1743 |  759 | `		int a = zA[i], b = zB[i];` |
|      1743 |  760 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      1743 |  761 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      1743 |  762 | `		if( a != b ) return a - b;` |
|       644 |  763 | `	}` |
|       293 |  764 | `	return 0;` |
|       379 |  765 | `}` |
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
|     45858 |  805 | `static int GenStateIsReservedTypeWord(const SyString *pName)` |
|         5 |  806 | `{` |
|         - |  807 | `	static const char *azWords[] = {` |
|         - |  808 | `		"false","true","mixed","iterable","callable","null","void","never",` |
|         - |  809 | `		"bool","boolean","int","integer","float","double","string","array",` |
|         - |  810 | `		"object","self","static","parent"` |
|         - |  811 | `	};` |
|         - |  812 | `	sxu32 i;` |
|    958613 |  813 | `	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){` |
|    913009 |  814 | `		sxu32 n = (sxu32)SyStrlen(azWords[i]);` |
|    913009 |  815 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azWords[i],n) == 0 ){` |
|       259 |  816 | `			return 1;` |
|         - |  817 | `		}` |
|    456380 |  818 | `	}` |
|     45609 |  819 | `	return 0;` |
|     22934 |  820 | `}` |
|    215696 |  821 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|         5 |  822 | `{` |
|    215701 |  823 | `	SyToken *pIn = pGen->pIn;` |
|    215701 |  824 | `	int bAbsolute = 0;` |
|    215701 |  825 | `	SyZero(pOut, sizeof(*pOut));` |
|    215701 |  826 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    215701 |  827 | `	if( pIn >= pGen->pEnd ){` |
|       ! 0 |  828 | `		return SXERR_SYNTAX;` |
|         - |  829 | `	}` |
|         - |  830 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    215701 |  831 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|        12 |  832 | `		bAbsolute = 1; /* fully-qualified: never prefix the current namespace */` |
|        12 |  833 | `		pIn++;` |
|        12 |  834 | `		if( pIn >= pGen->pEnd ){` |
|       ! 0 |  835 | `			return SXERR_SYNTAX;` |
|         - |  836 | `		}` |
|         5 |  837 | `	}` |
|         - |  838 | ``	/* `namespace\X` type hint: the CURRENT namespace spelled out, fully qualified`` |
|         - |  839 | `` 	 * from there. Collected here rather than below because the leading `namespace` `` |
|         - |  840 | `	 * is a KEYWORD token, which the atom parser would otherwise reject outright. */` |
|    215701 |  841 | `	if( !bAbsolute ){` |
|         - |  842 | `		SyBlob sRel;` |
|    215691 |  843 | `		SyBlobInit(&sRel,&pGen->pVm->sAllocator);` |
|    215691 |  844 | `		if( GenStateNsRelPrefix(pGen,&pIn,pGen->pEnd,&sRel) ){` |
|         - |  845 | `			char *zDup;` |
|         7 |  846 | `			SyBlobAppend(&sRel,pIn->sData.zString,pIn->sData.nByte);` |
|         7 |  847 | `			pIn++;` |
|         9 |  848 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|         7 |  849 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       ! 0 |  850 | `				SyBlobAppend(&sRel,"\\",1);` |
|       ! 0 |  851 | `				SyBlobAppend(&sRel,pIn[1].sData.zString,pIn[1].sData.nByte);` |
|       ! 0 |  852 | `				pIn += 2;` |
|       ! 0 |  853 | `			}` |
|        10 |  854 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         6 |  855 | `				(const char *)SyBlobData(&sRel),SyBlobLength(&sRel));` |
|         7 |  856 | `			if( zDup == 0 ){` |
|       ! 0 |  857 | `				SyBlobRelease(&sRel);` |
|       ! 0 |  858 | `				return SXERR_ABORT;` |
|         - |  859 | `			}` |
|         7 |  860 | `			pOut->nType = SXU32_HIGH;` |
|         7 |  861 | `			SyStringInitFromBuf(&pOut->sClass,zDup,SyBlobLength(&sRel));` |
|         7 |  862 | `			SyBlobRelease(&sRel);` |
|         7 |  863 | `			pGen->pIn = pIn;` |
|         7 |  864 | `			return SXRET_OK;` |
|         - |  865 | `		}` |
|    215685 |  866 | `		SyBlobRelease(&sRel);` |
|    107840 |  867 | `	}` |
|    215695 |  868 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  869 | `		return SXERR_SYNTAX;` |
|         - |  870 | `	}` |
|    215695 |  871 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|    169545 |  872 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|    169545 |  873 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|     18257 |  874 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|    160419 |  875 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      9155 |  876 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|    146718 |  877 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|     55301 |  878 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|    114495 |  879 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|     86651 |  880 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|     43524 |  881 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|        61 |  882 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|       173 |  883 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|        41 |  884 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|       127 |  885 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|        44 |  886 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       106 |  887 | `			pOut->nType = SXU32_HIGH;` |
|       106 |  888 | `			pOut->sClass = pIn->sData;` |
|        55 |  889 | `		}else{` |
|         3 |  890 | `			return SXERR_SYNTAX;` |
|         - |  891 | `		}` |
|    169543 |  892 | `		pIn++;` |
|     84774 |  893 | `	}else{` |
|         - |  894 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|         - |  895 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|     46155 |  896 | `		SyString *pT = &pIn->sData;` |
|     46155 |  897 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|        36 |  898 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|        36 |  899 | `			pIn++;` |
|     46139 |  900 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|       227 |  901 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|       227 |  902 | `			pIn++;` |
|     46012 |  903 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|        33 |  904 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|        33 |  905 | `			pIn++;` |
|        19 |  906 | `		}else{` |
|         - |  907 | `			/* Class / interface name; consume namespace path a\b\c */` |
|     45873 |  908 | `			SyToken *pFirst = pIn;` |
|     45873 |  909 | `			SyToken *pLast = pIn;` |
|     45873 |  910 | `			pOut->nType = SXU32_HIGH;` |
|     45873 |  911 | `			pOut->sClass = pIn->sData;` |
|     45873 |  912 | `			pIn++;` |
|     68805 |  913 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|     45876 |  914 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|         3 |  915 | `				pLast = &pIn[1];` |
|         3 |  916 | `				pIn += 2;` |
|         1 |  917 | `			}` |
|     45873 |  918 | `			if( pLast != pFirst ){` |
|         3 |  919 | `				const char *zFirst = pFirst->sData.zString;` |
|         3 |  920 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|         3 |  921 | `				pOut->sClass.zString = zFirst;` |
|         3 |  922 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|         1 |  923 | `			}` |
|         - |  924 | `			/* Namespace-qualify a bare (single-segment, non-absolute) class type so` |
|         - |  925 | ``			 * a `: Base` / `Base $x` hint in namespace N resolves to N\Base (or a`` |
|         - |  926 | ``			 * `use` alias) at type-check time instead of the global \Base — mirrors`` |
|         - |  927 | `			 * the NEW/CALL/instanceof qualification. Absolute (\Base) and already-` |
|         - |  928 | `			 * qualified (A\B) names are left as written, matching GenStateNsQualifyName. */` |
|         - |  929 | `			/* Reserved type words that reach this identifier branch (false, true,` |
|         - |  930 | `			 * mixed, iterable, callable) are NOT classes and must not be qualified` |
|         - |  931 | ``			 * (else `false\|string` becomes `Ns\false\|string`). */`` |
|     45873 |  932 | `			if( !bAbsolute && pLast == pFirst && !GenStateIsReservedTypeWord(&pOut->sClass) ){` |
|         - |  933 | `				SyBlob sFqn;` |
|     45609 |  934 | `				SyBlobInit(&sFqn,&pGen->pVm->sAllocator);` |
|     45609 |  935 | `				GenStateResolveName(pGen,&pOut->sClass,&sFqn);` |
|     45604 |  936 | `				if( SyBlobLength(&sFqn) != pOut->sClass.nByte` |
|     45602 |  937 | `				 \|\| SyMemcmp(SyBlobData(&sFqn),(const void *)pOut->sClass.zString,pOut->sClass.nByte) != 0 ){` |
|        24 |  938 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        14 |  939 | `						(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));` |
|        17 |  940 | `					if( zDup ){` |
|        17 |  941 | `						SyStringInitFromBuf(&pOut->sClass,zDup,SyBlobLength(&sFqn));` |
|         7 |  942 | `					}` |
|         7 |  943 | `				}` |
|     45609 |  944 | `				SyBlobRelease(&sFqn);` |
|     22802 |  945 | `			}` |
|         - |  946 | `		}` |
|         - |  947 | `	}` |
|    215693 |  948 | `	pGen->pIn = pIn;` |
|    215693 |  949 | `	return SXRET_OK;` |
|    107853 |  950 | `}` |
|         - |  951 |  |
|         - |  952 | `/* Class-name ATOMS that php files with the BUILT-IN types rather than the` |
|         - |  953 | `` * classes. `callable`, `false` and `true` are type-mask bits in zend, so they`` |
|         - |  954 | `` * print AFTER every class name however they were written (`false\|A1` reads`` |
|         - |  955 | `` * `A1\|false`). `iterable` is two types at once: the class `Traversable`, which`` |
|         - |  956 | `` * keeps the atom's declaration position among the classes, plus `array`, which`` |
|         - |  957 | `` * prints with the builtins (`iterable\|A1` reads `Traversable\|A1\|array`). PH7`` |
|         - |  958 | ` * parses all four as class-name atoms, which is why they used to sort with the` |
|         - |  959 | ` * classes. */` |
|         - |  960 | `#define GEN_ATOM_PLAIN    0` |
|         - |  961 | `#define GEN_ATOM_CALLABLE 1` |
|         - |  962 | `#define GEN_ATOM_FALSE    2` |
|         - |  963 | `#define GEN_ATOM_TRUE     3` |
|         - |  964 | `#define GEN_ATOM_ITERABLE 4` |
|    785328 |  965 | `static int GenAtomMaskKind(const PhlTypeAtom *pAtom)` |
|         5 |  966 | `{` |
|    785333 |  967 | `	const SyString *p = &pAtom->sClass;` |
|    785333 |  968 | `	if( pAtom->nType != SXU32_HIGH \|\| p->zString == 0 ){` |
|    623775 |  969 | `		return GEN_ATOM_PLAIN;` |
|         - |  970 | `	}` |
|    161563 |  971 | `	if( p->nByte == 8 && SyStrnicmp(p->zString,"callable",8) == 0 ) return GEN_ATOM_CALLABLE;` |
|    161065 |  972 | `	if( p->nByte == 5 && SyStrnicmp(p->zString,"false",5) == 0 )    return GEN_ATOM_FALSE;` |
|    160913 |  973 | `	if( p->nByte == 4 && SyStrnicmp(p->zString,"true",4) == 0 )     return GEN_ATOM_TRUE;` |
|    160853 |  974 | `	if( p->nByte == 8 && SyStrnicmp(p->zString,"iterable",8) == 0 ) return GEN_ATOM_ITERABLE;` |
|    160691 |  975 | `	return GEN_ATOM_PLAIN;` |
|    392669 |  976 | `}` |
|         - |  977 | ``/* Emit one class-like atom: when *bExpandIterable*, `iterable` contributes only`` |
|         - |  978 | `` * its Traversable half here (the `array` half rides the built-in pass). php`` |
|         - |  979 | `` * expands it only in a COMPOUND type — a standalone `iterable`/`?iterable` keeps`` |
|         - |  980 | ` * its name in the canonical text (which is what Reflection prints; the TypeError` |
|         - |  981 | ` * for a standalone hint is rendered separately, by VmClassHintTypeName). */` |
|     32190 |  982 | `static void GenAppendClassAtom(SyBlob *pBlob, const PhlTypeAtom *pAtom, int bExpandIterable)` |
|         5 |  983 | `{` |
|     32195 |  984 | `	if( bExpandIterable && GenAtomMaskKind(pAtom) == GEN_ATOM_ITERABLE ){` |
|        13 |  985 | `		SyBlobAppend(pBlob, "Traversable", sizeof("Traversable")-1);` |
|        13 |  986 | `		return;` |
|         - |  987 | `	}` |
|     32183 |  988 | `	SyBlobAppend(pBlob, pAtom->sClass.zString, pAtom->sClass.nByte);` |
|     16100 |  989 | `}` |
|         - |  990 | `/*` |
|         - |  991 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|         - |  992 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|         - |  993 | ` *   classes (in declaration order)` |
|         - |  994 | ` *   \| callable \| object \| array \| string \| int \| float \| bool \| false \| true` |
|         - |  995 | ` *   [\| null]` |
|         - |  996 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|         - |  997 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|         - |  998 | ` */` |
|    215406 |  999 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|         5 | 1000 | `{` |
|         - | 1001 | `	int i;` |
|    215411 | 1002 | `	int nNonNull = 0;` |
|    215411 | 1003 | `	int bAnyIntersection = 0;` |
|         - | 1004 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    215411 | 1005 | `	sxu32 nMaxGroup = 0;` |
|   7108403 | 1006 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    431081 | 1007 | `	for( i = 0; i < nAtoms; i++ ){` |
|    215675 | 1008 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    215643 | 1009 | `			nNonNull++;` |
|    215643 | 1010 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    215643 | 1011 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    215643 | 1012 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|    107819 | 1013 | `			}` |
|    107819 | 1014 | `		}` |
|    107840 | 1015 | `	}` |
|    431017 | 1016 | `	for( i = 0; i < nAtoms; i++ ){` |
|    215639 | 1017 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        32 | 1018 | `			bAnyIntersection = 1;` |
|        32 | 1019 | `			break;` |
|         - | 1020 | `		}` |
|    107808 | 1021 | `	}` |
|    215411 | 1022 | `	if( bAnyIntersection ){` |
|         - | 1023 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|         - | 1024 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|         - | 1025 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|        32 | 1026 | `		sxu32 g, nGroups = 0;` |
|        32 | 1027 | `		int bFirstGroup = 1;` |
|        70 | 1028 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|        70 | 1029 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|        42 | 1030 | `			int bFirstMember = 1;` |
|         - | 1031 | `			int bWrap;` |
|        42 | 1032 | `			if( aGroupCount[g] == 0 ) continue;` |
|         - | 1033 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|         - | 1034 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|         - | 1035 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|         - | 1036 | `			 * parens, matching PHP's canonical text. */` |
|        56 | 1037 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|        42 | 1038 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|        42 | 1039 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|       138 | 1040 | `			for( i = 0; i < nAtoms; i++ ){` |
|       100 | 1041 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|        70 | 1042 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|        70 | 1043 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|        66 | 1044 | `					GenAppendClassAtom(pBlob, &aAtoms[i], 1);` |
|        35 | 1045 | `				}else{` |
|         6 | 1046 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - | 1047 | `				}` |
|        70 | 1048 | `				bFirstMember = 0;` |
|        37 | 1049 | `			}` |
|        42 | 1050 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|        42 | 1051 | `			bFirstGroup = 0;` |
|        23 | 1052 | `		}` |
|         - | 1053 | ``		/* `iterable` printed its Traversable half in place; its `array` half goes`` |
|         - | 1054 | ``		 * last, as php does (`(I1&I2)\|iterable` reads `(I1&I2)\|Traversable\|array`). */`` |
|        96 | 1055 | `		for( i = 0; i < nAtoms; i++ ){` |
|        70 | 1056 | `			if( GenAtomMaskKind(&aAtoms[i]) == GEN_ATOM_ITERABLE ){` |
|         3 | 1057 | `				SyBlobAppend(pBlob, "\|", 1);` |
|         3 | 1058 | `				SyBlobAppend(pBlob, "array", sizeof("array")-1);` |
|         3 | 1059 | `				break;` |
|         - | 1060 | `			}` |
|        36 | 1061 | `		}` |
|        32 | 1062 | `		if( bNullable ){` |
|       ! 0 | 1063 | `			SyBlobAppend(pBlob, "\|", 1);` |
|       ! 0 | 1064 | `			SyBlobAppend(pBlob, "null", 4);` |
|       ! 0 | 1065 | `		}` |
|     11437 | 1066 | `		return;` |
|         - | 1067 | `	}` |
|    215383 | 1068 | `	if( nNonNull == 1 && bNullable ){` |
|         - | 1069 | `		/* Shorthand: ?T */` |
|     22815 | 1070 | `		for( i = 0; i < nAtoms; i++ ){` |
|     22815 | 1071 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     22815 | 1072 | `			SyBlobAppend(pBlob, "?", 1);` |
|     22815 | 1073 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|     13649 | 1074 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      6827 | 1075 | `			}else{` |
|      9171 | 1076 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - | 1077 | `			}` |
|     22815 | 1078 | `			return;` |
|       ! 0 | 1079 | `		}` |
|       ! 0 | 1080 | `	}` |
|         - | 1081 | `	{` |
|    192573 | 1082 | `		int bFirst = 1;` |
|         - | 1083 | `		/* 1) Classes in declaration order — minus the class-name atoms php counts` |
|         - | 1084 | ``		 * as built-in types (see GenAtomMaskKind); `iterable` leaves Traversable. */`` |
|    385353 | 1085 | `		for( i = 0; i < nAtoms; i++ ){` |
|         - | 1086 | `			int nKind;` |
|    192785 | 1087 | `			if( aAtoms[i].nType != SXU32_HIGH ) continue;` |
|     32275 | 1088 | `			nKind = GenAtomMaskKind(&aAtoms[i]);` |
|     32275 | 1089 | `			if( nKind == GEN_ATOM_CALLABLE \|\| nKind == GEN_ATOM_FALSE \|\| nKind == GEN_ATOM_TRUE ){` |
|       147 | 1090 | `				continue;` |
|         - | 1091 | `			}` |
|     32133 | 1092 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     32133 | 1093 | `			GenAppendClassAtom(pBlob, &aAtoms[i], nNonNull > 1);` |
|     32133 | 1094 | `			bFirst = 0;` |
|     16069 | 1095 | `		}` |
|         - | 1096 | `		/* 2) Built-ins in php's canonical order. A slot is filled either by a` |
|         - | 1097 | `		 * plain atom of that MEMOBJ_* type or by a class-name atom of the matching` |
|         - | 1098 | ``		 * kind; the `array` slot also takes `iterable`'s second half. */`` |
|         - | 1099 | `		{` |
|         - | 1100 | `			static const struct {` |
|         - | 1101 | `				sxu32 nType;        /* plain atom type, 0 when kind-only */` |
|         - | 1102 | `				int nKind;          /* GEN_ATOM_* atom, GEN_ATOM_PLAIN when type-only */` |
|         - | 1103 | `				const char *zText;` |
|         - | 1104 | `				sxu32 nText;` |
|         - | 1105 | `			} aOrder[] = {` |
|         - | 1106 | `				{ 0,               GEN_ATOM_CALLABLE, "callable", sizeof("callable")-1 },` |
|         - | 1107 | `				{ MEMOBJ_OBJ,      GEN_ATOM_PLAIN,    "object",   sizeof("object")-1 },` |
|         - | 1108 | `				{ MEMOBJ_HASHMAP,  GEN_ATOM_ITERABLE, "array",    sizeof("array")-1 },` |
|         - | 1109 | `				{ MEMOBJ_STRING,   GEN_ATOM_PLAIN,    "string",   sizeof("string")-1 },` |
|         - | 1110 | `				{ MEMOBJ_INT,      GEN_ATOM_PLAIN,    "int",      sizeof("int")-1 },` |
|         - | 1111 | `				{ MEMOBJ_REAL,     GEN_ATOM_PLAIN,    "float",    sizeof("float")-1 },` |
|         - | 1112 | `				{ MEMOBJ_BOOL,     GEN_ATOM_PLAIN,    "bool",     sizeof("bool")-1 },` |
|         - | 1113 | `				{ 0,               GEN_ATOM_FALSE,    "false",    sizeof("false")-1 },` |
|         - | 1114 | `				{ 0,               GEN_ATOM_TRUE,     "true",     sizeof("true")-1 }` |
|         - | 1115 | `			};` |
|         - | 1116 | `			int k;` |
|   1925685 | 1117 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|   3307541 | 1118 | `				for( i = 0; i < nAtoms; i++ ){` |
|   2232960 | 1119 | `					int bHit = (aOrder[k].nType != 0 && aAtoms[i].nType == aOrder[k].nType)` |
|   3065916 | 1120 | `						\|\| (aOrder[k].nKind != GEN_ATOM_PLAIN` |
|   1163706 | 1121 | `						    && GenAtomMaskKind(&aAtoms[i]) == aOrder[k].nKind` |
|    376504 | 1122 | `						    && (aOrder[k].nKind != GEN_ATOM_ITERABLE \|\| nNonNull > 1));` |
|   1734831 | 1123 | `					if( !bHit ) continue;` |
|    160407 | 1124 | `					if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    160407 | 1125 | `					SyBlobAppend(pBlob, aOrder[k].zText, aOrder[k].nText);` |
|    160407 | 1126 | `					bFirst = 0;` |
|    160407 | 1127 | `					break;` |
|       ! 0 | 1128 | `				}` |
|    866561 | 1129 | `			}` |
|         - | 1130 | `		}` |
|         - | 1131 | `		/* 3) null suffix */` |
|    192573 | 1132 | `		if( bNullable ){` |
|        22 | 1133 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|        22 | 1134 | `			SyBlobAppend(pBlob, "null", 4);` |
|         9 | 1135 | `		}` |
|         - | 1136 | `	}` |
|    107708 | 1137 | `}` |
|         - | 1138 |  |
|         - | 1139 | `/*` |
|         - | 1140 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|         - | 1141 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|         - | 1142 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|         - | 1143 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|         - | 1144 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|         - | 1145 | ` * whether it was parenthesized.` |
|         - | 1146 | ` *` |
|         - | 1147 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|         - | 1148 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|         - | 1149 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|         - | 1150 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|         - | 1151 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|         - | 1152 | ` */` |
|    215666 | 1153 | `static sxi32 GenStateParsePart(` |
|         - | 1154 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|         - | 1155 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|         5 | 1156 | `{` |
|         - | 1157 | `	sxi32 rc;` |
|    215671 | 1158 | `	int nMembers = 0;` |
|    215671 | 1159 | `	int bParen = 0;` |
|    215671 | 1160 | `	*pnMembers = 0;` |
|    215671 | 1161 | `	*pbParen = 0;` |
|    215671 | 1162 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        13 | 1163 | `		bParen = 1;` |
|        13 | 1164 | `		pGen->pIn++; /* skip '(' */` |
|         5 | 1165 | `	}` |
|    107833 | 1166 | `	for(;;){` |
|    215701 | 1167 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|       ! 0 | 1168 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1169 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|       ! 0 | 1170 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 1171 | `		}` |
|    215701 | 1172 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    215701 | 1173 | `		if( rc != SXRET_OK ){` |
|         3 | 1174 | `			return rc;` |
|         - | 1175 | `		}` |
|    215699 | 1176 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    215699 | 1177 | `		(*pnAtoms)++;` |
|    215699 | 1178 | `		nMembers++;` |
|         - | 1179 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    215699 | 1180 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        43 | 1181 | `			SyToken *pNext = &pGen->pIn[1];` |
|        38 | 1182 | `			if( pNext < pGen->pEnd` |
|        43 | 1183 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        35 | 1184 | `				pGen->pIn++; /* skip '&' */` |
|        35 | 1185 | `				continue;` |
|         - | 1186 | `			}` |
|         4 | 1187 | `		}` |
|    215669 | 1188 | `		break;` |
|       ! 0 | 1189 | `	}` |
|    215669 | 1190 | `	if( bParen ){` |
|        13 | 1191 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 1192 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1193 | `				"Malformed DNF type: expecting ')'");` |
|       ! 0 | 1194 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 1195 | `		}` |
|        13 | 1196 | `		pGen->pIn++; /* skip ')' */` |
|        13 | 1197 | `		if( nMembers < 2 ){` |
|       ! 0 | 1198 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1199 | `				"Parenthesized type must be an intersection of at least two types");` |
|       ! 0 | 1200 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 1201 | `		}` |
|         5 | 1202 | `	}` |
|    215669 | 1203 | `	*pnMembers = nMembers;` |
|    215669 | 1204 | `	*pbParen = bParen;` |
|    215669 | 1205 | `	return SXRET_OK;` |
|    107838 | 1206 | `}` |
|         - | 1207 |  |
|         - | 1208 | `/*` |
|         - | 1209 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|         - | 1210 | ` *` |
|         - | 1211 | ` * Outputs:` |
|         - | 1212 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|         - | 1213 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|         - | 1214 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|         - | 1215 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|         - | 1216 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|         - | 1217 | ` *     already be initialized by the caller (allocator set, etc).` |
|         - | 1218 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|         - | 1219 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|         - | 1220 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|         - | 1221 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|         - | 1222 | ` *` |
|         - | 1223 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|         - | 1224 | ` * SXERR_ABORT on fatal compile errors.` |
|         - | 1225 | ` */` |
|    215422 | 1226 | `PH7_PRIVATE sxi32 GenStateParseUnionTypeDecl(` |
|         - | 1227 | `	ph7_gen_state *pGen,` |
|         - | 1228 | `	sxu32 *pnType,` |
|         - | 1229 | `	SyString *pClass,` |
|         - | 1230 | `	SySet *pAlts,` |
|         - | 1231 | `	sxi32 *piTypeFlags,` |
|         - | 1232 | `	SyString *pTypeText,` |
|         - | 1233 | `	int iNullableFlag,` |
|         - | 1234 | `	int iUnionFlag,` |
|         - | 1235 | `	int bAllowVoid,` |
|         - | 1236 | `	sxu32 nLine` |
|         5 | 1237 | `){` |
|         - | 1238 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    215427 | 1239 | `	int nAtoms = 0;` |
|    215427 | 1240 | `	int bShortNullable = 0;` |
|    215427 | 1241 | `	int bExplicitNull = 0;` |
|         - | 1242 | `	sxi32 rc;` |
|    215427 | 1243 | `	*pnType = 0;` |
|    215427 | 1244 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    215427 | 1245 | `	*piTypeFlags = 0;` |
|    215427 | 1246 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|         - | 1247 |  |
|    215427 | 1248 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 1249 | `		return SXRET_OK;` |
|         - | 1250 | `	}` |
|         - | 1251 | ``	/* Optional `?` shorthand prefix */`` |
|    215422 | 1252 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|     22803 | 1253 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|     22803 | 1254 | `		bShortNullable = 1;` |
|     22803 | 1255 | `		pGen->pIn++;` |
|     22803 | 1256 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 1257 | `			return SXERR_SYNTAX;` |
|         - | 1258 | `		}` |
|     11399 | 1259 | `	}` |
|         - | 1260 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|         - | 1261 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|         - | 1262 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|         - | 1263 | `	{` |
|         - | 1264 | `		int nMembers, bParen;` |
|    215427 | 1265 | `		sxu32 iGroup = 0;` |
|    215427 | 1266 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    215427 | 1267 | `		if( rc != SXRET_OK ){` |
|         4 | 1268 | `			return rc;` |
|         - | 1269 | `		}` |
|         - | 1270 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|         - | 1271 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|         - | 1272 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|         - | 1273 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|         - | 1274 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|    323501 | 1275 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    215796 | 1276 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       251 | 1277 | `			if( bShortNullable ){` |
|         - | 1278 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|         - | 1279 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|         - | 1280 | `				 * already reported" so callers skip their own error emission. */` |
|         3 | 1281 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - | 1282 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|         3 | 1283 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|         - | 1284 | `			}` |
|       249 | 1285 | `			if( nMembers >= 2 && !bParen ){` |
|       ! 0 | 1286 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|         - | 1287 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 | 1288 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 1289 | `			}` |
|       249 | 1290 | ``			pGen->pIn++; /* skip `\|` */`` |
|       249 | 1291 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|       249 | 1292 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1293 | `				return rc;` |
|         - | 1294 | `			}` |
|         5 | 1295 | `		}` |
|    215423 | 1296 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|       ! 0 | 1297 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1298 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 | 1299 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 1300 | `		}` |
|         - | 1301 | `	}` |
|         - | 1302 | `	/* Validation pass.` |
|         - | 1303 | `	 *` |
|         - | 1304 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|         - | 1305 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|         - | 1306 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|         - | 1307 | `	 */` |
|         - | 1308 | `	{` |
|         - | 1309 | `		int i, j;` |
|    215423 | 1310 | `		int bHasNonNull = 0;` |
|    215423 | 1311 | `		int bAnyIntersection = 0;` |
|         - | 1312 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|         - | 1313 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|         - | 1314 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|   7108799 | 1315 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    431115 | 1316 | `		for( i = 0; i < nAtoms; i++ ){` |
|    215697 | 1317 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|    107851 | 1318 | `		}` |
|    431047 | 1319 | `		for( i = 0; i < nAtoms; i++ ){` |
|    215659 | 1320 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|    107817 | 1321 | `		}` |
|         - | 1322 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|         - | 1323 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    215423 | 1324 | `		if( bShortNullable && bAnyIntersection ){` |
|       ! 0 | 1325 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1326 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|       ! 0 | 1327 | `			return SXERR_SYNTAX;` |
|         - | 1328 | `		}` |
|    431101 | 1329 | `		for( i = 0; i < nAtoms; i++ ){` |
|         - | 1330 | `			/* Intersection members must be class/interface types (PHP rejects` |
|         - | 1331 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|         - | 1332 | ``			 * `true`/`false` in an intersection). */`` |
|    215695 | 1333 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        63 | 1334 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|        63 | 1335 | `				if( bClassLike ){` |
|        60 | 1336 | `					SyString *pC = &aAtoms[i].sClass;` |
|        56 | 1337 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|        56 | 1338 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|        56 | 1339 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|        60 | 1340 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|       ! 0 | 1341 | `						bClassLike = 0;` |
|       ! 0 | 1342 | `					}` |
|        28 | 1343 | `				}` |
|        63 | 1344 | `				if( !bClassLike ){` |
|         - | 1345 | `					const char *zName; sxu32 nName;` |
|         3 | 1346 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 | 1347 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|       ! 0 | 1348 | `					}else{` |
|         3 | 1349 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|         - | 1350 | `					}` |
|         4 | 1351 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1352 | `						"Type %.*s cannot be part of an intersection type",` |
|         1 | 1353 | `						(int)nName, zName);` |
|         3 | 1354 | `					return SXERR_SYNTAX;` |
|         - | 1355 | `				}` |
|        28 | 1356 | `			}` |
|    215693 | 1357 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|       227 | 1358 | `				if( nAtoms > 1 ){` |
|         3 | 1359 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1360 | `						"Void can only be used as a standalone type");` |
|         3 | 1361 | `					return SXERR_SYNTAX;` |
|         - | 1362 | `				}` |
|       225 | 1363 | `				if( !bAllowVoid ){` |
|       ! 0 | 1364 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1365 | `						"void cannot be used here");` |
|       ! 0 | 1366 | `					return SXERR_SYNTAX;` |
|         - | 1367 | `				}` |
|       225 | 1368 | `				if( bShortNullable ){` |
|       ! 0 | 1369 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1370 | `						"Void type cannot be nullable");` |
|       ! 0 | 1371 | `					return SXERR_SYNTAX;` |
|         - | 1372 | `				}` |
|       110 | 1373 | `			}` |
|    215691 | 1374 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|         - | 1375 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|         - | 1376 | `				 * type (never = the function does not return). Mirrors the void` |
|         - | 1377 | `				 * validation above; accepted here and enforced at compile time` |
|         - | 1378 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|        33 | 1379 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|         - | 1380 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|         - | 1381 | `					 * same as any other non-standalone use. */` |
|         6 | 1382 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1383 | `						"never can only be used as a standalone type");` |
|         6 | 1384 | `					return SXERR_SYNTAX;` |
|         - | 1385 | `				}` |
|        28 | 1386 | `				if( !bAllowVoid ){` |
|         - | 1387 | `					/* Return-only: params call with bAllowVoid=0. */` |
|         3 | 1388 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1389 | `						"never cannot be used as a parameter type");` |
|         3 | 1390 | `					return SXERR_SYNTAX;` |
|         - | 1391 | `				}` |
|        11 | 1392 | `			}` |
|    215685 | 1393 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|        36 | 1394 | `				bExplicitNull = 1;` |
|        20 | 1395 | `			}else{` |
|    215653 | 1396 | `				bHasNonNull = 1;` |
|         - | 1397 | `			}` |
|         - | 1398 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|         - | 1399 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|         - | 1400 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|         - | 1401 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|         - | 1402 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    216015 | 1403 | `			for( j = 0; j < i; j++ ){` |
|       337 | 1404 | `				int bDup = 0;` |
|       337 | 1405 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|       653 | 1406 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|       332 | 1407 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|       337 | 1408 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|       317 | 1409 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|        85 | 1410 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|        78 | 1411 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|        65 | 1412 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|        21 | 1413 | `								aAtoms[j].sClass.zString,` |
|        42 | 1414 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|       ! 0 | 1415 | `							bDup = 1;` |
|       ! 0 | 1416 | `						}` |
|        44 | 1417 | `					}else{` |
|         3 | 1418 | `						bDup = 1;` |
|         - | 1419 | `					}` |
|        40 | 1420 | `				}` |
|       317 | 1421 | `				if( bDup ){` |
|         - | 1422 | `					const char *zName;` |
|         - | 1423 | `					sxu32 nName;` |
|         3 | 1424 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 | 1425 | `						zName = aAtoms[i].sClass.zString;` |
|       ! 0 | 1426 | `						nName = aAtoms[i].sClass.nByte;` |
|       ! 0 | 1427 | `					}else{` |
|         3 | 1428 | `						zName = aAtoms[i].zCanon;` |
|         3 | 1429 | `						nName = aAtoms[i].nCanon;` |
|         - | 1430 | `					}` |
|         4 | 1431 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         1 | 1432 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|         3 | 1433 | `					return SXERR_SYNTAX;` |
|         - | 1434 | `				}` |
|       160 | 1435 | `			}` |
|    107844 | 1436 | `		}` |
|    215411 | 1437 | `		if( !bHasNonNull && bExplicitNull ){` |
|         7 | 1438 | `			if( bShortNullable ){` |
|         - | 1439 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|       ! 0 | 1440 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - | 1441 | `					"Null can not be used as a standalone type");` |
|       ! 0 | 1442 | `				return SXERR_SYNTAX;` |
|         - | 1443 | `			}` |
|         - | 1444 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|         - | 1445 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|         - | 1446 | `			 * path below leaves *pnType untouched when there is no non-null` |
|         - | 1447 | `			 * atom, so set it here. */` |
|         7 | 1448 | `			*pnType = MEMOBJ_NULL;` |
|         3 | 1449 | `		}` |
|         - | 1450 | `	}` |
|         - | 1451 | `	/* Compute nullability flag */` |
|    215411 | 1452 | `	if( bShortNullable \|\| bExplicitNull ){` |
|     22833 | 1453 | `		*piTypeFlags \|= iNullableFlag;` |
|     11414 | 1454 | `	}` |
|         - | 1455 | `	/* Build canonical type text */` |
|    215411 | 1456 | `	if( pTypeText ){` |
|         - | 1457 | `		SyBlob sBlob;` |
|    215411 | 1458 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|    311716 | 1459 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|    107703 | 1460 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    215411 | 1461 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|    322751 | 1462 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    215164 | 1463 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    215169 | 1464 | `			if( zDup ){` |
|    215169 | 1465 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|    107582 | 1466 | `			}` |
|    107582 | 1467 | `		}` |
|    215411 | 1468 | `		SyBlobRelease(&sBlob);` |
|    107703 | 1469 | `	}` |
|         - | 1470 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|         - | 1471 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|         - | 1472 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|         - | 1473 | `	{` |
|    215411 | 1474 | `		int nNonNull = 0;` |
|    215411 | 1475 | `		int iNonNullIdx = -1;` |
|         - | 1476 | `		int i;` |
|    431081 | 1477 | `		for( i = 0; i < nAtoms; i++ ){` |
|    215675 | 1478 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    215643 | 1479 | `				nNonNull++;` |
|    215643 | 1480 | `				iNonNullIdx = i;` |
|    107819 | 1481 | `			}` |
|    107840 | 1482 | `		}` |
|    215411 | 1483 | `		if( nNonNull <= 1 ){` |
|         - | 1484 | `			/* Fast path: store as single type. */` |
|    215211 | 1485 | `			if( iNonNullIdx >= 0 ){` |
|    215205 | 1486 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    215205 | 1487 | `				if( pA->nType == SXU32_HIGH ){` |
|     68660 | 1488 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     22885 | 1489 | `						pA->sClass.zString, pA->sClass.nByte);` |
|     45775 | 1490 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|     45775 | 1491 | `					*pnType = SXU32_HIGH;` |
|     45775 | 1492 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    192320 | 1493 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|       225 | 1494 | `					*pnType = MEMOBJ_VOID;` |
|    169325 | 1495 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|        25 | 1496 | `					*pnType = MEMOBJ_NEVER;` |
|        14 | 1497 | `				}else{` |
|    169193 | 1498 | `					*pnType = pA->nType;` |
|         - | 1499 | `				}` |
|    107600 | 1500 | `			}` |
|    107608 | 1501 | `		}else{` |
|         - | 1502 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|       205 | 1503 | `			*piTypeFlags \|= iUnionFlag;` |
|       655 | 1504 | `			for( i = 0; i < nAtoms; i++ ){` |
|         - | 1505 | `				ph7_type_alt sAlt;` |
|       455 | 1506 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       443 | 1507 | `				SyZero(&sAlt, sizeof(sAlt));` |
|       443 | 1508 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|       443 | 1509 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       314 | 1510 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       103 | 1511 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       211 | 1512 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|       211 | 1513 | `					sAlt.nType = SXU32_HIGH;` |
|       211 | 1514 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|       108 | 1515 | `				}else{` |
|       237 | 1516 | `					sAlt.nType = aAtoms[i].nType;` |
|       237 | 1517 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|         - | 1518 | `				}` |
|       443 | 1519 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|       224 | 1520 | `			}` |
|         - | 1521 | `		}` |
|         - | 1522 | `	}` |
|    215411 | 1523 | `	return SXRET_OK;` |
|    107716 | 1524 | `}` |
|         - | 1525 |  |
|         - | 1526 | `/*` |
|         - | 1527 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|         - | 1528 | `` * pGen->pIn should point to the token after `)`.`` |
|         - | 1529 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|         - | 1530 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|         - | 1531 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|         - | 1532 | `` *          and union types `: T\|U`.`` |
|         - | 1533 | ` */` |
|   3879190 | 1534 | `PH7_PRIVATE sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|         5 | 1535 | `{` |
|   3879195 | 1536 | `	sxi32 iFlags = 0;` |
|         - | 1537 | `	sxi32 rc;` |
|         - | 1538 | `	sxu32 nLine;` |
|   3879195 | 1539 | `	pFunc->nReturnType = 0;` |
|   3879195 | 1540 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|   3879195 | 1541 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|         - | 1542 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|         - | 1543 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|         - | 1544 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|         - | 1545 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|         - | 1546 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|   3879195 | 1547 | `	SySetReset(&pFunc->aReturnUnion);` |
|   3879195 | 1548 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|   3879195 | 1549 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|   3841879 | 1550 | `		return SXRET_OK;` |
|         - | 1551 | `	}` |
|     37321 | 1552 | `	pGen->pIn++; /* Skip ':' */` |
|     37321 | 1553 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 1554 | `		return SXRET_OK;` |
|         - | 1555 | `	}` |
|     37321 | 1556 | `	nLine = pGen->pIn->nLine;` |
|     37321 | 1557 | `	rc = GenStateParseUnionTypeDecl(` |
|     18658 | 1558 | `		pGen,` |
|     18658 | 1559 | `		&pFunc->nReturnType,` |
|     18658 | 1560 | `		&pFunc->sReturnClass,` |
|     18658 | 1561 | `		&pFunc->aReturnUnion,` |
|         - | 1562 | `		&iFlags,` |
|     18658 | 1563 | `		&pFunc->sReturnTypeName,` |
|         - | 1564 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|         - | 1565 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|         - | 1566 | `		/* iUnionFlag */ 0,` |
|         - | 1567 | `		/* bAllowVoid */ 1,` |
|     18658 | 1568 | `		nLine);` |
|     37321 | 1569 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 1570 | `		return SXERR_ABORT;` |
|         - | 1571 | `	}` |
|     37321 | 1572 | `	if( rc == SXERR_CORRUPT ){` |
|         - | 1573 | `		/* Error already reported */` |
|       ! 0 | 1574 | `		return SXERR_SYNTAX;` |
|         - | 1575 | `	}` |
|     37321 | 1576 | `	if( rc == SXERR_SYNTAX ){` |
|         9 | 1577 | `		if( pGen->pIn < pGen->pEnd ){` |
|        12 | 1578 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - | 1579 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|         6 | 1580 | `				&pGen->pIn->sData);` |
|         6 | 1581 | `		}else{` |
|       ! 0 | 1582 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|         - | 1583 | `				"syntax error, unexpected end of file in return type declaration");` |
|         - | 1584 | `		}` |
|         9 | 1585 | `		return SXERR_SYNTAX;` |
|         - | 1586 | `	}` |
|     37315 | 1587 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     37315 | 1588 | `	return SXRET_OK;` |
|   1939600 | 1589 | `}` |
|         - | 1590 |  |
|    596704 | 1591 | `PH7_PRIVATE sxi32 GenStateCompileFunc(` |
|         - | 1592 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 1593 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|         - | 1594 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 1595 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|         - | 1596 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|         - | 1597 | `	)` |
|         5 | 1598 | `{` |
|         - | 1599 | `	ph7_vm_func *pFunc;` |
|         - | 1600 | `	SyToken *pEnd;` |
|         - | 1601 | `	sxu32 nLine;` |
|         - | 1602 | `	char *zName;` |
|         - | 1603 | `	sxi32 rc;` |
|         - | 1604 | `	/* Extract line number */` |
|    596709 | 1605 | `	nLine = pGen->pIn->nLine;` |
|         - | 1606 | `	/* Jump the left parenthesis '(' */` |
|    596709 | 1607 | `	pGen->pIn++;` |
|         - | 1608 | `	/* Delimit the function signature */` |
|    596709 | 1609 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    596709 | 1610 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 1611 | `		/* Syntax error */` |
|        12 | 1612 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable");` |
|         4 | 1613 | `		(void)pName;` |
|        12 | 1614 | `		if( rc == SXERR_ABORT ){` |
|         - | 1615 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 1616 | `			return SXERR_ABORT;` |
|         - | 1617 | `		}` |
|        12 | 1618 | `		pGen->pIn = pGen->pEnd;` |
|        12 | 1619 | `		return SXRET_OK;` |
|         - | 1620 | `	}` |
|         - | 1621 | `	/* Create the function state */` |
|    596701 | 1622 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|    596701 | 1623 | `	if( pFunc == 0 ){` |
|       ! 0 | 1624 | `		goto OutOfMem;` |
|         - | 1625 | `	}` |
|         - | 1626 | `	/* Build the function name, prepending namespace if active.` |
|         - | 1627 | `	 * A NAMED function (never a closure) also answers to php's import rules: its` |
|         - | 1628 | ``	 * short name must not already be a local `use function` import, and the name it`` |
|         - | 1629 | ``	 * takes is remembered so a later `use function` in this unit sees it. */`` |
|    596722 | 1630 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|         - | 1631 | `		SyBlob sFQN;` |
|         - | 1632 | `		sxu32 nLen;` |
|        47 | 1633 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        47 | 1634 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        47 | 1635 | `		SyBlobAppend(&sFQN,"\\",1);` |
|        47 | 1636 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|        47 | 1637 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|        47 | 1638 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|        47 | 1639 | `		SyBlobRelease(&sFQN);` |
|        47 | 1640 | `		if( zName == 0 ){` |
|       ! 0 | 1641 | `			goto OutOfMem;` |
|         - | 1642 | `		}` |
|        47 | 1643 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|        26 | 1644 | `	}else{` |
|    596659 | 1645 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    596659 | 1646 | `		if( zName == 0 ){` |
|       ! 0 | 1647 | `			goto OutOfMem;` |
|         - | 1648 | `		}` |
|    596659 | 1649 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|         - | 1650 | `	}` |
|    596701 | 1651 | `	if( !bHandleClosure ){` |
|    595119 | 1652 | `		if( GenStateGuardImportRedeclare(pGen,1,pName,&pFunc->sName,nLine) == SXERR_ABORT ){` |
|       ! 0 | 1653 | `			return SXERR_ABORT;` |
|         - | 1654 | `		}` |
|    595119 | 1655 | `		GenStateRecordDeclaredName(pGen,1,&pFunc->sName);` |
|    297557 | 1656 | `	}` |
|         - | 1657 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|         - | 1658 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|    596701 | 1659 | `	pFunc->nLine = nLine;` |
|    596701 | 1660 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|    596701 | 1661 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 1662 | `		return SXERR_ABORT;` |
|         - | 1663 | `	}` |
|    596701 | 1664 | `	if( pGen->pIn < pEnd ){` |
|         - | 1665 | `		/* Collect function arguments */` |
|    517631 | 1666 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|    517631 | 1667 | `		if( rc == SXERR_ABORT ){` |
|         - | 1668 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|         3 | 1669 | `			return SXERR_ABORT;` |
|         - | 1670 | `		}` |
|    258812 | 1671 | `	}` |
|         - | 1672 | `	/* Point past ')' and parse optional return type ': type' */` |
|    596699 | 1673 | `	pGen->pIn = &pEnd[1];` |
|         - | 1674 | `	{` |
|    596699 | 1675 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|    596699 | 1676 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 | 1677 | `			return SXERR_ABORT;` |
|    596699 | 1678 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|         9 | 1679 | `			return SXERR_SYNTAX;` |
|         - | 1680 | `		}` |
|         - | 1681 | `	}` |
|    596693 | 1682 | `	if( bHandleClosure ){` |
|         - | 1683 | `		ph7_vm_func_closure_env sEnv;` |
|      1587 | 1684 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|      1582 | 1685 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       894 | 1686 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|       201 | 1687 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - | 1688 | `				/* Closure,record environment variable */` |
|       201 | 1689 | `				pGen->pIn++;` |
|       201 | 1690 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 | 1691 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|       ! 0 | 1692 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 1693 | `						return SXERR_ABORT;` |
|         - | 1694 | `					}` |
|       ! 0 | 1695 | `				}` |
|       201 | 1696 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|         - | 1697 | `				/* Compile until we hit the first closing parenthesis */` |
|       435 | 1698 | `				while( pGen->pIn < pGen->pEnd ){` |
|       435 | 1699 | `					int iFlagsLocal = 0;` |
|       435 | 1700 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|       199 | 1701 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|       199 | 1702 | `						break;` |
|         - | 1703 | `					}` |
|       241 | 1704 | `					nLineLocal = pGen->pIn->nLine;` |
|       241 | 1705 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - | 1706 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|         - | 1707 | `						 * to the variable's memory slot instead of copying its value. */` |
|        93 | 1708 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|        93 | 1709 | `						pGen->pIn++;` |
|        45 | 1710 | `					}` |
|       236 | 1711 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       241 | 1712 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 1713 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|         - | 1714 | `								"Closure: Unexpected token. Expecting a variable name");` |
|       ! 0 | 1715 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 1716 | `								return SXERR_ABORT;` |
|         - | 1717 | `							}` |
|         - | 1718 | `							/* Find the closing parenthesis */` |
|       ! 0 | 1719 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 1720 | `								pGen->pIn++;` |
|       ! 0 | 1721 | `							}` |
|       ! 0 | 1722 | `							if(pGen->pIn < pGen->pEnd){` |
|       ! 0 | 1723 | `								pGen->pIn++;` |
|       ! 0 | 1724 | `							}` |
|       ! 0 | 1725 | `							break;` |
|         - | 1726 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|       ! 0 | 1727 | `					}else{` |
|         - | 1728 | `						SyString *pNameLocal;` |
|         - | 1729 | `						char *zDup;` |
|         - | 1730 | `						/* Duplicate variable name */` |
|       241 | 1731 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       241 | 1732 | `						if( PH7_VmIsAutoGlobal(pNameLocal->zString,pNameLocal->nByte) ){` |
|         - | 1733 | `							/* php's compile fatal. It is a real protection, not a` |
|         - | 1734 | `							 * style rule: the import resolves through hSuper, so` |
|         - | 1735 | `							 * installing the captured value would overwrite the` |
|         - | 1736 | ``							 * superglobal's own slot — `use ($GLOBALS)` replaced the`` |
|         - | 1737 | `							 * live symbol-table view with a snapshot and every later` |
|         - | 1738 | `							 * global went missing program-wide. */` |
|         3 | 1739 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|         - | 1740 | `								"Cannot use auto-global as lexical variable");` |
|         3 | 1741 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 1742 | `								return SXERR_ABORT;` |
|         - | 1743 | `							}` |
|         3 | 1744 | `							return SXERR_SYNTAX;` |
|         - | 1745 | `						}` |
|       239 | 1746 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       239 | 1747 | `						if( zDup ){` |
|         - | 1748 | `							/* Zero the structure */` |
|       239 | 1749 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       239 | 1750 | `							sEnv.iFlags = iFlagsLocal;` |
|       239 | 1751 | `							sEnv.nLine = nLineLocal; /* the capture's own source line (php warns here) */` |
|       239 | 1752 | `							sEnv.nIdx = SXU32_HIGH;` |
|       239 | 1753 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       239 | 1754 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|       263 | 1755 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|        48 | 1756 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|       ! 0 | 1757 | `									got_this = 1;` |
|       ! 0 | 1758 | `							}` |
|         - | 1759 | `							/* Save imported variable */` |
|       239 | 1760 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       122 | 1761 | `						}else{` |
|       ! 0 | 1762 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1763 | `							 return SXERR_ABORT;` |
|         - | 1764 | `						}` |
|         - | 1765 | `					}` |
|       239 | 1766 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|       281 | 1767 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - | 1768 | `						/* Ignore trailing commas */` |
|        45 | 1769 | `						pGen->pIn++;` |
|         3 | 1770 | `					}` |
|         5 | 1771 | `				}` |
|         - | 1772 | `				/* php 7.1+: the return type follows the use clause —` |
|         - | 1773 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|         - | 1774 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|         - | 1775 | `				 * so an unconditional call would wipe a type parsed at the` |
|         - | 1776 | `				 * legacy pre-use position. */` |
|       199 | 1777 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|         7 | 1778 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|         7 | 1779 | `					if( rcRt2 == SXERR_ABORT ){` |
|       ! 0 | 1780 | `						return SXERR_ABORT;` |
|         7 | 1781 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|       ! 0 | 1782 | `						return SXERR_SYNTAX;` |
|         - | 1783 | `					}` |
|         3 | 1784 | `				}` |
|        97 | 1785 | `		}` |
|      1585 | 1786 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|         - | 1787 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|         - | 1788 | `			 * available to the closure environment — for EVERY non-static` |
|         - | 1789 | `			 * anonymous function, use list or not (php binds $this to any` |
|         - | 1790 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|         - | 1791 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|         - | 1792 | `			 * a global-scope closure is silently dropped at install. A static` |
|         - | 1793 | `			 * closure never binds $this (php). */` |
|      1543 | 1794 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      1543 | 1795 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      1543 | 1796 | `			sEnv.nIdx = SXU32_HIGH;` |
|      1543 | 1797 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      1543 | 1798 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      1543 | 1799 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       769 | 1800 | `		}` |
|      1585 | 1801 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|         - | 1802 | `			/* Mark as closure */` |
|      1547 | 1803 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       771 | 1804 | `		}` |
|       790 | 1805 | `	}` |
|         - | 1806 | `	/* Compile the body */` |
|    596691 | 1807 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|    596691 | 1808 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 1809 | `		return SXERR_ABORT;` |
|         - | 1810 | `	}` |
|         - | 1811 | `	/* The cursor sits just past the body's closing brace */` |
|    596691 | 1812 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|    596691 | 1813 | `	if( ppFunc ){` |
|    596691 | 1814 | `		*ppFunc = pFunc;` |
|    298343 | 1815 | `	}` |
|    596691 | 1816 | `	rc = SXRET_OK;` |
|    596691 | 1817 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|         - | 1818 | `		/* Reject a php-fatal redeclaration before hoisting the function */` |
|    595149 | 1819 | `		if( GenStateGuardFuncRedeclaration(pGen,pFunc) == SXERR_ABORT ){` |
|         9 | 1820 | `			return SXERR_ABORT;` |
|         - | 1821 | `		}` |
|         - | 1822 | `		/* Finally register the function */` |
|    595143 | 1823 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    297569 | 1824 | `	}` |
|    596685 | 1825 | `	if( rc == SXRET_OK ){` |
|    596685 | 1826 | `		return SXRET_OK;` |
|         - | 1827 | `	}` |
|         - | 1828 | `	/* Fall through if something goes wrong */` |
|       ! 0 | 1829 | `OutOfMem:` |
|         - | 1830 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - | 1831 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|         - | 1832 | `	 */` |
|       ! 0 | 1833 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 | 1834 | `	return SXERR_ABORT;` |
|    298357 | 1835 | `}` |
|         - | 1836 | `/*` |
|         - | 1837 | ` * Compile a standard PHP function.` |
|         - | 1838 | ` *  Refer to the block-comment above for more information.` |
|         - | 1839 | ` */` |
|    595130 | 1840 | `PH7_PRIVATE sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|         5 | 1841 | `{` |
|         - | 1842 | `	SyString *pName;` |
|         - | 1843 | `	sxi32 iFlags;` |
|         - | 1844 | `	sxu32 nKwLine;` |
|         - | 1845 | `	sxu32 nLine;` |
|         - | 1846 | `	sxi32 rc;` |
|         - | 1847 |  |
|    595135 | 1848 | `	nLine = pGen->pIn->nLine;` |
|    595135 | 1849 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    595135 | 1850 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|    595135 | 1851 | `	iFlags = 0;` |
|    595135 | 1852 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - | 1853 | `		/* Return by reference,remember that */` |
|        12 | 1854 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|         - | 1855 | `		/* Jump the '&' token */` |
|        12 | 1856 | `		pGen->pIn++;` |
|         5 | 1857 | `	}` |
|    595135 | 1858 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - | 1859 | `		/* Invalid function name */` |
|         8 | 1860 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         8 | 1861 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1862 | `			return SXERR_ABORT;` |
|         - | 1863 | `		}` |
|         - | 1864 | `		/* Sychronize with the next semi-colon or braces*/` |
|        22 | 1865 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        16 | 1866 | `			pGen->pIn++;` |
|         2 | 1867 | `		}` |
|         8 | 1868 | `		return SXRET_OK;` |
|         - | 1869 | `	}` |
|    595129 | 1870 | `	pName = &pGen->pIn->sData;` |
|    595129 | 1871 | `	nLine = pGen->pIn->nLine;` |
|         - | 1872 | `	/* Jump the function name */` |
|    595129 | 1873 | `	pGen->pIn++;` |
|    595129 | 1874 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 1875 | `		/* Syntax error */` |
|         3 | 1876 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         3 | 1877 | `		if( rc == SXERR_ABORT ){` |
|         - | 1878 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 1879 | `			return SXERR_ABORT;` |
|         - | 1880 | `		}` |
|         - | 1881 | `		/* Sychronize with the next semi-colon or '{' */` |
|         3 | 1882 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 | 1883 | `			pGen->pIn++;` |
|       ! 0 | 1884 | `		}` |
|         3 | 1885 | `		return SXRET_OK;` |
|         - | 1886 | `	}` |
|         - | 1887 | `	/* Compile function body */` |
|         - | 1888 | `	{` |
|    595127 | 1889 | `		ph7_vm_func *pFuncState = 0;` |
|    595127 | 1890 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|    595127 | 1891 | `		if( pFuncState ){` |
|         - | 1892 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|    595111 | 1893 | `			pFuncState->nLine = nKwLine;` |
|    297553 | 1894 | `		}` |
|         - | 1895 | `	}` |
|    595127 | 1896 | `	return rc;` |
|    297570 | 1897 | `}` |
|         - | 1898 |  |
