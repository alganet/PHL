# src/ph7/compile_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2659/3441 lines (77.27%)

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
|         - |    8 | `/* Forward declaration — deferred class declarations (defined with the deferral` |
|         - |    9 | ` * helpers ahead of GenStateCompileClassEx; used by the interface/trait` |
|         - |   10 | ` * compilers that precede them in this file). */` |
|         - |   11 | `static int GenStateMaybeDeferClass(ph7_gen_state *pGen,sxi32 iFlags,int iSelfKind,sxi32 *pRc);` |
|         - |   12 | `/*` |
|         - |   13 | ` * Section:` |
|         - |   14 | ` *    Class/OO compilation: classes, interfaces, traits, enums, anonymous` |
|         - |   15 | ` *    classes, class constants, typed properties, property hooks and methods.` |
|         - |   16 | ` * Status:` |
|         - |   17 | ` *    Stable.` |
|         - |   18 | ` */` |
|        10 |   19 | `static const char * GenStateClassKind(const ph7_class *pClass)` |
|         3 |   20 | `{` |
|        13 |   21 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){ return "interface"; }` |
|        11 |   22 | `	if( pClass->iFlags & PH7_CLASS_TRAIT ){ return "trait"; }` |
|         9 |   23 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){ return "enum"; }` |
|         6 |   24 | `	return "class";` |
|         8 |   25 | `}` |
|         - |   26 | `/*` |
|         - |   27 | ` * Guard a class/interface/trait/enum about to be installed. Returns SXERR_ABORT` |
|         - |   28 | ` * (after emitting the fatal) if it redeclares an already-bound type; otherwise` |
|         - |   29 | ` * marks it bound (when unconditional & top-level) and returns SXRET_OK.` |
|         - |   30 | ` */` |
|    609768 |   31 | `static sxi32 GenStateGuardClassRedeclaration(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 |   32 | `{` |
|         - |   33 | `	SyHashEntry *pEntry;` |
|    609773 |   34 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|        43 |   35 | `		return SXRET_OK; /* conditional/nested: keep hoisting */` |
|         - |   36 | `	}` |
|    609735 |   37 | `	pClass->iFlags \|= PH7_CLASS_BOUND;` |
|    609735 |   38 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|    606757 |   39 | `		return SXRET_OK; /* the prelude installs each builtin exactly once */` |
|         - |   40 | `	}` |
|      2983 |   41 | `	pEntry = SyHashGet(&pGen->pVm->hClass,(const void *)pClass->sName.zString,pClass->sName.nByte);` |
|      2983 |   42 | `	if( pEntry ){` |
|        16 |   43 | `		ph7_class *pPrev = (ph7_class *)pEntry->pUserData;` |
|        18 |   44 | `		while( pPrev ){` |
|        16 |   45 | `			if( pPrev->iFlags & PH7_CLASS_BOUND ){` |
|         - |   46 | `				/* php names the entity by the PREVIOUS declaration's kind and omits` |
|         - |   47 | `				 * the "(previously declared in ...)" clause for internal symbols. */` |
|        13 |   48 | `				if( pPrev->sFile.nByte > 0 ){` |
|        15 |   49 | `					PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|         - |   50 | `						"Cannot redeclare %s %z (previously declared in %.*s:%u)",` |
|         4 |   51 | `						GenStateClassKind(pPrev),&pClass->sName,` |
|         4 |   52 | `						pPrev->sFile.nByte,pPrev->sFile.zString,pPrev->nLine);` |
|         7 |   53 | `				}else{` |
|         4 |   54 | `					PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|         1 |   55 | `						"Cannot redeclare %s %z",GenStateClassKind(pPrev),&pClass->sName);` |
|         - |   56 | `				}` |
|        13 |   57 | `				return SXERR_ABORT;` |
|         - |   58 | `			}` |
|         3 |   59 | `			pPrev = pPrev->pNextName;` |
|         1 |   60 | `		}` |
|         1 |   61 | `	}` |
|      2973 |   62 | `	return SXRET_OK;` |
|    304889 |   63 | `}` |
|         - |   64 | `/*` |
|         - |   65 | ` * Extract the visibility level associated with a given keyword.` |
|         - |   66 | ` * According to the PHP language reference manual` |
|         - |   67 | ` *  Visibility:` |
|         - |   68 | ` *  The visibility of a property or method can be defined by prefixing` |
|         - |   69 | ` *  the declaration with the keywords public, protected or private.` |
|         - |   70 | ` *  Class members declared public can be accessed everywhere.` |
|         - |   71 | ` *  Members declared protected can be accessed only within the class` |
|         - |   72 | ` *  itself and by inherited and parent classes. Members declared as private` |
|         - |   73 | ` *  may only be accessed by the class that defines the member.` |
|         - |   74 | ` */` |
|   4355802 |   75 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|         5 |   76 | `{` |
|   4355807 |   77 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    312733 |   78 | `		return PH7_CLASS_PROT_PRIVATE;` |
|   4043079 |   79 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|    258223 |   80 | `		return PH7_CLASS_PROT_PROTECTED;` |
|         - |   81 | `	}` |
|         - |   82 | `	/* Assume public by default */` |
|   3784861 |   83 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   2177906 |   84 | `}` |
|         - |   85 | `/*` |
|         - |   86 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|         - |   87 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|         - |   88 | ` * token immediately followed by '='. Anything else with a leading type token` |
|         - |   89 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|         - |   90 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|         - |   91 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|         - |   92 | ` */` |
|    398880 |   93 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|         5 |   94 | `{` |
|         - |   95 | `	SyToken *p0, *p1;` |
|    398885 |   96 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |   97 | `		return 0;` |
|         - |   98 | `	}` |
|    398885 |   99 | `	p0 = pGen->pIn;` |
|         - |  100 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|    398885 |  101 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|       ! 0 |  102 | `		return 1;` |
|         - |  103 | `	}` |
|    398885 |  104 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|         5 |  105 | `		return 1;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* A name-like first token begins a type only when followed by another` |
|         - |  108 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|         - |  109 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|    398881 |  110 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|    398881 |  111 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|    398881 |  112 | `		if( p1 ){` |
|    398881 |  113 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|        44 |  114 | `				return 1;` |
|         - |  115 | `			}` |
|    398841 |  116 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|         5 |  117 | `				return 1;` |
|         - |  118 | `			}` |
|    199416 |  119 | `		}` |
|    199416 |  120 | `	}` |
|    398837 |  121 | `	return 0;` |
|    199445 |  122 | `}` |
|         - |  123 | `/*` |
|         - |  124 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|         - |  125 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|         - |  126 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|         - |  127 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|         - |  128 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|         - |  129 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|         - |  130 | ` * Peek only; never consumes tokens.` |
|         - |  131 | ` */` |
|        32 |  132 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|         4 |  133 | `{` |
|        36 |  134 | `	SyToken *p = pGen->pIn;` |
|        51 |  135 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        24 |  136 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|         3 |  137 | `		p++; /* skip leading unary sign(s) */` |
|         1 |  138 | `	}` |
|        36 |  139 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|        32 |  140 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|         - |  141 | `	}` |
|         6 |  142 | `	p++;` |
|         - |  143 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|         6 |  144 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|        20 |  145 | `}` |
|         - |  146 | `/*` |
|         - |  147 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|         - |  148 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|         - |  149 | `` * `$o->new`), not a `new` expression.`` |
|         - |  150 | ` */` |
|       164 |  151 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|         5 |  152 | `{` |
|         - |  153 | `	sxi32 iOp;` |
|       169 |  154 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|        27 |  155 | `		return 0;` |
|         - |  156 | `	}` |
|       145 |  157 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       145 |  158 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|        87 |  159 | `}` |
|         - |  160 | `/*` |
|         - |  161 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|         - |  162 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|         - |  163 | ` * interface-constant and (instance/static) property-default initializers` |
|         - |  164 | ` * ("New expressions are not supported in this context") while still allowing it` |
|         - |  165 | ` * in global constants, parameter defaults and static-local initializers (which` |
|         - |  166 | ` * are compiled by different functions and left untouched). The scan is` |
|         - |  167 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|         - |  168 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|         - |  169 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|         - |  170 | ` *` |
|         - |  171 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|         - |  172 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|         - |  173 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|         - |  174 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|         - |  175 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|         - |  176 | ` */` |
|    843636 |  177 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|         5 |  178 | `{` |
|    843641 |  179 | `	SyToken *p = pGen->pIn;` |
|    843641 |  180 | `	int iDepth = 0;` |
|   2226075 |  181 | `	while( p < pGen->pEnd ){` |
|   2226075 |  182 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    843589 |  183 | `			break; /* end of this initializer */` |
|         - |  184 | `		}` |
|   1382486 |  185 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    700335 |  186 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|     18169 |  187 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|         - |  188 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|         - |  189 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|         - |  190 | `			 * expression. */` |
|        13 |  191 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|        13 |  192 | `			p++;` |
|        13 |  193 | `			if( bArrow ){` |
|         - |  194 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|         - |  195 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|       ! 0 |  196 | `				int iBase = iDepth;` |
|       ! 0 |  197 | `				while( p < pGen->pEnd ){` |
|       ! 0 |  198 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  199 | `						iDepth++;` |
|       ! 0 |  200 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  201 | `						if( iDepth <= iBase ){` |
|       ! 0 |  202 | `							break; /* closes an enclosing group, not the fn's own */` |
|         - |  203 | `						}` |
|       ! 0 |  204 | `						iDepth--;` |
|       ! 0 |  205 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|       ! 0 |  206 | `						break;` |
|         - |  207 | `					}` |
|       ! 0 |  208 | `					p++;` |
|       ! 0 |  209 | `				}` |
|       ! 0 |  210 | `			}else{` |
|         - |  211 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|         - |  212 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|         - |  213 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|         - |  214 | `				 * then skip the balanced brace block. */` |
|        13 |  215 | `				int iLocal = 0;` |
|        33 |  216 | `				while( p < pGen->pEnd ){` |
|        33 |  217 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|        13 |  218 | `						break; /* body brace */` |
|         - |  219 | `					}` |
|        23 |  220 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        13 |  221 | `						iLocal++;` |
|        18 |  222 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        13 |  223 | `						if( iLocal > 0 ){` |
|        13 |  224 | `							iLocal--;` |
|         5 |  225 | `						}` |
|         5 |  226 | `					}` |
|        23 |  227 | `					p++;` |
|         3 |  228 | `				}` |
|        13 |  229 | `				if( p < pGen->pEnd ){` |
|        13 |  230 | `					int iBrace = 0; /* p is on the body '{' */` |
|        95 |  231 | `					while( p < pGen->pEnd ){` |
|        95 |  232 | `						if( p->nType & PH7_TK_OCB ){` |
|        15 |  233 | `							iBrace++;` |
|        89 |  234 | `						}else if( p->nType & PH7_TK_CCB ){` |
|        15 |  235 | `							iBrace--;` |
|        15 |  236 | `							if( iBrace == 0 ){` |
|        13 |  237 | `								p++;` |
|        13 |  238 | `								break;` |
|         - |  239 | `							}` |
|         1 |  240 | `						}` |
|        85 |  241 | `						p++;` |
|         3 |  242 | `					}` |
|         5 |  243 | `				}` |
|         - |  244 | `			}` |
|        13 |  245 | `			continue;` |
|         - |  246 | `		}` |
|   1382481 |  247 | `		if( p->nType & PH7_TK_OCB ){` |
|        45 |  248 | `			if( iDepth == 0 ){` |
|         - |  249 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|         - |  250 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|         - |  251 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|         - |  252 | `				 * is legal — don't scan into it. */` |
|        45 |  253 | `				break;` |
|         - |  254 | `			}` |
|       ! 0 |  255 | `			iDepth++;` |
|   1382437 |  256 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     58975 |  257 | `			iDepth++;` |
|   1352952 |  258 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     58973 |  259 | `			if( iDepth > 0 ){` |
|     58973 |  260 | `				iDepth--;` |
|     29484 |  261 | `			}` |
|   1293983 |  262 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    455425 |  263 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|         - |  264 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|         - |  265 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|         - |  266 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|        11 |  267 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|        11 |  268 | `				return 1;` |
|         - |  269 | `			}` |
|       ! 0 |  270 | `		}` |
|   1382429 |  271 | `		p++;` |
|         5 |  272 | `	}` |
|    843633 |  273 | `	return 0;` |
|    421823 |  274 | `}` |
|         - |  275 | `/*` |
|         - |  276 | ` * php keeps class CONSTANTS and PROPERTIES in separate namespaces: a class may` |
|         - |  277 | `` * declare `const C` and `public $C` together, and `$obj->C` never resolves to the`` |
|         - |  278 | ` * constant. PHL stores each in its own table (constants in hConst, properties in` |
|         - |  279 | ` * hAttr), so these two lookups target the right namespace and never collide.` |
|         - |  280 | ` */` |
|    398872 |  281 | `static ph7_class_attr * GenStateExtractConstant(ph7_class *pClass,SyString *pName)` |
|         5 |  282 | `{` |
|    398877 |  283 | `	return PH7_ClassExtractConstant(pClass,pName->zString,pName->nByte);` |
|         5 |  284 | `}` |
|    676196 |  285 | `static ph7_class_attr * GenStateExtractProperty(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|         5 |  286 | `{` |
|    676201 |  287 | `	return PH7_ClassExtractAttribute(pClass,zName,nByte);` |
|         5 |  288 | `}` |
|         - |  289 | `/*` |
|         - |  290 | ` * Return TRUE if the constant expression starting at the current token performs a` |
|         - |  291 | ` * FUNCTION CALL, which php rejects with "Constant expression contains invalid` |
|         - |  292 | `` * operations" in every constant-expression context (global `const`, class/interface`` |
|         - |  293 | ` * constants, property defaults, parameter defaults, attribute arguments).` |
|         - |  294 | ` *` |
|         - |  295 | ` * Shares GenStateInitHasNewExpr's walk: depth-aware so a nested call is caught` |
|         - |  296 | `` * (`[1, f()]`) and an inner comma does not end the scan, and skipping any`` |
|         - |  297 | `` * `function`/`fn` construct outright — a call inside a closure body runs when the`` |
|         - |  298 | ` * closure is invoked, so php allows it.` |
|         - |  299 | ` *` |
|         - |  300 | ` * Deliberately NOT rejected, because php accepts them:` |
|         - |  301 | `` *   - first-class callables, `strlen(...)` — the parens hold only the ellipsis;`` |
|         - |  302 | ``  *   - `new X(...)` — constructor calls are legal in the contexts that allow `new` `` |
|         - |  303 | ` *     at all, and GenStateInitHasNewExpr owns the contexts that do not;` |
|         - |  304 | ` *   - anything inside a ternary. php FOLDS a constant condition and only rejects a` |
|         - |  305 | `` *     call that survives, so `true ? 1 : f()` is legal while `false ? 1 : f()` is`` |
|         - |  306 | ` *     not. PHL does not constant-fold here, so rather than risk rejecting valid` |
|         - |  307 | `` *     code this scan skips an initializer containing a depth-0 `?` entirely. The`` |
|         - |  308 | ` *     residual is a call hiding in a TAKEN ternary branch, which stays accepted.` |
|         - |  309 | ` */` |
|    843756 |  310 | `PH7_PRIVATE int PH7_GenStateInitHasCallExpr(ph7_gen_state *pGen)` |
|         5 |  311 | `{` |
|    843761 |  312 | `	SyToken *p = pGen->pIn;` |
|    843761 |  313 | `	int iDepth = 0;` |
|         - |  314 | `	/* Conservative ternary bail-out (see the note above). */` |
|         - |  315 | `	{` |
|    843761 |  316 | `		SyToken *q = pGen->pIn;` |
|    843761 |  317 | `		int iQd = 0;` |
|   2227699 |  318 | `		while( q < pGen->pEnd ){` |
|   2227661 |  319 | `			if( iQd == 0 && (q->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    843717 |  320 | `				break;` |
|         - |  321 | `			}` |
|   1383949 |  322 | `			if( q->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     59145 |  323 | `				iQd++;` |
|   1354379 |  324 | `			}else if( q->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     59145 |  325 | `				if( iQd > 0 ){ iQd--; }` |
|   1295239 |  326 | `			}else if( (q->nType & PH7_TK_OP) && q->pUserData` |
|    455661 |  327 | `				&& ((const ph7_expr_op *)q->pUserData)->iOp == EXPR_OP_QUESTY ){` |
|         8 |  328 | `				return 0;` |
|         - |  329 | `			}` |
|   1383943 |  330 | `			q++;` |
|         5 |  331 | `		}` |
|         - |  332 | `	}` |
|   2226427 |  333 | `	while( p < pGen->pEnd ){` |
|   2226427 |  334 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    843709 |  335 | `			break; /* end of this initializer */` |
|         - |  336 | `		}` |
|   1382718 |  337 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    700455 |  338 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|     18175 |  339 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|         - |  340 | `			/* A call inside a closure/arrow-fn is deferred to call time: skip the` |
|         - |  341 | `			 * whole construct. Delegating to the sibling scanner is not possible` |
|         - |  342 | ``			 * (it reports `new`), so mirror its bracket walk. */`` |
|        17 |  343 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|        17 |  344 | `			int iBase = iDepth;` |
|        17 |  345 | `			p++;` |
|        17 |  346 | `			if( bArrow ){` |
|       ! 0 |  347 | `				while( p < pGen->pEnd ){` |
|       ! 0 |  348 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  349 | `						iDepth++;` |
|       ! 0 |  350 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  351 | `						if( iDepth <= iBase ){` |
|       ! 0 |  352 | `							break;` |
|         - |  353 | `						}` |
|       ! 0 |  354 | `						iDepth--;` |
|       ! 0 |  355 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|       ! 0 |  356 | `						break;` |
|         - |  357 | `					}` |
|       ! 0 |  358 | `					p++;` |
|       ! 0 |  359 | `				}` |
|       ! 0 |  360 | `			}else{` |
|        17 |  361 | `				int iLocal = 0;` |
|        45 |  362 | `				while( p < pGen->pEnd ){` |
|        45 |  363 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|        17 |  364 | `						break;` |
|         - |  365 | `					}` |
|        31 |  366 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        17 |  367 | `						iLocal++;` |
|        24 |  368 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        17 |  369 | `						if( iLocal > 0 ){ iLocal--; }` |
|         7 |  370 | `					}` |
|        31 |  371 | `					p++;` |
|         3 |  372 | `				}` |
|        17 |  373 | `				if( p < pGen->pEnd ){` |
|        17 |  374 | `					int iBrace = 0;` |
|       121 |  375 | `					while( p < pGen->pEnd ){` |
|       121 |  376 | `						if( p->nType & PH7_TK_OCB ){` |
|        19 |  377 | `							iBrace++;` |
|       113 |  378 | `						}else if( p->nType & PH7_TK_CCB ){` |
|        19 |  379 | `							iBrace--;` |
|        19 |  380 | `							if( iBrace == 0 ){` |
|        17 |  381 | `								p++;` |
|        17 |  382 | `								break;` |
|         - |  383 | `							}` |
|         1 |  384 | `						}` |
|       107 |  385 | `						p++;` |
|         3 |  386 | `					}` |
|         7 |  387 | `				}` |
|         - |  388 | `			}` |
|        17 |  389 | `			continue;` |
|         - |  390 | `		}` |
|   1382709 |  391 | `		if( p->nType & PH7_TK_OCB ){` |
|        43 |  392 | `			if( iDepth == 0 ){` |
|        43 |  393 | `				break; /* property-hook list: the default expression ends here */` |
|         - |  394 | `			}` |
|       ! 0 |  395 | `			iDepth++;` |
|   1382667 |  396 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|         - |  397 | `			/* A '(' directly after a NAME is a call. A name here is an identifier` |
|         - |  398 | ``			 * token; `new X(` is excluded by looking for the `new` operator, and`` |
|         - |  399 | ``			 * `f(...)` (first-class callable) by peeking for a lone ellipsis. */`` |
|     58994 |  400 | `			if( (p->nType & PH7_TK_LPAREN) && p > pGen->pIn` |
|     18144 |  401 | `				&& (p[-1].nType & PH7_TK_ID)` |
|      9085 |  402 | `				&& !((p[-1].nType & PH7_TK_OP) && p[-1].pUserData` |
|       ! 0 |  403 | `					&& ((const ph7_expr_op *)p[-1].pUserData)->iOp == EXPR_OP_NEW) ){` |
|        20 |  404 | `				int bNewCtor = 0;` |
|        20 |  405 | `				SyToken *q = &p[-1];` |
|         - |  406 | ``				/* Walk back over a qualified name (A\B, A::b, $o->m) to a `new`. */`` |
|        20 |  407 | `				while( q > pGen->pIn && (q[-1].nType & (PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) ){` |
|        10 |  408 | `					if( (q[-1].nType & PH7_TK_OP) && q[-1].pUserData` |
|        13 |  409 | `						&& ((const ph7_expr_op *)q[-1].pUserData)->iOp == EXPR_OP_NEW ){` |
|        13 |  410 | `						bNewCtor = 1;` |
|        13 |  411 | `						break;` |
|         - |  412 | `					}` |
|       ! 0 |  413 | `					if( !GenStateTokenIsMemberOp(&q[-1]) && (q[-1].nType & PH7_TK_NSSEP) == 0 ){` |
|       ! 0 |  414 | `						break;` |
|         - |  415 | `					}` |
|       ! 0 |  416 | `					q--;` |
|       ! 0 |  417 | `				}` |
|        16 |  418 | `				if( !bNewCtor` |
|        15 |  419 | `					&& !(&p[1] < pGen->pEnd && (p[1].nType & PH7_TK_ELLIPSIS)) ){` |
|         6 |  420 | `					return 1;` |
|         - |  421 | `				}` |
|         6 |  422 | `			}` |
|     58995 |  423 | `			iDepth++;` |
|   1353168 |  424 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     58995 |  425 | `			if( iDepth > 0 ){` |
|     58995 |  426 | `				iDepth--;` |
|     29495 |  427 | `			}` |
|     29495 |  428 | `		}` |
|   1382663 |  429 | `		p++;` |
|         5 |  430 | `	}` |
|    843751 |  431 | `	return 0;` |
|    421883 |  432 | `}` |
|         - |  433 | `/*` |
|         - |  434 | ` * Scan a constant-expression initializer for a closure / arrow-function literal` |
|         - |  435 | ` * and report php's two compile-time rules the call scanner above does NOT: a` |
|         - |  436 | `` * NON-STATIC closure is `Fatal error: Closures in constant expressions must be`` |
|         - |  437 | `` * static`, and ANY arrow function is `Constant expression contains invalid`` |
|         - |  438 | `` * operations` (there is no static-`fn` escape -- `static fn()=>1` is rejected`` |
|         - |  439 | `` * too). Only `static function(){...}` is accepted; its body is regular runtime`` |
|         - |  440 | ` * code, so a closure NESTED inside it is skipped, not rejected.` |
|         - |  441 | ` *` |
|         - |  442 | ` * Returns 0 (clean), 1 (arrow fn -> "invalid operations") or 2 (non-static` |
|         - |  443 | ` * closure -> "must be static"). Mirrors PH7_GenStateInitHasCallExpr's construct` |
|         - |  444 | ` * skip and initializer-terminator tracking, but WITHOUT its conservative ternary` |
|         - |  445 | ` * bail-out: php rejects the closure even inside a branch it would fold away` |
|         - |  446 | `` * (`true ? function(){} : 1` still fatals), because nothing is constant-folded at`` |
|         - |  447 | `` * this stage. Wired beside every call-scanner site (global `const`, class /`` |
|         - |  448 | ` * interface constants, property defaults).` |
|         - |  449 | ` */` |
|    843762 |  450 | `PH7_PRIVATE int PH7_GenStateInitClosureError(ph7_gen_state *pGen)` |
|         5 |  451 | `{` |
|    843767 |  452 | `	SyToken *p = pGen->pIn;` |
|    843767 |  453 | `	int iDepth = 0;` |
|   2226477 |  454 | `	while( p < pGen->pEnd ){` |
|   2226477 |  455 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    843717 |  456 | `			break; /* end of this initializer */` |
|         - |  457 | `		}` |
|   1382760 |  458 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    700480 |  459 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|     18181 |  460 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|        24 |  461 | `			if( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ){` |
|         - |  462 | `				/* An arrow function is never a valid constant expression. */` |
|         3 |  463 | `				return 1;` |
|         - |  464 | `			}` |
|         - |  465 | ``			/* A closure literal must be `static function`: the modifier sits in the`` |
|         - |  466 | ``			 * token immediately before `function`. `static::X` never reaches here`` |
|         - |  467 | ``			 * (its next token is `::`, not the keyword). */`` |
|        21 |  468 | `			if( !(p > pGen->pIn && (p[-1].nType & PH7_TK_KEYWORD)` |
|        14 |  469 | `				&& SX_PTR_TO_INT(p[-1].pUserData) == PH7_TKWRD_STATIC) ){` |
|         6 |  470 | `				return 2;` |
|         - |  471 | `			}` |
|         - |  472 | ``			/* `static function(){...}`: accepted -- skip the whole construct`` |
|         - |  473 | `			 * (parameter parens then the brace-balanced body) exactly like the call` |
|         - |  474 | `			 * scanner, then keep looking for a sibling closure in the initializer. */` |
|        17 |  475 | `			p++;` |
|         - |  476 | `			{` |
|        17 |  477 | `				int iLocal = 0;` |
|        45 |  478 | `				while( p < pGen->pEnd ){` |
|        45 |  479 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|        17 |  480 | `						break;` |
|         - |  481 | `					}` |
|        31 |  482 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        17 |  483 | `						iLocal++;` |
|        24 |  484 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        17 |  485 | `						if( iLocal > 0 ){ iLocal--; }` |
|         7 |  486 | `					}` |
|        31 |  487 | `					p++;` |
|         3 |  488 | `				}` |
|        17 |  489 | `				if( p < pGen->pEnd ){` |
|        17 |  490 | `					int iBrace = 0;` |
|       121 |  491 | `					while( p < pGen->pEnd ){` |
|       121 |  492 | `						if( p->nType & PH7_TK_OCB ){` |
|        19 |  493 | `							iBrace++;` |
|       113 |  494 | `						}else if( p->nType & PH7_TK_CCB ){` |
|        19 |  495 | `							iBrace--;` |
|        19 |  496 | `							if( iBrace == 0 ){` |
|        17 |  497 | `								p++;` |
|        17 |  498 | `								break;` |
|         - |  499 | `							}` |
|         1 |  500 | `						}` |
|       107 |  501 | `						p++;` |
|         3 |  502 | `					}` |
|         7 |  503 | `				}` |
|         - |  504 | `			}` |
|        17 |  505 | `			continue;` |
|         - |  506 | `		}` |
|   1382745 |  507 | `		if( p->nType & PH7_TK_OCB ){` |
|        45 |  508 | `			if( iDepth == 0 ){` |
|        45 |  509 | `				break; /* property-hook list: the default expression ends here */` |
|         - |  510 | `			}` |
|       ! 0 |  511 | `			iDepth++;` |
|   1382701 |  512 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     58999 |  513 | `			iDepth++;` |
|   1353204 |  514 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     58999 |  515 | `			if( iDepth > 0 ){` |
|     58999 |  516 | `				iDepth--;` |
|     29497 |  517 | `			}` |
|     29497 |  518 | `		}` |
|   1382701 |  519 | `		p++;` |
|         5 |  520 | `	}` |
|    843761 |  521 | `	return 0;` |
|    421886 |  522 | `}` |
|         - |  523 | `/*` |
|         - |  524 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|         - |  525 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|         - |  526 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|         - |  527 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|         - |  528 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|         - |  529 | ` * share the same backing.` |
|         - |  530 | ` */` |
|     18660 |  531 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|         - |  532 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|         5 |  533 | `{` |
|     18665 |  534 | `	pAttr->nType = nType;` |
|     18665 |  535 | `	pAttr->sClass = *pClass;` |
|     18665 |  536 | `	pAttr->sTypeName = *pTypeName;` |
|     18665 |  537 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  538 | `		sxu32 i;` |
|       115 |  539 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        79 |  540 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|        79 |  541 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|        42 |  542 | `		}` |
|        18 |  543 | `	}` |
|     18665 |  544 | `}` |
|    398880 |  545 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  546 | `{` |
|    398885 |  547 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  548 | `	SySet *pInstrContainer;` |
|         - |  549 | `	ph7_class_attr *pCons;` |
|         - |  550 | `	SyString *pName;` |
|         - |  551 | `	sxi32 rc;` |
|    398885 |  552 | `	sxu32 nType = 0;` |
|         - |  553 | `	SyString sTypeClass;` |
|         - |  554 | `	SyString sTypeText;` |
|         - |  555 | `	SySet aUnionAlts;` |
|    398885 |  556 | `	sxi32 iTypeFlags = 0;` |
|    398885 |  557 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    398885 |  558 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    398885 |  559 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  560 | `	/* Extract visibility level */` |
|    398885 |  561 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  562 | `	/* Mark as constant */` |
|    398885 |  563 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|    398885 |  564 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|         - |  565 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|         - |  566 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|    398909 |  567 | `	if( GenStateClassConstHasType(pGen) ){` |
|        76 |  568 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|        48 |  569 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|         - |  570 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|         - |  571 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|         - |  572 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|         - |  573 | `		 * and success paths release. */` |
|        52 |  574 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  575 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  576 | `			goto Synchronize;` |
|        52 |  577 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  578 | `			return SXERR_ABORT;` |
|        52 |  579 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  580 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  581 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|       ! 0 |  582 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  583 | `				return SXERR_ABORT;` |
|         - |  584 | `			}` |
|       ! 0 |  585 | `			goto Synchronize;` |
|         - |  586 | `		}` |
|        52 |  587 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        24 |  588 | `	}` |
|    199440 |  589 | `loop:` |
|         - |  590 | ``	/* php 8 accepts EVERY reserved word as a class-constant name — `const list = 5`,`` |
|         - |  591 | ``	 * `const match`, `const function`, even `const true` — because a class constant is`` |
|         - |  592 | ``	 * addressed only through `C::name`, where no keyword can be ambiguous. The single`` |
|         - |  593 | ``	 * exception is `class`, reserved for `C::class`, and it gets its own message.`` |
|         - |  594 | `	 * (Method names already accept the whole set; this is the member-name side of the` |
|         - |  595 | ``	 * same rule. Global `const` is NOT the same rule: php rejects a reserved word there.)`` |
|         - |  596 | `	 * A keyword arrives as PH7_TK_KEYWORD, which this ID-only test rejected — so PHL` |
|         - |  597 | ``	 * accepted only the alpha-OPERATOR keywords (`const new`, `const and`), which the`` |
|         - |  598 | `	 * lexer marks PH7_TK_ID\|PH7_TK_OP, and that partial allow-list looked like a design. */` |
|    398891 |  599 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  600 | `		/* Invalid constant name */` |
|       ! 0 |  601 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|       ! 0 |  602 | `		if( rc == SXERR_ABORT ){` |
|         - |  603 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  604 | `			return SXERR_ABORT;` |
|         - |  605 | `		}` |
|       ! 0 |  606 | `		goto Synchronize;` |
|         - |  607 | `	}` |
|         - |  608 | `	/* Peek constant name */` |
|    398891 |  609 | `	pName = &pGen->pIn->sData;` |
|    398886 |  610 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|    201745 |  611 | `		&& (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CLASS ){` |
|         3 |  612 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  613 | `			"A class constant must not be called 'class'; it is reserved for class name fetching");` |
|         3 |  614 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  615 | `			return SXERR_ABORT;` |
|         - |  616 | `		}` |
|         3 |  617 | `		goto Synchronize;` |
|         - |  618 | `	}` |
|         - |  619 | `	/* No reserved-CONSTANT check here: true/false/null are reserved GLOBAL constant` |
|         - |  620 | ``	 * names (compile_stmt.c still rejects `const true = 1`), but `C::true` addresses a`` |
|         - |  621 | `	 * class constant and php accepts the declaration like any other reserved word. The` |
|         - |  622 | `	 * member-name flag keeps the read from folding into the boolean literal. */` |
|         - |  623 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|    398889 |  624 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        76 |  625 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|        48 |  626 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|        24 |  627 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|        52 |  628 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  629 | `			return SXERR_ABORT;` |
|        52 |  630 | `		}else if( rc != SXRET_OK ){` |
|         3 |  631 | `			goto Synchronize;` |
|         - |  632 | `		}` |
|        23 |  633 | `	}` |
|         - |  634 | `	/* Advance the stream cursor */` |
|    398887 |  635 | `	pGen->pIn++;` |
|    398887 |  636 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  637 | `		/* Invalid declaration */` |
|       ! 0 |  638 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|       ! 0 |  639 | `		if( rc == SXERR_ABORT ){` |
|         - |  640 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  641 | `			return SXERR_ABORT;` |
|         - |  642 | `		}` |
|       ! 0 |  643 | `		goto Synchronize;` |
|         - |  644 | `	}` |
|    398887 |  645 | `	pGen->pIn++; /* Jump the equal sign */` |
|         - |  646 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|         - |  647 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|         - |  648 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|         - |  649 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|    398882 |  650 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|        49 |  651 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|         8 |  652 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  653 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|         2 |  654 | `			&pClass->sName,pName,&sTypeText);` |
|         6 |  655 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  656 | `			return SXERR_ABORT;` |
|         - |  657 | `		}` |
|         6 |  658 | `		goto Synchronize;` |
|         - |  659 | `	}` |
|         - |  660 | ``	/* php: a closure in a class/interface constant must be `static function`;`` |
|         - |  661 | ``	 * same rule (and messages) as the global `const` path in compile_stmt.c. */`` |
|         - |  662 | `	{` |
|    398883 |  663 | `		int iClo = PH7_GenStateInitClosureError(pGen);` |
|    398883 |  664 | `		if( iClo ){` |
|         4 |  665 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  666 | `				iClo == 2 ? "Closures in constant expressions must be static"` |
|         - |  667 | `				          : "Constant expression contains invalid operations");` |
|         3 |  668 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  669 | `				return SXERR_ABORT;` |
|         - |  670 | `			}` |
|         3 |  671 | `			goto Synchronize;` |
|         - |  672 | `		}` |
|         - |  673 | `	}` |
|         - |  674 | `	/* php: a constant expression may not CALL anything. Same rule as the global` |
|         - |  675 | ``	 * `const` path in compile_stmt.c. */`` |
|    398881 |  676 | `	if( PH7_GenStateInitHasCallExpr(pGen) ){` |
|       ! 0 |  677 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  678 | `			"Constant expression contains invalid operations");` |
|       ! 0 |  679 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  680 | `			return SXERR_ABORT;` |
|         - |  681 | `		}` |
|       ! 0 |  682 | `		goto Synchronize;` |
|         - |  683 | `	}` |
|         - |  684 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|         - |  685 | `	 * constant initializer ("New expressions are not supported in this context").` |
|         - |  686 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|    398881 |  687 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|         6 |  688 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  689 | `			"New expressions are not supported in this context");` |
|         6 |  690 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  691 | `			return SXERR_ABORT;` |
|         - |  692 | `		}` |
|         6 |  693 | `		goto Synchronize;` |
|         - |  694 | `	}` |
|         - |  695 | `	/* php: a class constant may not be redefined in the same class body. The` |
|         - |  696 | `	 * property path already guarded this; the constant path did not, so` |
|         - |  697 | ``	 * `class C{const X=1; const X=2;}` silently kept one of them. */`` |
|         - |  698 | ``	/* php keeps constants and properties in SEPARATE namespaces, so `const C` and`` |
|         - |  699 | ``	 * `public $C` coexist. PHL now stores them in disjoint tables (hConst / hAttr),`` |
|         - |  700 | `	 * so no collision — only a genuine constant redefinition is rejected below. */` |
|    398877 |  701 | `	if( GenStateExtractConstant(pClass,pName) != 0 ){` |
|       ! 0 |  702 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  703 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|       ! 0 |  704 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  705 | `			return SXERR_ABORT;` |
|         - |  706 | `		}` |
|       ! 0 |  707 | `		goto Synchronize;` |
|         - |  708 | `	}` |
|         - |  709 | `	/* Allocate a new class attribute */` |
|    398877 |  710 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    398877 |  711 | `	if( pCons ){` |
|    398877 |  712 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|    398877 |  713 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  714 | `			return SXERR_ABORT;` |
|         - |  715 | `		}` |
|    199436 |  716 | `	}` |
|    398877 |  717 | `	if( pCons == 0 ){` |
|       ! 0 |  718 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  719 | `		return SXERR_ABORT;` |
|         - |  720 | `	}` |
|    398877 |  721 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        46 |  722 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|        21 |  723 | `	}` |
|         - |  724 | `	/* Swap bytecode container */` |
|    398877 |  725 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    398877 |  726 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|         - |  727 | `	/* Compile constant value.` |
|         - |  728 | `	 */` |
|    398877 |  729 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    398877 |  730 | `	if( rc == SXERR_EMPTY ){` |
|         3 |  731 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|         3 |  732 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  733 | `			return SXERR_ABORT;` |
|         - |  734 | `		}` |
|         1 |  735 | `	}` |
|         - |  736 | `	/* Emit the done instruction */` |
|    398877 |  737 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    398877 |  738 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    398877 |  739 | `	if( rc == SXERR_ABORT ){` |
|         - |  740 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  741 | `		return SXERR_ABORT;` |
|         - |  742 | `	}` |
|         - |  743 | `	/* All done,install the constant */` |
|    398877 |  744 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|    398877 |  745 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  746 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  747 | `		return SXERR_ABORT;` |
|         - |  748 | `	}` |
|    398877 |  749 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  750 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|         7 |  751 | `		pGen->pIn++; /* Jump the comma */` |
|         - |  752 | `		/* A reserved word is a valid name for EVERY constant in the declaration, not` |
|         - |  753 | ``		 * just the first (`const list = 1, match = 2`) — same allow-list as the head. */`` |
|         7 |  754 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  755 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  756 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  757 | `				pTok--;` |
|       ! 0 |  758 | `			}` |
|       ! 0 |  759 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  760 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|       ! 0 |  761 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  762 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  763 | `				return SXERR_ABORT;` |
|         - |  764 | `			}` |
|       ! 0 |  765 | `		}else{` |
|         7 |  766 | `			goto loop;` |
|         - |  767 | `		}` |
|       ! 0 |  768 | `	}` |
|    398871 |  769 | `	SySetRelease(&aUnionAlts);` |
|    398871 |  770 | `	return SXRET_OK;` |
|         7 |  771 | `Synchronize:` |
|        17 |  772 | `	SySetRelease(&aUnionAlts);` |
|         - |  773 | `	/* Synchronize with the first semi-colon */` |
|        67 |  774 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        53 |  775 | `		pGen->pIn++;` |
|         3 |  776 | `	}` |
|        17 |  777 | `	return SXERR_CORRUPT;` |
|    199445 |  778 | `}` |
|         - |  779 | `/*` |
|         - |  780 | ` * complie a class attribute or Properties in the PHP jargon.` |
|         - |  781 | ` * According to the PHP language reference manual` |
|         - |  782 | ` *  Properties` |
|         - |  783 | ` *  Class member variables are called "properties". You may also see them referred` |
|         - |  784 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|         - |  785 | ` *  of this reference we will use "properties". They are defined by using one` |
|         - |  786 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|         - |  787 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|         - |  788 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|         - |  789 | ` *  and must not depend on run-time information in order to be evaluated.` |
|         - |  790 | ` * Symisc eXtension.` |
|         - |  791 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|         - |  792 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  793 | ` *  Example:` |
|         - |  794 | ` *   class Test{` |
|         - |  795 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  796 | ` *   };` |
|         - |  797 | ` *   var_dump(TEST::myVar);` |
|         - |  798 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  799 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  800 | ` */` |
|         - |  801 | `/*` |
|         - |  802 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|         - |  803 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|         - |  804 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|         - |  805 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|         - |  806 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|         - |  807 | ` */` |
|   2946042 |  808 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|         5 |  809 | `{` |
|   2946047 |  810 | `	SyToken *p = pStart;` |
|   2946047 |  811 | `	int bFirst = 1;` |
|   2946047 |  812 | `	if( p >= pEnd ) return 0;` |
|         - |  813 | ``	/* Optional nullable `?` shorthand. */`` |
|   2946047 |  814 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|        56 |  815 | `		p++;` |
|        56 |  816 | `		if( p >= pEnd ) return 0;` |
|        26 |  817 | `	}` |
|         - |  818 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|         - |  819 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|         - |  820 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|         - |  821 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   1473021 |  822 | `	for(;;){` |
|   2946081 |  823 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|         - |  824 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|         3 |  825 | `			p++;` |
|         9 |  826 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|         3 |  827 | `			if( p >= pEnd ) return 0;` |
|         3 |  828 | `			p++; /* skip ')' */` |
|         2 |  829 | `		}else{` |
|         - |  830 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|         - |  831 | ``			 * then any `&`-joined intersection members. */`` |
|   2946079 |  832 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|   2946079 |  833 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  834 | `				return 0;` |
|         - |  835 | `			}` |
|         - |  836 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|         - |  837 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|         - |  838 | `			 * may still appear at the initial dispatch site). */` |
|   2946079 |  839 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|   2946003 |  840 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|   2945998 |  841 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    132139 |  842 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|   2927437 |  843 | `					return 0;` |
|         - |  844 | `				}` |
|      9283 |  845 | `			}` |
|     18647 |  846 | `			p++;` |
|     18649 |  847 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  848 | `				p += 2;` |
|         1 |  849 | `			}` |
|     27966 |  850 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|     18650 |  851 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  852 | `				p++; /* skip '&' */` |
|         3 |  853 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|         3 |  854 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|         3 |  855 | `				p++;` |
|         3 |  856 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       ! 0 |  857 | `					p += 2;` |
|       ! 0 |  858 | `				}` |
|         1 |  859 | `			}` |
|         - |  860 | `		}` |
|     18649 |  861 | `		bFirst = 0;` |
|     18644 |  862 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        39 |  863 | `			&& p->sData.zString[0] == '\|' ){` |
|        39 |  864 | ``			p++; /* next `\|`-separated part */`` |
|        39 |  865 | `			continue;` |
|         - |  866 | `		}` |
|     18615 |  867 | `		break;` |
|       ! 0 |  868 | `	}` |
|     18615 |  869 | `	if( p >= pEnd ) return 0;` |
|     18615 |  870 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   1473026 |  871 | `}` |
|         - |  872 |  |
|         - |  873 | `/*` |
|         - |  874 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|         - |  875 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|         - |  876 | ` * if not). Recognized forms:` |
|         - |  877 | ` *   ?Type, array, bool, int, float, string, object,` |
|         - |  878 | ` *   self, parent, \Ns\ClassName, ClassName` |
|         - |  879 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|         - |  880 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|         - |  881 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|         - |  882 | ` * on unrecoverable error.` |
|         - |  883 | ` *` |
|         - |  884 | ` * When a type is parsed:` |
|         - |  885 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|         - |  886 | ` *   *pClass is set to the class name (for class types)` |
|         - |  887 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|         - |  888 | ` *   *pTypeText is set to the original text span of the type` |
|         - |  889 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|         - |  890 | ` */` |
|     18622 |  891 | `static sxi32 GenStateParsePropertyType(` |
|         - |  892 | `	ph7_gen_state *pGen,` |
|         - |  893 | `	sxu32 *pnType,` |
|         - |  894 | `	SyString *pClass,` |
|         - |  895 | `	sxi32 *piTypeFlags,` |
|         - |  896 | `	SyString *pTypeText,` |
|         - |  897 | `	SySet *pAlts` |
|         5 |  898 | `){` |
|     18627 |  899 | `	sxi32 iFlags = 0;` |
|         - |  900 | `	sxi32 rc;` |
|     18627 |  901 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  902 | `		return SXRET_OK;` |
|         - |  903 | `	}` |
|         - |  904 | `	/* If the first token is '$', there's no type */` |
|     18627 |  905 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       ! 0 |  906 | `		return SXRET_OK;` |
|         - |  907 | `	}` |
|     18627 |  908 | `	rc = GenStateParseUnionTypeDecl(` |
|      9311 |  909 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|         - |  910 | `		PH7_CLASS_ATTR_NULLABLE,` |
|         - |  911 | `		PH7_CLASS_ATTR_UNION,` |
|         - |  912 | `		/* bAllowVoid */ 0,` |
|     18622 |  913 | `		pGen->pIn->nLine);` |
|     18627 |  914 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  915 | `		return rc;` |
|         - |  916 | `	}` |
|         - |  917 | `	/* Verify next token is '$' (start of property name) */` |
|     18627 |  918 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  919 | `		return SXERR_SYNTAX;` |
|         - |  920 | `	}` |
|     18627 |  921 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     18627 |  922 | `	return SXRET_OK;` |
|      9316 |  923 | `}` |
|         - |  924 |  |
|         - |  925 | `/*` |
|         - |  926 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|         - |  927 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|         - |  928 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|         - |  929 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|         - |  930 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|         - |  931 | ` * by the type parser itself before reaching here.` |
|         - |  932 | ` *` |
|         - |  933 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|         - |  934 | ` * use in the error message.` |
|         - |  935 | ` */` |
|     18846 |  936 | `static int GenStateIsDisallowedPropertyAtom(` |
|         - |  937 | `	sxu32 nType,` |
|         - |  938 | `	const SyString *pClass,` |
|         - |  939 | `	const char **pzName,` |
|         - |  940 | `	sxu32 *pnName)` |
|         5 |  941 | `{` |
|         - |  942 | `	const char *z;` |
|         - |  943 | `	sxu32 n;` |
|     18851 |  944 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     18759 |  945 | `		return 0;` |
|         - |  946 | `	}` |
|        97 |  947 | `	z = pClass->zString;` |
|        97 |  948 | `	n = pClass->nByte;` |
|        97 |  949 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|         8 |  950 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|         - |  951 | `	}` |
|         - |  952 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|         - |  953 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|         - |  954 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|        91 |  955 | `	return 0;` |
|      9428 |  956 | `}` |
|         - |  957 |  |
|         - |  958 | `/*` |
|         - |  959 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|         - |  960 | ` * constant) — the main atom plus any union alternatives — against the` |
|         - |  961 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|         - |  962 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|         - |  963 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|         - |  964 | ` * type T" vs "Class constant C::X cannot have type T").` |
|         - |  965 | ` *` |
|         - |  966 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|         - |  967 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|         - |  968 | ` */` |
|     18756 |  969 | `PH7_PRIVATE sxi32 GenStateValidateMemberType(` |
|         - |  970 | `	ph7_gen_state *pGen,` |
|         - |  971 | `	ph7_class *pClass,` |
|         - |  972 | `	const SyString *pMemberName,` |
|         - |  973 | `	sxu32 nType,` |
|         - |  974 | `	const SyString *pTypeClass,` |
|         - |  975 | `	const SyString *pTypeText,` |
|         - |  976 | `	SySet *pUnionAlts,` |
|         - |  977 | `	const char *zErrFmt,` |
|         - |  978 | `	sxu32 nLine)` |
|         5 |  979 | `{` |
|     18761 |  980 | `	const char *zBad = 0;` |
|     18761 |  981 | `	sxu32 nBad = 0;` |
|         - |  982 | `	SyString sFallback;` |
|         - |  983 | `	const SyString *pBad;` |
|         - |  984 | `	sxi32 rc;` |
|     18761 |  985 | `	int bDisallowed = 0;` |
|     18761 |  986 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|         5 |  987 | `		bDisallowed = 1;` |
|     18759 |  988 | `	}else if( pUnionAlts ){` |
|         - |  989 | `		sxu32 i;` |
|       137 |  990 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|        95 |  991 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|        95 |  992 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|         3 |  993 | `				bDisallowed = 1;` |
|         3 |  994 | `				break;` |
|         - |  995 | `			}` |
|        49 |  996 | `		}` |
|        22 |  997 | `	}` |
|     18761 |  998 | `	if( !bDisallowed ){` |
|     18755 |  999 | `		return SXRET_OK;` |
|         - | 1000 | `	}` |
|         - | 1001 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|         - | 1002 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|         - | 1003 | `	 * canonical spelling if the type text is unavailable. */` |
|         8 | 1004 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|         8 | 1005 | `		pBad = pTypeText;` |
|         5 | 1006 | `	}else{` |
|       ! 0 | 1007 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|       ! 0 | 1008 | `		pBad = &sFallback;` |
|         - | 1009 | `	}` |
|        11 | 1010 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         3 | 1011 | `		zErrFmt,` |
|         3 | 1012 | `		&pClass->sName,pMemberName,pBad);` |
|         8 | 1013 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 1014 | `		return SXERR_ABORT;` |
|         - | 1015 | `	}` |
|         8 | 1016 | `	return SXERR_SYNTAX;` |
|      9383 | 1017 | `}` |
|         - | 1018 | `/*` |
|         - | 1019 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|         - | 1020 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|         - | 1021 | ` * matched as a plain identifier in the class-member modifier position rather` |
|         - | 1022 | ` * than promoted to a lexer keyword.` |
|         - | 1023 | ` */` |
|  27170380 | 1024 | `PH7_PRIVATE int GenStateIsReadonly(SyToken *pTok)` |
|         5 | 1025 | `{` |
|  27437945 | 1026 | `	return (pTok->nType & PH7_TK_ID)` |
|  13852750 | 1027 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
|  27437940 | 1028 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|         5 | 1029 | `}` |
|         - | 1030 | `/*` |
|         - | 1031 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|         - | 1032 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|         - | 1033 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|         - | 1034 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|         - | 1035 | ` */` |
|   9600690 | 1036 | `PH7_PRIVATE sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|         5 | 1037 | `{` |
|   9600695 | 1038 | `	*pnTok = 0;` |
|   9600690 | 1039 | `	if( &pTok[3] < pEnd` |
|   8965634 | 1040 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|   7281363 | 1041 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|   3116082 | 1042 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        16 | 1043 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|        16 | 1044 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|        21 | 1045 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|        17 | 1046 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|        17 | 1047 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|        17 | 1048 | `			*pnTok = 4;` |
|        17 | 1049 | `			return nKw;` |
|         - | 1050 | `		}` |
|       ! 0 | 1051 | `	}` |
|   9600679 | 1052 | `	return 0;` |
|   4800350 | 1053 | `}` |
|         - | 1054 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|        16 | 1055 | `PH7_PRIVATE sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|         1 | 1056 | `{` |
|        17 | 1057 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|        13 | 1058 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|         - | 1059 | `	}` |
|         5 | 1060 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|         3 | 1061 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|         - | 1062 | `	}` |
|         3 | 1063 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|         9 | 1064 | `}` |
|    676116 | 1065 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 | 1066 | `{` |
|    676121 | 1067 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 1068 | `	ph7_class_attr *pAttr;` |
|         - | 1069 | `	SyString *pName;` |
|         - | 1070 | `	sxi32 rc;` |
|    676121 | 1071 | `	sxu32 nType = 0;` |
|         - | 1072 | `	SyString sTypeClass;` |
|         - | 1073 | `	SyString sTypeText;` |
|         - | 1074 | `	SySet aUnionAlts;` |
|    676121 | 1075 | `	sxi32 iTypeFlags = 0;` |
|    676121 | 1076 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    676121 | 1077 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    676121 | 1078 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - | 1079 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|         - | 1080 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|         - | 1081 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|    676121 | 1082 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|        21 | 1083 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|         9 | 1084 | `	}` |
|         - | 1085 | `	/* Extract visibility level */` |
|    676121 | 1086 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - | 1087 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|    685432 | 1088 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     18627 | 1089 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     18627 | 1090 | `		if( rc == SXERR_CORRUPT ){` |
|         - | 1091 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 | 1092 | `			goto Synchronize;` |
|     18627 | 1093 | `		}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 | 1094 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 1095 | `				"Invalid property type or declaration near '%z'",` |
|       ! 0 | 1096 | `				&pGen->pIn->sData);` |
|       ! 0 | 1097 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1098 | `				return SXERR_ABORT;` |
|         - | 1099 | `			}` |
|       ! 0 | 1100 | `			goto Synchronize;` |
|     18627 | 1101 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 | 1102 | `			return SXERR_ABORT;` |
|         - | 1103 | `		}` |
|      9311 | 1104 | `	}` |
|       ! 0 | 1105 | `loop:` |
|    676125 | 1106 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 | 1107 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|       ! 0 | 1108 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1109 | `			return SXERR_ABORT;` |
|         - | 1110 | `		}` |
|       ! 0 | 1111 | `		goto Synchronize;` |
|         - | 1112 | `	}` |
|    676125 | 1113 | `	pGen->pIn++; /* Jump the dollar sign */` |
|    676125 | 1114 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         - | 1115 | `		/* Invalid attribute name */` |
|       ! 0 | 1116 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|       ! 0 | 1117 | `		if( rc == SXERR_ABORT ){` |
|         - | 1118 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 1119 | `			return SXERR_ABORT;` |
|         - | 1120 | `		}` |
|       ! 0 | 1121 | `		goto Synchronize;` |
|         - | 1122 | `	}` |
|         - | 1123 | `	/* Peek attribute name */` |
|    676125 | 1124 | `	pName = &pGen->pIn->sData;` |
|         - | 1125 | `	/* Advance the stream cursor */` |
|    676125 | 1126 | `	pGen->pIn++;` |
|    676125 | 1127 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|         - | 1128 | `		/* Invalid declaration */` |
|         - | 1129 | `		/* php reports the offending token here, expecting "," or ";". */` |
|         3 | 1130 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\",\" or \";\"");` |
|         3 | 1131 | `		if( rc == SXERR_ABORT ){` |
|         - | 1132 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 1133 | `			return SXERR_ABORT;` |
|         - | 1134 | `		}` |
|         3 | 1135 | `		goto Synchronize;` |
|         - | 1136 | `	}` |
|         - | 1137 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|         - | 1138 | `	 * the read visibility must not be narrower than the set visibility. */` |
|    676123 | 1139 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|        13 | 1140 | `		const char *zAvErr = 0;` |
|        19 | 1141 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|        10 | 1142 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|         2 | 1143 | `			: PH7_CLASS_PROT_PUBLIC;` |
|        13 | 1144 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 | 1145 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|        13 | 1146 | `		}else if( iProtection > iSetLevel ){` |
|       ! 0 | 1147 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|       ! 0 | 1148 | `		}` |
|        13 | 1149 | `		if( zAvErr ){` |
|       ! 0 | 1150 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|       ! 0 | 1151 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1152 | `				return SXERR_ABORT;` |
|         - | 1153 | `			}` |
|       ! 0 | 1154 | `			goto Synchronize;` |
|         - | 1155 | `		}` |
|         6 | 1156 | `	}` |
|         - | 1157 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|         - | 1158 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|    676123 | 1159 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        53 | 1160 | `		const char *zRoErr = 0;` |
|        53 | 1161 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|         3 | 1162 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|        52 | 1163 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         6 | 1164 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|        49 | 1165 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|         6 | 1166 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|         2 | 1167 | `		}` |
|        53 | 1168 | `		if( zRoErr ){` |
|        13 | 1169 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|        13 | 1170 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1171 | `				return SXERR_ABORT;` |
|         - | 1172 | `			}` |
|        13 | 1173 | `			goto Synchronize;` |
|         - | 1174 | `		}` |
|        19 | 1175 | `	}` |
|         - | 1176 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|         - | 1177 | `	 * type atom or any union alternative. void/never are already rejected` |
|         - | 1178 | `	 * by the type parser. */` |
|    676113 | 1179 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     27935 | 1180 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|         - | 1181 | `			&sTypeText,` |
|     18620 | 1182 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      9310 | 1183 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     18625 | 1184 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1185 | `			return SXERR_ABORT;` |
|     18625 | 1186 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 | 1187 | `			goto Synchronize;` |
|         - | 1188 | `		}` |
|      9310 | 1189 | `	}` |
|         - | 1190 | `	/* Reject redeclaration (catches clash with an earlier promoted property).` |
|         - | 1191 | `	 * A same-name class CONSTANT is NOT a clash — php's separate namespaces let` |
|         - | 1192 | ``	 * `const C` and `public $C` coexist (stored in disjoint hConst / hAttr tables). */`` |
|    676113 | 1193 | `	if( GenStateExtractProperty(pClass,pName->zString,pName->nByte) != 0 ){` |
|         4 | 1194 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 | 1195 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|         3 | 1196 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1197 | `			return SXERR_ABORT;` |
|         - | 1198 | `		}` |
|         3 | 1199 | `		goto Synchronize;` |
|         - | 1200 | `	}` |
|         - | 1201 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|         - | 1202 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|         - | 1203 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|         - | 1204 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|         - | 1205 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|         - | 1206 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|         - | 1207 | ``	/* php: a property default holding a closure must use `static function` too. */`` |
|    676111 | 1208 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|    444765 | 1209 | `		int iClo = PH7_GenStateInitClosureError(pGen);` |
|    444765 | 1210 | `		if( iClo ){` |
|       ! 0 | 1211 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 1212 | `				iClo == 2 ? "Closures in constant expressions must be static"` |
|         - | 1213 | `				          : "Constant expression contains invalid operations");` |
|       ! 0 | 1214 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1215 | `				return SXERR_ABORT;` |
|         - | 1216 | `			}` |
|       ! 0 | 1217 | `			goto Synchronize;` |
|         - | 1218 | `		}` |
|    222380 | 1219 | `	}` |
|         - | 1220 | `	/* php: a property default may not CALL anything either. */` |
|    676111 | 1221 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && PH7_GenStateInitHasCallExpr(pGen) ){` |
|       ! 0 | 1222 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 1223 | `			"Constant expression contains invalid operations");` |
|       ! 0 | 1224 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1225 | `			return SXERR_ABORT;` |
|         - | 1226 | `		}` |
|       ! 0 | 1227 | `		goto Synchronize;` |
|         - | 1228 | `	}` |
|    676111 | 1229 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|         6 | 1230 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 1231 | `			"New expressions are not supported in this context");` |
|         6 | 1232 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1233 | `			return SXERR_ABORT;` |
|         - | 1234 | `		}` |
|         6 | 1235 | `		goto Synchronize;` |
|         - | 1236 | `	}` |
|         - | 1237 | `	/* Allocate a new class attribute */` |
|    676107 | 1238 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    676107 | 1239 | `	if( pAttr ){` |
|    676107 | 1240 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|    676107 | 1241 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 1242 | `			return SXERR_ABORT;` |
|         - | 1243 | `		}` |
|    338051 | 1244 | `	}` |
|    676107 | 1245 | `	if( pAttr == 0 ){` |
|       ! 0 | 1246 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 1247 | `		return SXERR_ABORT;` |
|         - | 1248 | `	}` |
|    676107 | 1249 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     18623 | 1250 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      9309 | 1251 | `	}` |
|    676107 | 1252 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|         - | 1253 | `		SySet *pInstrContainer;` |
|    444761 | 1254 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|    444761 | 1255 | `		pGen->pIn++; /*Jump the equal sign */` |
|         - | 1256 | `		{` |
|         - | 1257 | `			/* Delimit the default expression: it ends at the declaration's` |
|         - | 1258 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|         - | 1259 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|         - | 1260 | `			 * compiler would otherwise run into the hook tokens. */` |
|    444761 | 1261 | `			SyToken *pScan = pGen->pIn;` |
|    444761 | 1262 | `			sxi32 iNest = 0;` |
|    979391 | 1263 | `			while( pScan < pGen->pEnd ){` |
|    979391 | 1264 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     58957 | 1265 | `					iNest++;` |
|    949915 | 1266 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|     58957 | 1267 | `					iNest--;` |
|    890963 | 1268 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|    444761 | 1269 | `					break;` |
|         - | 1270 | `				}` |
|    534635 | 1271 | `				pScan++;` |
|         5 | 1272 | `			}` |
|    444761 | 1273 | `			pGen->pEnd = pScan;` |
|         - | 1274 | `		}` |
|         - | 1275 | `		/* Swap bytecode container */` |
|    444761 | 1276 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    444761 | 1277 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|         - | 1278 | `		/* Compile attribute value. The default is a const-expression belonging to` |
|         - | 1279 | `		 * pClass (see iInMemberDefault) — __TRAIT__ in it reads pCurClass. */` |
|    444761 | 1280 | `		pGen->iInMemberDefault++;` |
|    444761 | 1281 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    444761 | 1282 | `		pGen->iInMemberDefault--;` |
|    444761 | 1283 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 | 1284 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|       ! 0 | 1285 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1286 | `				return SXERR_ABORT;` |
|         - | 1287 | `			}` |
|       ! 0 | 1288 | `		}` |
|         - | 1289 | `		/* Emit the done instruction */` |
|    444761 | 1290 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    444761 | 1291 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    444761 | 1292 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|    444761 | 1293 | `		pGen->pEnd = pSavedDefEnd;` |
|    222378 | 1294 | `	}` |
|         - | 1295 | `	/* All done,install the attribute */` |
|    676107 | 1296 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|    676107 | 1297 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1298 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1299 | `		return SXERR_ABORT;` |
|         - | 1300 | `	}` |
|    676107 | 1301 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|         - | 1302 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|         - | 1303 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|       147 | 1304 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|       147 | 1305 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1306 | `			return SXERR_ABORT;` |
|         - | 1307 | `		}` |
|       147 | 1308 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1309 | `			goto Synchronize;` |
|         - | 1310 | `		}` |
|       147 | 1311 | `		SySetRelease(&aUnionAlts);` |
|       147 | 1312 | `		return SXRET_OK;` |
|         - | 1313 | `	}` |
|    675965 | 1314 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - | 1315 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|         - | 1316 | `		 * wording differs per declaration site) */` |
|       ! 0 | 1317 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 1318 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|         - | 1319 | `				? "Interfaces may only include hooked properties"` |
|         - | 1320 | `				: "Only hooked properties may be declared abstract");` |
|       ! 0 | 1321 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1322 | `			return SXERR_ABORT;` |
|         - | 1323 | `		}` |
|       ! 0 | 1324 | `		goto Synchronize;` |
|         - | 1325 | `	}` |
|    675965 | 1326 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - | 1327 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|         5 | 1328 | `		pGen->pIn++; /* Jump the comma */` |
|         5 | 1329 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 | 1330 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 | 1331 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 | 1332 | `				pTok--;` |
|       ! 0 | 1333 | `			}` |
|       ! 0 | 1334 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 1335 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|       ! 0 | 1336 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 | 1337 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1338 | `				return SXERR_ABORT;` |
|         - | 1339 | `			}` |
|       ! 0 | 1340 | `		}else{` |
|         5 | 1341 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         5 | 1342 | `				goto loop;` |
|         - | 1343 | `			}` |
|         - | 1344 | `		}` |
|       ! 0 | 1345 | `	}` |
|    675961 | 1346 | `	SySetRelease(&aUnionAlts);` |
|    675961 | 1347 | `	return SXRET_OK;` |
|         9 | 1348 | `Synchronize:` |
|         - | 1349 | `	/* Synchronize with the first semi-colon */` |
|        56 | 1350 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        38 | 1351 | `		pGen->pIn++;` |
|         4 | 1352 | `	}` |
|        22 | 1353 | `	SySetRelease(&aUnionAlts);` |
|        22 | 1354 | `	return SXERR_CORRUPT;` |
|    338063 | 1355 | `}` |
|         - | 1356 | `/*` |
|         - | 1357 | ` * php validates a magic method's DECLARATION at compile time` |
|         - | 1358 | ` * (zend_check_magic_method_implementation): the ENGINE builds the arguments and` |
|         - | 1359 | ` * calls these methods on its own, so a wrong shape is rejected where it is` |
|         - | 1360 | ` * written rather than discovered — or silently tolerated — at the dispatch.` |
|         - | 1361 | ` *` |
|         - | 1362 | ` * One row per magic name; its fields are the checks php makes for that name.` |
|         - | 1363 | ` * Arity is the first of them, in php's order — which is what decides the message` |
|         - | 1364 | ` * when a declaration breaks more than one of php's rules at once.` |
|         - | 1365 | ` */` |
|         - | 1366 | `typedef struct MagicMethodRule MagicMethodRule;` |
|         - | 1367 | `struct MagicMethodRule` |
|         - | 1368 | `{` |
|         - | 1369 | `	const char *zName; /* Magic method name */` |
|         - | 1370 | `	sxu32 nName;       /* Its length */` |
|         - | 1371 | `	int nArgs;         /* Declared arguments php requires, -1 when it does not check */` |
|         - | 1372 | `	int bStatic;       /* TRUE: must be static · FALSE: must NOT be static */` |
|         - | 1373 | `	int bPublic;       /* TRUE: must be public — a WARNING, and dispatched anyway */` |
|         - | 1374 | `	int bNoReturnType; /* TRUE: declaring ANY return type is a fatal */` |
|         - | 1375 | `};` |
|         - | 1376 | `#define MAGIC_METHOD_ROW(N,A,S,P,R) { N, sizeof(N)-1, A, S, P, R }` |
|         - | 1377 | `static const MagicMethodRule aMagicMethod[] = {` |
|         - | 1378 | `	MAGIC_METHOD_ROW("__construct",  -1, FALSE, FALSE, TRUE),` |
|         - | 1379 | `	MAGIC_METHOD_ROW("__destruct",    0, FALSE, FALSE, TRUE),` |
|         - | 1380 | `	MAGIC_METHOD_ROW("__clone",       0, FALSE, FALSE, FALSE),` |
|         - | 1381 | `	MAGIC_METHOD_ROW("__get",         1, FALSE, TRUE,  FALSE),` |
|         - | 1382 | `	MAGIC_METHOD_ROW("__set",         2, FALSE, TRUE,  FALSE),` |
|         - | 1383 | `	MAGIC_METHOD_ROW("__isset",       1, FALSE, TRUE,  FALSE),` |
|         - | 1384 | `	MAGIC_METHOD_ROW("__unset",       1, FALSE, TRUE,  FALSE),` |
|         - | 1385 | `	MAGIC_METHOD_ROW("__call",        2, FALSE, TRUE,  FALSE),` |
|         - | 1386 | `	MAGIC_METHOD_ROW("__callStatic",  2, TRUE,  TRUE,  FALSE),` |
|         - | 1387 | `	MAGIC_METHOD_ROW("__toString",    0, FALSE, TRUE,  FALSE),` |
|         - | 1388 | `	MAGIC_METHOD_ROW("__invoke",     -1, FALSE, TRUE,  FALSE),` |
|         - | 1389 | `	MAGIC_METHOD_ROW("__debugInfo",   0, FALSE, TRUE,  FALSE),` |
|         - | 1390 | `	MAGIC_METHOD_ROW("__serialize",   0, FALSE, TRUE,  FALSE),` |
|         - | 1391 | `	MAGIC_METHOD_ROW("__unserialize", 1, FALSE, TRUE,  FALSE),` |
|         - | 1392 | `	MAGIC_METHOD_ROW("__sleep",       0, FALSE, TRUE,  FALSE),` |
|         - | 1393 | `	MAGIC_METHOD_ROW("__wakeup",      0, FALSE, TRUE,  FALSE),` |
|         - | 1394 | `	MAGIC_METHOD_ROW("__set_state",   1, TRUE,  TRUE,  FALSE)` |
|         - | 1395 | `};` |
|         - | 1396 | `#undef MAGIC_METHOD_ROW` |
|         - | 1397 | `/*` |
|         - | 1398 | ` * Find the rule for a declared method name, or 0 when the name is not magic.` |
|         - | 1399 | `` * php matches method names case-insensitively everywhere, so `__GET` is `__get`;`` |
|         - | 1400 | ` * the two-underscore prefix test is php's own cheap reject.` |
|         - | 1401 | ` */` |
|   3381428 | 1402 | `static const MagicMethodRule * GenStateMagicMethodRule(const SyString *pName)` |
|         5 | 1403 | `{` |
|         - | 1404 | `	sxu32 n;` |
|   3381433 | 1405 | `	if( pName->nByte < sizeof("__x")-1 \|\| pName->zString[0] != '_' \|\| pName->zString[1] != '_' ){` |
|   2714197 | 1406 | `		return 0;` |
|         - | 1407 | `	}` |
|   5726311 | 1408 | `	for( n = 0 ; n < SX_ARRAYSIZE(aMagicMethod) ; ++n ){` |
|   5540650 | 1409 | `		if( pName->nByte == aMagicMethod[n].nName` |
|   3226365 | 1410 | `		 && SyStrnicmp(pName->zString,aMagicMethod[n].zName,aMagicMethod[n].nName) == 0 ){` |
|    481585 | 1411 | `			return &aMagicMethod[n];` |
|         - | 1412 | `		}` |
|   2529540 | 1413 | `	}` |
|    185661 | 1414 | `	return 0;` |
|   1690719 | 1415 | `}` |
|         - | 1416 | `/*` |
|         - | 1417 | ` * TRUE when php requires this magic method to be PUBLIC: the rows it merely` |
|         - | 1418 | ` * WARNS about at the declaration and then dispatches regardless of what the` |
|         - | 1419 | ` * declaration said. The runtime's visibility gate reads this to let an` |
|         - | 1420 | ` * engine-built call through — a call the user WROTE stays denied.` |
|         - | 1421 | ` *` |
|         - | 1422 | `` * `__construct`/`__destruct`/`__clone` are deliberately not in the set: a`` |
|         - | 1423 | ` * private constructor is the singleton idiom, and php enforces those three at` |
|         - | 1424 | ` * the call like any other method.` |
|         - | 1425 | ` */` |
|    100624 | 1426 | `PH7_PRIVATE int PH7_MagicMethodMustBePublic(const SyString *pName)` |
|         5 | 1427 | `{` |
|    100629 | 1428 | `	const MagicMethodRule *pRule = GenStateMagicMethodRule(pName);` |
|    100629 | 1429 | `	return pRule != 0 && pRule->bPublic;` |
|         5 | 1430 | `}` |
|         - | 1431 | `/*` |
|         - | 1432 | ` * Enforce the rules of pRule against the declaration just parsed. pName is the` |
|         - | 1433 | ` * name AS WRITTEN — php quotes that spelling, not the canonical one.` |
|         - | 1434 | ` *` |
|         - | 1435 | ` * The diagnostic is FORMATTED, not reported: php decides these rules while the` |
|         - | 1436 | ` * signature is in hand (so the arity beats __toString's return-type rule) but` |
|         - | 1437 | ` * raises them only once the declaration has cleared the checks php makes` |
|         - | 1438 | ` * first — the redeclaration and abstract-placement rules, and any parse error` |
|         - | 1439 | ` * in the body php has already read. The caller reports the buffer at that` |
|         - | 1440 | ` * point.` |
|         - | 1441 | ` *` |
|         - | 1442 | ` * Returns the severity it wrote: E_ERROR, E_WARNING, or 0 for a clean` |
|         - | 1443 | ` * declaration.` |
|         - | 1444 | ` */` |
|   3280804 | 1445 | `static sxi32 GenStateCheckMagicMethod(` |
|         - | 1446 | `	ph7_class *pClass,` |
|         - | 1447 | `	const SyString *pName,` |
|         - | 1448 | `	ph7_class_method *pMeth,` |
|         - | 1449 | `	char *zErr,` |
|         - | 1450 | `	int nErrBuf` |
|         - | 1451 | `	)` |
|         5 | 1452 | `{` |
|   3280809 | 1453 | `	const MagicMethodRule *pRule = GenStateMagicMethodRule(pName);` |
|   3280809 | 1454 | `	if( pRule == 0 ){` |
|   2899853 | 1455 | `		return 0;` |
|         - | 1456 | `	}` |
|    380961 | 1457 | `	if( pRule->nArgs >= 0 ){` |
|         - | 1458 | `		/* php counts DECLARED parameters — an optional one counts` |
|         - | 1459 | ``		 * (`__destruct($a = null)` is rejected) and the variadic tail does not`` |
|         - | 1460 | ``		 * (`__clone(...$a)` declares zero and passes, `__get(...$a)` declares`` |
|         - | 1461 | `		 * zero where one is required and does not). */` |
|    145187 | 1462 | `		sxu32 nDecl = SySetUsed(&pMeth->sFunc.aArgs);` |
|    145187 | 1463 | `		sxu32 nGiven = 0;` |
|         - | 1464 | `		sxu32 n;` |
|    204215 | 1465 | `		for( n = 0 ; n < nDecl ; ++n ){` |
|     59033 | 1466 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,n);` |
|     59033 | 1467 | `			if( pArg && (pArg->iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|     59031 | 1468 | `				nGiven++;` |
|     29513 | 1469 | `			}` |
|     29519 | 1470 | `		}` |
|    145187 | 1471 | `		if( nGiven != (sxu32)pRule->nArgs ){` |
|         9 | 1472 | `			if( pRule->nArgs == 0 ){` |
|         4 | 1473 | `				SyBufferFormat(zErr,nErrBuf,"Method %z::%z() cannot take arguments",` |
|         1 | 1474 | `					&pClass->sName,pName);` |
|         2 | 1475 | `			}else{` |
|         8 | 1476 | `				SyBufferFormat(zErr,nErrBuf,"Method %z::%z() must take exactly %d argument%s",` |
|         6 | 1477 | `					&pClass->sName,pName,pRule->nArgs,pRule->nArgs == 1 ? "" : "s");` |
|         - | 1478 | `			}` |
|         9 | 1479 | `			return E_ERROR;` |
|         - | 1480 | `		}` |
|         - | 1481 | `		/* None of the arguments the engine builds may be by-reference — there is` |
|         - | 1482 | `		 * no caller variable to write back to. php checks as many arguments as` |
|         - | 1483 | `		 * the rule counts, and the count above already skipped variadics, so` |
|         - | 1484 | `		 * this walk skips them the same way. */` |
|    204199 | 1485 | `		for( n = 0 ; n < nDecl ; ++n ){` |
|     59025 | 1486 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,n);` |
|     59025 | 1487 | `			if( pArg == 0 \|\| (pArg->iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|         3 | 1488 | `				continue;` |
|         - | 1489 | `			}` |
|     59023 | 1490 | `			if( pArg->iFlags & VM_FUNC_ARG_BY_REF ){` |
|         4 | 1491 | `				SyBufferFormat(zErr,nErrBuf,"Method %z::%z() cannot take arguments by reference",` |
|         1 | 1492 | `					&pClass->sName,pName);` |
|         3 | 1493 | `				return E_ERROR;` |
|         - | 1494 | `			}` |
|     29513 | 1495 | `		}` |
|     72587 | 1496 | `	}` |
|         - | 1497 | `	/* Static-ness. Whether the engine has a receiver for a magic method is not` |
|         - | 1498 | ``	 * the declaration's to choose: `__callStatic` and `__set_state` are reached`` |
|         - | 1499 | `	 * with a class and nothing else, every other row is reached through an` |
|         - | 1500 | `	 * object. PHL took the declaration at its word and then dispatched the` |
|         - | 1501 | ``	 * method anyway — a `static function __get()` ran with no `$this` at all,`` |
|         - | 1502 | `	 * so a hook property table or a lazy-loading accessor read whatever the` |
|         - | 1503 | `	 * unbound scope happened to hold. php checks this after the arity, which is` |
|         - | 1504 | ``	 * why `static function __get($a,$b)` reports the count first. */`` |
|    380953 | 1505 | `	if( pRule->bStatic != ((pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0) ){` |
|         8 | 1506 | `		SyBufferFormat(zErr,nErrBuf,"Method %z::%z() %s be static",` |
|         4 | 1507 | `			&pClass->sName,pName,pRule->bStatic ? "must" : "cannot");` |
|         6 | 1508 | `		return E_ERROR;` |
|         - | 1509 | `	}` |
|         - | 1510 | `	/* Visibility. This one is a WARNING: php names the declaration and then` |
|         - | 1511 | ``	 * dispatches the method anyway, because the engine calling `__get` is not`` |
|         - | 1512 | `	 * the outside world reaching for a private member. PHL was silent at the` |
|         - | 1513 | ``	 * declaration and threw `Call to private method C::__get()` at the ACCESS —`` |
|         - | 1514 | `	 * the one rule of this family that changed what a program php RUNS does,` |
|         - | 1515 | `	 * and it killed the script. The dispatch half is` |
|         - | 1516 | `	 * PH7_MagicMethodMustBePublic, read by the runtime visibility gate. */` |
|    380949 | 1517 | `	if( pRule->bPublic && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        51 | 1518 | `		SyBufferFormat(zErr,nErrBuf,"The magic method %z::%z() must have public visibility",` |
|        16 | 1519 | `			&pClass->sName,pName);` |
|        35 | 1520 | `		return E_WARNING;` |
|         - | 1521 | `	}` |
|         - | 1522 | ``	/* A return type on the two methods that have no return VALUE. `new C` is the`` |
|         - | 1523 | `	 * instance, never whatever __construct returned, and __destruct is called by` |
|         - | 1524 | `	 * the engine at a point with nowhere to put an answer — so php rejects any` |
|         - | 1525 | ``	 * declared type on either, `void` and `never` included, rather than let a`` |
|         - | 1526 | ``	 * declaration promise something no caller can read. (`__clone` is NOT in`` |
|         - | 1527 | ``	 * this row: `: void` on it is valid php.) PHL enforced the declared type at`` |
|         - | 1528 | ``	 * runtime instead, so `__construct(): int` raised a TypeError at every`` |
|         - | 1529 | `	 * instantiation — a diagnostic on the CALL for a mistake in the` |
|         - | 1530 | `	 * declaration. */` |
|    380912 | 1531 | `	if( pRule->bNoReturnType` |
|    439769 | 1532 | `	 && (pMeth->sFunc.nReturnType > 0` |
|    249308 | 1533 | `	  \|\| SyStringLength(&pMeth->sFunc.sReturnClass) > 0` |
|    249306 | 1534 | `	  \|\| SySetUsed(&pMeth->sFunc.aReturnUnion) > 0) ){` |
|         8 | 1535 | `		SyBufferFormat(zErr,nErrBuf,"Method %z::%z() cannot declare a return type",` |
|         2 | 1536 | `			&pClass->sName,pName);` |
|         6 | 1537 | `		return E_ERROR;` |
|         - | 1538 | `	}` |
|    380913 | 1539 | `	return 0;` |
|   1640407 | 1540 | `}` |
|         - | 1541 | `/*` |
|         - | 1542 | ` * Raise the diagnostic GenStateCheckMagicMethod parked, once, and disarm it.` |
|         - | 1543 | ` * Suppressed when this declaration has already reported a fatal php decides` |
|         - | 1544 | ` * FIRST — a redeclaration, an abstract method in a non-abstract class, a parse` |
|         - | 1545 | ` * error in the body — since php stops at its own first fatal.` |
|         - | 1546 | ` */` |
|   3280788 | 1547 | `static sxi32 GenStateRaiseMagicDiag(` |
|         - | 1548 | `	ph7_gen_state *pGen,` |
|         - | 1549 | `	sxi32 *pnSeverity,   /* IN/OUT: the parked severity, zeroed here */` |
|         - | 1550 | `	const char *zErr,` |
|         - | 1551 | `	sxu32 nLine,` |
|         - | 1552 | `	sxu32 nErrEntry      /* pGen->nErr when this declaration started */` |
|         - | 1553 | `	)` |
|         5 | 1554 | `{` |
|   3280793 | 1555 | `	sxi32 rc = SXRET_OK;` |
|   3280793 | 1556 | `	if( *pnSeverity != 0 ){` |
|        52 | 1557 | `		if( pGen->nErr == nErrEntry ){` |
|        52 | 1558 | `			rc = PH7_GenCompileError(pGen,*pnSeverity,nLine,"%s",zErr);` |
|        24 | 1559 | `		}` |
|        52 | 1560 | `		*pnSeverity = 0;` |
|        24 | 1561 | `	}` |
|   3280793 | 1562 | `	return rc;` |
|         5 | 1563 | `}` |
|         - | 1564 | `/*` |
|         - | 1565 | ` * Compile a class method.` |
|         - | 1566 | ` *` |
|         - | 1567 | ` * Refer to the official documentation for more information` |
|         - | 1568 | ` * on the powerful extension introduced by the PH7 engine` |
|         - | 1569 | ` * to the OO subsystem such as full type hinting,method` |
|         - | 1570 | ` * overloading and many more.` |
|         - | 1571 | ` */` |
|   3280806 | 1572 | `static sxi32 GenStateCompileClassMethod(` |
|         - | 1573 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 1574 | `	sxi32 iProtection,   /* Visibility level */` |
|         - | 1575 | `	sxi32 iFlags,        /* Configuration flags */` |
|         - | 1576 | `	int doBody,          /* TRUE to process method body */` |
|         - | 1577 | `	ph7_class *pClass    /* Class this method belongs */` |
|         - | 1578 | `	)` |
|         5 | 1579 | `{` |
|   3280811 | 1580 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   3280811 | 1581 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|   3280811 | 1582 | `	sxu32 nErrEntry = pGen->nErr; /* Errors already reported when this declaration started */` |
|         - | 1583 | `	char zMagicErr[256];          /* Pending magic-method rule violation, reported at the end */` |
|   3280811 | 1584 | `	sxi32 nMagicSeverity = 0;     /* E_ERROR / E_WARNING while zMagicErr is still unreported */` |
|   3280811 | 1585 | `	int bMagicFatal = FALSE;      /* The parked diagnostic was a fatal: do not install the method */` |
|         - | 1586 | `	ph7_class_method *pMeth;` |
|         - | 1587 | `	sxi32 iFuncFlags;` |
|         - | 1588 | `	SyString *pName;` |
|         - | 1589 | `	SyToken *pEnd;` |
|         - | 1590 | `	sxi32 rc;` |
|         - | 1591 | `	/* Extract visibility level */` |
|   3280811 | 1592 | `	iProtection = GetProtectionLevel(iProtection);` |
|   3280811 | 1593 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   3280811 | 1594 | `	iFuncFlags = 0;` |
|   3280811 | 1595 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - | 1596 | `		/* Invalid method name */` |
|       ! 0 | 1597 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|       ! 0 | 1598 | `		if( rc == SXERR_ABORT ){` |
|         - | 1599 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 1600 | `			return SXERR_ABORT;` |
|         - | 1601 | `		}` |
|       ! 0 | 1602 | `		goto Synchronize;` |
|         - | 1603 | `	}` |
|   3280811 | 1604 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - | 1605 | `		/* Return by reference,remember that */` |
|         3 | 1606 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|         - | 1607 | `		/* Jump the '&' token */` |
|         3 | 1608 | `		pGen->pIn++;` |
|         1 | 1609 | `	}` |
|   3280811 | 1610 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - | 1611 | `		/* Invalid method name */` |
|       ! 0 | 1612 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|       ! 0 | 1613 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1614 | `			return SXERR_ABORT;` |
|         - | 1615 | `		}` |
|       ! 0 | 1616 | `		goto Synchronize;` |
|         - | 1617 | `	}` |
|         - | 1618 | `	/* Peek method name */` |
|   3280811 | 1619 | `	pName = &pGen->pIn->sData;` |
|   3280811 | 1620 | `	nLine = pGen->pIn->nLine;` |
|         - | 1621 | `	/* Jump the method name */` |
|   3280811 | 1622 | `	pGen->pIn++;` |
|   3280811 | 1623 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - | 1624 | `		/* Abstract method */` |
|    163107 | 1625 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       ! 0 | 1626 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 1627 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|       ! 0 | 1628 | `				&pClass->sName,pName);` |
|       ! 0 | 1629 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1630 | `				return SXERR_ABORT;` |
|         - | 1631 | `			}` |
|       ! 0 | 1632 | `		}` |
|         - | 1633 | `		/* Assemble method signature only */` |
|    163107 | 1634 | `		doBody = FALSE;` |
|     81551 | 1635 | `	}` |
|   3280811 | 1636 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 1637 | `		/* Syntax error */` |
|       ! 0 | 1638 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|       ! 0 | 1639 | `		if( rc == SXERR_ABORT ){` |
|         - | 1640 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 1641 | `			return SXERR_ABORT;` |
|         - | 1642 | `		}` |
|       ! 0 | 1643 | `		goto Synchronize;` |
|         - | 1644 | `	}` |
|         - | 1645 | `	/* Allocate a new class_method instance */` |
|   3280811 | 1646 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   3280811 | 1647 | `	if( pMeth == 0 ){` |
|       ! 0 | 1648 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1649 | `		return SXERR_ABORT;` |
|         - | 1650 | `	}` |
|   3280811 | 1651 | `	pMeth->sFunc.nLine = nKwLine;` |
|   3280811 | 1652 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|   3280811 | 1653 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 1654 | `		return SXERR_ABORT;` |
|         - | 1655 | `	}` |
|         - | 1656 | `	/* Jump the left parenthesis '(' */` |
|   3280811 | 1657 | `	pGen->pIn++;` |
|   3280811 | 1658 | `	pEnd = 0; /* cc warning */` |
|         - | 1659 | `	/* Delimit the method signature */` |
|   3280811 | 1660 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   3280811 | 1661 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 1662 | `		/* Syntax error */` |
|         3 | 1663 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|         3 | 1664 | `		if( rc == SXERR_ABORT ){` |
|         - | 1665 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 1666 | `			return SXERR_ABORT;` |
|         - | 1667 | `		}` |
|         3 | 1668 | `		goto Synchronize;` |
|         - | 1669 | `	}` |
|         - | 1670 | `	{` |
|   3280809 | 1671 | `		int bIsCtor = 0;` |
|   3280809 | 1672 | `		int bAbstractCtor = 0;` |
|         - | 1673 | `		/* Only __construct is the constructor (PHP-4 class-name constructors removed` |
|         - | 1674 | `		 * in 8.0): a method named like the class is a plain method, so promoted` |
|         - | 1675 | `		 * properties in it are rejected exactly as php does elsewhere. */` |
|   3280804 | 1676 | `		if( pName->nByte == sizeof("__construct") - 1` |
|   1930392 | 1677 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0 ){` |
|    235715 | 1678 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         3 | 1679 | `				bAbstractCtor = 1;` |
|         2 | 1680 | `			}else{` |
|    235713 | 1681 | `				bIsCtor = 1;` |
|         - | 1682 | `			}` |
|    117855 | 1683 | `		}` |
|   3280809 | 1684 | `		if( pGen->pIn < pEnd ){` |
|         - | 1685 | `			/* Collect method arguments */` |
|   1264241 | 1686 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   1264241 | 1687 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1688 | `				return SXERR_ABORT;` |
|         - | 1689 | `			}` |
|    632118 | 1690 | `		}` |
|         - | 1691 | `	}` |
|         - | 1692 | `	/* Point past ')' and parse optional return type ': type' */` |
|   3280809 | 1693 | `	pGen->pIn = &pEnd[1];` |
|         - | 1694 | `	{` |
|   3280809 | 1695 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|   3280809 | 1696 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 | 1697 | `			return SXERR_ABORT;` |
|   3280809 | 1698 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       ! 0 | 1699 | `			goto Synchronize;` |
|         - | 1700 | `		}` |
|         - | 1701 | `	}` |
|         - | 1702 | `	/* php's compile-time magic-method declaration rules, DECIDED here — with the` |
|         - | 1703 | `	 * signature in hand and before the __toString return-type rule below, which` |
|         - | 1704 | ``	 * is php's own order (`static function __toString($a): int` reports the`` |
|         - | 1705 | `	 * arity). Reported at the end of this function; see zMagicErr there. */` |
|   3280809 | 1706 | `	nMagicSeverity = GenStateCheckMagicMethod(pClass,pName,pMeth,zMagicErr,(int)sizeof(zMagicErr));` |
|   3280809 | 1707 | `	if( nMagicSeverity == E_ERROR ){` |
|         - | 1708 | `		/* Suppress the __toString rule below: php never reaches it on a` |
|         - | 1709 | `		 * declaration the magic rules already rejected. */` |
|        20 | 1710 | `		bMagicFatal = TRUE;` |
|        20 | 1711 | `		goto SkipToStringType;` |
|         - | 1712 | `	}` |
|         - | 1713 | `	/*` |
|         - | 1714 | ``	 * php gives __toString() an IMPLICIT `string` return type. That is what makes`` |
|         - | 1715 | ``	 * `return 42` coerce to "42" and `return null` / an array / an object / falling`` |
|         - | 1716 | `	 * off the end raise` |
|         - | 1717 | `	 *   C::__toString(): Return value must be of type string, X returned` |
|         - | 1718 | `	 * PHL enforced DECLARED return types only, so an undeclared __toString could` |
|         - | 1719 | `	 * answer anything and MemObjStringValue fell back to the "Object" placeholder` |
|         - | 1720 | `	 * for whatever was not a non-empty string. Installing the type here reuses the` |
|         - | 1721 | `	 * enforcement that already matches php byte for byte.` |
|         - | 1722 | `	 *` |
|         - | 1723 | `	 * sReturnTypeName is filled in as well, for two reasons: reflection reports the` |
|         - | 1724 | `	 * implicit type exactly as php does (hasReturnType() TRUE, getReturnType()` |
|         - | 1725 | `	 * "string" for an undeclared __toString), and the generator-return-type fatal` |
|         - | 1726 | ``	 * renders from it — so a __toString with a `yield` in it now reports php's`` |
|         - | 1727 | `	 * "Generator return type must be a supertype of Generator, string given".` |
|         - | 1728 | `	 *` |
|         - | 1729 | ``	 * Declaring any OTHER return type is php's own compile fatal, `?string`, a`` |
|         - | 1730 | ``	 * union, `mixed`, `static` and `void` included. (php checks`` |
|         - | 1731 | ``	 * "A void method must not return a value" FIRST when a `: void` __toString also`` |
|         - | 1732 | `	 * returns a value; PHL has no such check yet, so it reports this one instead —` |
|         - | 1733 | `	 * both reject, on doubly-invalid input only.)` |
|         - | 1734 | `	 */` |
|   3280788 | 1735 | `	if( pName->nByte == sizeof("__toString")-1` |
|   1826187 | 1736 | `	 && SyStrnicmp(pName->zString,"__toString",sizeof("__toString")-1) == 0 ){` |
|     77091 | 1737 | `		ph7_vm_func *pTsFunc = &pMeth->sFunc;` |
|     77091 | 1738 | `		int bTsDeclared = pTsFunc->nReturnType > 0 \|\| SySetUsed(&pTsFunc->aReturnUnion) > 0;` |
|     77091 | 1739 | `		if( bTsDeclared ){` |
|      4600 | 1740 | `			if( pTsFunc->nReturnType != MEMOBJ_STRING` |
|      4599 | 1741 | `			 \|\| SySetUsed(&pTsFunc->aReturnUnion) > 0` |
|      4603 | 1742 | `			 \|\| (pTsFunc->iFlags & VM_FUNC_RETURN_NULLABLE) ){` |
|         - | 1743 | `				/* php raises this one AFTER the magic rules, so a parked` |
|         - | 1744 | `				 * visibility warning is php's first line here rather than a` |
|         - | 1745 | `				 * casualty of the fatal about to be counted. */` |
|         6 | 1746 | `				if( GenStateRaiseMagicDiag(pGen,&nMagicSeverity,zMagicErr,` |
|         6 | 1747 | `						nKwLine,nErrEntry) == SXERR_ABORT ){` |
|       ! 0 | 1748 | `					return SXERR_ABORT;` |
|         - | 1749 | `				}` |
|         8 | 1750 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 1751 | `					"%z::%z(): Return type must be string when declared",` |
|         2 | 1752 | `					&pClass->sName,pName);` |
|         6 | 1753 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 1754 | `					return SXERR_ABORT;` |
|         - | 1755 | `				}` |
|         6 | 1756 | `				goto Synchronize;` |
|         - | 1757 | `			}` |
|      2303 | 1758 | `		}else{` |
|     72491 | 1759 | `			char *zTsType = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         - | 1760 | `				"string",sizeof("string")-1);` |
|     72491 | 1761 | `			pTsFunc->nReturnType = MEMOBJ_STRING;` |
|     72491 | 1762 | `			if( zTsType ){` |
|     72491 | 1763 | `				SyStringInitFromBuf(&pTsFunc->sReturnTypeName,zTsType,sizeof("string")-1);` |
|     36243 | 1764 | `			}` |
|         - | 1765 | `		}` |
|     38541 | 1766 | `	}` |
|       ! 0 | 1767 | `SkipToStringType:` |
|         - | 1768 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|         - | 1769 | `	 * property init/typecheck is handled by the generic typed-property path` |
|         - | 1770 | `	 * since we mint real ph7_class_attr entries. */` |
|         - | 1771 | `	{` |
|   3280805 | 1772 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|         - | 1773 | `		sxu32 i;` |
|   5174667 | 1774 | `		for( i = 0; i < nArg; i++ ){` |
|   1893877 | 1775 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|         - | 1776 | `			ph7_class_attr *pAttr;` |
|   1893877 | 1777 | `			sxi32 iAttrFlags = 0;` |
|         - | 1778 | `			int bArgTyped;` |
|   1893877 | 1779 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1893783 | 1780 | `				continue;` |
|         - | 1781 | `			}` |
|         - | 1782 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|         - | 1783 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|         - | 1784 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|        64 | 1785 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|       100 | 1786 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|        99 | 1787 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|         3 | 1788 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 1789 | `					"Cannot declare variadic promoted property");` |
|         3 | 1790 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 1791 | `					return SXERR_ABORT;` |
|         - | 1792 | `				}` |
|         3 | 1793 | `				goto Synchronize;` |
|         - | 1794 | `			}` |
|         - | 1795 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|         - | 1796 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|         - | 1797 | `			 * appear as an alternative of a union type. */` |
|        97 | 1798 | `			if( bArgTyped ){` |
|       137 | 1799 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|        88 | 1800 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|        88 | 1801 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|        44 | 1802 | `					"Property %z::$%z cannot have type %z",nLine);` |
|        93 | 1803 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 1804 | `					return SXERR_ABORT;` |
|        93 | 1805 | `				}else if( rc != SXRET_OK ){` |
|         6 | 1806 | `					goto Synchronize;` |
|         - | 1807 | `				}` |
|        42 | 1808 | `			}` |
|         - | 1809 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|        93 | 1810 | `			if( GenStateExtractProperty(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|         4 | 1811 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 | 1812 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|         3 | 1813 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 1814 | `					return SXERR_ABORT;` |
|         - | 1815 | `				}` |
|         3 | 1816 | `				goto Synchronize;` |
|         - | 1817 | `			}` |
|        91 | 1818 | `			if( bArgTyped ){` |
|        87 | 1819 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        41 | 1820 | `			}` |
|        91 | 1821 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|         3 | 1822 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|         1 | 1823 | `			}` |
|        91 | 1824 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|         8 | 1825 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|         3 | 1826 | `			}` |
|        91 | 1827 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|         - | 1828 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|         - | 1829 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|        26 | 1830 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         4 | 1831 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 | 1832 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|         3 | 1833 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 1834 | `						return SXERR_ABORT;` |
|         - | 1835 | `					}` |
|         3 | 1836 | `					goto Synchronize;` |
|         - | 1837 | `				}` |
|        24 | 1838 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        10 | 1839 | `			}` |
|        89 | 1840 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|         - | 1841 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|         5 | 1842 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 | 1843 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 1844 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|       ! 0 | 1845 | `						&pClass->sName,&pArg->sName);` |
|       ! 0 | 1846 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 1847 | `						return SXERR_ABORT;` |
|         - | 1848 | `					}` |
|       ! 0 | 1849 | `					goto Synchronize;` |
|         - | 1850 | `				}` |
|         5 | 1851 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|         2 | 1852 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|         2 | 1853 | `			}` |
|        89 | 1854 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|        89 | 1855 | `			if( pAttr == 0 ){` |
|       ! 0 | 1856 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1857 | `				return SXERR_ABORT;` |
|         - | 1858 | `			}` |
|        89 | 1859 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|        87 | 1860 | `				pAttr->nType = pArg->nType;` |
|        87 | 1861 | `				pAttr->sClass = pArg->sClass;` |
|        87 | 1862 | `				pAttr->sTypeName = pArg->sTypeName;` |
|        87 | 1863 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|         - | 1864 | `					sxu32 k;` |
|        20 | 1865 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|        14 | 1866 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|        14 | 1867 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|         8 | 1868 | `					}` |
|         3 | 1869 | `				}` |
|        41 | 1870 | `			}` |
|        89 | 1871 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|        89 | 1872 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1873 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1874 | `				return SXERR_ABORT;` |
|         - | 1875 | `			}` |
|        47 | 1876 | `		}` |
|         - | 1877 | `	}` |
|   3280795 | 1878 | `	if( doBody ){` |
|         - | 1879 | `		/* Compile method body */` |
|   3117693 | 1880 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   3117693 | 1881 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1882 | `			return SXERR_ABORT;` |
|         - | 1883 | `		}` |
|         - | 1884 | `		/* The cursor sits just past the body's closing brace */` |
|   3117693 | 1885 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   1558849 | 1886 | `	}else{` |
|         - | 1887 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|    163107 | 1888 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|    163107 | 1889 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|     81551 | 1890 | `		}` |
|         - | 1891 | `		/* Only method signature is allowed */` |
|    163107 | 1892 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|       ! 0 | 1893 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 1894 | `				"Expected ';' after method signature '%z'",pName);` |
|       ! 0 | 1895 | `				if( rc == SXERR_ABORT ){` |
|         - | 1896 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 | 1897 | `					return SXERR_ABORT;` |
|         - | 1898 | `				}` |
|       ! 0 | 1899 | `				return SXERR_CORRUPT;` |
|         - | 1900 | `			}` |
|         - | 1901 | `	}` |
|         - | 1902 | `	/* php: an abstract method in a class that was not DECLARED abstract is a fatal,` |
|         - | 1903 | `	 * and it names the method -- distinct from the "contains N abstract methods"` |
|         - | 1904 | `	 * wording php uses for an unimplemented INHERITED one, which` |
|         - | 1905 | `	 * GenStateCheckAbstractMethods already handles. Interfaces and traits may carry` |
|         - | 1906 | `	 * abstract methods freely. */` |
|   3280790 | 1907 | `	if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT)` |
|   1721951 | 1908 | `	 && (pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|         4 | 1909 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 1910 | `			"Class %z declares abstract method %z() and must therefore be declared abstract",` |
|         1 | 1911 | `			&pClass->sName,pName);` |
|         3 | 1912 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1913 | `			return SXERR_ABORT;` |
|         - | 1914 | `		}` |
|         3 | 1915 | `		return SXRET_OK;` |
|         - | 1916 | `	}` |
|         - | 1917 | `	/* php: two methods of the same name in one class body is a fatal, and the` |
|         - | 1918 | ``	 * comparison is case-INSENSITIVE (hMethod now matches that way), so `f` and`` |
|         - | 1919 | ``	 * `F` collide. php names the offending declaration with the spelling used at`` |
|         - | 1920 | `	 * the SECOND site. */` |
|   3280793 | 1921 | `	if( PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte) != 0 ){` |
|         8 | 1922 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         2 | 1923 | `			"Cannot redeclare %z::%z()",&pClass->sName,pName);` |
|         6 | 1924 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1925 | `			return SXERR_ABORT;` |
|         - | 1926 | `		}` |
|         6 | 1927 | `		return SXRET_OK;` |
|         - | 1928 | `	}` |
|         - | 1929 | `	/* The magic-method rule this declaration broke (decided above, with the` |
|         - | 1930 | `	 * signature in hand). It is raised HERE because php raises it last of the` |
|         - | 1931 | `	 * declaration's fatals: a redeclaration, an abstract method in a class that` |
|         - | 1932 | `	 * is not abstract, and any parse error inside the body php has already read` |
|         - | 1933 | `	 * all win — and each of them has, by now, either returned or bumped nErr.` |
|         - | 1934 | `	 * php stops at its first fatal, so one is all this declaration reports. The` |
|         - | 1935 | ``	 * line is the `function` KEYWORD's, which is where php points once a`` |
|         - | 1936 | `	 * signature wraps across lines. */` |
|   3280789 | 1937 | `	if( GenStateRaiseMagicDiag(pGen,&nMagicSeverity,zMagicErr,nKwLine,nErrEntry) == SXERR_ABORT ){` |
|       ! 0 | 1938 | `		return SXERR_ABORT;` |
|         - | 1939 | `	}` |
|   3280789 | 1940 | `	if( bMagicFatal ){` |
|         - | 1941 | `		/* Never install a method php refused to declare. A WARNING falls through:` |
|         - | 1942 | `		 * php keeps the method and calls it. */` |
|        20 | 1943 | `		return SXRET_OK;` |
|         - | 1944 | `	}` |
|         - | 1945 | `	/* All done,install the method */` |
|   3280773 | 1946 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   3280773 | 1947 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1948 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1949 | `		return SXERR_ABORT;` |
|         - | 1950 | `	}` |
|   3280773 | 1951 | `	return SXRET_OK;` |
|         8 | 1952 | `Synchronize:` |
|         - | 1953 | `	/* Synchronize with the first semi-colon */` |
|        56 | 1954 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        40 | 1955 | `		pGen->pIn++;` |
|         4 | 1956 | `	}` |
|        20 | 1957 | `	return SXERR_CORRUPT;` |
|   1640408 | 1958 | `}` |
|         - | 1959 | `/*` |
|         - | 1960 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|         - | 1961 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|         - | 1962 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|         - | 1963 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|         - | 1964 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|         - | 1965 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|         - | 1966 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|         - | 1967 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|         - | 1968 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|         - | 1969 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|         - | 1970 | `` * implicit `$value` formal.`` |
|         - | 1971 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|         - | 1972 | ` */` |
|         - | 1973 | `/*` |
|         - | 1974 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|         - | 1975 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|         - | 1976 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|         - | 1977 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|         - | 1978 | ` * allowed, excluded from the raw object surfaces.` |
|         - | 1979 | ` */` |
|       142 | 1980 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|         5 | 1981 | `{` |
|         - | 1982 | `	SyToken *p;` |
|       683 | 1983 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|       597 | 1984 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       485 | 1985 | `			continue;` |
|         - | 1986 | `		}` |
|         - | 1987 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|       112 | 1988 | `		if( p + 3 < pEnd` |
|       112 | 1989 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       112 | 1990 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|        97 | 1991 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|        82 | 1992 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|        82 | 1993 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        82 | 1994 | `		 && p[3].sData.nByte == pName->nByte` |
|        75 | 1995 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        54 | 1996 | `			return 1;` |
|         - | 1997 | `		}` |
|         - | 1998 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|         - | 1999 | `		 * hook operates on the shared per-instance backing store, so the` |
|         - | 2000 | `		 * property is backed (php compiles a default alongside it). */` |
|        60 | 2001 | `		if( p > pStart` |
|        56 | 2002 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|        28 | 2003 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         4 | 2004 | `		 && p[1].sData.nByte == pName->nByte` |
|         8 | 2005 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|         6 | 2006 | `			return 1;` |
|         - | 2007 | `		}` |
|        31 | 2008 | `	}` |
|        91 | 2009 | `	return 0;` |
|        76 | 2010 | `}` |
|         - | 2011 | `/*` |
|         - | 2012 | ` * True when p opens php 8.4's parent-hook call form` |
|         - | 2013 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|         - | 2014 | ` */` |
|      1266 | 2015 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|         5 | 2016 | `{` |
|      1480 | 2017 | `	return p + 6 < pEnd` |
|       842 | 2018 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       299 | 2019 | `	 && p->sData.nByte == sizeof("parent")-1` |
|       102 | 2020 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|        18 | 2021 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|        12 | 2022 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|        12 | 2023 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        12 | 2024 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|        12 | 2025 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        12 | 2026 | `	 && p[5].sData.nByte == 3` |
|        12 | 2027 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|         8 | 2028 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|      1475 | 2029 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|         5 | 2030 | `}` |
|         - | 2031 | `/*` |
|         - | 2032 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|         - | 2033 | ` * hook body into calls of the parent class's synthesized hook method` |
|         - | 2034 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|         - | 2035 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|         - | 2036 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|         - | 2037 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|         - | 2038 | ` * or SXERR_MEM.` |
|         - | 2039 | ` */` |
|         6 | 2040 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|         - | 2041 | `	SyToken *pStart,SyToken *pEnd)` |
|         2 | 2042 | `{` |
|         8 | 2043 | `	SyToken *p = pStart;` |
|        56 | 2044 | `	while( p < pEnd ){` |
|        50 | 2045 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|         - | 2046 | `			SyToken sTok;` |
|         - | 2047 | `			char zName[384];` |
|         - | 2048 | `			sxu32 nName;` |
|         - | 2049 | `			char *zDup;` |
|         - | 2050 | ``			/* `parent` `::` */`` |
|         8 | 2051 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|         8 | 2052 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|        11 | 2053 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|         6 | 2054 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|         8 | 2055 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|         8 | 2056 | `			if( zDup == 0 ){` |
|       ! 0 | 2057 | `				return SXERR_MEM;` |
|         - | 2058 | `			}` |
|         8 | 2059 | `			sTok = p[3]; /* keep the line info of the property name */` |
|         8 | 2060 | `			sTok.nType = PH7_TK_ID;` |
|         8 | 2061 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|         8 | 2062 | `			sTok.pUserData = 0;` |
|         8 | 2063 | `			SySetPut(pCopy,(const void *)&sTok);` |
|         8 | 2064 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|         8 | 2065 | `			continue;` |
|         - | 2066 | `		}` |
|        44 | 2067 | `		SySetPut(pCopy,(const void *)p);` |
|        44 | 2068 | `		p++;` |
|         2 | 2069 | `	}` |
|         8 | 2070 | `	return SXRET_OK;` |
|         5 | 2071 | `}` |
|         - | 2072 | `/*` |
|         - | 2073 | `` * A `get` hook's return type IS the property's declared type — php never lets a`` |
|         - | 2074 | ` * hook declare one, so there is nothing else it could be, and that is what makes` |
|         - | 2075 | `` * `public int $p { get { return "5"; } }` answer int(5) and a `get` returning`` |
|         - | 2076 | `` * "x" raise `C::$p::get(): Return value must be of type int, string returned`.`` |
|         - | 2077 | ` * Installing it on the synthesized method reuses the return enforcement that` |
|         - | 2078 | ` * already matches php byte for byte (the same move the __toString implicit` |
|         - | 2079 | `` * `string` type made), and lets the compile-time bare-`return;` check see the`` |
|         - | 2080 | ` * hook as the typed function php treats it as.` |
|         - | 2081 | ` *` |
|         - | 2082 | ` * The union alternatives are SHARED, not copied: their class-name SyStrings are` |
|         - | 2083 | ` * VM-allocator owned and outlive both records, which is the same contract` |
|         - | 2084 | ` * GenStateCopyTypeToAttr relies on.` |
|         - | 2085 | ` */` |
|       110 | 2086 | `static void GenStateHookGetReturnType(ph7_vm_func *pFunc,ph7_class_attr *pAttr)` |
|         5 | 2087 | `{` |
|       115 | 2088 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        13 | 2089 | `		return; /* untyped property: the hook is untyped too */` |
|         - | 2090 | `	}` |
|       103 | 2091 | `	pFunc->nReturnType = pAttr->nType;` |
|       103 | 2092 | `	pFunc->sReturnClass = pAttr->sClass;` |
|       103 | 2093 | `	pFunc->sReturnTypeName = pAttr->sTypeName;` |
|       103 | 2094 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|        14 | 2095 | `		pFunc->iFlags \|= VM_FUNC_RETURN_NULLABLE;` |
|         6 | 2096 | `	}` |
|       103 | 2097 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|         - | 2098 | `		sxu32 i;` |
|       ! 0 | 2099 | `		for( i = 0 ; i < SySetUsed(&pAttr->aUnionAlts) ; i++ ){` |
|       ! 0 | 2100 | `			SySetPut(&pFunc->aReturnUnion,SySetAt(&pAttr->aUnionAlts,i));` |
|       ! 0 | 2101 | `		}` |
|       ! 0 | 2102 | `	}` |
|        60 | 2103 | `}` |
|         - | 2104 | `/*` |
|         - | 2105 | `` * The mirror for a `set` hook. php gives it two implicit pieces of signature:`` |
|         - | 2106 | `` * the implicit `$value` formal carries the PROPERTY's declared type (so`` |
|         - | 2107 | `` * `public int $p { set { ... } }` coerces `$o->p = "7"` to int(7) and rejects`` |
|         - | 2108 | `` * "abc" with `C::$p::set(): Argument #1 ($value) must be of type int, string`` |
|         - | 2109 | `` * given`), and the hook itself returns `void` — a set hook that returns a value`` |
|         - | 2110 | `` * is php's `A void method must not return a value`, on an untyped property too.`` |
|         - | 2111 | `` * An EXPLICIT `set(T $v)` keeps its own declared type; only the implicit formal`` |
|         - | 2112 | ` * is typed from the property, which is why the caller passes pValueArg only` |
|         - | 2113 | ` * when it synthesized one.` |
|         - | 2114 | ` */` |
|        86 | 2115 | `static void GenStateHookSetSignature(ph7_gen_state *pGen,ph7_vm_func *pFunc,` |
|         - | 2116 | `	ph7_class_attr *pAttr,ph7_vm_func_arg *pValueArg)` |
|         3 | 2117 | `{` |
|         - | 2118 | `	char *zVoid;` |
|        89 | 2119 | `	if( pValueArg && (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|        65 | 2120 | `		pValueArg->nType = pAttr->nType;` |
|        65 | 2121 | `		pValueArg->sClass = pAttr->sClass;` |
|        65 | 2122 | `		pValueArg->sTypeName = pAttr->sTypeName;` |
|        65 | 2123 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|        14 | 2124 | `			pValueArg->iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|         6 | 2125 | `		}` |
|        65 | 2126 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|         - | 2127 | `			sxu32 i;` |
|         3 | 2128 | `			pValueArg->iFlags \|= VM_FUNC_ARG_UNION;` |
|         7 | 2129 | `			for( i = 0 ; i < SySetUsed(&pAttr->aUnionAlts) ; i++ ){` |
|         5 | 2130 | `				SySetPut(&pValueArg->aUnionAlts,SySetAt(&pAttr->aUnionAlts,i));` |
|         3 | 2131 | `			}` |
|         1 | 2132 | `		}` |
|        31 | 2133 | `	}` |
|        89 | 2134 | `	pFunc->nReturnType = MEMOBJ_VOID;` |
|        89 | 2135 | `	zVoid = SyMemBackendStrDup(&pGen->pVm->sAllocator,"void",sizeof("void")-1);` |
|        89 | 2136 | `	if( zVoid ){` |
|        89 | 2137 | `		SyStringInitFromBuf(&pFunc->sReturnTypeName,zVoid,sizeof("void")-1);` |
|        43 | 2138 | `	}` |
|        89 | 2139 | `}` |
|       142 | 2140 | `PH7_PRIVATE sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|         5 | 2141 | `{` |
|       147 | 2142 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 2143 | `	sxi32 rc;` |
|       147 | 2144 | `	int bRefsSelf = 0;` |
|       147 | 2145 | `	pGen->pIn++; /* Jump '{' */` |
|       359 | 2146 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|         - | 2147 | `		char zHook[384];` |
|         - | 2148 | `		SyString sHookName;` |
|         - | 2149 | `		ph7_class_method *pMeth;` |
|         - | 2150 | `		int bGet;` |
|       217 | 2151 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|       217 | 2152 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        18 | 2153 | `			pGen->pIn++; /* stray ';' between hooks */` |
|        26 | 2154 | `			continue;` |
|         - | 2155 | `		}` |
|       201 | 2156 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - | 2157 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|       ! 0 | 2158 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - | 2159 | `				"By-reference property hooks are not supported for %z::$%z",` |
|       ! 0 | 2160 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 | 2161 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2162 | `				return SXERR_ABORT;` |
|         - | 2163 | `			}` |
|       ! 0 | 2164 | `			return SXERR_CORRUPT;` |
|         - | 2165 | `		}` |
|       201 | 2166 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 2167 | `			goto HookSyntax;` |
|         - | 2168 | `		}` |
|       196 | 2169 | `		if( pGen->pIn->sData.nByte == 3` |
|       201 | 2170 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|       115 | 2171 | `			bGet = 1;` |
|       146 | 2172 | `		}else if( pGen->pIn->sData.nByte == 3` |
|        89 | 2173 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|        89 | 2174 | `			bGet = 0;` |
|        46 | 2175 | `		}else{` |
|       ! 0 | 2176 | `			goto HookSyntax;` |
|         - | 2177 | `		}` |
|       201 | 2178 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|       201 | 2179 | `		sHookName.zString = zHook;` |
|       299 | 2180 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|        98 | 2181 | `			bGet ? "get" : "set",&pAttr->sName);` |
|       201 | 2182 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|         - | 2183 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|         - | 2184 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|         - | 2185 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|         - | 2186 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|         - | 2187 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|        16 | 2188 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|        10 | 2189 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 2190 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - | 2191 | `					"Non-abstract property hook must have a body");` |
|       ! 0 | 2192 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 2193 | `					return SXERR_ABORT;` |
|         - | 2194 | `				}` |
|       ! 0 | 2195 | `				return SXERR_CORRUPT;` |
|         - | 2196 | `			}` |
|        18 | 2197 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - | 2198 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|        18 | 2199 | `			if( pMeth == 0 ){` |
|       ! 0 | 2200 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 2201 | `				return SXERR_ABORT;` |
|         - | 2202 | `			}` |
|        18 | 2203 | `			pMeth->sFunc.nLine = nHLine;` |
|        18 | 2204 | `			if( bGet ){` |
|        12 | 2205 | `				GenStateHookGetReturnType(&pMeth->sFunc,pAttr);` |
|         5 | 2206 | `			}` |
|        18 | 2207 | `			if( !bGet ){` |
|         - | 2208 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|         - | 2209 | `				 * compatible with concrete set-hook implementations (which` |
|         - | 2210 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|         - | 2211 | `				 * type (php: the abstract set's parameter type IS the property` |
|         - | 2212 | `				 * type), so the override contravariance check accepts a typed` |
|         - | 2213 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|         - | 2214 | `				ph7_vm_func_arg sVArg;` |
|         7 | 2215 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|         7 | 2216 | `				if( zVName == 0 ){` |
|       ! 0 | 2217 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 2218 | `					return SXERR_ABORT;` |
|         - | 2219 | `				}` |
|         7 | 2220 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|         7 | 2221 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|         7 | 2222 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         7 | 2223 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         7 | 2224 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|         7 | 2225 | `				GenStateHookSetSignature(&(*pGen),&pMeth->sFunc,pAttr,&sVArg);` |
|         7 | 2226 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|         3 | 2227 | `			}` |
|        18 | 2228 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|        18 | 2229 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 2230 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 2231 | `				return SXERR_ABORT;` |
|         - | 2232 | `			}` |
|        18 | 2233 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        18 | 2234 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|         - | 2235 | `		}` |
|       180 | 2236 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|       185 | 2237 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|         - | 2238 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|       ! 0 | 2239 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - | 2240 | `				"Abstract property hook cannot have body");` |
|       ! 0 | 2241 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2242 | `				return SXERR_ABORT;` |
|         - | 2243 | `			}` |
|       ! 0 | 2244 | `			return SXERR_CORRUPT;` |
|         - | 2245 | `		}` |
|       185 | 2246 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - | 2247 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|       185 | 2248 | `		if( pMeth == 0 ){` |
|       ! 0 | 2249 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 2250 | `			return SXERR_ABORT;` |
|         - | 2251 | `		}` |
|       185 | 2252 | `		pMeth->sFunc.nLine = nHLine;` |
|       185 | 2253 | `		if( bGet ){` |
|       105 | 2254 | `			GenStateHookGetReturnType(&pMeth->sFunc,pAttr);` |
|        50 | 2255 | `		}` |
|       185 | 2256 | `		if( !bGet ){` |
|         - | 2257 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|        83 | 2258 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        20 | 2259 | `				SyToken *pRp = 0;` |
|        20 | 2260 | `				pGen->pIn++;` |
|        20 | 2261 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|        20 | 2262 | `				if( pRp >= pGen->pEnd ){` |
|       ! 0 | 2263 | `					goto HookSyntax;` |
|         - | 2264 | `				}` |
|        20 | 2265 | `				if( pGen->pIn < pRp ){` |
|        20 | 2266 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|        20 | 2267 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2268 | `						return SXERR_ABORT;` |
|         - | 2269 | `					}` |
|         9 | 2270 | `				}` |
|        20 | 2271 | `				pGen->pIn = &pRp[1];` |
|         9 | 2272 | `			}` |
|        83 | 2273 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|         - | 2274 | `				/* Implicit $value formal */` |
|         - | 2275 | `				ph7_vm_func_arg sVArg;` |
|        65 | 2276 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        65 | 2277 | `				if( zVName == 0 ){` |
|       ! 0 | 2278 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 2279 | `					return SXERR_ABORT;` |
|         - | 2280 | `				}` |
|        65 | 2281 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        65 | 2282 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        65 | 2283 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        65 | 2284 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        65 | 2285 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        65 | 2286 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|        65 | 2287 | `				GenStateHookSetSignature(&(*pGen),&pMeth->sFunc,pAttr,&sVArg);` |
|        65 | 2288 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        34 | 2289 | `			}else{` |
|         - | 2290 | ``				/* An EXPLICIT `set(T $v)` keeps its own parameter type; only the`` |
|         - | 2291 | `				 * void return is implicit. */` |
|        20 | 2292 | `				GenStateHookSetSignature(&(*pGen),&pMeth->sFunc,pAttr,0);` |
|         - | 2293 | `			}` |
|        40 | 2294 | `		}` |
|       242 | 2295 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - | 2296 | `			/* Block body */` |
|       119 | 2297 | `			SyToken *pBodyStart = pGen->pIn;` |
|       119 | 2298 | `			SyToken *pCloser = 0;` |
|       119 | 2299 | `			int bParentCall = 0;` |
|       119 | 2300 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|       119 | 2301 | `			if( pCloser < pGen->pEnd ){` |
|         - | 2302 | `				SyToken *pScan;` |
|      1049 | 2303 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|       939 | 2304 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|         6 | 2305 | `						bParentCall = 1;` |
|         6 | 2306 | `						break;` |
|         - | 2307 | `					}` |
|       470 | 2308 | `				}` |
|        57 | 2309 | `			}` |
|       119 | 2310 | `			if( bParentCall ){` |
|         - | 2311 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|         - | 2312 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|         - | 2313 | `				 * hook method), then continue past the original body. */` |
|         - | 2314 | `				SySet sBody;` |
|         6 | 2315 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|         6 | 2316 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         6 | 2317 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|         6 | 2318 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 2319 | `					SySetRelease(&sBody);` |
|       ! 0 | 2320 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 2321 | `					return SXERR_ABORT;` |
|         - | 2322 | `				}` |
|         6 | 2323 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         6 | 2324 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         6 | 2325 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|         6 | 2326 | `				pGen->pIn = &pCloser[1];` |
|         6 | 2327 | `				pGen->pEnd = pSavedEnd;` |
|         6 | 2328 | `				SySetRelease(&sBody);` |
|         6 | 2329 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 2330 | `					return SXERR_ABORT;` |
|         - | 2331 | `				}` |
|         6 | 2332 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|         4 | 2333 | `			}else{` |
|       115 | 2334 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|       115 | 2335 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 2336 | `					return SXERR_ABORT;` |
|         - | 2337 | `				}` |
|       115 | 2338 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|         - | 2339 | `			}` |
|       119 | 2340 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        23 | 2341 | `				bRefsSelf = 1;` |
|        15 | 2342 | `			}` |
|       159 | 2343 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|         - | 2344 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|         - | 2345 | `			GenBlock *pBlock;` |
|         - | 2346 | `			SySet *pInstrContainer;` |
|         - | 2347 | `			SyToken *pBodyStart;` |
|         - | 2348 | `			SyToken *pExprEnd;` |
|        69 | 2349 | `			SyToken *pSavedEnd = 0;` |
|         - | 2350 | `			SySet sBody;` |
|        69 | 2351 | `			int bParentCall = 0;` |
|        69 | 2352 | `			pGen->pIn++; /* Jump '=>' */` |
|        69 | 2353 | `			pBodyStart = pGen->pIn;` |
|         - | 2354 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|         - | 2355 | `			 * would end the enclosing hook list) and rewrite any` |
|         - | 2356 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|         - | 2357 | `			 * method on a token copy. */` |
|         - | 2358 | `			{` |
|        69 | 2359 | `				sxi32 iNest = 0;` |
|        69 | 2360 | `				pExprEnd = pBodyStart;` |
|       371 | 2361 | `				while( pExprEnd < pGen->pEnd ){` |
|       371 | 2362 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         9 | 2363 | `						iNest++;` |
|       367 | 2364 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         9 | 2365 | `						if( iNest <= 0 ){` |
|       ! 0 | 2366 | `							break;` |
|         - | 2367 | `						}` |
|         9 | 2368 | `						iNest--;` |
|       359 | 2369 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|        69 | 2370 | `						break;` |
|         - | 2371 | `					}` |
|       305 | 2372 | `					pExprEnd++;` |
|         3 | 2373 | `				}` |
|         - | 2374 | `			}` |
|         - | 2375 | `			{` |
|         - | 2376 | `				SyToken *pScan;` |
|       351 | 2377 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|       287 | 2378 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|         3 | 2379 | `						bParentCall = 1;` |
|         3 | 2380 | `						break;` |
|         - | 2381 | `					}` |
|       144 | 2382 | `				}` |
|         - | 2383 | `			}` |
|        69 | 2384 | `			if( bParentCall ){` |
|         3 | 2385 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 | 2386 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|         3 | 2387 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 2388 | `					SySetRelease(&sBody);` |
|       ! 0 | 2389 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 2390 | `					return SXERR_ABORT;` |
|         - | 2391 | `				}` |
|         3 | 2392 | `				pSavedEnd = pGen->pEnd;` |
|         3 | 2393 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 | 2394 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         1 | 2395 | `			}` |
|       102 | 2396 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|        66 | 2397 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|        69 | 2398 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 2399 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|       ! 0 | 2400 | `				return SXERR_ABORT;` |
|         - | 2401 | `			}` |
|        69 | 2402 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        69 | 2403 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|        69 | 2404 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|        69 | 2405 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        69 | 2406 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        69 | 2407 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        69 | 2408 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        69 | 2409 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        69 | 2410 | `			if( bParentCall ){` |
|         3 | 2411 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|         3 | 2412 | `				pGen->pEnd = pSavedEnd;` |
|         3 | 2413 | `				SySetRelease(&sBody);` |
|         1 | 2414 | `			}` |
|        69 | 2415 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2416 | `				return SXERR_ABORT;` |
|         - | 2417 | `			}` |
|        69 | 2418 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|        69 | 2419 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        37 | 2420 | `				bRefsSelf = 1;` |
|        18 | 2421 | `			}` |
|        69 | 2422 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        69 | 2423 | `				pGen->pIn++; /* Jump ';' */` |
|        33 | 2424 | `			}` |
|        69 | 2425 | `			if( !bGet ){` |
|         - | 2426 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|         - | 2427 | `				 * the dispatcher consumes the implicit return value — which` |
|         - | 2428 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|         - | 2429 | ``				 * for `$this->NAME = expr`). */`` |
|         6 | 2430 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|         6 | 2431 | `				bRefsSelf = 1;` |
|         2 | 2432 | `			}` |
|        36 | 2433 | `		}else{` |
|       ! 0 | 2434 | `			goto HookSyntax;` |
|         - | 2435 | `		}` |
|       185 | 2436 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       185 | 2437 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 2438 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 2439 | `			return SXERR_ABORT;` |
|         - | 2440 | `		}` |
|       185 | 2441 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|         5 | 2442 | `	}` |
|       147 | 2443 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|       ! 0 | 2444 | `		goto HookSyntax;` |
|         - | 2445 | `	}` |
|       147 | 2446 | `	pGen->pIn++; /* Jump '}' */` |
|       147 | 2447 | `	if( !bRefsSelf ){` |
|         - | 2448 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|         - | 2449 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|         - | 2450 | `		 * a default value (compile fatal, php's exact wording). */` |
|        89 | 2451 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|        89 | 2452 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       ! 0 | 2453 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 2454 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|       ! 0 | 2455 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 | 2456 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2457 | `				return SXERR_ABORT;` |
|         - | 2458 | `			}` |
|       ! 0 | 2459 | `			return SXERR_CORRUPT;` |
|         - | 2460 | `		}` |
|        42 | 2461 | `	}` |
|       147 | 2462 | `	return SXRET_OK;` |
|       ! 0 | 2463 | `HookSyntax:` |
|       ! 0 | 2464 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 2465 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|       ! 0 | 2466 | `		&pClass->sName,&pAttr->sName);` |
|       ! 0 | 2467 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 2468 | `		return SXERR_ABORT;` |
|         - | 2469 | `	}` |
|       ! 0 | 2470 | `	return SXERR_CORRUPT;` |
|        76 | 2471 | `}` |
|         - | 2472 | `/*` |
|         - | 2473 | ` * Compile an object interface.` |
|         - | 2474 | ` *  According to the PHP language reference manual` |
|         - | 2475 | ` *   Object Interfaces:` |
|         - | 2476 | ` *   Object interfaces allow you to create code which specifies which methods` |
|         - | 2477 | ` *   a class must implement, without having to define how these methods are handled.` |
|         - | 2478 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|         - | 2479 | ` *   class, but without any of the methods having their contents defined.` |
|         - | 2480 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|         - | 2481 | ` */` |
|     81674 | 2482 | `PH7_PRIVATE sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|         5 | 2483 | `{` |
|     81679 | 2484 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 2485 | `	ph7_class *pClass,*pBase;` |
|     81679 | 2486 | `	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' */` |
|         - | 2487 | `	SyToken *pEnd,*pTmp;` |
|         - | 2488 | `	SyString *pName;` |
|         - | 2489 | `	sxi32 nKwrd;` |
|         - | 2490 | `	sxi32 rc;` |
|         - | 2491 | `	{` |
|         - | 2492 | `		/* Deferral gate: parent interfaces may need an autoloader` |
|         - | 2493 | `		 * that has not run yet. */` |
|         - | 2494 | `		sxi32 rcDefer;` |
|     81679 | 2495 | `		if( GenStateMaybeDeferClass(pGen,0,PH7_DEFER_KIND_INTERFACE,&rcDefer) ){` |
|         3 | 2496 | `			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - | 2497 | `		}` |
|         - | 2498 | `	}` |
|         - | 2499 | `	/* Jump the 'interface' keyword */` |
|     81677 | 2500 | `	pGen->pIn++;` |
|         - | 2501 | `	/* Extract interface name */` |
|     81677 | 2502 | `	pName = &pGen->pIn->sData;` |
|         - | 2503 | `	/* Advance the stream cursor */` |
|     81677 | 2504 | `	pGen->pIn++;` |
|         - | 2505 | `	/* Build FQN and obtain a raw class */ {` |
|         - | 2506 | `		SyBlob sFQN;` |
|         - | 2507 | `		SyString sFQNStr;` |
|     81677 | 2508 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     81677 | 2509 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     81677 | 2510 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|         - | 2511 | ``		/* php refuses a declaration whose short name a local `use` already took. */`` |
|     81677 | 2512 | `		if( GenStateGuardImportRedeclare(pGen,0,pName,&sFQNStr,nLine) == SXERR_ABORT ){` |
|       ! 0 | 2513 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 2514 | `			return SXERR_ABORT;` |
|         - | 2515 | `		}` |
|     81677 | 2516 | `		GenStateRecordDeclaredName(pGen,0,&sFQNStr);` |
|     81677 | 2517 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     81677 | 2518 | `		SyBlobRelease(&sFQN);` |
|         - | 2519 | `	}` |
|     81677 | 2520 | `	if( pClass == 0 ){` |
|       ! 0 | 2521 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 2522 | `		return SXERR_ABORT;` |
|         - | 2523 | `	}` |
|     81677 | 2524 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     81677 | 2525 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 2526 | `		return SXERR_ABORT;` |
|         - | 2527 | `	}` |
|         - | 2528 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|     81677 | 2529 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|         - | 2530 | `	/* Assume no base class is given */` |
|     81677 | 2531 | `	pBase = 0;` |
|     81677 | 2532 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     31717 | 2533 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     31717 | 2534 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a, c, … */ ){` |
|         - | 2535 | `			/* php lets an interface extend SEVERAL parent interfaces. The first` |
|         - | 2536 | `			 * becomes pBase (single-inheritance chain, hDerived); every extra one` |
|         - | 2537 | `			 * is recorded via PH7_ClassImplement so it lands in aInterface — the` |
|         - | 2538 | `			 * runtime subtype walk (VmInterfaceReaches) follows both. */` |
|     31717 | 2539 | `			pGen->pIn++;` |
|     15857 | 2540 | `			for(;;){` |
|         - | 2541 | `				SyBlob sResolved;` |
|         - | 2542 | `				SyString sBaseName;` |
|         - | 2543 | `				sxu32 nRefLine;` |
|         - | 2544 | `				ph7_class *pParent;` |
|     31719 | 2545 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     31719 | 2546 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     31719 | 2547 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 2548 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 2549 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 2550 | `						"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|       ! 0 | 2551 | `						pName);` |
|       ! 0 | 2552 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 2553 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2554 | `						return SXERR_ABORT;` |
|         - | 2555 | `					}` |
|       ! 0 | 2556 | `					return SXRET_OK;` |
|         - | 2557 | `				}` |
|     47576 | 2558 | `				pParent = PH7_VmExtractClass(pGen->pVm,` |
|     31714 | 2559 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     31719 | 2560 | `				SyStringInitFromBuf(&sBaseName,` |
|         - | 2561 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 2562 | `				/* Only interfaces is allowed */` |
|     31719 | 2563 | `				while( pParent && (pParent->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 2564 | `					pParent = pParent->pNextName;` |
|       ! 0 | 2565 | `				}` |
|     31719 | 2566 | `				if( pParent == 0 ){` |
|       ! 0 | 2567 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 2568 | `						"Nonexistent base interface '%z'",&sBaseName);` |
|       ! 0 | 2569 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2570 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 2571 | `						return SXERR_ABORT;` |
|       ! 0 | 2572 | `					}` |
|     31719 | 2573 | `				}else if( pBase == 0 ){` |
|         - | 2574 | `					/* First parent → single-inheritance base */` |
|     31717 | 2575 | `					pBase = pParent;` |
|     15861 | 2576 | `				}else{` |
|         - | 2577 | `					/* Additional parent → record it in aInterface (+ copy its` |
|         - | 2578 | `					 * constants/method stubs) so instanceof reaches it too. */` |
|         3 | 2579 | `					PH7_ClassImplement(pClass,pParent);` |
|         - | 2580 | `				}` |
|     31719 | 2581 | `				SyBlobRelease(&sResolved);` |
|         - | 2582 | `				/* Continue on a comma-separated list */` |
|     31719 | 2583 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|         3 | 2584 | `					pGen->pIn++;` |
|         3 | 2585 | `					continue;` |
|         - | 2586 | `				}` |
|     31717 | 2587 | `				break;` |
|       ! 0 | 2588 | `			}` |
|     15856 | 2589 | `		}` |
|     15856 | 2590 | `	}` |
|     81677 | 2591 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - | 2592 | `		/* Syntax error */` |
|       ! 0 | 2593 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|       ! 0 | 2594 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 2595 | `		if( rc == SXERR_ABORT ){` |
|         - | 2596 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 2597 | `			return SXERR_ABORT;` |
|         - | 2598 | `		}` |
|       ! 0 | 2599 | `		return SXRET_OK;` |
|         - | 2600 | `	}` |
|     81677 | 2601 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     81677 | 2602 | `	pEnd = 0; /* cc warning */` |
|         - | 2603 | `	/* Delimit the interface body */` |
|     81677 | 2604 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|     81677 | 2605 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 2606 | `		/* Syntax error */` |
|       ! 0 | 2607 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|       ! 0 | 2608 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 2609 | `		if( rc == SXERR_ABORT ){` |
|         - | 2610 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 2611 | `			return SXERR_ABORT;` |
|         - | 2612 | `		}` |
|       ! 0 | 2613 | `		return SXRET_OK;` |
|         - | 2614 | `	}` |
|         - | 2615 | `	/* The delimiter token is the interface body's closing brace */` |
|     81677 | 2616 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 2617 | `	/* Swap token stream */` |
|     81677 | 2618 | `	pTmp = pGen->pEnd;` |
|     81677 | 2619 | `	pGen->pEnd = pEnd;` |
|         - | 2620 | `	/* This interface is now the lexical class for its body (see pCurClass) — a` |
|         - | 2621 | `	 * const default here is not a trait, so __TRAIT__ stays "". */` |
|     81677 | 2622 | `	pGen->pCurClass = pClass;` |
|         - | 2623 | `	/* Start the parse process` |
|         - | 2624 | `	 * Note (According to the PHP reference manual):` |
|         - | 2625 | `	 *  Only constants and function signatures(without body) are allowed.` |
|         - | 2626 | `	 *  Only 'public' visibility is allowed.` |
|         - | 2627 | `	 */` |
|    149549 | 2628 | `	for(;;){` |
|         - | 2629 | `		/* Jump leading/trailing semi-colons */` |
|    516533 | 2630 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    217431 | 2631 | `			pGen->pIn++;` |
|         5 | 2632 | `		}` |
|    299107 | 2633 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 2634 | `			/* End of interface body */` |
|     81673 | 2635 | `			break;` |
|         - | 2636 | `		}` |
|         - | 2637 | `		/* Bind a directly-preceding docblock to this member */` |
|    217439 | 2638 | `		GenStateSetPendingDoc(&(*pGen));` |
|    217439 | 2639 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 2640 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 2641 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|       ! 0 | 2642 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 2643 | `			if( rc == SXERR_ABORT ){` |
|         - | 2644 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 2645 | `				return SXERR_ABORT;` |
|         - | 2646 | `			}` |
|       ! 0 | 2647 | `			goto done;` |
|         - | 2648 | `		}` |
|         - | 2649 | `		/* Extract the current keyword */` |
|    217439 | 2650 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    217439 | 2651 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - | 2652 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|         - | 2653 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|         3 | 2654 | `			const char *zKind = "member";` |
|         3 | 2655 | `			SyString *pMemberName = 0;` |
|         3 | 2656 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|         3 | 2657 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|         3 | 2658 | `				if( nNext == PH7_TKWRD_CONST ){` |
|         3 | 2659 | `					zKind = "constant";` |
|         3 | 2660 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|         3 | 2661 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|         2 | 2662 | `					}` |
|         1 | 2663 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 2664 | `					zKind = "method";` |
|       ! 0 | 2665 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       ! 0 | 2666 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       ! 0 | 2667 | `					}` |
|       ! 0 | 2668 | `				}` |
|         1 | 2669 | `			}` |
|         3 | 2670 | `			if( pMemberName ){` |
|         4 | 2671 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         1 | 2672 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|         2 | 2673 | `			}else{` |
|       ! 0 | 2674 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 2675 | `					"Access type for interface %s must be public",zKind);` |
|         - | 2676 | `			}` |
|         3 | 2677 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2678 | `				return SXERR_ABORT;` |
|         - | 2679 | `			}` |
|         3 | 2680 | `			goto done;` |
|         - | 2681 | `		}` |
|    217437 | 2682 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|       ! 0 | 2683 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 2684 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 | 2685 | `			if( rc == SXERR_ABORT ){` |
|         - | 2686 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 2687 | `				return SXERR_ABORT;` |
|         - | 2688 | `			}` |
|       ! 0 | 2689 | `			goto done;` |
|         - | 2690 | `		}` |
|    217437 | 2691 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|         - | 2692 | `			/* Advance the stream cursor */` |
|    154017 | 2693 | `			pGen->pIn++;` |
|    154012 | 2694 | `			if( pGen->pIn < pGen->pEnd` |
|    154017 | 2695 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|    154012 | 2696 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         - | 2697 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|         - | 2698 | `				 * requirement. The attribute compiler + hook parser handle it` |
|         - | 2699 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|         - | 2700 | `				 * property without hooks is ITS "Interfaces may only include` |
|         - | 2701 | `				 * hooked properties" error). */` |
|       ! 0 | 2702 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 | 2703 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 | 2704 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 2705 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2706 | `						return SXERR_ABORT;` |
|         - | 2707 | `					}` |
|       ! 0 | 2708 | `					goto done;` |
|         - | 2709 | `				}` |
|       ! 0 | 2710 | `				continue;` |
|         - | 2711 | `			}` |
|    154017 | 2712 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|         - | 2713 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|         - | 2714 | `				 * '$' also opens a hooked-property requirement. */` |
|       ! 0 | 2715 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|       ! 0 | 2716 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|       ! 0 | 2717 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|       ! 0 | 2718 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 | 2719 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 | 2720 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 2721 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 2722 | `							return SXERR_ABORT;` |
|         - | 2723 | `						}` |
|       ! 0 | 2724 | `						goto done;` |
|         - | 2725 | `					}` |
|       ! 0 | 2726 | `					continue;` |
|         - | 2727 | `				}` |
|       ! 0 | 2728 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 2729 | `					"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 | 2730 | `				if( rc == SXERR_ABORT ){` |
|         - | 2731 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 | 2732 | `					return SXERR_ABORT;` |
|         - | 2733 | `				}` |
|       ! 0 | 2734 | `				goto done;` |
|         - | 2735 | `			}` |
|    154017 | 2736 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    154017 | 2737 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|         - | 2738 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|         - | 2739 | `				 * hooked-property requirement (PHP 8.4). */` |
|         4 | 2740 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|         5 | 2741 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|         7 | 2742 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|         2 | 2743 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|         5 | 2744 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 2745 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 2746 | `							return SXERR_ABORT;` |
|         - | 2747 | `						}` |
|       ! 0 | 2748 | `						goto done;` |
|         - | 2749 | `					}` |
|         5 | 2750 | `					continue;` |
|         - | 2751 | `				}` |
|       ! 0 | 2752 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 2753 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 | 2754 | `				if( rc == SXERR_ABORT ){` |
|         - | 2755 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 | 2756 | `					return SXERR_ABORT;` |
|         - | 2757 | `				}` |
|       ! 0 | 2758 | `				goto done;` |
|         - | 2759 | `			}` |
|     77004 | 2760 | `		}` |
|    217433 | 2761 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|         - | 2762 | `			/* Parse constant */` |
|     63421 | 2763 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|     63421 | 2764 | `			if( rc != SXRET_OK ){` |
|         3 | 2765 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 2766 | `					return SXERR_ABORT;` |
|         - | 2767 | `				}` |
|         3 | 2768 | `				goto done;` |
|         - | 2769 | `			}` |
|     31712 | 2770 | `		}else{` |
|    154017 | 2771 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|    154017 | 2772 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - | 2773 | `				/* Static method,record that */` |
|     13589 | 2774 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|         - | 2775 | `				/* Advance the stream cursor */` |
|     13589 | 2776 | `				pGen->pIn++;` |
|     13584 | 2777 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     13589 | 2778 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 2779 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 2780 | `							"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 | 2781 | `						if( rc == SXERR_ABORT ){` |
|         - | 2782 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 2783 | `							return SXERR_ABORT;` |
|         - | 2784 | `						}` |
|       ! 0 | 2785 | `						goto done;` |
|         - | 2786 | `				}` |
|      6792 | 2787 | `			}` |
|         - | 2788 | `			/* Process method signature (no body for interface methods) */` |
|    154017 | 2789 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|    154017 | 2790 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 2791 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 2792 | `					return SXERR_ABORT;` |
|         - | 2793 | `				}` |
|       ! 0 | 2794 | `				goto done;` |
|         - | 2795 | `			}` |
|         - | 2796 | `		}` |
|         5 | 2797 | `	}` |
|         - | 2798 | `	/* Reject a php-fatal redeclaration before hoisting the interface */` |
|     81673 | 2799 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|         3 | 2800 | `		return SXERR_ABORT;` |
|         - | 2801 | `	}` |
|         - | 2802 | `	/* Install the interface */` |
|     81671 | 2803 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     81671 | 2804 | `	if( rc == SXRET_OK && pBase ){` |
|         - | 2805 | `		/* Inherit from the base interface */` |
|     31717 | 2806 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     15856 | 2807 | `	}` |
|     81671 | 2808 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 2809 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 2810 | `		return SXERR_ABORT;` |
|         - | 2811 | `	}` |
|     40833 | 2812 | `done:` |
|     81675 | 2813 | `	pGen->pCurClass = pSavedCurClass;` |
|         - | 2814 | `	/* Point beyond the interface body */` |
|     81675 | 2815 | `	pGen->pIn  = &pEnd[1];` |
|     81675 | 2816 | `	pGen->pEnd = pTmp;` |
|     81675 | 2817 | `	return PH7_OK;` |
|     40842 | 2818 | `}` |
|         - | 2819 | `/*` |
|         - | 2820 | ` * Compile a user-defined class.` |
|         - | 2821 | ` * According to the PHP language reference manual` |
|         - | 2822 | ` *  class` |
|         - | 2823 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|         - | 2824 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|         - | 2825 | ` *  of the properties and methods belonging to the class.` |
|         - | 2826 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|         - | 2827 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|         - | 2828 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|         - | 2829 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - | 2830 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|         - | 2831 | ` *  (called "methods").` |
|         - | 2832 | ` */` |
|         - | 2833 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|         - | 2834 | `typedef struct TraitUseEntry TraitUseEntry;` |
|         - | 2835 | `struct TraitUseEntry {` |
|         - | 2836 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|         - | 2837 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|         - | 2838 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|         - | 2839 | `};` |
|         - | 2840 | `/*` |
|         - | 2841 | ` * Validate that methods implementing interface contracts have compatible` |
|         - | 2842 | ` * signatures: public visibility and at least as many parameters as declared.` |
|         - | 2843 | ` */` |
|    518884 | 2844 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 2845 | `{` |
|         - | 2846 | `	ph7_class **apIface;` |
|         - | 2847 | `	sxu32 nIface,i;` |
|         - | 2848 | `	sxi32 rc;` |
|    518889 | 2849 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|       ! 0 | 2850 | `		return SXRET_OK;` |
|         - | 2851 | `	}` |
|    518889 | 2852 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    518889 | 2853 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   1008409 | 2854 | `	for(i = 0; i < nIface; i++){` |
|    489525 | 2855 | `		ph7_class *pIface = apIface[i];` |
|         - | 2856 | `		SyHashEntry *pEntry;` |
|    489525 | 2857 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   1427575 | 2858 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|    938055 | 2859 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|         - | 2860 | `			ph7_class_method *pImplMeth;` |
|    938055 | 2861 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|         - | 2862 | `			/* Find the implementing method in the class */` |
|    938055 | 2863 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|    938055 | 2864 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        23 | 2865 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|         - | 2866 | `			}` |
|         - | 2867 | `			/* Check visibility: interface methods must be implemented as public */` |
|    938037 | 2868 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|         4 | 2869 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - | 2870 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|         1 | 2871 | `					&pClass->sName,pMName,&pIface->sName);` |
|         3 | 2872 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 2873 | `					return SXERR_ABORT;` |
|         - | 2874 | `				}` |
|         1 | 2875 | `			}` |
|         - | 2876 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|         - | 2877 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|         - | 2878 | `			 */` |
|         - | 2879 | `			{` |
|    938037 | 2880 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|    938037 | 2881 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|    938037 | 2882 | `				int sigError = 0;` |
|    938037 | 2883 | `				if( nImplArgs < nIfaceArgs ){` |
|         3 | 2884 | `					sigError = 1;` |
|    938036 | 2885 | `				}else if( nImplArgs > nIfaceArgs ){` |
|         - | 2886 | `					/* Extra parameters must all have default values */` |
|      4537 | 2887 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|         - | 2888 | `					sxu32 k;` |
|      9067 | 2889 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|      4537 | 2890 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|         3 | 2891 | `							sigError = 1;` |
|         3 | 2892 | `							break;` |
|         - | 2893 | `						}` |
|      2270 | 2894 | `					}` |
|      2266 | 2895 | `				}` |
|    938037 | 2896 | `				if( sigError ){` |
|         - | 2897 | `					SyBlob sImplSig, sIfaceSig;` |
|         - | 2898 | `					ph7_vm_func_arg *aArgs;` |
|         - | 2899 | `					sxu32 j;` |
|         6 | 2900 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|         6 | 2901 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|         - | 2902 | `					/* Build implementing method signature */` |
|         6 | 2903 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        12 | 2904 | `					for(j = 0; j < nImplArgs; j++){` |
|         8 | 2905 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|         8 | 2906 | `						SyBlobAppend(&sImplSig,"$",1);` |
|         8 | 2907 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 | 2908 | `					}` |
|         - | 2909 | `					/* Build interface method signature */` |
|         6 | 2910 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|        12 | 2911 | `					for(j = 0; j < nIfaceArgs; j++){` |
|         8 | 2912 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|         8 | 2913 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|         8 | 2914 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 | 2915 | `					}` |
|         8 | 2916 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - | 2917 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|         2 | 2918 | `						&pClass->sName,pMName,` |
|         4 | 2919 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|         2 | 2920 | `						&pIface->sName,pMName,` |
|         4 | 2921 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|         6 | 2922 | `					SyBlobRelease(&sImplSig);` |
|         6 | 2923 | `					SyBlobRelease(&sIfaceSig);` |
|         6 | 2924 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2925 | `						return SXERR_ABORT;` |
|         - | 2926 | `					}` |
|         2 | 2927 | `				}` |
|         - | 2928 | `			}` |
|         5 | 2929 | `		}` |
|    244765 | 2930 | `	}` |
|    518889 | 2931 | `	return SXRET_OK;` |
|    259447 | 2932 | `}` |
|         - | 2933 | `/*` |
|         - | 2934 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|         - | 2935 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|         - | 2936 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|         - | 2937 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|         - | 2938 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|         - | 2939 | ` * means that specific hook is still missing.` |
|         - | 2940 | ` */` |
|        38 | 2941 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|         5 | 2942 | `{` |
|         - | 2943 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|         - | 2944 | `	ph7_class_attr *pProp;` |
|        38 | 2945 | `	if( pMName->nByte <= nPfx` |
|        27 | 2946 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|         4 | 2947 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|        36 | 2948 | `		return 0; /* not a hook stub */` |
|         - | 2949 | `	}` |
|         7 | 2950 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|         7 | 2951 | `	return pProp != 0` |
|         6 | 2952 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|         3 | 2953 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|        24 | 2954 | `}` |
|         - | 2955 | `/*` |
|         - | 2956 | ` * Append an abstract member's display name to the message blob, translating a` |
|         - | 2957 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|         - | 2958 | ` */` |
|        16 | 2959 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|         4 | 2960 | `{` |
|         - | 2961 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        16 | 2962 | `	if( pMName->nByte > nPfx` |
|        12 | 2963 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|       ! 0 | 2964 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|       ! 0 | 2965 | `		SyBlobAppend(pMsg,"$",1);` |
|       ! 0 | 2966 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|       ! 0 | 2967 | `		SyBlobAppend(pMsg,"::",2);` |
|       ! 0 | 2968 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|       ! 0 | 2969 | `		return;` |
|         - | 2970 | `	}` |
|        20 | 2971 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|        12 | 2972 | `}` |
|         - | 2973 | `/*` |
|         - | 2974 | ` * Check that a concrete class has no remaining abstract methods.` |
|         - | 2975 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|         - | 2976 | ` */` |
|    518884 | 2977 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 2978 | `{` |
|         - | 2979 | `	ph7_class_method *pMeth;` |
|         - | 2980 | `	SyHashEntry *pEntry;` |
|         - | 2981 | `	sxu32 nAbstract;` |
|         - | 2982 | `	SyBlob sMsg;` |
|         - | 2983 | `	sxi32 rc;` |
|         - | 2984 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|    518889 | 2985 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     22715 | 2986 | `		return SXRET_OK;` |
|         - | 2987 | `	}` |
|         - | 2988 | `	/* Count abstract methods */` |
|    496179 | 2989 | `	nAbstract = 0;` |
|    496179 | 2990 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   7431304 | 2991 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   6687043 | 2992 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   6687043 | 2993 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        27 | 2994 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|         7 | 2995 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 2996 | `			}` |
|        20 | 2997 | `			nAbstract++;` |
|         8 | 2998 | `		}` |
|         5 | 2999 | `	}` |
|    496179 | 3000 | `	if( nAbstract == 0 ){` |
|    496165 | 3001 | `		return SXRET_OK;` |
|         - | 3002 | `	}` |
|         - | 3003 | `	/* Build the error message listing all abstract methods with origins */` |
|        18 | 3004 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        18 | 3005 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|         - | 3006 | `		"be declared abstract or implement the remaining method%s (",` |
|         7 | 3007 | `		&pClass->sName,nAbstract,` |
|         7 | 3008 | `		(nAbstract > 1 ? "s" : ""),` |
|         7 | 3009 | `		(nAbstract > 1 ? "s" : ""));` |
|         - | 3010 | `	/* Second pass: list methods with origins */` |
|         - | 3011 | `	{` |
|        18 | 3012 | `		sxu32 nListed = 0;` |
|        18 | 3013 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|        36 | 3014 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|        22 | 3015 | `			ph7_class *pOrigin = 0;` |
|         - | 3016 | `			SyString *pMName;` |
|        22 | 3017 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        22 | 3018 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|         3 | 3019 | `				continue;` |
|         - | 3020 | `			}` |
|        20 | 3021 | `			pMName = &pMeth->sFunc.sName;` |
|        20 | 3022 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|       ! 0 | 3023 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 3024 | `			}` |
|        20 | 3025 | `			if( nListed > 0 ){` |
|         3 | 3026 | `				SyBlobAppend(&sMsg,", ",2);` |
|         1 | 3027 | `			}` |
|         - | 3028 | `			/* Find the origin of this abstract method.` |
|         - | 3029 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|         - | 3030 | `			 * inheritance chains) take precedence for interface-declared` |
|         - | 3031 | `			 * methods. Abstract class methods only win when the class` |
|         - | 3032 | `			 * itself declared the abstract method (not inherited from` |
|         - | 3033 | `			 * an interface). Trait methods are adopted into the using` |
|         - | 3034 | `			 * class's namespace.` |
|         - | 3035 | `			 */` |
|         - | 3036 | `			{` |
|         - | 3037 | `				ph7_class **apIface;` |
|         - | 3038 | `				ph7_class **apTrait;` |
|         - | 3039 | `				ph7_class *pWalk;` |
|         - | 3040 | `				sxu32 i;` |
|         - | 3041 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|         - | 3042 | `				 * (one that was written in the class body, not inherited from an` |
|         - | 3043 | `				 * interface). PHP attributes origin to the declaring class.` |
|         - | 3044 | `				 */` |
|        20 | 3045 | `				if( pClass->pBase ){` |
|        11 | 3046 | `					pWalk = pClass->pBase;` |
|        19 | 3047 | `					while( pWalk ){` |
|         - | 3048 | `						ph7_class_method *pParentMeth;` |
|        13 | 3049 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|        13 | 3050 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|         - | 3051 | `							/* Exclude methods that came from an interface anywhere` |
|         - | 3052 | `							 * in this class's ancestor chain.` |
|         - | 3053 | `							 */` |
|        13 | 3054 | `							int fromIface = 0;` |
|        13 | 3055 | `							ph7_class *pAnc = pWalk;` |
|        17 | 3056 | `							while( pAnc ){` |
|         - | 3057 | `								ph7_class **apPI;` |
|         - | 3058 | `								sxu32 j;` |
|        15 | 3059 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|        15 | 3060 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|        10 | 3061 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|        10 | 3062 | `										fromIface = 1;` |
|        10 | 3063 | `										break;` |
|         - | 3064 | `									}` |
|       ! 0 | 3065 | `								}` |
|        15 | 3066 | `								if( fromIface ) break;` |
|         6 | 3067 | `								pAnc = pAnc->pBase;` |
|         2 | 3068 | `							}` |
|        13 | 3069 | `							if( !fromIface ){` |
|         3 | 3070 | `								pOrigin = pWalk;` |
|         3 | 3071 | `								break;` |
|         - | 3072 | `							}` |
|         4 | 3073 | `						}` |
|        10 | 3074 | `						pWalk = pWalk->pBase;` |
|         2 | 3075 | `					}` |
|         4 | 3076 | `				}` |
|         - | 3077 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|         - | 3078 | `				 * each interface's own parent chain for the deepest origin.` |
|         - | 3079 | `				 */` |
|        20 | 3080 | `				if( !pOrigin ){` |
|        18 | 3081 | `					pWalk = pClass;` |
|        40 | 3082 | `					while( pWalk && !pOrigin ){` |
|        26 | 3083 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|        26 | 3084 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|        16 | 3085 | `							ph7_class *pIface = apIface[i];` |
|        16 | 3086 | `							ph7_class *pDeepest = 0;` |
|        28 | 3087 | `							while( pIface ){` |
|        16 | 3088 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|        16 | 3089 | `									pDeepest = pIface;` |
|         6 | 3090 | `								}` |
|        16 | 3091 | `								pIface = pIface->pBase;` |
|         4 | 3092 | `							}` |
|        16 | 3093 | `							if( pDeepest ){` |
|        16 | 3094 | `								pOrigin = pDeepest;` |
|        16 | 3095 | `								break;` |
|         - | 3096 | `							}` |
|       ! 0 | 3097 | `						}` |
|        26 | 3098 | `						pWalk = pWalk->pBase;` |
|         4 | 3099 | `					}` |
|         7 | 3100 | `				}` |
|         - | 3101 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|        20 | 3102 | `				if( !pOrigin ){` |
|         3 | 3103 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|         3 | 3104 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|         3 | 3105 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|         3 | 3106 | `							pOrigin = pClass;` |
|         3 | 3107 | `							break;` |
|         - | 3108 | `						}` |
|       ! 0 | 3109 | `					}` |
|         1 | 3110 | `				}` |
|         - | 3111 | `			}` |
|        20 | 3112 | `			if( pOrigin ){` |
|        20 | 3113 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|        12 | 3114 | `			}else{` |
|         - | 3115 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|       ! 0 | 3116 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|         - | 3117 | `			}` |
|        20 | 3118 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|        20 | 3119 | `			nListed++;` |
|         4 | 3120 | `		}` |
|         - | 3121 | `	}` |
|        18 | 3122 | `	SyBlobAppend(&sMsg,")",1);` |
|        25 | 3123 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|        14 | 3124 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|        18 | 3125 | `	SyBlobRelease(&sMsg);` |
|        18 | 3126 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 3127 | `		return SXERR_ABORT;` |
|         - | 3128 | `	}` |
|        18 | 3129 | `	return SXRET_OK;` |
|    259447 | 3130 | `}` |
|         - | 3131 | `/*` |
|         - | 3132 | ` * Parse a class/interface name reference from the current token stream.` |
|         - | 3133 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|         - | 3134 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|         - | 3135 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|         - | 3136 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|         - | 3137 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|         - | 3138 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|         - | 3139 | ` */` |
|   1163474 | 3140 | `PH7_PRIVATE sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|         5 | 3141 | `{` |
|   1163479 | 3142 | `	int isAbsolute = 0;` |
|   1163479 | 3143 | `	SyToken *pStart = pGen->pIn;` |
|         - | 3144 | `	SyBlob sName;` |
|   1163479 | 3145 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      9919 | 3146 | `		isAbsolute = 1;` |
|      9919 | 3147 | `		pGen->pIn++;` |
|      4957 | 3148 | `	}` |
|   1163479 | 3149 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|         - | 3150 | ``	/* `namespace\X` names the CURRENT namespace and is fully qualified from there. */`` |
|   1163479 | 3151 | `	if( !isAbsolute && GenStateNsRelPrefix(pGen,&pGen->pIn,pGen->pEnd,&sName) ){` |
|        17 | 3152 | `		isAbsolute = 1;` |
|         8 | 3153 | `	}` |
|   1163479 | 3154 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        11 | 3155 | `		SyBlobRelease(&sName);` |
|        11 | 3156 | `		pGen->pIn = pStart;` |
|        11 | 3157 | `		return SXERR_INVALID;` |
|         - | 3158 | `	}` |
|   1163471 | 3159 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   1163471 | 3160 | `	pGen->pIn++;` |
|   1745361 | 3161 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    581900 | 3162 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       113 | 3163 | `		SyBlobAppend(&sName,"\\",1);` |
|       113 | 3164 | `		pGen->pIn++;` |
|       113 | 3165 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       113 | 3166 | `		pGen->pIn++;` |
|         5 | 3167 | `	}` |
|   1163471 | 3168 | `	if( isAbsolute ){` |
|      9931 | 3169 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      4968 | 3170 | `	}else{` |
|         - | 3171 | `		SyString sRaw;` |
|   1153545 | 3172 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   1153545 | 3173 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|         - | 3174 | `	}` |
|   1163471 | 3175 | `	SyBlobRelease(&sName);` |
|   1163471 | 3176 | `	return SXRET_OK;` |
|    581742 | 3177 | `}` |
|         - | 3178 | `/*` |
|         - | 3179 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|         - | 3180 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|         - | 3181 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|         - | 3182 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|         - | 3183 | ` * either direction cannot run unbounded.` |
|         - | 3184 | ` */` |
|         - | 3185 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    226638 | 3186 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|         5 | 3187 | `{` |
|         - | 3188 | `	ph7_class **apParent;` |
|         - | 3189 | `	sxu32 n;` |
|    598219 | 3190 | `	while( pInterface ){` |
|    380647 | 3191 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|       ! 0 | 3192 | `			return FALSE;` |
|         - | 3193 | `		}` |
|    425948 | 3194 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|     90602 | 3195 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|      9071 | 3196 | `			return TRUE;` |
|         - | 3197 | `		}` |
|    371581 | 3198 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    371583 | 3199 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|         3 | 3200 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|       ! 0 | 3201 | `				return TRUE;` |
|         - | 3202 | `			}` |
|         2 | 3203 | `		}` |
|    371581 | 3204 | `		pInterface = pInterface->pBase;` |
|    371581 | 3205 | `		iDepth++;` |
|         5 | 3206 | `	}` |
|    217577 | 3207 | `	return FALSE;` |
|    113324 | 3208 | `}` |
|    226636 | 3209 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|         5 | 3210 | `{` |
|    226641 | 3211 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|         5 | 3212 | `}` |
|         - | 3213 | `/*` |
|         - | 3214 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|         - | 3215 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|         - | 3216 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|         - | 3217 | ` */` |
|      9066 | 3218 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|         5 | 3219 | `{` |
|      9075 | 3220 | `	while( pBase ){` |
|        10 | 3221 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|         2 | 3222 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|         3 | 3223 | `			return TRUE;` |
|         - | 3224 | `		}` |
|        10 | 3225 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|         6 | 3226 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|         3 | 3227 | `			return TRUE;` |
|         - | 3228 | `		}` |
|         5 | 3229 | `		pBase = pBase->pBase;` |
|         1 | 3230 | `	}` |
|      9067 | 3231 | `	return FALSE;` |
|      4538 | 3232 | `}` |
|         - | 3233 | `/*` |
|         - | 3234 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|         - | 3235 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|         - | 3236 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|         - | 3237 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|         - | 3238 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|         - | 3239 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|         - | 3240 | ` * pClass->aEnumCases for cases().` |
|         - | 3241 | ` */` |
|      9136 | 3242 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 3243 | `{` |
|      9141 | 3244 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 3245 | `	SySet *pInstrContainer;` |
|         - | 3246 | `	ph7_class_attr *pCase;` |
|         - | 3247 | `	SyString *pName;` |
|         - | 3248 | `	sxi32 rc;` |
|      9141 | 3249 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|      9141 | 3250 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 3251 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 3252 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|       ! 0 | 3253 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 3254 | `			return SXERR_ABORT;` |
|         - | 3255 | `		}` |
|       ! 0 | 3256 | `		goto Synchronize;` |
|         - | 3257 | `	}` |
|      9141 | 3258 | `	pName = &pGen->pIn->sData;` |
|         - | 3259 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|      9141 | 3260 | `	if( SyHashGet(&pClass->hConst,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       ! 0 | 3261 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 3262 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|       ! 0 | 3263 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 3264 | `			return SXERR_ABORT;` |
|         - | 3265 | `		}` |
|       ! 0 | 3266 | `		goto Synchronize;` |
|         - | 3267 | `	}` |
|      9141 | 3268 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 3269 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|      9141 | 3270 | `	if( pCase == 0 ){` |
|       ! 0 | 3271 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 3272 | `		return SXERR_ABORT;` |
|         - | 3273 | `	}` |
|      9141 | 3274 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|      9141 | 3275 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 3276 | `		return SXERR_ABORT;` |
|         - | 3277 | `	}` |
|      9141 | 3278 | `	pGen->pIn++; /* Jump the case name */` |
|      9141 | 3279 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|      9119 | 3280 | `		if( pClass->nEnumBacking == 0 ){` |
|         8 | 3281 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         2 | 3282 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|         6 | 3283 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 3284 | `				return SXERR_ABORT;` |
|         - | 3285 | `			}` |
|         6 | 3286 | `			goto Synchronize;` |
|         - | 3287 | `		}` |
|      9115 | 3288 | `		pGen->pIn++; /* Jump the equal sign */` |
|         - | 3289 | `		/* Compile the backing value expression into the case's own container` |
|         - | 3290 | `		 * (same technique as class constants). */` |
|      9115 | 3291 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      9115 | 3292 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|      9115 | 3293 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      9115 | 3294 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 | 3295 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 3296 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|       ! 0 | 3297 | `		}` |
|      9115 | 3298 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      9115 | 3299 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      9115 | 3300 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 3301 | `			return SXERR_ABORT;` |
|         - | 3302 | `		}` |
|      4560 | 3303 | `	}else{` |
|        27 | 3304 | `		if( pClass->nEnumBacking != 0 ){` |
|       ! 0 | 3305 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 3306 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|       ! 0 | 3307 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 3308 | `				return SXERR_ABORT;` |
|         - | 3309 | `			}` |
|       ! 0 | 3310 | `			goto Synchronize;` |
|         - | 3311 | `		}` |
|         - | 3312 | `	}` |
|      9137 | 3313 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|      9137 | 3314 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 3315 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 3316 | `		return SXERR_ABORT;` |
|         - | 3317 | `	}` |
|      9137 | 3318 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|      9137 | 3319 | `	return SXRET_OK;` |
|         2 | 3320 | `Synchronize:` |
|         - | 3321 | `	/* Synchronize with the first semi-colon */` |
|        14 | 3322 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|        10 | 3323 | `		pGen->pIn++;` |
|         2 | 3324 | `	}` |
|         6 | 3325 | `	return SXERR_CORRUPT;` |
|      4573 | 3326 | `}` |
|         - | 3327 | `/*` |
|         - | 3328 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|         - | 3329 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|         - | 3330 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|         - | 3331 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|         - | 3332 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|         - | 3333 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|         - | 3334 | ` * pointers into it (see the constructor-promotion precedent above).` |
|         - | 3335 | ` */` |
|      4584 | 3336 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 3337 | `{` |
|         - | 3338 | `	SyToken *pSaveIn,*pSaveEnd;` |
|         - | 3339 | `	const char *zBack;` |
|         - | 3340 | `	SySet sToken;` |
|         - | 3341 | `	char *zSrc;` |
|         - | 3342 | `	sxu32 nSrc,nMax;` |
|      4589 | 3343 | `	sxi32 rc = SXRET_OK;` |
|      4589 | 3344 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|      4584 | 3345 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|      4589 | 3346 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|      4589 | 3347 | `	if( zSrc == 0 ){` |
|       ! 0 | 3348 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 3349 | `		return SXERR_ABORT;` |
|         - | 3350 | `	}` |
|      4589 | 3351 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|      4589 | 3352 | `	if( pClass->nEnumBacking != 0 ){` |
|      6839 | 3353 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         - | 3354 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|         - | 3355 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|         - | 3356 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|      2278 | 3357 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|      2283 | 3358 | `	}else{` |
|        47 | 3359 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        14 | 3360 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|         - | 3361 | `	}` |
|      4589 | 3362 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      4589 | 3363 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|      4589 | 3364 | `	pSaveIn = pGen->pIn;` |
|      4589 | 3365 | `	pSaveEnd = pGen->pEnd;` |
|      4589 | 3366 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      4589 | 3367 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|     18285 | 3368 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|     13701 | 3369 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|         5 | 3370 | `	}` |
|      4589 | 3371 | `	pGen->pIn = pSaveIn;` |
|      4589 | 3372 | `	pGen->pEnd = pSaveEnd;` |
|      4589 | 3373 | `	SySetRelease(&sToken);` |
|      4589 | 3374 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|      2297 | 3375 | `}` |
|         - | 3376 | `/*` |
|         - | 3377 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|         - | 3378 | ` * __call/__callStatic/__invoke stay allowed).` |
|         - | 3379 | ` */` |
|         - | 3380 | `static const char *azEnumBannedMagic[] = {` |
|         - | 3381 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|         - | 3382 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|         - | 3383 | `};` |
|         - | 3384 | `/*` |
|         - | 3385 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|         - | 3386 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|         - | 3387 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|         - | 3388 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|         - | 3389 | ` * and before the class is installed.` |
|         - | 3390 | ` */` |
|      4584 | 3391 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|         5 | 3392 | `{` |
|         - | 3393 | `	SyHashEntry *pEntry;` |
|         - | 3394 | `	sxi32 rc;` |
|         - | 3395 | `	sxu32 n;` |
|         - | 3396 | `	/* php: "Enum %s cannot include properties" */` |
|      4589 | 3397 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|      4589 | 3398 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|         3 | 3399 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|         3 | 3400 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|         3 | 3401 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|         1 | 3402 | `				"Enum %z cannot include properties",&pClass->sName);` |
|         3 | 3403 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 3404 | `				return SXERR_ABORT;` |
|         - | 3405 | `			}` |
|         3 | 3406 | `			break;` |
|         - | 3407 | `		}` |
|       ! 0 | 3408 | `	}` |
|         - | 3409 | `	/* php: "Enum %s cannot include magic method %s" */` |
|     64181 | 3410 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|     89388 | 3411 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|     59597 | 3412 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|       ! 0 | 3413 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 3414 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|       ! 0 | 3415 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 3416 | `				return SXERR_ABORT;` |
|         - | 3417 | `			}` |
|       ! 0 | 3418 | `		}` |
|     29801 | 3419 | `	}` |
|         - | 3420 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|         - | 3421 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|         - | 3422 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|         - | 3423 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|         - | 3424 | `	{` |
|         - | 3425 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|         - | 3426 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|         - | 3427 | `		ph7_class_attr *pAttr;` |
|      4589 | 3428 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 3429 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      4589 | 3430 | `		if( pAttr == 0 ){` |
|       ! 0 | 3431 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 3432 | `			return SXERR_ABORT;` |
|         - | 3433 | `		}` |
|      4589 | 3434 | `		pAttr->nType = MEMOBJ_STRING;` |
|      4589 | 3435 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|      4589 | 3436 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|      4589 | 3437 | `		if( pClass->nEnumBacking != 0 ){` |
|      4561 | 3438 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 3439 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      4561 | 3440 | `			if( pAttr == 0 ){` |
|       ! 0 | 3441 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 3442 | `				return SXERR_ABORT;` |
|         - | 3443 | `			}` |
|      4561 | 3444 | `			pAttr->nType = pClass->nEnumBacking;` |
|      4561 | 3445 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        10 | 3446 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|         6 | 3447 | `			}else{` |
|      4553 | 3448 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|         - | 3449 | `			}` |
|      4561 | 3450 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|      2278 | 3451 | `		}` |
|         - | 3452 | `	}` |
|      4589 | 3453 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|      2297 | 3454 | `}` |
|         - | 3455 | `/*` |
|         - | 3456 | ` * Deferred class declarations (class/anonymous-class extending an` |
|         - | 3457 | ` * autoloaded parent).` |
|         - | 3458 | ` *` |
|         - | 3459 | ` * A class declaration compiles INLINE while its enclosing file compiles, so a` |
|         - | 3460 | ` * parent/interface/trait that an autoloader would provide is unreachable when` |
|         - | 3461 | ` * the autoloader's own spl_autoload_register() statement has not EXECUTED yet` |
|         - | 3462 | `` * (same-file registration, or `new class extends \App\Child {}` anywhere).`` |
|         - | 3463 | ` * php's model has no such problem: a declaration with unresolved dependencies` |
|         - | 3464 | ` * is declared at its EXECUTION point, in statement order, not hoisted.` |
|         - | 3465 | ` *` |
|         - | 3466 | ` * These helpers reproduce that: before compiling a declaration, scan its` |
|         - | 3467 | `` * header (extends/implements) and body (depth-1 trait `use`) for referenced`` |
|         - | 3468 | ` * names and try to resolve each (firing autoload exactly where the normal` |
|         - | 3469 | ` * compile would). If any name is still missing, the WHOLE declaration is` |
|         - | 3470 | `` * captured as re-compilable source — a reconstructed `namespace`/`use`-import/`` |
|         - | 3471 | ` * doc/attribute/modifier prefix plus the declaration's raw text — recorded in` |
|         - | 3472 | ` * a VmDeferredClass, and OP_CLASS_DEFER is emitted at the declaration site.` |
|         - | 3473 | ` * At runtime (VmExecDeferredClass, vm_include.c) the autoloader is live: each` |
|         - | 3474 | `` * recorded name resolves or throws php's catchable `... not found` Error, and`` |
|         - | 3475 | ` * the chunk re-compiles through VmEvalChunk. An anonymous class re-compiles` |
|         - | 3476 | `` * inside `if (false) { new ... }` (installing the class without instantiating`` |
|         - | 3477 | ` * it) under its original synthesized name via pVm->sDeferAnonName; the site's` |
|         - | 3478 | ` * own OP_NEW then instantiates it with the site-compiled arguments.` |
|         - | 3479 | ` *` |
|         - | 3480 | ` * Behavior shifts only for declarations that previously died with the` |
|         - | 3481 | ` * compile-time "Nonexistent base class" fatal: they now follow php — succeed` |
|         - | 3482 | ` * when the autoloader is registered first, or throw php's catchable` |
|         - | 3483 | `` * `Class/Interface/Trait "X" not found` Error at the declaration point.`` |
|         - | 3484 | ` * A deferred declaration's OTHER compile errors (a body syntax error) shift` |
|         - | 3485 | ` * from file-compile time to the declaration's execution — still loud, timing` |
|         - | 3486 | ` * differs from php (recorded).` |
|         - | 3487 | ` */` |
|        84 | 3488 | `static void GenStateDeferEmitUses(SyBlob *pOut,SyHash *pTable,const char *zKind)` |
|         2 | 3489 | `{` |
|         - | 3490 | `	SyHashEntry *pEntry;` |
|        86 | 3491 | `	SyHashResetLoopCursor(pTable);` |
|       128 | 3492 | `	while( (pEntry = SyHashGetNextEntry(pTable)) != 0 ){` |
|       ! 0 | 3493 | `		const char *zFqn = (const char *)pEntry->pUserData;` |
|       ! 0 | 3494 | `		if( zFqn ){` |
|       ! 0 | 3495 | `			SyBlobFormat(pOut,"use %s%s as %.*s;\n",zKind,zFqn,` |
|       ! 0 | 3496 | `				(int)pEntry->nKeyLen,(const char *)pEntry->pKey);` |
|       ! 0 | 3497 | `		}` |
|       ! 0 | 3498 | `	}` |
|        86 | 3499 | `}` |
|         - | 3500 | `/*` |
|         - | 3501 | ` * Parse one class reference at *ppCur (bounded by pEnd) with the SAME` |
|         - | 3502 | ` * namespace/import resolution the real compile uses, and append it to pNames.` |
|         - | 3503 | ` * Advances *ppCur past the reference. Returns SXERR_INVALID on a malformed` |
|         - | 3504 | ` * reference (caller bails out of deferral and lets the normal path report).` |
|         - | 3505 | ` */` |
|    566856 | 3506 | `static sxi32 GenStateDeferRecordRef(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd,` |
|         - | 3507 | `	sxu8 cKind,SySet *pNames)` |
|         5 | 3508 | `{` |
|    566861 | 3509 | `	SyToken *pSavedIn = pGen->pIn;` |
|    566861 | 3510 | `	SyToken *pSavedEnd = pGen->pEnd;` |
|         - | 3511 | `	SyBlob sFqn;` |
|         - | 3512 | `	VmDeferredReq sReq;` |
|         - | 3513 | `	char *zDup;` |
|         - | 3514 | `	sxi32 rc;` |
|    566861 | 3515 | `	SyBlobInit(&sFqn,&pGen->pVm->sAllocator);` |
|    566861 | 3516 | `	pGen->pIn = *ppCur;` |
|    566861 | 3517 | `	pGen->pEnd = pEnd;` |
|    566861 | 3518 | `	rc = GenStateParseClassReference(pGen,&sFqn);` |
|    566861 | 3519 | `	*ppCur = pGen->pIn;` |
|    566861 | 3520 | `	pGen->pIn = pSavedIn;` |
|    566861 | 3521 | `	pGen->pEnd = pSavedEnd;` |
|    566861 | 3522 | `	if( rc != SXRET_OK \|\| SyBlobLength(&sFqn) < 1 ){` |
|         3 | 3523 | `		SyBlobRelease(&sFqn);` |
|         3 | 3524 | `		return SXERR_INVALID;` |
|         - | 3525 | `	}` |
|    850286 | 3526 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    566854 | 3527 | `		(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));` |
|    566859 | 3528 | `	if( zDup == 0 ){` |
|       ! 0 | 3529 | `		SyBlobRelease(&sFqn);` |
|       ! 0 | 3530 | `		return SXERR_INVALID;` |
|         - | 3531 | `	}` |
|    566859 | 3532 | `	SyStringInitFromBuf(&sReq.sName,zDup,SyBlobLength(&sFqn));` |
|    566859 | 3533 | `	sReq.cKind = cKind;` |
|    566859 | 3534 | `	SySetPut(pNames,(const void *)&sReq);` |
|    566859 | 3535 | `	SyBlobRelease(&sFqn);` |
|    566859 | 3536 | `	return SXRET_OK;` |
|    283433 | 3537 | `}` |
|         - | 3538 | `/*` |
|         - | 3539 | ` * Scan the declaration whose keyword pGen->pIn sits on (class/enum/interface/` |
|         - | 3540 | `` * trait, or an anonymous `class(args)`) WITHOUT consuming tokens. Collects`` |
|         - | 3541 | ` * every referenced dependency name, locates the body braces, and filters the` |
|         - | 3542 | ` * collected names down to the UNRESOLVABLE ones (each lookup fires autoload,` |
|         - | 3543 | ` * exactly like the compile it replaces). SXRET_OK with an empty pMissing set` |
|         - | 3544 | ` * means "compile normally"; a non-empty set means "defer". Any structural` |
|         - | 3545 | ` * surprise returns SXERR_INVALID so the normal compile reports it.` |
|         - | 3546 | ` */` |
|    609852 | 3547 | `static sxi32 GenStateScanDeferDeps(ph7_gen_state *pGen,int bAnon,int iSelfKind,` |
|         - | 3548 | `	SySet *pMissing,SyToken **ppBody,SyToken **ppBodyEnd,SyBlob *pSelfFqn)` |
|         5 | 3549 | `{` |
|    609857 | 3550 | `	SyToken *pCur = pGen->pIn; /* on the declaration keyword */` |
|    609857 | 3551 | `	SyToken *pEnd = pGen->pEnd;` |
|         - | 3552 | `	SySet aNames;` |
|    609857 | 3553 | `	sxi32 rc = SXRET_OK;` |
|    609857 | 3554 | `	*ppBody = *ppBodyEnd = 0;` |
|    609857 | 3555 | `	SySetInit(&aNames,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|    609857 | 3556 | `	pCur++; /* Jump the keyword */` |
|    609857 | 3557 | `	if( bAnon ){` |
|        61 | 3558 | `		if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|        10 | 3559 | `			SyToken *pClose = 0;` |
|        10 | 3560 | `			pCur++;` |
|        10 | 3561 | `			PH7_DelimitNestedTokens(pCur,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|        10 | 3562 | `			if( pClose == 0 \|\| pClose >= pEnd ){` |
|       ! 0 | 3563 | `				SySetRelease(&aNames);` |
|       ! 0 | 3564 | `				return SXERR_INVALID;` |
|         - | 3565 | `			}` |
|        10 | 3566 | `			pCur = &pClose[1];` |
|         4 | 3567 | `		}` |
|        33 | 3568 | `	}else{` |
|    609801 | 3569 | `		if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 3570 | `			SySetRelease(&aNames);` |
|       ! 0 | 3571 | `			return SXERR_INVALID;` |
|         - | 3572 | `		}` |
|    609801 | 3573 | `		GenStateBuildFQN(pGen,&pCur->sData,pSelfFqn);` |
|    609801 | 3574 | `		pCur++;` |
|         - | 3575 | `	}` |
|         - | 3576 | ``	/* Header: extends/implements lists up to the '{' (an enum's `: int` backing`` |
|         - | 3577 | `	 * and any stray tokens pass through; malformed headers bail to the normal` |
|         - | 3578 | `	 * path's diagnostics). */` |
|   1095077 | 3579 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_OCB) == 0 ){` |
|    485227 | 3580 | `		int iKind = -1;` |
|    485227 | 3581 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    480667 | 3582 | `			sxi32 nKw = SX_PTR_TO_INT(pCur->pUserData);` |
|    480667 | 3583 | `			if( nKw == PH7_TKWRD_EXTENDS ){` |
|    321927 | 3584 | `				iKind = (iSelfKind == PH7_DEFER_KIND_INTERFACE)` |
|    160961 | 3585 | `					? PH7_DEFER_KIND_INTERFACE : PH7_DEFER_KIND_CLASS;` |
|    319706 | 3586 | `			}else if( nKw == PH7_TKWRD_IMPLEMENTS ){` |
|    154185 | 3587 | `				iKind = PH7_DEFER_KIND_INTERFACE;` |
|     77090 | 3588 | `			}` |
|    240331 | 3589 | `		}` |
|    485227 | 3590 | `		if( iKind < 0 ){` |
|      9125 | 3591 | `			pCur++;` |
|      9125 | 3592 | `			continue;` |
|         - | 3593 | `		}` |
|    476107 | 3594 | `		pCur++; /* Jump extends/implements */` |
|    238051 | 3595 | `		for(;;){` |
|    548571 | 3596 | `			if( GenStateDeferRecordRef(pGen,&pCur,pEnd,(sxu8)iKind,&aNames) != SXRET_OK ){` |
|         3 | 3597 | `				SySetRelease(&aNames);` |
|         3 | 3598 | `				return SXERR_INVALID;` |
|         - | 3599 | `			}` |
|    548569 | 3600 | `			if( pCur < pEnd && (pCur->nType & PH7_TK_COMMA) ){` |
|     72469 | 3601 | `				pCur++;` |
|     72469 | 3602 | `				continue;` |
|         - | 3603 | `			}` |
|    476105 | 3604 | `			break;` |
|       ! 0 | 3605 | `		}` |
|         5 | 3606 | `	}` |
|    609855 | 3607 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 | 3608 | `		SySetRelease(&aNames);` |
|       ! 0 | 3609 | `		return SXERR_INVALID;` |
|         - | 3610 | `	}` |
|    609855 | 3611 | `	*ppBody = pCur;` |
|         - | 3612 | `	{` |
|    609855 | 3613 | `		SyToken *pClose = 0;` |
|    609855 | 3614 | `		PH7_DelimitNestedTokens(&pCur[1],pEnd,PH7_TK_OCB,PH7_TK_CCB,&pClose);` |
|    609855 | 3615 | `		if( pClose == 0 \|\| pClose >= pEnd ){` |
|       ! 0 | 3616 | `			SySetRelease(&aNames);` |
|       ! 0 | 3617 | `			return SXERR_INVALID;` |
|         - | 3618 | `		}` |
|    609855 | 3619 | `		*ppBodyEnd = pClose;` |
|         - | 3620 | `	}` |
|         - | 3621 | ``	/* Body: depth-1 trait `use Name[, Name]` statements. Statement position only`` |
|         - | 3622 | ``	 * (previous token one of '{' '}' ';'), so a closure's `use ($x)` — which`` |
|         - | 3623 | `	 * follows a ')' — never matches. */` |
|         - | 3624 | `	{` |
|    609855 | 3625 | `		SyToken *p = &(*ppBody)[1];` |
|    609855 | 3626 | `		int bStmtPos = 1;` |
|    609855 | 3627 | `		sxi32 iDepth = 1;` |
| 143316303 | 3628 | `		while( p < *ppBodyEnd ){` |
| 142706453 | 3629 | `			if( p->nType & PH7_TK_OCB ){` |
|   5382077 | 3630 | `				iDepth++;` |
|   5382077 | 3631 | `				bStmtPos = 1;` |
|   5382077 | 3632 | `				p++;` |
|   5382077 | 3633 | `				continue;` |
|         - | 3634 | `			}` |
| 137324381 | 3635 | `			if( p->nType & PH7_TK_CCB ){` |
|   5382077 | 3636 | `				iDepth--;` |
|   5382077 | 3637 | `				bStmtPos = 1;` |
|   5382077 | 3638 | `				p++;` |
|   5382077 | 3639 | `				continue;` |
|         - | 3640 | `			}` |
| 131942309 | 3641 | `			if( p->nType & PH7_TK_SEMI ){` |
|   9232833 | 3642 | `				bStmtPos = 1;` |
|   9232833 | 3643 | `				p++;` |
|   9232833 | 3644 | `				continue;` |
|         - | 3645 | `			}` |
| 122709476 | 3646 | `			if( iDepth == 1 && bStmtPos && (p->nType & PH7_TK_KEYWORD)` |
|   4369538 | 3647 | `			 && SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_USE ){` |
|     18281 | 3648 | `				p++;` |
|      9138 | 3649 | `				for(;;){` |
|     18295 | 3650 | `					if( GenStateDeferRecordRef(pGen,&p,*ppBodyEnd,PH7_DEFER_KIND_TRAIT,&aNames) != SXRET_OK ){` |
|       ! 0 | 3651 | `						SySetRelease(&aNames);` |
|       ! 0 | 3652 | `						return SXERR_INVALID;` |
|         - | 3653 | `					}` |
|     18295 | 3654 | `					if( p < *ppBodyEnd && (p->nType & PH7_TK_COMMA) ){` |
|        17 | 3655 | `						p++;` |
|        17 | 3656 | `						continue;` |
|         - | 3657 | `					}` |
|     18281 | 3658 | `					break;` |
|       ! 0 | 3659 | `				}` |
|     18281 | 3660 | `				continue;` |
|         - | 3661 | `			}` |
| 122691205 | 3662 | `			bStmtPos = 0;` |
| 122691205 | 3663 | `			p++;` |
|         5 | 3664 | `		}` |
|         - | 3665 | `	}` |
|         - | 3666 | `	/* Filter: keep only the names that do NOT resolve. The lookup fires the` |
|         - | 3667 | `	 * autoloader exactly where the replaced compile would. */` |
|         - | 3668 | `	{` |
|    609855 | 3669 | `		VmDeferredReq *aReq = (VmDeferredReq *)SySetBasePtr(&aNames);` |
|         - | 3670 | `		sxu32 n;` |
|   1176709 | 3671 | `		for( n = 0 ; n < SySetUsed(&aNames) ; ++n ){` |
|    566859 | 3672 | `			if( PH7_VmExtractClass(pGen->pVm,aReq[n].sName.zString,aReq[n].sName.nByte,FALSE,0) == 0 ){` |
|        34 | 3673 | `				SySetPut(pMissing,(const void *)&aReq[n]);` |
|        16 | 3674 | `			}` |
|    283432 | 3675 | `		}` |
|         - | 3676 | `	}` |
|    609855 | 3677 | `	SySetRelease(&aNames);` |
|    609855 | 3678 | `	return rc;` |
|    304931 | 3679 | `}` |
|         - | 3680 | `/*` |
|         - | 3681 | ` * Capture the declaration as a re-compilable chunk, record it, and emit` |
|         - | 3682 | ` * OP_CLASS_DEFER at the current emission point. On return the statement` |
|         - | 3683 | ` * cursor sits past the declaration's closing '}'. pMissing's entries are` |
|         - | 3684 | ` * COPIED into the record (their name bytes are already allocator-owned).` |
|         - | 3685 | ` */` |
|        28 | 3686 | `static sxi32 GenStateEmitDeferredClass(ph7_gen_state *pGen,sxi32 iFlags,int bAnon,` |
|         - | 3687 | `	SySet *pMissing,SyToken *pBodyEnd,SyBlob *pSelfFqn,const SyString *pAnonName)` |
|         2 | 3688 | `{` |
|        30 | 3689 | `	SyToken *pKw = pGen->pIn; /* the declaration keyword */` |
|         - | 3690 | `	VmDeferredClass *pDefer;` |
|         - | 3691 | `	SyBlob sChunk;` |
|         - | 3692 | `	const char *zFrom;` |
|         - | 3693 | `	const char *zTo;` |
|         - | 3694 | `	char *zDup;` |
|        30 | 3695 | `	SyBlobInit(&sChunk,&pGen->pVm->sAllocator);` |
|         - | 3696 | `	/* The declaration site's compile context is replayed as literal statements:` |
|         - | 3697 | `	 * strict_types first (it must open the chunk), then namespace and the` |
|         - | 3698 | `	 * use-import tables — the runtime re-compile starts in a fresh scope. */` |
|        30 | 3699 | `	if( pGen->bStrictTypes ){` |
|       ! 0 | 3700 | `		SyBlobAppend(&sChunk,"declare(strict_types=1);\n",sizeof("declare(strict_types=1);\n")-1);` |
|       ! 0 | 3701 | `	}` |
|        30 | 3702 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       ! 0 | 3703 | `		SyBlobFormat(&sChunk,"namespace %.*s;\n",` |
|       ! 0 | 3704 | `			(int)SyBlobLength(&pGen->sNamespace),(const char *)SyBlobData(&pGen->sNamespace));` |
|       ! 0 | 3705 | `	}` |
|        30 | 3706 | `	GenStateDeferEmitUses(&sChunk,&pGen->hUseImports,"");` |
|        30 | 3707 | `	GenStateDeferEmitUses(&sChunk,&pGen->hUseFuncImports,"function ");` |
|        30 | 3708 | `	GenStateDeferEmitUses(&sChunk,&pGen->hUseConstImports,"const ");` |
|         - | 3709 | `	/* Doc-comment and attribute groups precede the keyword in the raw source,` |
|         - | 3710 | `	 * outside the captured span — re-emit them from the trivia sidecar. */` |
|        30 | 3711 | `	if( !bAnon && pGen->sPendingDoc.nByte > 0 ){` |
|       ! 0 | 3712 | `		SyBlobAppend(&sChunk,pGen->sPendingDoc.zString,pGen->sPendingDoc.nByte);` |
|       ! 0 | 3713 | `		SyBlobAppend(&sChunk,"\n",1);` |
|       ! 0 | 3714 | `	}` |
|         - | 3715 | `	{` |
|         - | 3716 | `		ph7_trivia *aT;` |
|         - | 3717 | `		sxu32 nT,n;` |
|        30 | 3718 | `		if( bAnon ){` |
|         - | 3719 | ``			/* `new #[A] class` trivia is keyed to the 'class' token */`` |
|         8 | 3720 | `			SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|         8 | 3721 | `			aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|         8 | 3722 | `			nT = SySetUsed(&pGen->aTrivia);` |
|         8 | 3723 | `			if( pGen->pTokenSet && pKw >= pBase && pKw < &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         8 | 3724 | `				sxu32 nIdx = (sxu32)(pKw - pBase);` |
|         8 | 3725 | `				for( n = 0 ; n < nT ; ++n ){` |
|       ! 0 | 3726 | `					if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|       ! 0 | 3727 | `						SyBlobFormat(&sChunk,"#[%.*s]\n",(int)aT[n].sText.nByte,aT[n].sText.zString);` |
|       ! 0 | 3728 | `					}` |
|       ! 0 | 3729 | `				}` |
|         3 | 3730 | `			}` |
|         5 | 3731 | `		}else{` |
|        24 | 3732 | `			aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|        24 | 3733 | `			nT = SySetUsed(&pGen->aPendingAttrs);` |
|        24 | 3734 | `			for( n = 0 ; n < nT ; ++n ){` |
|       ! 0 | 3735 | `				if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|       ! 0 | 3736 | `					SyBlobFormat(&sChunk,"#[%.*s]\n",(int)aT[n].sText.nByte,aT[n].sText.zString);` |
|       ! 0 | 3737 | `				}` |
|       ! 0 | 3738 | `			}` |
|         - | 3739 | `		}` |
|         - | 3740 | `	}` |
|         - | 3741 | `	/* Pad the prefix with newlines so the declaration keyword sits on its` |
|         - | 3742 | `	 * ORIGINAL line inside the chunk — runtime diagnostics from the deferred` |
|         - | 3743 | `	 * compile then report the source's real line. Best-effort: a prefix` |
|         - | 3744 | `	 * already longer than the declaration line skips the padding. */` |
|         - | 3745 | `	{` |
|        30 | 3746 | `		const char *zScan = (const char *)SyBlobData(&sChunk);` |
|        30 | 3747 | `		sxu32 nHave = 0;` |
|         - | 3748 | `		sxu32 nScan;` |
|        30 | 3749 | `		for( nScan = 0 ; nScan < SyBlobLength(&sChunk) ; ++nScan ){` |
|       ! 0 | 3750 | `			if( zScan[nScan] == '\n' ){` |
|       ! 0 | 3751 | `				nHave++;` |
|       ! 0 | 3752 | `			}` |
|       ! 0 | 3753 | `		}` |
|       304 | 3754 | `		while( nHave + 1 < pKw->nLine ){` |
|       276 | 3755 | `			SyBlobAppend(&sChunk,"\n",1);` |
|       276 | 3756 | `			nHave++;` |
|         2 | 3757 | `		}` |
|         - | 3758 | `	}` |
|        30 | 3759 | `	if( bAnon ){` |
|         - | 3760 | ``		/* `if (false) { new class <header-minus-args> { body } ; }` — installs`` |
|         - | 3761 | `		 * the class at the chunk's compile, never instantiates it. */` |
|         8 | 3762 | `		SyToken *pAfterArgs = &pKw[1];` |
|         8 | 3763 | `		SyBlobAppend(&sChunk,"if (false) { new ",sizeof("if (false) { new ")-1);` |
|         8 | 3764 | `		SyBlobAppend(&sChunk,pKw->sData.zString,pKw->sData.nByte);` |
|         8 | 3765 | `		if( pAfterArgs < pGen->pEnd && (pAfterArgs->nType & PH7_TK_LPAREN) ){` |
|         3 | 3766 | `			SyToken *pClose = 0;` |
|         3 | 3767 | `			PH7_DelimitNestedTokens(&pAfterArgs[1],pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|         3 | 3768 | `			if( pClose == 0 \|\| pClose >= pGen->pEnd ){` |
|       ! 0 | 3769 | `				SyBlobRelease(&sChunk);` |
|       ! 0 | 3770 | `				return SXERR_INVALID;` |
|         - | 3771 | `			}` |
|         3 | 3772 | `			pAfterArgs = &pClose[1];` |
|         1 | 3773 | `		}` |
|         8 | 3774 | `		if( pAfterArgs < pBodyEnd ){` |
|         8 | 3775 | `			SyBlobAppend(&sChunk," ",1);` |
|         8 | 3776 | `			zFrom = pAfterArgs->sData.zString;` |
|         8 | 3777 | `			zTo = pBodyEnd->sData.zString + pBodyEnd->sData.nByte;` |
|         8 | 3778 | `			SyBlobAppend(&sChunk,zFrom,(sxu32)(zTo - zFrom));` |
|         3 | 3779 | `		}` |
|         8 | 3780 | `		SyBlobAppend(&sChunk,"; }",sizeof("; }")-1);` |
|         5 | 3781 | `	}else{` |
|         - | 3782 | `		/* Modifiers were consumed before this compiler ran; reconstruct them` |
|         - | 3783 | ``		 * (an enum's implicit `final` must NOT be spelled out). */`` |
|        22 | 3784 | `		if( (iFlags & PH7_CLASS_ENUM) == 0` |
|        21 | 3785 | `		 && (pKw->nType & PH7_TK_KEYWORD)` |
|        22 | 3786 | `		 && SX_PTR_TO_INT(pKw->pUserData) == PH7_TKWRD_CLASS ){` |
|        16 | 3787 | `			if( iFlags & PH7_CLASS_FINAL ){` |
|         3 | 3788 | `				SyBlobAppend(&sChunk,"final ",sizeof("final ")-1);` |
|         1 | 3789 | `			}` |
|        16 | 3790 | `			if( iFlags & PH7_CLASS_ABSTRACT ){` |
|       ! 0 | 3791 | `				SyBlobAppend(&sChunk,"abstract ",sizeof("abstract ")-1);` |
|       ! 0 | 3792 | `			}` |
|        16 | 3793 | `			if( iFlags & PH7_CLASS_READONLY ){` |
|       ! 0 | 3794 | `				SyBlobAppend(&sChunk,"readonly ",sizeof("readonly ")-1);` |
|       ! 0 | 3795 | `			}` |
|         7 | 3796 | `		}` |
|        24 | 3797 | `		zFrom = pKw->sData.zString;` |
|        24 | 3798 | `		zTo = pBodyEnd->sData.zString + pBodyEnd->sData.nByte;` |
|        24 | 3799 | `		SyBlobAppend(&sChunk,zFrom,(sxu32)(zTo - zFrom));` |
|         - | 3800 | `	}` |
|        30 | 3801 | `	pDefer = (VmDeferredClass *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmDeferredClass));` |
|        30 | 3802 | `	if( pDefer == 0 ){` |
|       ! 0 | 3803 | `		SyBlobRelease(&sChunk);` |
|       ! 0 | 3804 | `		PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 3805 | `		return SXERR_ABORT;` |
|         - | 3806 | `	}` |
|        30 | 3807 | `	SyZero(pDefer,sizeof(VmDeferredClass));` |
|        44 | 3808 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        28 | 3809 | `		(const char *)SyBlobData(&sChunk),SyBlobLength(&sChunk));` |
|        30 | 3810 | `	SyBlobRelease(&sChunk);` |
|        30 | 3811 | `	if( zDup == 0 ){` |
|       ! 0 | 3812 | `		PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 3813 | `		return SXERR_ABORT;` |
|         - | 3814 | `	}` |
|        30 | 3815 | `	SyStringInitFromBuf(&pDefer->sText,zDup,SyStrlen(zDup));` |
|        30 | 3816 | `	if( bAnon ){` |
|         8 | 3817 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pAnonName->zString,pAnonName->nByte);` |
|         8 | 3818 | `		if( zDup == 0 ){` |
|       ! 0 | 3819 | `			PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 3820 | `			return SXERR_ABORT;` |
|         - | 3821 | `		}` |
|         8 | 3822 | `		SyStringInitFromBuf(&pDefer->sAnonName,zDup,pAnonName->nByte);` |
|         8 | 3823 | `		pDefer->sSelfName = pDefer->sAnonName;` |
|         5 | 3824 | `	}else{` |
|        35 | 3825 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        22 | 3826 | `			(const char *)SyBlobData(pSelfFqn),SyBlobLength(pSelfFqn));` |
|        24 | 3827 | `		if( zDup == 0 ){` |
|       ! 0 | 3828 | `			PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 3829 | `			return SXERR_ABORT;` |
|         - | 3830 | `		}` |
|        24 | 3831 | `		SyStringInitFromBuf(&pDefer->sSelfName,zDup,SyBlobLength(pSelfFqn));` |
|         - | 3832 | `	}` |
|        30 | 3833 | `	SySetInit(&pDefer->aRequired,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|         - | 3834 | `	{` |
|        30 | 3835 | `		VmDeferredReq *aReq = (VmDeferredReq *)SySetBasePtr(pMissing);` |
|         - | 3836 | `		sxu32 n;` |
|        62 | 3837 | `		for( n = 0 ; n < SySetUsed(pMissing) ; ++n ){` |
|        34 | 3838 | `			SySetPut(&pDefer->aRequired,(const void *)&aReq[n]);` |
|        18 | 3839 | `		}` |
|         - | 3840 | `	}` |
|        30 | 3841 | `	pDefer->nLine = pKw->nLine;` |
|        30 | 3842 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLASS_DEFER,0,0,(void *)pDefer,0);` |
|         - | 3843 | `	/* Skip the declaration: the statement cursor lands past its '}' */` |
|        30 | 3844 | `	pGen->pIn = &pBodyEnd[1];` |
|        30 | 3845 | `	return SXRET_OK;` |
|        16 | 3846 | `}` |
|         - | 3847 | `/*` |
|         - | 3848 | ` * Deferral gate shared by the named-declaration compilers: scan the` |
|         - | 3849 | ` * declaration at pGen->pIn; when a dependency is missing, capture + emit the` |
|         - | 3850 | ` * deferred record and return TRUE (the caller returns immediately — the` |
|         - | 3851 | ` * declaration compiles at execution time). FALSE means compile normally.` |
|         - | 3852 | ` * *pRc carries SXERR_ABORT out of the capture path.` |
|         - | 3853 | ` */` |
|    609796 | 3854 | `static int GenStateMaybeDeferClass(ph7_gen_state *pGen,sxi32 iFlags,int iSelfKind,sxi32 *pRc)` |
|         5 | 3855 | `{` |
|         - | 3856 | `	SySet aMissing;` |
|    609801 | 3857 | `	SyToken *pBody = 0;` |
|    609801 | 3858 | `	SyToken *pBodyEnd = 0;` |
|         - | 3859 | `	SyBlob sSelfFqn;` |
|    609801 | 3860 | `	int bDefer = 0;` |
|    609801 | 3861 | `	*pRc = SXRET_OK;` |
|    609801 | 3862 | `	SySetInit(&aMissing,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|    609801 | 3863 | `	SyBlobInit(&sSelfFqn,&pGen->pVm->sAllocator);` |
|    609796 | 3864 | `	if( GenStateScanDeferDeps(pGen,0,iSelfKind,&aMissing,&pBody,&pBodyEnd,&sSelfFqn) == SXRET_OK` |
|    609800 | 3865 | `	 && SySetUsed(&aMissing) > 0 ){` |
|        24 | 3866 | `		*pRc = GenStateEmitDeferredClass(pGen,iFlags,0,&aMissing,pBodyEnd,&sSelfFqn,0);` |
|        24 | 3867 | `		bDefer = 1;` |
|        11 | 3868 | `	}` |
|    609801 | 3869 | `	SySetRelease(&aMissing);` |
|    609801 | 3870 | `	SyBlobRelease(&sSelfFqn);` |
|    609801 | 3871 | `	return bDefer;` |
|         5 | 3872 | `}` |
|         - | 3873 | `/*` |
|         - | 3874 | ``  * Apply a declaration body's collected `use Trait[, Trait] [{ resolution }]` `` |
|         - | 3875 | ` * entries to pClass — plain application when no resolution block is present,` |
|         - | 3876 | ` * otherwise the two-pass insteadof/as machinery. Shared by the CLASS body and` |
|         - | 3877 | ` * (since the adaptation-block port) the TRAIT body compiler. Returns the last` |
|         - | 3878 | ` * application status (non-OK = out of memory at a copy site).` |
|         - | 3879 | ` */` |
|    528100 | 3880 | `static sxi32 GenStateApplyTraitUses(ph7_gen_state *pGen,ph7_class *pClass,SySet *pUseEntries)` |
|         5 | 3881 | `{` |
|    528105 | 3882 | `	sxi32 rc = SXRET_OK;` |
|         - | 3883 | `	{` |
|         - | 3884 | `		TraitUseEntry *apUse;` |
|         - | 3885 | `		sxu32 nU;` |
|    528105 | 3886 | `		apUse = (TraitUseEntry *)SySetBasePtr(pUseEntries);` |
|    546369 | 3887 | `		for( nU = 0 ; nU < SySetUsed(pUseEntries) ; nU++ ){` |
|     18269 | 3888 | `			TraitUseEntry *pUse = &apUse[nU];` |
|     18269 | 3889 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|     18269 | 3890 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|     18269 | 3891 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|         - | 3892 | `			sxu32 nT;` |
|     18269 | 3893 | `			if( !hasResolution ){` |
|         - | 3894 | `				/* No conflict resolution block: use standard trait application */` |
|     36489 | 3895 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|     18251 | 3896 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|     18251 | 3897 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 3898 | `						break;` |
|         - | 3899 | `					}` |
|      9128 | 3900 | `				}` |
|      9124 | 3901 | `			}else{` |
|         - | 3902 | `				/* With resolution block: copy attributes, record traits,` |
|         - | 3903 | `				 * then use the block to resolve method conflicts.` |
|         - | 3904 | `				 */` |
|         - | 3905 | `				SyToken *pR;` |
|        61 | 3906 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        35 | 3907 | `					ph7_class *pTR = apTrait[nT];` |
|         - | 3908 | `					ph7_class_attr *pAR;` |
|         - | 3909 | `					SyHashEntry *pER;` |
|         - | 3910 | `					SyString *pNR;` |
|        35 | 3911 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|        51 | 3912 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|       ! 0 | 3913 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|       ! 0 | 3914 | `						pNR = &pAR->sName;` |
|       ! 0 | 3915 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       ! 0 | 3916 | `							SyHashInsertTail(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|       ! 0 | 3917 | `						}` |
|       ! 0 | 3918 | `					}` |
|         - | 3919 | `					/* Trait constants (PHP 8.2) live in the separate hConst namespace */` |
|        35 | 3920 | `					SyHashResetLoopCursor(&pTR->hConst);` |
|        51 | 3921 | `					while((pER = SyHashGetNextEntry(&pTR->hConst)) != 0 ){` |
|       ! 0 | 3922 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|       ! 0 | 3923 | `						pNR = &pAR->sName;` |
|       ! 0 | 3924 | `						if( SyHashGet(&pClass->hConst,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       ! 0 | 3925 | `							SyHashInsertTail(&pClass->hConst,(const void *)pNR->zString,pNR->nByte,pAR);` |
|       ! 0 | 3926 | `						}` |
|       ! 0 | 3927 | `					}` |
|        35 | 3928 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|        19 | 3929 | `				}` |
|         - | 3930 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|        29 | 3931 | `				pR = pUse->pResolvStart;` |
|        65 | 3932 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 3933 | `					SyString sTrait,sMethod;` |
|         - | 3934 | `					ph7_class *pSrcTrait;` |
|         - | 3935 | `					ph7_class_method *pMeth;` |
|         - | 3936 | `					sxi32 nRKwrd;` |
|       101 | 3937 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        65 | 3938 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        39 | 3939 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        39 | 3940 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        39 | 3941 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        39 | 3942 | `					sMethod = pR->sData;` |
|        39 | 3943 | `					pR++;` |
|        39 | 3944 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        16 | 3945 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        16 | 3946 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        16 | 3947 | `							sTrait = sMethod;` |
|        16 | 3948 | `							pR++;` |
|        16 | 3949 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        16 | 3950 | `							sMethod = pR->sData;` |
|        16 | 3951 | `							pR++;` |
|         7 | 3952 | `						}` |
|         7 | 3953 | `					}` |
|        39 | 3954 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 3955 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 3956 | `						continue;` |
|         - | 3957 | `					}` |
|        39 | 3958 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        39 | 3959 | `					pR++;` |
|        39 | 3960 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|        10 | 3961 | `						pSrcTrait = 0;` |
|        12 | 3962 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        12 | 3963 | `							SyString *pTN = &apTrait[nT]->sName;` |
|        17 | 3964 | `							if( pTN->nByte >= sTrait.nByte &&` |
|        10 | 3965 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        10 | 3966 | `								pSrcTrait = apTrait[nT];` |
|        10 | 3967 | `								break;` |
|         - | 3968 | `							}` |
|         2 | 3969 | `						}` |
|        10 | 3970 | `						if( pSrcTrait ){` |
|        10 | 3971 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        10 | 3972 | `							if( pMeth ){` |
|        10 | 3973 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|        10 | 3974 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|        10 | 3975 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|         4 | 3976 | `								}` |
|         4 | 3977 | `							}` |
|         4 | 3978 | `						}` |
|         4 | 3979 | `					}` |
|        81 | 3980 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 3981 | `				}` |
|         - | 3982 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|        61 | 3983 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         - | 3984 | `					ph7_class_method *pMR;` |
|         - | 3985 | `					SyHashEntry *pER;` |
|         - | 3986 | `					SyString *pNR;` |
|        35 | 3987 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|        97 | 3988 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|        49 | 3989 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|        49 | 3990 | `						pNR = &pMR->sFunc.sName;` |
|        49 | 3991 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|        33 | 3992 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|        15 | 3993 | `						}` |
|         3 | 3994 | `					}` |
|        19 | 3995 | `				}` |
|         - | 3996 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|        29 | 3997 | `				pR = pUse->pResolvStart;` |
|        65 | 3998 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 3999 | `					SyString sTrait,sMethod,sAlias;` |
|         - | 4000 | `					ph7_class *pSrcTrait;` |
|         - | 4001 | `					ph7_class_method *pMeth;` |
|        65 | 4002 | `					int hasQual = 0;` |
|         - | 4003 | `					sxi32 nRKwrd;` |
|       101 | 4004 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        65 | 4005 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        39 | 4006 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        39 | 4007 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        39 | 4008 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|        39 | 4009 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        39 | 4010 | `					sMethod = pR->sData;` |
|        39 | 4011 | `					pR++;` |
|        39 | 4012 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        16 | 4013 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        16 | 4014 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        16 | 4015 | `							sTrait = sMethod;` |
|        16 | 4016 | `							hasQual = 1;` |
|        16 | 4017 | `							pR++;` |
|        16 | 4018 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        16 | 4019 | `							sMethod = pR->sData;` |
|        16 | 4020 | `							pR++;` |
|         7 | 4021 | `						}` |
|         7 | 4022 | `					}` |
|        39 | 4023 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 4024 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 4025 | `						continue;` |
|         - | 4026 | `					}` |
|        39 | 4027 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        39 | 4028 | `					pR++;` |
|        39 | 4029 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|        31 | 4030 | `						sxi32 iNewVis = -1;` |
|        31 | 4031 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|        10 | 4032 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|        10 | 4033 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|        10 | 4034 | `								iNewVis = nAK;` |
|        10 | 4035 | `								pR++;` |
|         4 | 4036 | `							}` |
|         4 | 4037 | `						}` |
|        31 | 4038 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|        29 | 4039 | `							sAlias = pR->sData;` |
|        29 | 4040 | `							pR++;` |
|        13 | 4041 | `						}` |
|        31 | 4042 | `						pMeth = 0;` |
|        31 | 4043 | `						if( hasQual ){` |
|         8 | 4044 | `							pSrcTrait = 0;` |
|        14 | 4045 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        14 | 4046 | `								SyString *pTN = &apTrait[nT]->sName;` |
|        20 | 4047 | `								if( pTN->nByte >= sTrait.nByte &&` |
|        12 | 4048 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         8 | 4049 | `									pSrcTrait = apTrait[nT];` |
|         8 | 4050 | `									break;` |
|         - | 4051 | `								}` |
|         5 | 4052 | `							}` |
|         8 | 4053 | `							if( pSrcTrait ){` |
|         8 | 4054 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         3 | 4055 | `							}` |
|         5 | 4056 | `						}else{` |
|        25 | 4057 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|         - | 4058 | `						}` |
|        31 | 4059 | `						if( pMeth ){` |
|         - | 4060 | `							/* php: a method declared in the class BODY wins over a trait alias` |
|         - | 4061 | ``							 * of the same name (e.g. an explicit __construct over `init as`` |
|         - | 4062 | ``							 * __construct`). If pClass already declares sAlias ITSELF — an own`` |
|         - | 4063 | `							 * method, sFunc.pUserData == pClass — keep it: SyHashInsert is LIFO,` |
|         - | 4064 | `							 * so an unconditional insert would shadow the class method at lookup` |
|         - | 4065 | ``							 * and `new` would run the alias. A name held only by another trait is`` |
|         - | 4066 | `							 * a genuine conflict resolved by the insteadof pass above. */` |
|        31 | 4067 | `							int bClassWins = 0;` |
|        31 | 4068 | `							if( sAlias.nByte > 0 ){` |
|        29 | 4069 | `								ph7_class_method *pOwn = PH7_ClassExtractMethod(pClass,sAlias.zString,sAlias.nByte);` |
|        29 | 4070 | `								bClassWins = (pOwn && pOwn->sFunc.pUserData == pClass);` |
|        13 | 4071 | `							}` |
|        41 | 4072 | `							if( sAlias.nByte > 0 && !bClassWins ){` |
|         - | 4073 | `								/* Create a shallow copy of the method struct for the alias` |
|         - | 4074 | `								 * so it can carry its own visibility without affecting the original.` |
|         - | 4075 | `								 */` |
|         - | 4076 | `								ph7_class_method *pAlias;` |
|         - | 4077 | `								char *zAliasDup;` |
|        23 | 4078 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        23 | 4079 | `								if( pAlias ){` |
|        23 | 4080 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|        23 | 4081 | `									if( iNewVis >= 0 ){` |
|         8 | 4082 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         6 | 4083 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 4084 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         3 | 4085 | `									}` |
|        23 | 4086 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        23 | 4087 | `									if( zAliasDup ){` |
|        23 | 4088 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|        10 | 4089 | `									}` |
|        13 | 4090 | `								}` |
|        20 | 4091 | `							}else if( sAlias.nByte == 0 && iNewVis >= 0 ){` |
|         - | 4092 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|         - | 4093 | `								ph7_class_method *pCopy;` |
|         3 | 4094 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|         3 | 4095 | `								if( pCopy ){` |
|         3 | 4096 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|         3 | 4097 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|         3 | 4098 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 4099 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 4100 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         - | 4101 | `									/* Replace the method in the class hash */` |
|         3 | 4102 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|         3 | 4103 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|         1 | 4104 | `								}` |
|         1 | 4105 | `							}` |
|        14 | 4106 | `						}` |
|        14 | 4107 | `						SXUNUSED(hasQual);` |
|        14 | 4108 | `					}` |
|        47 | 4109 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 4110 | `				}` |
|         - | 4111 | `			}` |
|     18269 | 4112 | `			SySetRelease(&pUse->aTraits);` |
|      9137 | 4113 | `		}` |
|         - | 4114 | `	}` |
|    528105 | 4115 | `	return rc;` |
|         5 | 4116 | `}` |
|         - | 4117 | `/*` |
|         - | 4118 | ` * Compile a class declaration, named or anonymous.` |
|         - | 4119 | ` *` |
|         - | 4120 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|         - | 4121 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|         - | 4122 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|         - | 4123 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|         - | 4124 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|         - | 4125 | ` * implements, body, install) is shared by both paths.` |
|         - | 4126 | ` */` |
|    518958 | 4127 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|         - | 4128 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|         5 | 4129 | `{` |
|    518963 | 4130 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 4131 | `	ph7_class *pClass,*pBase;` |
|    518963 | 4132 | `	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' (enclosing class for a nested anon) */` |
|         - | 4133 | `	SyToken *pEnd,*pTmp;` |
|         - | 4134 | `	sxi32 iProtection;` |
|         - | 4135 | `	SySet aInterfaces;` |
|         - | 4136 | `	SySet aUseEntries;` |
|         - | 4137 | `	sxi32 iAttrflags;` |
|         - | 4138 | `	SyString *pName;` |
|         - | 4139 | `	sxi32 nKwrd;` |
|         - | 4140 | `	sxi32 rc;` |
|    518963 | 4141 | `	if( pAnonName == 0 ){` |
|         - | 4142 | `		/* Deferral gate: an unresolvable parent/interface/trait —` |
|         - | 4143 | `		 * its autoloader has not RUN yet — re-compiles this declaration at its` |
|         - | 4144 | `		 * execution point instead of dying on "Nonexistent base class". */` |
|         - | 4145 | `		sxi32 rcDefer;` |
|    518913 | 4146 | `		if( GenStateMaybeDeferClass(pGen,iFlags,PH7_DEFER_KIND_CLASS,&rcDefer) ){` |
|        18 | 4147 | `			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - | 4148 | `		}` |
|    259446 | 4149 | `	}` |
|         - | 4150 | `	/* Jump the 'class' keyword */` |
|    518947 | 4151 | `	pGen->pIn++;` |
|    518947 | 4152 | `	if( pAnonName ){` |
|         - | 4153 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|         - | 4154 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|         - | 4155 | `		 * then use the synthesized name. */` |
|        55 | 4156 | `		*ppArgStart = *ppArgEnd = 0;` |
|        55 | 4157 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         7 | 4158 | `			pGen->pIn++; /* Jump '(' */` |
|         7 | 4159 | `			*ppArgStart = pGen->pIn;` |
|        10 | 4160 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|         3 | 4161 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|         7 | 4162 | `			pGen->pIn = *ppArgEnd;` |
|         7 | 4163 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|         3 | 4164 | `		}` |
|        55 | 4165 | `		pName = pAnonName;` |
|        55 | 4166 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|        30 | 4167 | `	}else{` |
|    518897 | 4168 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - | 4169 | `			/* Syntax error */` |
|       ! 0 | 4170 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|       ! 0 | 4171 | `			if( rc == SXERR_ABORT ){` |
|         - | 4172 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 4173 | `				return SXERR_ABORT;` |
|         - | 4174 | `			}` |
|         - | 4175 | `			/* Synchronize with the first semi-colon or curly braces */` |
|       ! 0 | 4176 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|       ! 0 | 4177 | `				pGen->pIn++;` |
|       ! 0 | 4178 | `			}` |
|       ! 0 | 4179 | `			return SXRET_OK;` |
|         - | 4180 | `		}` |
|         - | 4181 | `		/* Extract class name */` |
|    518897 | 4182 | `		pName = &pGen->pIn->sData;` |
|         - | 4183 | `		/* Advance the stream cursor */` |
|    518897 | 4184 | `		pGen->pIn++;` |
|         - | 4185 | `		/* Build FQN and obtain a raw class */ {` |
|         - | 4186 | `			SyBlob sFQN;` |
|         - | 4187 | `			SyString sFQNStr;` |
|    518897 | 4188 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    518897 | 4189 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|    518897 | 4190 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|         - | 4191 | ``			/* php refuses a declaration whose short name a local `use` already took. */`` |
|    518897 | 4192 | `			if( GenStateGuardImportRedeclare(pGen,0,pName,&sFQNStr,nLine) == SXERR_ABORT ){` |
|       ! 0 | 4193 | `				SyBlobRelease(&sFQN);` |
|       ! 0 | 4194 | `				return SXERR_ABORT;` |
|         - | 4195 | `			}` |
|    518897 | 4196 | `			GenStateRecordDeclaredName(pGen,0,&sFQNStr);` |
|    518897 | 4197 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    518897 | 4198 | `			SyBlobRelease(&sFQN);` |
|         - | 4199 | `		}` |
|         - | 4200 | `	}` |
|    518947 | 4201 | `	if( pClass == 0 ){` |
|       ! 0 | 4202 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 4203 | `		return SXERR_ABORT;` |
|         - | 4204 | `	}` |
|    518942 | 4205 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|      4593 | 4206 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|         - | 4207 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|      4563 | 4208 | `		pGen->pIn++; /* Jump ':' */` |
|      4558 | 4209 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      4563 | 4210 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|        10 | 4211 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|        10 | 4212 | `			pGen->pIn++;` |
|      4556 | 4213 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      4555 | 4214 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|      4553 | 4215 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|      4553 | 4216 | `			pGen->pIn++;` |
|      2279 | 4217 | `		}else{` |
|         3 | 4218 | `			SyToken *pTok = pGen->pIn;` |
|         3 | 4219 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|         4 | 4220 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|         1 | 4221 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|         3 | 4222 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 4223 | `				return SXERR_ABORT;` |
|         - | 4224 | `			}` |
|         3 | 4225 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|         3 | 4226 | `				pGen->pIn++; /* Skip the bogus type token */` |
|         1 | 4227 | `			}` |
|         - | 4228 | `		}` |
|      2279 | 4229 | `	}` |
|    518947 | 4230 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    518947 | 4231 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 4232 | `		return SXERR_ABORT;` |
|         - | 4233 | `	}` |
|         - | 4234 | `	/* implemented interfaces and per-use-statement trait containers */` |
|    518947 | 4235 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    518947 | 4236 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|         - | 4237 | `	/* Assume a standalone class */` |
|    518947 | 4238 | `	pBase = 0;` |
|    518947 | 4239 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    426237 | 4240 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    426237 | 4241 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|         - | 4242 | `			SyBlob sResolved;` |
|         - | 4243 | `			SyString sBaseName;` |
|         - | 4244 | `			sxu32 nRefLine;` |
|    290201 | 4245 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|         - | 4246 | `				/* php parse-fatals here (enums have no inheritance) */` |
|       ! 0 | 4247 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 4248 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|       ! 0 | 4249 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 4250 | `					return SXERR_ABORT;` |
|         - | 4251 | `				}` |
|       ! 0 | 4252 | `			}` |
|    290201 | 4253 | `			pGen->pIn++; /* Advance past 'extends' */` |
|    290201 | 4254 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    290201 | 4255 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    290201 | 4256 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         3 | 4257 | `				SyBlobRelease(&sResolved);` |
|         4 | 4258 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 4259 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|         1 | 4260 | `					pName);` |
|         3 | 4261 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|         3 | 4262 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 4263 | `					return SXERR_ABORT;` |
|         - | 4264 | `				}` |
|         3 | 4265 | `				return SXRET_OK;` |
|         - | 4266 | `			}` |
|    435296 | 4267 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    290194 | 4268 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    290199 | 4269 | `			SyStringInitFromBuf(&sBaseName,` |
|         - | 4270 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 4271 | `			/* Interfaces are not allowed */` |
|    290199 | 4272 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|       ! 0 | 4273 | `				pBase = pBase->pNextName;` |
|       ! 0 | 4274 | `			}` |
|    290199 | 4275 | `			if( pBase == 0 ){` |
|       ! 0 | 4276 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 4277 | `					"Nonexistent base class '%z'",&sBaseName);` |
|       ! 0 | 4278 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 4279 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 4280 | `					return SXERR_ABORT;` |
|         - | 4281 | `				}` |
|       ! 0 | 4282 | `			}else{` |
|    290199 | 4283 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|         4 | 4284 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 | 4285 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|         3 | 4286 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 4287 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 4288 | `						return SXERR_ABORT;` |
|         - | 4289 | `					}` |
|         3 | 4290 | `					pBase = 0; /* Never inherit from an enum */` |
|    290198 | 4291 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|       ! 0 | 4292 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 4293 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|       ! 0 | 4294 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 4295 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 4296 | `						return SXERR_ABORT;` |
|         - | 4297 | `					}` |
|       ! 0 | 4298 | `				}` |
|         - | 4299 | `			}` |
|    290199 | 4300 | `			SyBlobRelease(&sResolved);` |
|    290199 | 4301 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|       ! 0 | 4302 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|       ! 0 | 4303 | `			}` |
|    145097 | 4304 | `		}` |
|    426235 | 4305 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|         - | 4306 | `			ph7_class *pInterface;` |
|         - | 4307 | `			/* Interface implementation */` |
|    154179 | 4308 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    149549 | 4309 | `			for(;;){` |
|         - | 4310 | `				SyBlob sResolved;` |
|         - | 4311 | `				SyString sIntName;` |
|         - | 4312 | `				sxu32 nRefLine;` |
|    226641 | 4313 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    226641 | 4314 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    226641 | 4315 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 4316 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 4317 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 4318 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|       ! 0 | 4319 | `						pName);` |
|       ! 0 | 4320 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 4321 | `						return SXERR_ABORT;` |
|         - | 4322 | `					}` |
|       ! 0 | 4323 | `					break;` |
|         - | 4324 | `				}` |
|    453277 | 4325 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    226636 | 4326 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    226641 | 4327 | `				SyStringInitFromBuf(&sIntName,` |
|         - | 4328 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 4329 | `				/* Only interfaces are allowed */` |
|    226641 | 4330 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 4331 | `					pInterface = pInterface->pNextName;` |
|       ! 0 | 4332 | `				}` |
|    226641 | 4333 | `				if( pInterface == 0 ){` |
|       ! 0 | 4334 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 4335 | `						"Nonexistent base interface '%z'",&sIntName);` |
|       ! 0 | 4336 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 4337 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 4338 | `						return SXERR_ABORT;` |
|         - | 4339 | `					}` |
|       ! 0 | 4340 | `				}else{` |
|         - | 4341 | `					/* Reject user classes that try to implement Throwable` |
|         - | 4342 | `					 * directly (or via an interface that extends Throwable)` |
|         - | 4343 | `					 * unless they already extend Exception or Error.` |
|         - | 4344 | `					 * Exception and Error themselves are compiled from the` |
|         - | 4345 | `					 * built-in library and are exempt by FQN — a namespaced` |
|         - | 4346 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    226641 | 4347 | `					SyString *pFqn = &pClass->sName;` |
|    226641 | 4348 | `					int bIsExceptionOrError =` |
|    115586 | 4349 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    339958 | 4350 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    224380 | 4351 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|      4544 | 4352 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    231169 | 4353 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|     13602 | 4354 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|      4531 | 4355 | `						!bIsExceptionOrError ){` |
|        12 | 4356 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 4357 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|         3 | 4358 | `							&pClass->sName);` |
|         9 | 4359 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 4360 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 4361 | `							return SXERR_ABORT;` |
|         - | 4362 | `						}` |
|         - | 4363 | `						/* Skip registration so the follow-up abstract-method` |
|         - | 4364 | `						 * check does not produce a duplicate fatal. */` |
|         6 | 4365 | `					}else{` |
|    226635 | 4366 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|         - | 4367 | `					}` |
|         - | 4368 | `				}` |
|    226641 | 4369 | `				SyBlobRelease(&sResolved);` |
|    226641 | 4370 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     77092 | 4371 | `					break;` |
|         - | 4372 | `				}` |
|     72467 | 4373 | `				pGen->pIn++;/* Jump the comma */` |
|         5 | 4374 | `			}` |
|     77087 | 4375 | `		}` |
|    213115 | 4376 | `	}` |
|    518945 | 4377 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - | 4378 | `		/* Syntax error */` |
|       ! 0 | 4379 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|       ! 0 | 4380 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 4381 | `		if( rc == SXERR_ABORT ){` |
|         - | 4382 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 4383 | `			return SXERR_ABORT;` |
|         - | 4384 | `		}` |
|       ! 0 | 4385 | `		return SXRET_OK;` |
|         - | 4386 | `	}` |
|    518945 | 4387 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    518945 | 4388 | `	pEnd = 0; /* cc warning */` |
|         - | 4389 | `	/* Delimit the class body */` |
|    518945 | 4390 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    518945 | 4391 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 4392 | `		/* Syntax error */` |
|       ! 0 | 4393 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|       ! 0 | 4394 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 4395 | `		if( rc == SXERR_ABORT ){` |
|         - | 4396 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 4397 | `			return SXERR_ABORT;` |
|         - | 4398 | `		}` |
|       ! 0 | 4399 | `		return SXRET_OK;` |
|         - | 4400 | `	}` |
|         - | 4401 | `	/* The delimiter token is the class body's closing brace */` |
|    518945 | 4402 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 4403 | `	/* Swap token stream */` |
|    518945 | 4404 | `	pTmp = pGen->pEnd;` |
|    518945 | 4405 | `	pGen->pEnd = pEnd;` |
|         - | 4406 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|    518945 | 4407 | `	pClass->iFlags \|= iFlags;` |
|         - | 4408 | `	/* This class/enum is now the lexical class for its body — see pCurClass. */` |
|    518945 | 4409 | `	pGen->pCurClass = pClass;` |
|         - | 4410 | `	/* Start the parse process */` |
|   1922519 | 4411 | `	for(;;){` |
|         - | 4412 | `		/* Jump leading/trailing semi-colons */` |
|   5541995 | 4413 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   1020665 | 4414 | `			pGen->pIn++;` |
|         5 | 4415 | `		}` |
|   4521335 | 4416 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 4417 | `			/* End of class body */` |
|    518895 | 4418 | `			break;` |
|         - | 4419 | `		}` |
|         - | 4420 | `		/* Bind a directly-preceding docblock to this member */` |
|   4002445 | 4421 | `		GenStateSetPendingDoc(&(*pGen));` |
|   4002440 | 4422 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   2001225 | 4423 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|       ! 0 | 4424 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 4425 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 4426 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 4427 | `			if( rc == SXERR_ABORT ){` |
|         - | 4428 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 4429 | `				return SXERR_ABORT;` |
|         - | 4430 | `			}` |
|       ! 0 | 4431 | `			goto done;` |
|         - | 4432 | `		}` |
|         - | 4433 | `		/* Assume public visibility */` |
|   4002445 | 4434 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   4002445 | 4435 | `		iAttrflags = 0;` |
|         - | 4436 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|         - | 4437 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|         - | 4438 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|         - | 4439 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|   4002445 | 4440 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 4441 | `			int bMod = 0;` |
|       ! 0 | 4442 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 4443 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|         - | 4444 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|         - | 4445 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|         - | 4446 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|         - | 4447 | `			 * that the generic keyword dispatch would misread as a method. */` |
|       ! 0 | 4448 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       ! 0 | 4449 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 | 4450 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|       ! 0 | 4451 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|       ! 0 | 4452 | `			}` |
|       ! 0 | 4453 | `			if( !bMod ){` |
|       ! 0 | 4454 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 4455 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 4456 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 4457 | `						return SXERR_ABORT;` |
|         - | 4458 | `					}` |
|       ! 0 | 4459 | `					goto done;` |
|         - | 4460 | `				}` |
|       ! 0 | 4461 | `				continue;` |
|         - | 4462 | `			}` |
|       ! 0 | 4463 | `		}` |
|   4002445 | 4464 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 4465 | `			/* Extract the current keyword */` |
|   4002445 | 4466 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   4002445 | 4467 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|         - | 4468 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|      9141 | 4469 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|      9141 | 4470 | `				if( rc != SXRET_OK ){` |
|         6 | 4471 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 4472 | `						return SXERR_ABORT;` |
|         - | 4473 | `					}` |
|         6 | 4474 | `					goto done;` |
|         - | 4475 | `				}` |
|      9137 | 4476 | `				continue;` |
|         - | 4477 | `			}` |
|   3993309 | 4478 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 4479 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|         - | 4480 | `				TraitUseEntry sUse;` |
|     18257 | 4481 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|     18257 | 4482 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|     18257 | 4483 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      9136 | 4484 | `				for(;;){` |
|         - | 4485 | `					ph7_class *pTrait;` |
|         - | 4486 | `					SyBlob sResolved;` |
|         - | 4487 | `					SyString sTraitName;` |
|     18267 | 4488 | `					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|         - | 4489 | `					/* A trait name is a full class reference: it may be qualified or` |
|         - | 4490 | ``					 * fully-qualified (`use Foo\Bar\T;`, `use \Foo\Bar\T;`) — the generated`` |
|         - | 4491 | `					 * PHPUnit mocks name their traits absolutely. Parse it with the shared` |
|         - | 4492 | `					 * class-reference reader (handles the leading '\', every '\'-segment and` |
|         - | 4493 | `					 * namespace/import resolution) instead of a single-identifier read, which` |
|         - | 4494 | `					 * choked on the first '\'. */` |
|     18267 | 4495 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     18267 | 4496 | `					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 4497 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 4498 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|       ! 0 | 4499 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|       ! 0 | 4500 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 4501 | `							return SXERR_ABORT;` |
|         - | 4502 | `						}` |
|       ! 0 | 4503 | `						break;` |
|         - | 4504 | `					}` |
|     36529 | 4505 | `					pTrait = PH7_VmExtractClass(pGen->pVm,` |
|     18262 | 4506 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     18267 | 4507 | `					SyStringInitFromBuf(&sTraitName,` |
|         - | 4508 | `						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 4509 | `					/* Only traits are allowed */` |
|     18267 | 4510 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 4511 | `						pTrait = pTrait->pNextName;` |
|       ! 0 | 4512 | `					}` |
|     18267 | 4513 | `					if( pTrait == 0 ){` |
|       ! 0 | 4514 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|         - | 4515 | `							"'%z' is not a trait",&sTraitName);` |
|       ! 0 | 4516 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 4517 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 4518 | `							return SXERR_ABORT;` |
|         - | 4519 | `						}` |
|       ! 0 | 4520 | `					}else{` |
|     18267 | 4521 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|         - | 4522 | `					}` |
|     18267 | 4523 | `					SyBlobRelease(&sResolved);` |
|         - | 4524 | `					/* GenStateParseClassReference already advanced past the whole name —` |
|         - | 4525 | `					 * continue only across a comma-separated trait list. */` |
|     18267 | 4526 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      9131 | 4527 | `						break;` |
|         - | 4528 | `					}` |
|        13 | 4529 | `					pGen->pIn++; /* Jump the comma */` |
|         3 | 4530 | `				}` |
|         - | 4531 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|     18257 | 4532 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - | 4533 | `					SyToken *pBlock;` |
|        25 | 4534 | `					pGen->pIn++; /* Jump '{' */` |
|        25 | 4535 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|        25 | 4536 | `					sUse.pResolvStart = pGen->pIn;` |
|        25 | 4537 | `					sUse.pResolvEnd = pBlock;` |
|        25 | 4538 | `					if( pBlock < pGen->pEnd ){` |
|        25 | 4539 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|        14 | 4540 | `					}else{` |
|       ! 0 | 4541 | `						pGen->pIn = pGen->pEnd;` |
|         - | 4542 | `					}` |
|        11 | 4543 | `				}` |
|     18257 | 4544 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|         - | 4545 | `				/* The semicolon will be consumed by the outer loop */` |
|     18257 | 4546 | `				continue;` |
|         - | 4547 | `			}` |
|   3975057 | 4548 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - | 4549 | `				int nSetTok;` |
|   3340333 | 4550 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   3340333 | 4551 | `				if( nSetVis ){` |
|         - | 4552 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|         - | 4553 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|         3 | 4554 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 4555 | `					pGen->pIn += nSetTok;` |
|         2 | 4556 | `				}else{` |
|   3340331 | 4557 | `					iProtection = nKwrd;` |
|   3340331 | 4558 | `					pGen->pIn++; /* Jump the visibility token */` |
|         - | 4559 | `					/* Optional asymmetric set-visibility after the read` |
|         - | 4560 | ``					 * visibility: `public private(set) int $x`. */`` |
|   3340331 | 4561 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   3340331 | 4562 | `					if( nSetVis ){` |
|         9 | 4563 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         9 | 4564 | `						pGen->pIn += nSetTok;` |
|         4 | 4565 | `					}` |
|         - | 4566 | `				}` |
|         - | 4567 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|         - | 4568 | ``				 * `public private(set) readonly int $x`. */`` |
|   3340333 | 4569 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|        30 | 4570 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|        30 | 4571 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        13 | 4572 | `				}` |
|   3340328 | 4573 | `				if( pGen->pIn >= pGen->pEnd` |
|   3340333 | 4574 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 4575 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 4576 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 4577 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 4578 | `					if( rc == SXERR_ABORT ){` |
|         - | 4579 | `						/* Error count limit reached,abort immediately */` |
|       ! 0 | 4580 | `						return SXERR_ABORT;` |
|         - | 4581 | `					}` |
|       ! 0 | 4582 | `					goto done;` |
|         - | 4583 | `				}` |
|   3340333 | 4584 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 4585 | `					/* Attribute declaration (untyped) */` |
|    598493 | 4586 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    598493 | 4587 | `					if( rc != SXRET_OK ){` |
|        12 | 4588 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 4589 | `							return SXERR_ABORT;` |
|         - | 4590 | `						}` |
|        12 | 4591 | `						goto done;` |
|         - | 4592 | `					}` |
|    607757 | 4593 | `					continue;` |
|         - | 4594 | `				}` |
|   2741845 | 4595 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 4596 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     18555 | 4597 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     18555 | 4598 | `					if( rc != SXRET_OK ){` |
|         9 | 4599 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 4600 | `							return SXERR_ABORT;` |
|         - | 4601 | `						}` |
|         9 | 4602 | `						goto done;` |
|         - | 4603 | `					}` |
|     18549 | 4604 | `					continue;` |
|         - | 4605 | `				}` |
|         - | 4606 | `				/* Extract the keyword */` |
|   2723295 | 4607 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1361645 | 4608 | `			}` |
|   3358019 | 4609 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|         - | 4610 | `				/* Process constant declaration */` |
|    335455 | 4611 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|    335455 | 4612 | `				if( rc != SXRET_OK ){` |
|        15 | 4613 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 4614 | `						return SXERR_ABORT;` |
|         - | 4615 | `					}` |
|        15 | 4616 | `					goto done;` |
|         - | 4617 | `				}` |
|    167724 | 4618 | `			}else{` |
|   3022569 | 4619 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - | 4620 | `					/* Static method or attribute,record that */` |
|    104539 | 4621 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    104539 | 4622 | `					pGen->pIn++; /* Jump the static keyword */` |
|    104539 | 4623 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 4624 | `						int nSetTok;` |
|     72717 | 4625 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|     72717 | 4626 | `						if( nSetVis ){` |
|         - | 4627 | ``							/* `static private(set) int $x` — read side stays public */`` |
|         3 | 4628 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 4629 | `							pGen->pIn += nSetTok;` |
|         2 | 4630 | `						}else{` |
|         - | 4631 | `							/* Extract the keyword */` |
|     72715 | 4632 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     72715 | 4633 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 4634 | `								iProtection = nKwrd;` |
|       ! 0 | 4635 | `								pGen->pIn++; /* Jump the visibility token */` |
|       ! 0 | 4636 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|       ! 0 | 4637 | `								if( nSetVis ){` |
|       ! 0 | 4638 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|       ! 0 | 4639 | `									pGen->pIn += nSetTok;` |
|       ! 0 | 4640 | `								}` |
|       ! 0 | 4641 | `							}` |
|         - | 4642 | `						}` |
|     36356 | 4643 | `					}` |
|         - | 4644 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|         - | 4645 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|         - | 4646 | `					 * than a generic "expecting method" parse error. */` |
|    104539 | 4647 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 4648 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 4649 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       ! 0 | 4650 | `					}` |
|    104534 | 4651 | `					if( pGen->pIn >= pGen->pEnd` |
|    104539 | 4652 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 4653 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 4654 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|       ! 0 | 4655 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 4656 | `						if( rc == SXERR_ABORT ){` |
|         - | 4657 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 4658 | `							return SXERR_ABORT;` |
|         - | 4659 | `						}` |
|       ! 0 | 4660 | `						goto done;` |
|         - | 4661 | `					}` |
|    104539 | 4662 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 4663 | `						/* Attribute declaration */` |
|     31819 | 4664 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     31819 | 4665 | `						if( rc != SXRET_OK ){` |
|         3 | 4666 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 4667 | `								return SXERR_ABORT;` |
|         - | 4668 | `							}` |
|         3 | 4669 | `							goto done;` |
|         - | 4670 | `						}` |
|     31817 | 4671 | `						continue;` |
|         - | 4672 | `					}` |
|     72725 | 4673 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 4674 | `						/* Typed static attribute declaration */` |
|        57 | 4675 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        57 | 4676 | `						if( rc != SXRET_OK ){` |
|         3 | 4677 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 4678 | `								return SXERR_ABORT;` |
|         - | 4679 | `							}` |
|         3 | 4680 | `							goto done;` |
|         - | 4681 | `						}` |
|        55 | 4682 | `						continue;` |
|         - | 4683 | `					}` |
|         - | 4684 | `					/* Extract the keyword */` |
|     72673 | 4685 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2954369 | 4686 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         - | 4687 | `					/* Abstract method,record that.` |
|         - | 4688 | `					 * PHL used to also mark the whole CLASS abstract here, silently` |
|         - | 4689 | ``					 * promoting `class C{abstract function m();}` -- which php rejects`` |
|         - | 4690 | `					 * outright -- into a valid abstract class. That promotion is why` |
|         - | 4691 | `					 * GenStateCheckAbstractMethods never fired for it: by the time the` |
|         - | 4692 | `					 * check ran, the class looked declared-abstract. The declaration is` |
|         - | 4693 | `					 * now diagnosed where the method name is known (see the install` |
|         - | 4694 | `					 * site), so the class flag stays what the SOURCE said. */` |
|      9097 | 4695 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         - | 4696 | `					/* Advance the stream cursor */` |
|      9097 | 4697 | `					pGen->pIn++;` |
|      9097 | 4698 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      9097 | 4699 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      9097 | 4700 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      9095 | 4701 | `							iProtection = nKwrd;` |
|      9095 | 4702 | `							pGen->pIn++; /* Jump the visibility token */` |
|      4545 | 4703 | `						}` |
|      4546 | 4704 | `					}` |
|      9097 | 4705 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      9092 | 4706 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 4707 | `							/* Static method */` |
|       ! 0 | 4708 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 4709 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 4710 | `					}` |
|      9097 | 4711 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      9092 | 4712 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|         - | 4713 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|         - | 4714 | `							 * HOOKED property declaration. Route anything that is not a` |
|         - | 4715 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|         - | 4716 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|         - | 4717 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|         8 | 4718 | `							if( pGen->pIn < pGen->pEnd` |
|        10 | 4719 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|         4 | 4720 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|        10 | 4721 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        10 | 4722 | `								if( rc != SXRET_OK ){` |
|       ! 0 | 4723 | `									if( rc == SXERR_ABORT ){` |
|       ! 0 | 4724 | `										return SXERR_ABORT;` |
|         - | 4725 | `									}` |
|       ! 0 | 4726 | `									goto done;` |
|         - | 4727 | `								}` |
|        10 | 4728 | `								continue;` |
|         - | 4729 | `							}` |
|       ! 0 | 4730 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 4731 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|       ! 0 | 4732 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 4733 | `							if( rc == SXERR_ABORT ){` |
|         - | 4734 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 4735 | `								return SXERR_ABORT;` |
|         - | 4736 | `							}` |
|       ! 0 | 4737 | `							goto done;` |
|         - | 4738 | `					}` |
|      9089 | 4739 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   2913485 | 4740 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|         - | 4741 | `					/* final method ,record that */` |
|        24 | 4742 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|        24 | 4743 | `					pGen->pIn++; /* Jump the final keyword */` |
|        24 | 4744 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 4745 | `						/* Extract the keyword */` |
|        24 | 4746 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        24 | 4747 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        13 | 4748 | `							iProtection = nKwrd;` |
|        13 | 4749 | `							pGen->pIn++; /* Jump the visibility token */` |
|         5 | 4750 | `						}` |
|        10 | 4751 | `					}` |
|        24 | 4752 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        20 | 4753 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|         - | 4754 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|         - | 4755 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|         - | 4756 | `							 * child class is compiled (PH7_ClassInherit). */` |
|        16 | 4757 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|        16 | 4758 | `							if( rc != SXRET_OK ){` |
|       ! 0 | 4759 | `								if( rc == SXERR_ABORT ){` |
|       ! 0 | 4760 | `									return SXERR_ABORT;` |
|         - | 4761 | `								}` |
|       ! 0 | 4762 | `								goto done;` |
|         - | 4763 | `							}` |
|        16 | 4764 | `							continue;` |
|         - | 4765 | `					}` |
|         9 | 4766 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         6 | 4767 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 4768 | `							/* Static method */` |
|       ! 0 | 4769 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 4770 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 4771 | `					}` |
|         9 | 4772 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         6 | 4773 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 4774 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 4775 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|       ! 0 | 4776 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 4777 | `							if( rc == SXERR_ABORT ){` |
|         - | 4778 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 4779 | `								return SXERR_ABORT;` |
|         - | 4780 | `							}` |
|       ! 0 | 4781 | `							goto done;` |
|         - | 4782 | `					}` |
|         9 | 4783 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         3 | 4784 | `				}` |
|   2990681 | 4785 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 4786 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 4787 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|       ! 0 | 4788 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 4789 | `						if( rc == SXERR_ABORT ){` |
|         - | 4790 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 4791 | `							return SXERR_ABORT;` |
|         - | 4792 | `						}` |
|       ! 0 | 4793 | `						goto done;` |
|         - | 4794 | `				}` |
|   2990681 | 4795 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|         7 | 4796 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|         7 | 4797 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|       ! 0 | 4798 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 4799 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 4800 | `						if( rc == SXERR_ABORT ){` |
|         - | 4801 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 4802 | `							return SXERR_ABORT;` |
|         - | 4803 | `						}` |
|       ! 0 | 4804 | `						goto done;` |
|         - | 4805 | `					}` |
|         - | 4806 | `					/* Attribute declaration */` |
|         7 | 4807 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         4 | 4808 | `				}else{` |
|         - | 4809 | `					/* Process method declaration */` |
|   2990675 | 4810 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 4811 | `				}` |
|   2990681 | 4812 | `				if( rc != SXRET_OK ){` |
|        20 | 4813 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 4814 | `						return SXERR_ABORT;` |
|         - | 4815 | `					}` |
|        20 | 4816 | `					goto done;` |
|         - | 4817 | `				}` |
|         - | 4818 | `			}` |
|   1663054 | 4819 | `		}else{` |
|         - | 4820 | `			/* Attribute declaration */` |
|       ! 0 | 4821 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 4822 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 4823 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 4824 | `					return SXERR_ABORT;` |
|         - | 4825 | `				}` |
|       ! 0 | 4826 | `				goto done;` |
|         - | 4827 | `			}` |
|         - | 4828 | `		}` |
|         5 | 4829 | `	}` |
|         - | 4830 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|         - | 4831 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|         - | 4832 | `	 */` |
|    518895 | 4833 | `	rc = GenStateApplyTraitUses(&(*pGen),pClass,&aUseEntries);` |
|    518895 | 4834 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 4835 | `		SySetRelease(&aUseEntries);` |
|       ! 0 | 4836 | `		SySetRelease(&aInterfaces);` |
|       ! 0 | 4837 | `		return SXERR_ABORT;` |
|         - | 4838 | `	}` |
|    518895 | 4839 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 4840 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|         - | 4841 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|      4589 | 4842 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|      4589 | 4843 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 4844 | `			SySetRelease(&aUseEntries);` |
|       ! 0 | 4845 | `			SySetRelease(&aInterfaces);` |
|       ! 0 | 4846 | `			return SXERR_ABORT;` |
|         - | 4847 | `		}` |
|      2292 | 4848 | `	}` |
|         - | 4849 | `	/* Reject a php-fatal redeclaration before hoisting the class */` |
|    518895 | 4850 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|         9 | 4851 | `		return SXERR_ABORT;` |
|         - | 4852 | `	}` |
|         - | 4853 | `	/* Install the class */` |
|    518889 | 4854 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    518889 | 4855 | `	if( rc == SXRET_OK ){` |
|         - | 4856 | `		ph7_class **apInterface;` |
|         - | 4857 | `		sxu32 n;` |
|    518889 | 4858 | `		if( pBase ){` |
|         - | 4859 | `			/* Inherit from base class and mark as a subclass */` |
|    290197 | 4860 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|    145096 | 4861 | `		}` |
|    518889 | 4862 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|    745519 | 4863 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|         - | 4864 | `			/* Implements one or more interface */` |
|    226635 | 4865 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    226635 | 4866 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 4867 | `				break;` |
|         - | 4868 | `			}` |
|    113320 | 4869 | `		}` |
|         - | 4870 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|         - | 4871 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|    518889 | 4872 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|      4587 | 4873 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|      4587 | 4874 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 4875 | `				pIntf = pIntf->pNextName;` |
|       ! 0 | 4876 | `			}` |
|      4587 | 4877 | `			if( pIntf ){` |
|      4587 | 4878 | `				PH7_ClassImplement(pClass,pIntf);` |
|      2291 | 4879 | `			}` |
|      4587 | 4880 | `			if( pClass->nEnumBacking != 0 ){` |
|      4561 | 4881 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|      4561 | 4882 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 4883 | `					pIntf = pIntf->pNextName;` |
|       ! 0 | 4884 | `				}` |
|      4561 | 4885 | `				if( pIntf ){` |
|      4561 | 4886 | `					PH7_ClassImplement(pClass,pIntf);` |
|      2278 | 4887 | `				}` |
|      2278 | 4888 | `			}` |
|      2291 | 4889 | `		}` |
|         - | 4890 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|         - | 4891 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|    518884 | 4892 | `		if( rc == SXRET_OK` |
|    518884 | 4893 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|    518889 | 4894 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|    267343 | 4895 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|         - | 4896 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|    267343 | 4897 | `			if( pStringable ){` |
|    267343 | 4898 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    267343 | 4899 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|         - | 4900 | `				sxu32 i;` |
|    267343 | 4901 | `				int bAlready = 0;` |
|    321683 | 4902 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|     67931 | 4903 | `					if( apImpl[i] == pStringable ){` |
|     13591 | 4904 | `						bAlready = 1;` |
|     13591 | 4905 | `						break;` |
|         - | 4906 | `					}` |
|     27175 | 4907 | `				}` |
|    267343 | 4908 | `				if( !bAlready ){` |
|    253757 | 4909 | `					PH7_ClassImplement(pClass,pStringable);` |
|    126876 | 4910 | `				}` |
|    133669 | 4911 | `			}` |
|    133669 | 4912 | `		}` |
|         - | 4913 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|    518889 | 4914 | `		if( rc == SXRET_OK ){` |
|    518889 | 4915 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|    518889 | 4916 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 4917 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 4918 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 4919 | `				return SXERR_ABORT;` |
|         - | 4920 | `			}` |
|    259442 | 4921 | `		}` |
|         - | 4922 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|    518889 | 4923 | `		if( rc == SXRET_OK ){` |
|    518889 | 4924 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|    518889 | 4925 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 4926 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 4927 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 4928 | `				return SXERR_ABORT;` |
|         - | 4929 | `			}` |
|    259442 | 4930 | `		}` |
|    259442 | 4931 | `	}` |
|    518889 | 4932 | `	SySetRelease(&aUseEntries);` |
|    518889 | 4933 | `	SySetRelease(&aInterfaces);` |
|    518889 | 4934 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 4935 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 4936 | `		return SXERR_ABORT;` |
|         - | 4937 | `	}` |
|    259442 | 4938 | `done:` |
|    518939 | 4939 | `	pGen->pCurClass = pSavedCurClass;` |
|         - | 4940 | `	/* Point beyond the class body */` |
|    518939 | 4941 | `	pGen->pIn = &pEnd[1];` |
|    518939 | 4942 | `	pGen->pEnd = pTmp;` |
|    518939 | 4943 | `	return PH7_OK;` |
|    259484 | 4944 | `}` |
|         - | 4945 | `/* Compile a named class declaration (the common case). */` |
|    518908 | 4946 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|         5 | 4947 | `{` |
|    518913 | 4948 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|         5 | 4949 | `}` |
|         - | 4950 | `/*` |
|         - | 4951 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|         - | 4952 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|         - | 4953 | ` * compile + install the class body once (at compile time, like every other` |
|         - | 4954 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|         - | 4955 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|         - | 4956 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|         - | 4957 | ` */` |
|        56 | 4958 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 | 4959 | `{` |
|         - | 4960 | `	char zName[128];         /* Synthesized class name */` |
|         - | 4961 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|         - | 4962 | `	SyString sName;` |
|         - | 4963 | `	SyToken *pArgStart,*pArgEnd;` |
|        61 | 4964 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|         - | 4965 | `	                              * is keyed to this 'class' token */` |
|         - | 4966 | `	ph7_value *pObj;` |
|        61 | 4967 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 4968 | `	sxu32 nIdx,nLen;` |
|         - | 4969 | `	sxi32 nArg,rc;` |
|        28 | 4970 | `	SXUNUSED(iCompileFlag);` |
|        61 | 4971 | `	if( pGen->pVm->sDeferAnonName.nByte > 0 ){` |
|         - | 4972 | `		/* Deferred re-compile (VmExecDeferredClass): install under the SAME` |
|         - | 4973 | `		 * synthesized name the original site's OP_NEW loads. One-shot. */` |
|         5 | 4974 | `		sName = pGen->pVm->sDeferAnonName;` |
|         5 | 4975 | `		nLen = sName.nByte;` |
|         5 | 4976 | `		pGen->pVm->sDeferAnonName.zString = 0;` |
|         5 | 4977 | `		pGen->pVm->sDeferAnonName.nByte = 0;` |
|         3 | 4978 | `	}else{` |
|         - | 4979 | `		/* Generate a unique anonymous-class name (collision-checked) */` |
|        57 | 4980 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|        57 | 4981 | `		while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 | 4982 | `			nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       ! 0 | 4983 | `		}` |
|        57 | 4984 | `		SyStringInitFromBuf(&sName,zName,nLen);` |
|         - | 4985 | `	}` |
|         - | 4986 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|         - | 4987 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|         - | 4988 | `	 * delimited construct; GenStateCompileClassEx restores both on success.` |
|         - | 4989 | ``	 * Deferral gate: `new class extends \App\Child {}` where the`` |
|         - | 4990 | `	 * parent's autoloader has not RUN yet — capture the class for a runtime` |
|         - | 4991 | `	 * re-compile and keep only the site's argument/OP_NEW emission here. */` |
|        61 | 4992 | `	pArgStart = pArgEnd = 0;` |
|         - | 4993 | `	{` |
|         - | 4994 | `		SySet aMissing;` |
|        61 | 4995 | `		SyToken *pBody = 0;` |
|        61 | 4996 | `		SyToken *pBodyEnd = 0;` |
|         - | 4997 | `		SyBlob sSelfFqn;` |
|        61 | 4998 | `		int bDeferred = 0;` |
|        61 | 4999 | `		SySetInit(&aMissing,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|        61 | 5000 | `		SyBlobInit(&sSelfFqn,&pGen->pVm->sAllocator);` |
|        56 | 5001 | `		if( GenStateScanDeferDeps(pGen,1,PH7_DEFER_KIND_CLASS,&aMissing,&pBody,&pBodyEnd,&sSelfFqn) == SXRET_OK` |
|        61 | 5002 | `		 && SySetUsed(&aMissing) > 0 ){` |
|         8 | 5003 | `			if( &pTokKw[1] < pGen->pEnd && (pTokKw[1].nType & PH7_TK_LPAREN) ){` |
|         3 | 5004 | `				SyToken *pClose = 0;` |
|         3 | 5005 | `				PH7_DelimitNestedTokens(&pTokKw[2],pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|         3 | 5006 | `				if( pClose && pClose < pGen->pEnd ){` |
|         3 | 5007 | `					pArgStart = &pTokKw[2];` |
|         3 | 5008 | `					pArgEnd = pClose;` |
|         1 | 5009 | `				}` |
|         1 | 5010 | `			}` |
|         8 | 5011 | `			rc = GenStateEmitDeferredClass(pGen,0,1,&aMissing,pBodyEnd,0,&sName);` |
|         8 | 5012 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 5013 | `				SySetRelease(&aMissing);` |
|       ! 0 | 5014 | `				SyBlobRelease(&sSelfFqn);` |
|       ! 0 | 5015 | `				return SXERR_ABORT;` |
|         - | 5016 | `			}` |
|         8 | 5017 | `			bDeferred = ( rc == SXRET_OK );` |
|         3 | 5018 | `		}` |
|        61 | 5019 | `		SySetRelease(&aMissing);` |
|        61 | 5020 | `		SyBlobRelease(&sSelfFqn);` |
|        61 | 5021 | `		if( !bDeferred ){` |
|        55 | 5022 | `			rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|        55 | 5023 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 5024 | `				return rc;` |
|         - | 5025 | `			}` |
|         - | 5026 | `			{` |
|         - | 5027 | ``				/* Expression-position attributes (`new #[A] class {…}`) */`` |
|        55 | 5028 | `				ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,sName.zString,nLen,FALSE,0);` |
|        50 | 5029 | `				if( pAnonClass` |
|        55 | 5030 | `				 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 5031 | `					return SXERR_ABORT;` |
|         - | 5032 | `				}` |
|         - | 5033 | `			}` |
|        25 | 5034 | `		}` |
|         - | 5035 | `	}` |
|         - | 5036 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|         - | 5037 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|        61 | 5038 | `	nArg = 0;` |
|        61 | 5039 | `	if( pArgStart < pArgEnd ){` |
|        10 | 5040 | `		SyToken *pSavedIn = pGen->pIn;` |
|        10 | 5041 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|         - | 5042 | `		SyToken *pArgNext;` |
|        10 | 5043 | `		pGen->pIn = pArgStart;` |
|        10 | 5044 | `		pGen->pEnd = pArgEnd;` |
|        18 | 5045 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|        10 | 5046 | `			if( pGen->pIn < pArgNext ){` |
|        10 | 5047 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|        10 | 5048 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 5049 | `					pGen->pIn = pSavedIn;` |
|       ! 0 | 5050 | `					pGen->pEnd = pSavedEnd;` |
|       ! 0 | 5051 | `					return SXERR_ABORT;` |
|         - | 5052 | `				}` |
|        10 | 5053 | `				nArg++;` |
|         4 | 5054 | `			}` |
|        10 | 5055 | `			pGen->pIn = &pArgNext[1];` |
|         2 | 5056 | `		}` |
|        10 | 5057 | `		pGen->pIn = pSavedIn;` |
|        10 | 5058 | `		pGen->pEnd = pSavedEnd;` |
|         4 | 5059 | `	}` |
|         - | 5060 | `	/* Load the synthesized class name */` |
|        61 | 5061 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        61 | 5062 | `	if( pObj == 0 ){` |
|       ! 0 | 5063 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 5064 | `		return SXERR_ABORT;` |
|         - | 5065 | `	}` |
|        61 | 5066 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|        61 | 5067 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - | 5068 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|        61 | 5069 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        61 | 5070 | `	return SXRET_OK;` |
|        33 | 5071 | `}` |
|         - | 5072 | `/*` |
|         - | 5073 | ` * Compile a user-defined abstract class.` |
|         - | 5074 | ` *  According to the PHP language reference manual` |
|         - | 5075 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|         - | 5076 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|         - | 5077 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|         - | 5078 | ` *   the method's signature - they cannot define the implementation.` |
|         - | 5079 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|         - | 5080 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|         - | 5081 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|         - | 5082 | ` *   method is defined as protected, the function implementation must be defined as either` |
|         - | 5083 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|         - | 5084 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|         - | 5085 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|         - | 5086 | ` *   could differ.` |
|         - | 5087 | ` */` |
|         - | 5088 | `/*` |
|         - | 5089 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|         - | 5090 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|         - | 5091 | ` * receives the corresponding PH7_CLASS_* bit.` |
|         - | 5092 | ` */` |
|  16835024 | 5093 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|         5 | 5094 | `{` |
|  16835029 | 5095 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  10012025 | 5096 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  10012025 | 5097 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|   9948575 | 5098 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|   4951574 | 5099 | `	}` |
|  16726157 | 5100 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  16726097 | 5101 | `	return FALSE;` |
|   8417517 | 5102 | `}` |
|         - | 5103 | `/*` |
|         - | 5104 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|         - | 5105 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|         - | 5106 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|         - | 5107 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|         - | 5108 | ` */` |
|  16726092 | 5109 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|         5 | 5110 | `{` |
|  16726097 | 5111 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  16726097 | 5112 | `	sxi32 iFlags = 0,iFlag;` |
|  16835029 | 5113 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    108937 | 5114 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|         5 | 5115 | `			pDup = pIn;` |
|         2 | 5116 | `		}` |
|    108937 | 5117 | `		iFlags \|= iFlag;` |
|    108937 | 5118 | `		pIn++;` |
|         5 | 5119 | `	}` |
|  16726097 | 5120 | `	*ppIn = pIn;` |
|  16726097 | 5121 | `	if( ppDup ){ *ppDup = pDup; }` |
|  16726097 | 5122 | `	return iFlags;` |
|         5 | 5123 | `}` |
|         - | 5124 | `/*` |
|         - | 5125 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|         - | 5126 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|         - | 5127 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|         - | 5128 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|         - | 5129 | `` * `readonly`) to their existing handlers.`` |
|         - | 5130 | ` */` |
|  16676164 | 5131 | `PH7_PRIVATE int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|         5 | 5132 | `{` |
|  16676169 | 5133 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|   8397073 | 5134 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  16705658 | 5135 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|         5 | 5136 | `}` |
|         - | 5137 | `/*` |
|         - | 5138 | ` * Compile a class declaration carrying one or more leading modifiers` |
|         - | 5139 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|         - | 5140 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|         - | 5141 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|         - | 5142 | `` * `abstract`+`final` pair, like PHP.`` |
|         - | 5143 | ` */` |
|     49928 | 5144 | `PH7_PRIVATE sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|         5 | 5145 | `{` |
|         - | 5146 | `	SyToken *pDup;` |
|     49933 | 5147 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|         - | 5148 | `	sxi32 rc;` |
|     49933 | 5149 | `	if( pDup ){` |
|         4 | 5150 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|         2 | 5151 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|         3 | 5152 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 5153 | `			return SXERR_ABORT;` |
|         - | 5154 | `		}` |
|         1 | 5155 | `	}` |
|     49928 | 5156 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|     24969 | 5157 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|         3 | 5158 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 5159 | `			"Cannot use the final modifier on an abstract class");` |
|         3 | 5160 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 5161 | `			return SXERR_ABORT;` |
|         - | 5162 | `		}` |
|         1 | 5163 | `	}` |
|     49933 | 5164 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|     24969 | 5165 | `}` |
|         - | 5166 | `/*` |
|         - | 5167 | ` * Compile a user-defined trait.` |
|         - | 5168 | ` *  Traits are similar to classes, but only intended to group functionality` |
|         - | 5169 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|         - | 5170 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|         - | 5171 | ` */` |
|      9214 | 5172 | `PH7_PRIVATE sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|         5 | 5173 | `{` |
|      9219 | 5174 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 5175 | `	ph7_class *pClass;` |
|      9219 | 5176 | `	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' */` |
|         - | 5177 | `	SyToken *pEnd,*pTmp;` |
|         - | 5178 | `	sxi32 iProtection;` |
|         - | 5179 | `	sxi32 iAttrflags;` |
|         - | 5180 | ``	SySet aUseEntries; /* trait-body `use` statements (incl. adaptation blocks) */`` |
|         - | 5181 | `	SyString *pName;` |
|         - | 5182 | `	sxi32 nKwrd;` |
|         - | 5183 | `	sxi32 rc;` |
|         - | 5184 | `	{` |
|         - | 5185 | `		/* Deferral gate: a used trait may need an autoloader that` |
|         - | 5186 | `		 * has not run yet. */` |
|         - | 5187 | `		sxi32 rcDefer;` |
|      9219 | 5188 | `		if( GenStateMaybeDeferClass(pGen,0,PH7_DEFER_KIND_TRAIT,&rcDefer) ){` |
|         5 | 5189 | `			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - | 5190 | `		}` |
|         - | 5191 | `	}` |
|      9215 | 5192 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|         - | 5193 | `	/* Jump the 'trait' keyword */` |
|      9215 | 5194 | `	pGen->pIn++;` |
|      9215 | 5195 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 5196 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|       ! 0 | 5197 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 5198 | `			return SXERR_ABORT;` |
|         - | 5199 | `		}` |
|       ! 0 | 5200 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|       ! 0 | 5201 | `			pGen->pIn++;` |
|       ! 0 | 5202 | `		}` |
|       ! 0 | 5203 | `		return SXRET_OK;` |
|         - | 5204 | `	}` |
|         - | 5205 | `	/* Extract trait name */` |
|      9215 | 5206 | `	pName = &pGen->pIn->sData;` |
|      9215 | 5207 | `	pGen->pIn++;` |
|         - | 5208 | `	/* Build FQN and obtain a raw class */ {` |
|         - | 5209 | `		SyBlob sFQN;` |
|         - | 5210 | `		SyString sFQNStr;` |
|      9215 | 5211 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      9215 | 5212 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      9215 | 5213 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|         - | 5214 | ``		/* php refuses a declaration whose short name a local `use` already took. */`` |
|      9215 | 5215 | `		if( GenStateGuardImportRedeclare(pGen,0,pName,&sFQNStr,nLine) == SXERR_ABORT ){` |
|       ! 0 | 5216 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 5217 | `			return SXERR_ABORT;` |
|         - | 5218 | `		}` |
|      9215 | 5219 | `		GenStateRecordDeclaredName(pGen,0,&sFQNStr);` |
|      9215 | 5220 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      9215 | 5221 | `		SyBlobRelease(&sFQN);` |
|         - | 5222 | `	}` |
|      9215 | 5223 | `	if( pClass == 0 ){` |
|       ! 0 | 5224 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 5225 | `		return SXERR_ABORT;` |
|         - | 5226 | `	}` |
|      9215 | 5227 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|      9215 | 5228 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 5229 | `		return SXERR_ABORT;` |
|         - | 5230 | `	}` |
|         - | 5231 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      9215 | 5232 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 | 5233 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|       ! 0 | 5234 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 5235 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 5236 | `			return SXERR_ABORT;` |
|         - | 5237 | `		}` |
|       ! 0 | 5238 | `		return SXRET_OK;` |
|         - | 5239 | `	}` |
|      9215 | 5240 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      9215 | 5241 | `	pEnd = 0;` |
|      9215 | 5242 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      9215 | 5243 | `	if( pEnd >= pGen->pEnd ){` |
|       ! 0 | 5244 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|       ! 0 | 5245 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 5246 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 5247 | `			return SXERR_ABORT;` |
|         - | 5248 | `		}` |
|       ! 0 | 5249 | `		return SXRET_OK;` |
|         - | 5250 | `	}` |
|         - | 5251 | `	/* The delimiter token is the trait body's closing brace */` |
|      9215 | 5252 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 5253 | `	/* Swap token stream */` |
|      9215 | 5254 | `	pTmp = pGen->pEnd;` |
|      9215 | 5255 | `	pGen->pEnd = pEnd;` |
|         - | 5256 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|      9215 | 5257 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|         - | 5258 | `	/* This trait is now the lexical class for its body, so a property/parameter` |
|         - | 5259 | `	 * default here resolves __TRAIT__ to it (see pCurClass). */` |
|      9215 | 5260 | `	pGen->pCurClass = pClass;` |
|         - | 5261 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|     65819 | 5262 | `	for(;;){` |
|    186057 | 5263 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     27213 | 5264 | `			pGen->pIn++;` |
|         5 | 5265 | `		}` |
|    158849 | 5266 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      9215 | 5267 | `			break;` |
|         - | 5268 | `		}` |
|         - | 5269 | `		/* Bind a directly-preceding docblock to this member */` |
|    149639 | 5270 | `		GenStateSetPendingDoc(&(*pGen));` |
|    149639 | 5271 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|       ! 0 | 5272 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 5273 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 5274 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 5275 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 5276 | `				return SXERR_ABORT;` |
|         - | 5277 | `			}` |
|       ! 0 | 5278 | `			goto done;` |
|         - | 5279 | `		}` |
|    149639 | 5280 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|    149639 | 5281 | `		iAttrflags = 0;` |
|    149639 | 5282 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|    149639 | 5283 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    149639 | 5284 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 5285 | `				/* Trait uses another trait: use T[, T2] [{ resolution }]; A trait` |
|         - | 5286 | `				 * name is a full class reference — qualified or fully-qualified` |
|         - | 5287 | ``				 * (`use Foo\T;`, `use \Foo\T;`) — so parse it with the shared`` |
|         - | 5288 | `				 * class-reference reader like the CLASS body's trait-use does` |
|         - | 5289 | `				 * (the old single-identifier read choked on the leading '\'),` |
|         - | 5290 | `				 * and collect a TraitUseEntry so an adaptation block` |
|         - | 5291 | `				 * (insteadof/as) applies through the same shared machinery. */` |
|         - | 5292 | `				TraitUseEntry sUse;` |
|        16 | 5293 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|        16 | 5294 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|        16 | 5295 | `				pGen->pIn++; /* Jump 'use' */` |
|        10 | 5296 | `				for(;;){` |
|         - | 5297 | `					ph7_class *pUsedTrait;` |
|         - | 5298 | `					SyBlob sResolved;` |
|         - | 5299 | `					SyString sUsedName;` |
|        20 | 5300 | `					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|        20 | 5301 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        20 | 5302 | `					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 5303 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 5304 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|       ! 0 | 5305 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|       ! 0 | 5306 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 5307 | `							return SXERR_ABORT;` |
|         - | 5308 | `						}` |
|       ! 0 | 5309 | `						break;` |
|         - | 5310 | `					}` |
|        36 | 5311 | `					pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|        16 | 5312 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|        20 | 5313 | `					SyStringInitFromBuf(&sUsedName,` |
|         - | 5314 | `						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        20 | 5315 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 5316 | `						pUsedTrait = pUsedTrait->pNextName;` |
|       ! 0 | 5317 | `					}` |
|        20 | 5318 | `					if( pUsedTrait == 0 ){` |
|       ! 0 | 5319 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|         - | 5320 | `							"'%z' is not a trait",&sUsedName);` |
|       ! 0 | 5321 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 5322 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 5323 | `							return SXERR_ABORT;` |
|         - | 5324 | `						}` |
|       ! 0 | 5325 | `					}else{` |
|        20 | 5326 | `						SySetPut(&sUse.aTraits,(const void *)&pUsedTrait);` |
|         - | 5327 | `					}` |
|        20 | 5328 | `					SyBlobRelease(&sResolved);` |
|        20 | 5329 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|        10 | 5330 | `						break;` |
|         - | 5331 | `					}` |
|         6 | 5332 | `					pGen->pIn++;` |
|         2 | 5333 | `				}` |
|         - | 5334 | `				/* Optional adaptation block (conflict resolution) */` |
|        16 | 5335 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - | 5336 | `					SyToken *pBlock;` |
|         5 | 5337 | `					pGen->pIn++; /* Jump '{' */` |
|         5 | 5338 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|         5 | 5339 | `					sUse.pResolvStart = pGen->pIn;` |
|         5 | 5340 | `					sUse.pResolvEnd = pBlock;` |
|         5 | 5341 | `					if( pBlock < pGen->pEnd ){` |
|         5 | 5342 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|         3 | 5343 | `					}else{` |
|       ! 0 | 5344 | `						pGen->pIn = pGen->pEnd;` |
|         - | 5345 | `					}` |
|         2 | 5346 | `				}` |
|        16 | 5347 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|        16 | 5348 | `				continue;` |
|         - | 5349 | `			}` |
|    149627 | 5350 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|    149605 | 5351 | `				iProtection = nKwrd;` |
|    149605 | 5352 | `				pGen->pIn++;` |
|         - | 5353 | ``				/* Optional `readonly` after the visibility (PHP 8.1): `private readonly T`` |
|         - | 5354 | ``				 * $x` — the generated PHPUnit runtime traits (StubApi, …) declare their`` |
|         - | 5355 | `				 * readonly state this way. Mirrors the class-body attribute parser. */` |
|    149605 | 5356 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|         5 | 5357 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|         5 | 5358 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|         2 | 5359 | `				}` |
|    149600 | 5360 | `				if( pGen->pIn >= pGen->pEnd` |
|    149605 | 5361 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 5362 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 5363 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 5364 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 5365 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 5366 | `						return SXERR_ABORT;` |
|         - | 5367 | `					}` |
|       ! 0 | 5368 | `					goto done;` |
|         - | 5369 | `				}` |
|    149605 | 5370 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     27189 | 5371 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     27189 | 5372 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 5373 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 5374 | `							return SXERR_ABORT;` |
|         - | 5375 | `						}` |
|       ! 0 | 5376 | `						goto done;` |
|         - | 5377 | `					}` |
|     27189 | 5378 | `					continue;` |
|         - | 5379 | `				}` |
|    122421 | 5380 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         9 | 5381 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         9 | 5382 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 5383 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 5384 | `							return SXERR_ABORT;` |
|         - | 5385 | `						}` |
|       ! 0 | 5386 | `						goto done;` |
|         - | 5387 | `					}` |
|         9 | 5388 | `					continue;` |
|         - | 5389 | `				}` |
|    122413 | 5390 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     61204 | 5391 | `			}` |
|    122435 | 5392 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       ! 0 | 5393 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 5394 | `					"Traits cannot have constants");` |
|       ! 0 | 5395 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 5396 | `					return SXERR_ABORT;` |
|         - | 5397 | `				}` |
|       ! 0 | 5398 | `				goto done;` |
|       ! 0 | 5399 | `			}else{` |
|    122435 | 5400 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|      9073 | 5401 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      9073 | 5402 | `					pGen->pIn++;` |
|      9073 | 5403 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      9071 | 5404 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      9071 | 5405 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 5406 | `							iProtection = nKwrd;` |
|       ! 0 | 5407 | `							pGen->pIn++;` |
|       ! 0 | 5408 | `						}` |
|      4533 | 5409 | `					}` |
|      9068 | 5410 | `					if( pGen->pIn >= pGen->pEnd` |
|      9073 | 5411 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 5412 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 5413 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|       ! 0 | 5414 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 5415 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 5416 | `							return SXERR_ABORT;` |
|         - | 5417 | `						}` |
|       ! 0 | 5418 | `						goto done;` |
|         - | 5419 | `					}` |
|      9073 | 5420 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         3 | 5421 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         3 | 5422 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 5423 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 5424 | `								return SXERR_ABORT;` |
|         - | 5425 | `							}` |
|       ! 0 | 5426 | `							goto done;` |
|         - | 5427 | `						}` |
|         3 | 5428 | `						continue;` |
|         - | 5429 | `					}` |
|      9071 | 5430 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       ! 0 | 5431 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 5432 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 5433 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 5434 | `								return SXERR_ABORT;` |
|         - | 5435 | `							}` |
|       ! 0 | 5436 | `							goto done;` |
|         - | 5437 | `						}` |
|       ! 0 | 5438 | `						continue;` |
|         - | 5439 | `					}` |
|      9071 | 5440 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    117900 | 5441 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         9 | 5442 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         9 | 5443 | `					pGen->pIn++;` |
|         9 | 5444 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         9 | 5445 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         9 | 5446 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         9 | 5447 | `							iProtection = nKwrd;` |
|         9 | 5448 | `							pGen->pIn++;` |
|         3 | 5449 | `						}` |
|         3 | 5450 | `					}` |
|         9 | 5451 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         6 | 5452 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 5453 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 5454 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|       ! 0 | 5455 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 5456 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 5457 | `							return SXERR_ABORT;` |
|         - | 5458 | `						}` |
|       ! 0 | 5459 | `						goto done;` |
|         - | 5460 | `					}` |
|         9 | 5461 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         3 | 5462 | `				}` |
|    122433 | 5463 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 5464 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 5465 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|       ! 0 | 5466 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 5467 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 5468 | `						return SXERR_ABORT;` |
|         - | 5469 | `					}` |
|       ! 0 | 5470 | `					goto done;` |
|         - | 5471 | `				}` |
|    122433 | 5472 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       ! 0 | 5473 | `					pGen->pIn++;` |
|       ! 0 | 5474 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 | 5475 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 5476 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 5477 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 5478 | `							return SXERR_ABORT;` |
|         - | 5479 | `						}` |
|       ! 0 | 5480 | `						goto done;` |
|         - | 5481 | `					}` |
|       ! 0 | 5482 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 5483 | `				}else{` |
|    122433 | 5484 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 5485 | `				}` |
|    122433 | 5486 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 5487 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 5488 | `						return SXERR_ABORT;` |
|         - | 5489 | `					}` |
|       ! 0 | 5490 | `					goto done;` |
|         - | 5491 | `				}` |
|         - | 5492 | `			}` |
|     61219 | 5493 | `		}else{` |
|       ! 0 | 5494 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 5495 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 5496 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 5497 | `					return SXERR_ABORT;` |
|         - | 5498 | `				}` |
|       ! 0 | 5499 | `				goto done;` |
|         - | 5500 | `			}` |
|         - | 5501 | `		}` |
|         5 | 5502 | `	}` |
|         - | 5503 | ``	/* Apply the collected `use` entries (incl. adaptation blocks) through the`` |
|         - | 5504 | `	 * machinery shared with the class-body compiler. */` |
|      9215 | 5505 | `	rc = GenStateApplyTraitUses(&(*pGen),pClass,&aUseEntries);` |
|      9215 | 5506 | `	SySetRelease(&aUseEntries);` |
|      9215 | 5507 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 5508 | `		return SXERR_ABORT;` |
|         - | 5509 | `	}` |
|         - | 5510 | `	/* Reject a php-fatal redeclaration before hoisting the trait */` |
|      9215 | 5511 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|         3 | 5512 | `		return SXERR_ABORT;` |
|         - | 5513 | `	}` |
|         - | 5514 | `	/* Install the trait */` |
|      9213 | 5515 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      9213 | 5516 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 5517 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 5518 | `		return SXERR_ABORT;` |
|         - | 5519 | `	}` |
|      4604 | 5520 | `done:` |
|      9213 | 5521 | `	pGen->pCurClass = pSavedCurClass;` |
|         - | 5522 | `	/* Point beyond the trait body */` |
|      9213 | 5523 | `	pGen->pIn = &pEnd[1];` |
|      9213 | 5524 | `	pGen->pEnd = pTmp;` |
|      9213 | 5525 | `	return PH7_OK;` |
|      4612 | 5526 | `}` |
|         - | 5527 | `/*` |
|         - | 5528 | ` * Compile a user-defined class.` |
|         - | 5529 | ` *  According to the PHP language reference manual` |
|         - | 5530 | ` *   Basic class definitions begin with the keyword class, followed` |
|         - | 5531 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|         - | 5532 | ` *   the definitions of the properties and methods belonging to the class.` |
|         - | 5533 | ` *   A class may contain its own constants, variables (called "properties")` |
|         - | 5534 | ` *   and functions (called "methods").` |
|         - | 5535 | ` */` |
|    464390 | 5536 | `PH7_PRIVATE sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|         5 | 5537 | `{` |
|         - | 5538 | `	sxi32 rc;` |
|    464395 | 5539 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|    464395 | 5540 | `	return rc;` |
|         5 | 5541 | `}` |
|         - | 5542 | `/*` |
|         - | 5543 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|         - | 5544 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|         - | 5545 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|         - | 5546 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|         - | 5547 | `` * meaning; `enum Name` can never start a valid expression.`` |
|         - | 5548 | ` */` |
|  16617180 | 5549 | `PH7_PRIVATE int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|         5 | 5550 | `{` |
|  16875503 | 5551 | `	return (pIn->nType & PH7_TK_ID)` |
|   8566908 | 5552 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    269958 | 5553 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  16875498 | 5554 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|         5 | 5555 | `}` |
|         - | 5556 | `/*` |
|         - | 5557 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|         - | 5558 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|         - | 5559 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|         - | 5560 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|         - | 5561 | ` */` |
|      4590 | 5562 | `PH7_PRIVATE sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|         5 | 5563 | `{` |
|      4595 | 5564 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|         5 | 5565 | `}` |
|         - | 5566 |  |
