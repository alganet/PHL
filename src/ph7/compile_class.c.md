# src/ph7/compile_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2500/3273 lines (76.38%)

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
|    565548 |   31 | `static sxi32 GenStateGuardClassRedeclaration(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 |   32 | `{` |
|         - |   33 | `	SyHashEntry *pEntry;` |
|    565553 |   34 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|        41 |   35 | `		return SXRET_OK; /* conditional/nested: keep hoisting */` |
|         - |   36 | `	}` |
|    565517 |   37 | `	pClass->iFlags \|= PH7_CLASS_BOUND;` |
|    565517 |   38 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|    563045 |   39 | `		return SXRET_OK; /* the prelude installs each builtin exactly once */` |
|         - |   40 | `	}` |
|      2477 |   41 | `	pEntry = SyHashGet(&pGen->pVm->hClass,(const void *)pClass->sName.zString,pClass->sName.nByte);` |
|      2477 |   42 | `	if( pEntry ){` |
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
|      2467 |   62 | `	return SXRET_OK;` |
|    282779 |   63 | `}` |
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
|   4085708 |   75 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|         5 |   76 | `{` |
|   4085713 |   77 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    298319 |   78 | `		return PH7_CLASS_PROT_PRIVATE;` |
|   3787399 |   79 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|    236089 |   80 | `		return PH7_CLASS_PROT_PROTECTED;` |
|         - |   81 | `	}` |
|         - |   82 | `	/* Assume public by default */` |
|   3551315 |   83 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   2042859 |   84 | `}` |
|         - |   85 | `/*` |
|         - |   86 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|         - |   87 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|         - |   88 | ` * token immediately followed by '='. Anything else with a leading type token` |
|         - |   89 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|         - |   90 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|         - |   91 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|         - |   92 | ` */` |
|    364640 |   93 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|         5 |   94 | `{` |
|         - |   95 | `	SyToken *p0, *p1;` |
|    364645 |   96 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |   97 | `		return 0;` |
|         - |   98 | `	}` |
|    364645 |   99 | `	p0 = pGen->pIn;` |
|         - |  100 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|    364645 |  101 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|       ! 0 |  102 | `		return 1;` |
|         - |  103 | `	}` |
|    364645 |  104 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|         5 |  105 | `		return 1;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* A name-like first token begins a type only when followed by another` |
|         - |  108 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|         - |  109 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|    364641 |  110 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|    364641 |  111 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|    364641 |  112 | `		if( p1 ){` |
|    364641 |  113 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|        36 |  114 | `				return 1;` |
|         - |  115 | `			}` |
|    364609 |  116 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|         5 |  117 | `				return 1;` |
|         - |  118 | `			}` |
|    182300 |  119 | `		}` |
|    182300 |  120 | `	}` |
|    364605 |  121 | `	return 0;` |
|    182325 |  122 | `}` |
|         - |  123 | `/*` |
|         - |  124 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|         - |  125 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|         - |  126 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|         - |  127 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|         - |  128 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|         - |  129 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|         - |  130 | ` * Peek only; never consumes tokens.` |
|         - |  131 | ` */` |
|        26 |  132 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|         4 |  133 | `{` |
|        30 |  134 | `	SyToken *p = pGen->pIn;` |
|        42 |  135 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        21 |  136 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|         3 |  137 | `		p++; /* skip leading unary sign(s) */` |
|         1 |  138 | `	}` |
|        30 |  139 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|        25 |  140 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|         - |  141 | `	}` |
|         6 |  142 | `	p++;` |
|         - |  143 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|         6 |  144 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|        17 |  145 | `}` |
|         - |  146 | `/*` |
|         - |  147 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|         - |  148 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|         - |  149 | `` * `$o->new`), not a `new` expression.`` |
|         - |  150 | ` */` |
|       110 |  151 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|         3 |  152 | `{` |
|         - |  153 | `	sxi32 iOp;` |
|       113 |  154 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|        11 |  155 | `		return 0;` |
|         - |  156 | `	}` |
|       103 |  157 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       103 |  158 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|        58 |  159 | `}` |
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
|    771242 |  177 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|         5 |  178 | `{` |
|    771247 |  179 | `	SyToken *p = pGen->pIn;` |
|    771247 |  180 | `	int iDepth = 0;` |
|   2036099 |  181 | `	while( p < pGen->pEnd ){` |
|   2036099 |  182 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    771195 |  183 | `			break; /* end of this initializer */` |
|         - |  184 | `		}` |
|   1264904 |  185 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    640764 |  186 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|     16609 |  187 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|         - |  188 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|         - |  189 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|         - |  190 | `			 * expression. */` |
|        12 |  191 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|        12 |  192 | `			p++;` |
|        12 |  193 | `			if( bArrow ){` |
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
|        12 |  215 | `				int iLocal = 0;` |
|        32 |  216 | `				while( p < pGen->pEnd ){` |
|        32 |  217 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|        12 |  218 | `						break; /* body brace */` |
|         - |  219 | `					}` |
|        22 |  220 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        12 |  221 | `						iLocal++;` |
|        17 |  222 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        12 |  223 | `						if( iLocal > 0 ){` |
|        12 |  224 | `							iLocal--;` |
|         5 |  225 | `						}` |
|         5 |  226 | `					}` |
|        22 |  227 | `					p++;` |
|         2 |  228 | `				}` |
|        12 |  229 | `				if( p < pGen->pEnd ){` |
|        12 |  230 | `					int iBrace = 0; /* p is on the body '{' */` |
|        94 |  231 | `					while( p < pGen->pEnd ){` |
|        94 |  232 | `						if( p->nType & PH7_TK_OCB ){` |
|        14 |  233 | `							iBrace++;` |
|        88 |  234 | `						}else if( p->nType & PH7_TK_CCB ){` |
|        14 |  235 | `							iBrace--;` |
|        14 |  236 | `							if( iBrace == 0 ){` |
|        12 |  237 | `								p++;` |
|        12 |  238 | `								break;` |
|         - |  239 | `							}` |
|         1 |  240 | `						}` |
|        84 |  241 | `						p++;` |
|         2 |  242 | `					}` |
|         5 |  243 | `				}` |
|         - |  244 | `			}` |
|        12 |  245 | `			continue;` |
|         - |  246 | `		}` |
|   1264899 |  247 | `		if( p->nType & PH7_TK_OCB ){` |
|        45 |  248 | `			if( iDepth == 0 ){` |
|         - |  249 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|         - |  250 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|         - |  251 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|         - |  252 | `				 * is legal — don't scan into it. */` |
|        45 |  253 | `				break;` |
|         - |  254 | `			}` |
|       ! 0 |  255 | `			iDepth++;` |
|   1264855 |  256 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     53927 |  257 | `			iDepth++;` |
|   1237894 |  258 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     53925 |  259 | `			if( iDepth > 0 ){` |
|     53925 |  260 | `				iDepth--;` |
|     26960 |  261 | `			}` |
|   1183973 |  262 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    416863 |  263 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|         - |  264 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|         - |  265 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|         - |  266 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|        11 |  267 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|        11 |  268 | `				return 1;` |
|         - |  269 | `			}` |
|       ! 0 |  270 | `		}` |
|   1264847 |  271 | `		p++;` |
|         5 |  272 | `	}` |
|    771239 |  273 | `	return 0;` |
|    385626 |  274 | `}` |
|         - |  275 | `/*` |
|         - |  276 | ` * php keeps class CONSTANTS and PROPERTIES in separate namespaces: a class may` |
|         - |  277 | `` * declare `const C` and `public $C` together, and `$obj->C` never resolves to the`` |
|         - |  278 | ` * constant. PHL stores each in its own table (constants in hConst, properties in` |
|         - |  279 | ` * hAttr), so these two lookups target the right namespace and never collide.` |
|         - |  280 | ` */` |
|    364632 |  281 | `static ph7_class_attr * GenStateExtractConstant(ph7_class *pClass,SyString *pName)` |
|         5 |  282 | `{` |
|    364637 |  283 | `	return PH7_ClassExtractConstant(pClass,pName->zString,pName->nByte);` |
|         5 |  284 | `}` |
|    630598 |  285 | `static ph7_class_attr * GenStateExtractProperty(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|         5 |  286 | `{` |
|    630603 |  287 | `	return PH7_ClassExtractAttribute(pClass,zName,nByte);` |
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
|    771338 |  310 | `PH7_PRIVATE int PH7_GenStateInitHasCallExpr(ph7_gen_state *pGen)` |
|         5 |  311 | `{` |
|    771343 |  312 | `	SyToken *p = pGen->pIn;` |
|    771343 |  313 | `	int iDepth = 0;` |
|         - |  314 | `	/* Conservative ternary bail-out (see the note above). */` |
|         - |  315 | `	{` |
|    771343 |  316 | `		SyToken *q = pGen->pIn;` |
|    771343 |  317 | `		int iQd = 0;` |
|   2037675 |  318 | `		while( q < pGen->pEnd ){` |
|   2037637 |  319 | `			if( iQd == 0 && (q->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    771299 |  320 | `				break;` |
|         - |  321 | `			}` |
|   1266343 |  322 | `			if( q->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     54097 |  323 | `				iQd++;` |
|   1239297 |  324 | `			}else if( q->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     54097 |  325 | `				if( iQd > 0 ){ iQd--; }` |
|   1185205 |  326 | `			}else if( (q->nType & PH7_TK_OP) && q->pUserData` |
|    417099 |  327 | `				&& ((const ph7_expr_op *)q->pUserData)->iOp == EXPR_OP_QUESTY ){` |
|         8 |  328 | `				return 0;` |
|         - |  329 | `			}` |
|   1266337 |  330 | `			q++;` |
|         5 |  331 | `		}` |
|         - |  332 | `	}` |
|   2036403 |  333 | `	while( p < pGen->pEnd ){` |
|   2036403 |  334 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    771291 |  335 | `			break; /* end of this initializer */` |
|         - |  336 | `		}` |
|   1265112 |  337 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    640872 |  338 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|     16615 |  339 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|         - |  340 | `			/* A call inside a closure/arrow-fn is deferred to call time: skip the` |
|         - |  341 | `			 * whole construct. Delegating to the sibling scanner is not possible` |
|         - |  342 | ``			 * (it reports `new`), so mirror its bracket walk. */`` |
|        16 |  343 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|        16 |  344 | `			int iBase = iDepth;` |
|        16 |  345 | `			p++;` |
|        16 |  346 | `			if( bArrow ){` |
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
|        16 |  361 | `				int iLocal = 0;` |
|        44 |  362 | `				while( p < pGen->pEnd ){` |
|        44 |  363 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|        16 |  364 | `						break;` |
|         - |  365 | `					}` |
|        30 |  366 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        16 |  367 | `						iLocal++;` |
|        23 |  368 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        16 |  369 | `						if( iLocal > 0 ){ iLocal--; }` |
|         7 |  370 | `					}` |
|        30 |  371 | `					p++;` |
|         2 |  372 | `				}` |
|        16 |  373 | `				if( p < pGen->pEnd ){` |
|        16 |  374 | `					int iBrace = 0;` |
|       120 |  375 | `					while( p < pGen->pEnd ){` |
|       120 |  376 | `						if( p->nType & PH7_TK_OCB ){` |
|        18 |  377 | `							iBrace++;` |
|       112 |  378 | `						}else if( p->nType & PH7_TK_CCB ){` |
|        18 |  379 | `							iBrace--;` |
|        18 |  380 | `							if( iBrace == 0 ){` |
|        16 |  381 | `								p++;` |
|        16 |  382 | `								break;` |
|         - |  383 | `							}` |
|         1 |  384 | `						}` |
|       106 |  385 | `						p++;` |
|         2 |  386 | `					}` |
|         7 |  387 | `				}` |
|         - |  388 | `			}` |
|        16 |  389 | `			continue;` |
|         - |  390 | `		}` |
|   1265103 |  391 | `		if( p->nType & PH7_TK_OCB ){` |
|        43 |  392 | `			if( iDepth == 0 ){` |
|        43 |  393 | `				break; /* property-hook list: the default expression ends here */` |
|         - |  394 | `			}` |
|       ! 0 |  395 | `			iDepth++;` |
|   1265061 |  396 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|         - |  397 | `			/* A '(' directly after a NAME is a call. A name here is an identifier` |
|         - |  398 | ``			 * token; `new X(` is excluded by looking for the `new` operator, and`` |
|         - |  399 | ``			 * `f(...)` (first-class callable) by peeking for a lone ellipsis. */`` |
|     53946 |  400 | `			if( (p->nType & PH7_TK_LPAREN) && p > pGen->pIn` |
|     16592 |  401 | `				&& (p[-1].nType & PH7_TK_ID)` |
|      8309 |  402 | `				&& !((p[-1].nType & PH7_TK_OP) && p[-1].pUserData` |
|       ! 0 |  403 | `					&& ((const ph7_expr_op *)p[-1].pUserData)->iOp == EXPR_OP_NEW) ){` |
|        19 |  404 | `				int bNewCtor = 0;` |
|        19 |  405 | `				SyToken *q = &p[-1];` |
|         - |  406 | ``				/* Walk back over a qualified name (A\B, A::b, $o->m) to a `new`. */`` |
|        19 |  407 | `				while( q > pGen->pIn && (q[-1].nType & (PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) ){` |
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
|        14 |  419 | `					&& !(&p[1] < pGen->pEnd && (p[1].nType & PH7_TK_ELLIPSIS)) ){` |
|         6 |  420 | `					return 1;` |
|         - |  421 | `				}` |
|         6 |  422 | `			}` |
|     53947 |  423 | `			iDepth++;` |
|   1238086 |  424 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     53947 |  425 | `			if( iDepth > 0 ){` |
|     53947 |  426 | `				iDepth--;` |
|     26971 |  427 | `			}` |
|     26971 |  428 | `		}` |
|   1265057 |  429 | `		p++;` |
|         5 |  430 | `	}` |
|    771333 |  431 | `	return 0;` |
|    385674 |  432 | `}` |
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
|    771344 |  450 | `PH7_PRIVATE int PH7_GenStateInitClosureError(ph7_gen_state *pGen)` |
|         5 |  451 | `{` |
|    771349 |  452 | `	SyToken *p = pGen->pIn;` |
|    771349 |  453 | `	int iDepth = 0;` |
|   2036453 |  454 | `	while( p < pGen->pEnd ){` |
|   2036453 |  455 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    771299 |  456 | `			break; /* end of this initializer */` |
|         - |  457 | `		}` |
|   1265154 |  458 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    640897 |  459 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|     16621 |  460 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
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
|        16 |  475 | `			p++;` |
|         - |  476 | `			{` |
|        16 |  477 | `				int iLocal = 0;` |
|        44 |  478 | `				while( p < pGen->pEnd ){` |
|        44 |  479 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|        16 |  480 | `						break;` |
|         - |  481 | `					}` |
|        30 |  482 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        16 |  483 | `						iLocal++;` |
|        23 |  484 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        16 |  485 | `						if( iLocal > 0 ){ iLocal--; }` |
|         7 |  486 | `					}` |
|        30 |  487 | `					p++;` |
|         2 |  488 | `				}` |
|        16 |  489 | `				if( p < pGen->pEnd ){` |
|        16 |  490 | `					int iBrace = 0;` |
|       120 |  491 | `					while( p < pGen->pEnd ){` |
|       120 |  492 | `						if( p->nType & PH7_TK_OCB ){` |
|        18 |  493 | `							iBrace++;` |
|       112 |  494 | `						}else if( p->nType & PH7_TK_CCB ){` |
|        18 |  495 | `							iBrace--;` |
|        18 |  496 | `							if( iBrace == 0 ){` |
|        16 |  497 | `								p++;` |
|        16 |  498 | `								break;` |
|         - |  499 | `							}` |
|         1 |  500 | `						}` |
|       106 |  501 | `						p++;` |
|         2 |  502 | `					}` |
|         7 |  503 | `				}` |
|         - |  504 | `			}` |
|        16 |  505 | `			continue;` |
|         - |  506 | `		}` |
|   1265139 |  507 | `		if( p->nType & PH7_TK_OCB ){` |
|        45 |  508 | `			if( iDepth == 0 ){` |
|        45 |  509 | `				break; /* property-hook list: the default expression ends here */` |
|         - |  510 | `			}` |
|       ! 0 |  511 | `			iDepth++;` |
|   1265095 |  512 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     53951 |  513 | `			iDepth++;` |
|   1238122 |  514 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     53951 |  515 | `			if( iDepth > 0 ){` |
|     53951 |  516 | `				iDepth--;` |
|     26973 |  517 | `			}` |
|     26973 |  518 | `		}` |
|   1265095 |  519 | `		p++;` |
|         5 |  520 | `	}` |
|    771343 |  521 | `	return 0;` |
|    385677 |  522 | `}` |
|         - |  523 | `/*` |
|         - |  524 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|         - |  525 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|         - |  526 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|         - |  527 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|         - |  528 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|         - |  529 | ` * share the same backing.` |
|         - |  530 | ` */` |
|     17040 |  531 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|         - |  532 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|         5 |  533 | `{` |
|     17045 |  534 | `	pAttr->nType = nType;` |
|     17045 |  535 | `	pAttr->sClass = *pClass;` |
|     17045 |  536 | `	pAttr->sTypeName = *pTypeName;` |
|     17045 |  537 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  538 | `		sxu32 i;` |
|        91 |  539 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        63 |  540 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|        63 |  541 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|        34 |  542 | `		}` |
|        14 |  543 | `	}` |
|     17045 |  544 | `}` |
|    364640 |  545 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  546 | `{` |
|    364645 |  547 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  548 | `	SySet *pInstrContainer;` |
|         - |  549 | `	ph7_class_attr *pCons;` |
|         - |  550 | `	SyString *pName;` |
|         - |  551 | `	sxi32 rc;` |
|    364645 |  552 | `	sxu32 nType = 0;` |
|         - |  553 | `	SyString sTypeClass;` |
|         - |  554 | `	SyString sTypeText;` |
|         - |  555 | `	SySet aUnionAlts;` |
|    364645 |  556 | `	sxi32 iTypeFlags = 0;` |
|    364645 |  557 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    364645 |  558 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    364645 |  559 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  560 | `	/* Extract visibility level */` |
|    364645 |  561 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  562 | `	/* Mark as constant */` |
|    364645 |  563 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|    364645 |  564 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|         - |  565 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|         - |  566 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|    364665 |  567 | `	if( GenStateClassConstHasType(pGen) ){` |
|        64 |  568 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|        40 |  569 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|         - |  570 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|         - |  571 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|         - |  572 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|         - |  573 | `		 * and success paths release. */` |
|        44 |  574 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  575 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  576 | `			goto Synchronize;` |
|        44 |  577 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  578 | `			return SXERR_ABORT;` |
|        44 |  579 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  580 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  581 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|       ! 0 |  582 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  583 | `				return SXERR_ABORT;` |
|         - |  584 | `			}` |
|       ! 0 |  585 | `			goto Synchronize;` |
|         - |  586 | `		}` |
|        44 |  587 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        20 |  588 | `	}` |
|    182320 |  589 | `loop:` |
|         - |  590 | ``	/* php 8 accepts EVERY reserved word as a class-constant name — `const list = 5`,`` |
|         - |  591 | ``	 * `const match`, `const function`, even `const true` — because a class constant is`` |
|         - |  592 | ``	 * addressed only through `C::name`, where no keyword can be ambiguous. The single`` |
|         - |  593 | ``	 * exception is `class`, reserved for `C::class`, and it gets its own message.`` |
|         - |  594 | `	 * (Method names already accept the whole set; this is the member-name side of the` |
|         - |  595 | ``	 * same rule. Global `const` is NOT the same rule: php rejects a reserved word there.)`` |
|         - |  596 | `	 * A keyword arrives as PH7_TK_KEYWORD, which this ID-only test rejected — so PHL` |
|         - |  597 | ``	 * accepted only the alpha-OPERATOR keywords (`const new`, `const and`), which the`` |
|         - |  598 | `	 * lexer marks PH7_TK_ID\|PH7_TK_OP, and that partial allow-list looked like a design. */` |
|    364651 |  599 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  600 | `		/* Invalid constant name */` |
|       ! 0 |  601 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|       ! 0 |  602 | `		if( rc == SXERR_ABORT ){` |
|         - |  603 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  604 | `			return SXERR_ABORT;` |
|         - |  605 | `		}` |
|       ! 0 |  606 | `		goto Synchronize;` |
|         - |  607 | `	}` |
|         - |  608 | `	/* Peek constant name */` |
|    364651 |  609 | `	pName = &pGen->pIn->sData;` |
|    364646 |  610 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|    184431 |  611 | `		&& (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CLASS ){` |
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
|    364649 |  624 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        64 |  625 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|        40 |  626 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|        20 |  627 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|        44 |  628 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  629 | `			return SXERR_ABORT;` |
|        44 |  630 | `		}else if( rc != SXRET_OK ){` |
|         3 |  631 | `			goto Synchronize;` |
|         - |  632 | `		}` |
|        19 |  633 | `	}` |
|         - |  634 | `	/* Advance the stream cursor */` |
|    364647 |  635 | `	pGen->pIn++;` |
|    364647 |  636 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  637 | `		/* Invalid declaration */` |
|       ! 0 |  638 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|       ! 0 |  639 | `		if( rc == SXERR_ABORT ){` |
|         - |  640 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  641 | `			return SXERR_ABORT;` |
|         - |  642 | `		}` |
|       ! 0 |  643 | `		goto Synchronize;` |
|         - |  644 | `	}` |
|    364647 |  645 | `	pGen->pIn++; /* Jump the equal sign */` |
|         - |  646 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|         - |  647 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|         - |  648 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|         - |  649 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|    364642 |  650 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|        41 |  651 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
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
|    364643 |  663 | `		int iClo = PH7_GenStateInitClosureError(pGen);` |
|    364643 |  664 | `		if( iClo ){` |
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
|    364641 |  676 | `	if( PH7_GenStateInitHasCallExpr(pGen) ){` |
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
|    364641 |  687 | `	if( GenStateInitHasNewExpr(pGen) ){` |
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
|    364637 |  701 | `	if( GenStateExtractConstant(pClass,pName) != 0 ){` |
|       ! 0 |  702 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  703 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|       ! 0 |  704 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  705 | `			return SXERR_ABORT;` |
|         - |  706 | `		}` |
|       ! 0 |  707 | `		goto Synchronize;` |
|         - |  708 | `	}` |
|         - |  709 | `	/* Allocate a new class attribute */` |
|    364637 |  710 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    364637 |  711 | `	if( pCons ){` |
|    364637 |  712 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|    364637 |  713 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  714 | `			return SXERR_ABORT;` |
|         - |  715 | `		}` |
|    182316 |  716 | `	}` |
|    364637 |  717 | `	if( pCons == 0 ){` |
|       ! 0 |  718 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  719 | `		return SXERR_ABORT;` |
|         - |  720 | `	}` |
|    364637 |  721 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        37 |  722 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|        17 |  723 | `	}` |
|         - |  724 | `	/* Swap bytecode container */` |
|    364637 |  725 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    364637 |  726 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|         - |  727 | `	/* Compile constant value.` |
|         - |  728 | `	 */` |
|    364637 |  729 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    364637 |  730 | `	if( rc == SXERR_EMPTY ){` |
|         3 |  731 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|         3 |  732 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  733 | `			return SXERR_ABORT;` |
|         - |  734 | `		}` |
|         1 |  735 | `	}` |
|         - |  736 | `	/* Emit the done instruction */` |
|    364637 |  737 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    364637 |  738 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    364637 |  739 | `	if( rc == SXERR_ABORT ){` |
|         - |  740 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  741 | `		return SXERR_ABORT;` |
|         - |  742 | `	}` |
|         - |  743 | `	/* All done,install the constant */` |
|    364637 |  744 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|    364637 |  745 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  746 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  747 | `		return SXERR_ABORT;` |
|         - |  748 | `	}` |
|    364637 |  749 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
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
|    364631 |  769 | `	SySetRelease(&aUnionAlts);` |
|    364631 |  770 | `	return SXRET_OK;` |
|         7 |  771 | `Synchronize:` |
|        17 |  772 | `	SySetRelease(&aUnionAlts);` |
|         - |  773 | `	/* Synchronize with the first semi-colon */` |
|        67 |  774 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        53 |  775 | `		pGen->pIn++;` |
|         3 |  776 | `	}` |
|        17 |  777 | `	return SXERR_CORRUPT;` |
|    182325 |  778 | `}` |
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
|   2796798 |  808 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|         5 |  809 | `{` |
|   2796803 |  810 | `	SyToken *p = pStart;` |
|   2796803 |  811 | `	int bFirst = 1;` |
|   2796803 |  812 | `	if( p >= pEnd ) return 0;` |
|         - |  813 | ``	/* Optional nullable `?` shorthand. */`` |
|   2796803 |  814 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|        49 |  815 | `		p++;` |
|        49 |  816 | `		if( p >= pEnd ) return 0;` |
|        23 |  817 | `	}` |
|         - |  818 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|         - |  819 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|         - |  820 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|         - |  821 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   1398399 |  822 | `	for(;;){` |
|   2796829 |  823 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|         - |  824 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|         3 |  825 | `			p++;` |
|         9 |  826 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|         3 |  827 | `			if( p >= pEnd ) return 0;` |
|         3 |  828 | `			p++; /* skip ')' */` |
|         2 |  829 | `		}else{` |
|         - |  830 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|         - |  831 | ``			 * then any `&`-joined intersection members. */`` |
|   2796827 |  832 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|   2796827 |  833 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  834 | `				return 0;` |
|         - |  835 | `			}` |
|         - |  836 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|         - |  837 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|         - |  838 | `			 * may still appear at the initial dispatch site). */` |
|   2796827 |  839 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|   2796763 |  840 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|   2796758 |  841 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    133123 |  842 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|   2779803 |  843 | `					return 0;` |
|         - |  844 | `				}` |
|      8480 |  845 | `			}` |
|     17029 |  846 | `			p++;` |
|     17031 |  847 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  848 | `				p += 2;` |
|         1 |  849 | `			}` |
|     25539 |  850 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|     17032 |  851 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  852 | `				p++; /* skip '&' */` |
|         3 |  853 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|         3 |  854 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|         3 |  855 | `				p++;` |
|         3 |  856 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       ! 0 |  857 | `					p += 2;` |
|       ! 0 |  858 | `				}` |
|         1 |  859 | `			}` |
|         - |  860 | `		}` |
|     17031 |  861 | `		bFirst = 0;` |
|     17026 |  862 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        31 |  863 | `			&& p->sData.zString[0] == '\|' ){` |
|        31 |  864 | ``			p++; /* next `\|`-separated part */`` |
|        31 |  865 | `			continue;` |
|         - |  866 | `		}` |
|     17005 |  867 | `		break;` |
|       ! 0 |  868 | `	}` |
|     17005 |  869 | `	if( p >= pEnd ) return 0;` |
|     17005 |  870 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   1398404 |  871 | `}` |
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
|     17010 |  891 | `static sxi32 GenStateParsePropertyType(` |
|         - |  892 | `	ph7_gen_state *pGen,` |
|         - |  893 | `	sxu32 *pnType,` |
|         - |  894 | `	SyString *pClass,` |
|         - |  895 | `	sxi32 *piTypeFlags,` |
|         - |  896 | `	SyString *pTypeText,` |
|         - |  897 | `	SySet *pAlts` |
|         5 |  898 | `){` |
|     17015 |  899 | `	sxi32 iFlags = 0;` |
|         - |  900 | `	sxi32 rc;` |
|     17015 |  901 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  902 | `		return SXRET_OK;` |
|         - |  903 | `	}` |
|         - |  904 | `	/* If the first token is '$', there's no type */` |
|     17015 |  905 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       ! 0 |  906 | `		return SXRET_OK;` |
|         - |  907 | `	}` |
|     17015 |  908 | `	rc = GenStateParseUnionTypeDecl(` |
|      8505 |  909 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|         - |  910 | `		PH7_CLASS_ATTR_NULLABLE,` |
|         - |  911 | `		PH7_CLASS_ATTR_UNION,` |
|         - |  912 | `		/* bAllowVoid */ 0,` |
|     17010 |  913 | `		pGen->pIn->nLine);` |
|     17015 |  914 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  915 | `		return rc;` |
|         - |  916 | `	}` |
|         - |  917 | `	/* Verify next token is '$' (start of property name) */` |
|     17015 |  918 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  919 | `		return SXERR_SYNTAX;` |
|         - |  920 | `	}` |
|     17015 |  921 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     17015 |  922 | `	return SXRET_OK;` |
|      8510 |  923 | `}` |
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
|     17204 |  936 | `static int GenStateIsDisallowedPropertyAtom(` |
|         - |  937 | `	sxu32 nType,` |
|         - |  938 | `	const SyString *pClass,` |
|         - |  939 | `	const char **pzName,` |
|         - |  940 | `	sxu32 *pnName)` |
|         5 |  941 | `{` |
|         - |  942 | `	const char *z;` |
|         - |  943 | `	sxu32 n;` |
|     17209 |  944 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     17141 |  945 | `		return 0;` |
|         - |  946 | `	}` |
|        73 |  947 | `	z = pClass->zString;` |
|        73 |  948 | `	n = pClass->nByte;` |
|        73 |  949 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|         8 |  950 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|         - |  951 | `	}` |
|         - |  952 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|         - |  953 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|         - |  954 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|        66 |  955 | `	return 0;` |
|      8607 |  956 | `}` |
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
|     17130 |  969 | `PH7_PRIVATE sxi32 GenStateValidateMemberType(` |
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
|     17135 |  980 | `	const char *zBad = 0;` |
|     17135 |  981 | `	sxu32 nBad = 0;` |
|         - |  982 | `	SyString sFallback;` |
|         - |  983 | `	const SyString *pBad;` |
|         - |  984 | `	sxi32 rc;` |
|     17135 |  985 | `	int bDisallowed = 0;` |
|     17135 |  986 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|         5 |  987 | `		bDisallowed = 1;` |
|     17133 |  988 | `	}else if( pUnionAlts ){` |
|         - |  989 | `		sxu32 i;` |
|       113 |  990 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|        79 |  991 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|        79 |  992 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|         3 |  993 | `				bDisallowed = 1;` |
|         3 |  994 | `				break;` |
|         - |  995 | `			}` |
|        41 |  996 | `		}` |
|        18 |  997 | `	}` |
|     17135 |  998 | `	if( !bDisallowed ){` |
|     17129 |  999 | `		return SXRET_OK;` |
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
|      8570 | 1017 | `}` |
|         - | 1018 | `/*` |
|         - | 1019 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|         - | 1020 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|         - | 1021 | ` * matched as a plain identifier in the class-member modifier position rather` |
|         - | 1022 | ` * than promoted to a lexer keyword.` |
|         - | 1023 | ` */` |
|  25155576 | 1024 | `PH7_PRIVATE int GenStateIsReadonly(SyToken *pTok)` |
|         5 | 1025 | `{` |
|  25401375 | 1026 | `	return (pTok->nType & PH7_TK_ID)` |
|  12823582 | 1027 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
|  25401370 | 1028 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|         5 | 1029 | `}` |
|         - | 1030 | `/*` |
|         - | 1031 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|         - | 1032 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|         - | 1033 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|         - | 1034 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|         - | 1035 | ` */` |
|   9013144 | 1036 | `PH7_PRIVATE sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|         5 | 1037 | `{` |
|   9013149 | 1038 | `	*pnTok = 0;` |
|   9013144 | 1039 | `	if( &pTok[3] < pEnd` |
|   8426369 | 1040 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|   6872053 | 1041 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|   2952264 | 1042 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        16 | 1043 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|        16 | 1044 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|        21 | 1045 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|        17 | 1046 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|        17 | 1047 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|        17 | 1048 | `			*pnTok = 4;` |
|        17 | 1049 | `			return nKw;` |
|         - | 1050 | `		}` |
|       ! 0 | 1051 | `	}` |
|   9013133 | 1052 | `	return 0;` |
|   4506577 | 1053 | `}` |
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
|    630524 | 1065 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 | 1066 | `{` |
|    630529 | 1067 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 1068 | `	ph7_class_attr *pAttr;` |
|         - | 1069 | `	SyString *pName;` |
|         - | 1070 | `	sxi32 rc;` |
|    630529 | 1071 | `	sxu32 nType = 0;` |
|         - | 1072 | `	SyString sTypeClass;` |
|         - | 1073 | `	SyString sTypeText;` |
|         - | 1074 | `	SySet aUnionAlts;` |
|    630529 | 1075 | `	sxi32 iTypeFlags = 0;` |
|    630529 | 1076 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    630529 | 1077 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    630529 | 1078 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - | 1079 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|         - | 1080 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|         - | 1081 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|    630529 | 1082 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|        21 | 1083 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|         9 | 1084 | `	}` |
|         - | 1085 | `	/* Extract visibility level */` |
|    630529 | 1086 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - | 1087 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|    639034 | 1088 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     17015 | 1089 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     17015 | 1090 | `		if( rc == SXERR_CORRUPT ){` |
|         - | 1091 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 | 1092 | `			goto Synchronize;` |
|     17015 | 1093 | `		}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 | 1094 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 1095 | `				"Invalid property type or declaration near '%z'",` |
|       ! 0 | 1096 | `				&pGen->pIn->sData);` |
|       ! 0 | 1097 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1098 | `				return SXERR_ABORT;` |
|         - | 1099 | `			}` |
|       ! 0 | 1100 | `			goto Synchronize;` |
|     17015 | 1101 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 | 1102 | `			return SXERR_ABORT;` |
|         - | 1103 | `		}` |
|      8505 | 1104 | `	}` |
|       ! 0 | 1105 | `loop:` |
|    630533 | 1106 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 | 1107 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|       ! 0 | 1108 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1109 | `			return SXERR_ABORT;` |
|         - | 1110 | `		}` |
|       ! 0 | 1111 | `		goto Synchronize;` |
|         - | 1112 | `	}` |
|    630533 | 1113 | `	pGen->pIn++; /* Jump the dollar sign */` |
|    630533 | 1114 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         - | 1115 | `		/* Invalid attribute name */` |
|       ! 0 | 1116 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|       ! 0 | 1117 | `		if( rc == SXERR_ABORT ){` |
|         - | 1118 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 1119 | `			return SXERR_ABORT;` |
|         - | 1120 | `		}` |
|       ! 0 | 1121 | `		goto Synchronize;` |
|         - | 1122 | `	}` |
|         - | 1123 | `	/* Peek attribute name */` |
|    630533 | 1124 | `	pName = &pGen->pIn->sData;` |
|         - | 1125 | `	/* Advance the stream cursor */` |
|    630533 | 1126 | `	pGen->pIn++;` |
|    630533 | 1127 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
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
|    630531 | 1139 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
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
|    630531 | 1159 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
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
|    630521 | 1179 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     25517 | 1180 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|         - | 1181 | `			&sTypeText,` |
|     17008 | 1182 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      8504 | 1183 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     17013 | 1184 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1185 | `			return SXERR_ABORT;` |
|     17013 | 1186 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 | 1187 | `			goto Synchronize;` |
|         - | 1188 | `		}` |
|      8504 | 1189 | `	}` |
|         - | 1190 | `	/* Reject redeclaration (catches clash with an earlier promoted property).` |
|         - | 1191 | `	 * A same-name class CONSTANT is NOT a clash — php's separate namespaces let` |
|         - | 1192 | ``	 * `const C` and `public $C` coexist (stored in disjoint hConst / hAttr tables). */`` |
|    630521 | 1193 | `	if( GenStateExtractProperty(pClass,pName->zString,pName->nByte) != 0 ){` |
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
|    630519 | 1208 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|    406611 | 1209 | `		int iClo = PH7_GenStateInitClosureError(pGen);` |
|    406611 | 1210 | `		if( iClo ){` |
|       ! 0 | 1211 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 1212 | `				iClo == 2 ? "Closures in constant expressions must be static"` |
|         - | 1213 | `				          : "Constant expression contains invalid operations");` |
|       ! 0 | 1214 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1215 | `				return SXERR_ABORT;` |
|         - | 1216 | `			}` |
|       ! 0 | 1217 | `			goto Synchronize;` |
|         - | 1218 | `		}` |
|    203303 | 1219 | `	}` |
|         - | 1220 | `	/* php: a property default may not CALL anything either. */` |
|    630519 | 1221 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && PH7_GenStateInitHasCallExpr(pGen) ){` |
|       ! 0 | 1222 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 1223 | `			"Constant expression contains invalid operations");` |
|       ! 0 | 1224 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1225 | `			return SXERR_ABORT;` |
|         - | 1226 | `		}` |
|       ! 0 | 1227 | `		goto Synchronize;` |
|         - | 1228 | `	}` |
|    630519 | 1229 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|         5 | 1230 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 1231 | `			"New expressions are not supported in this context");` |
|         5 | 1232 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1233 | `			return SXERR_ABORT;` |
|         - | 1234 | `		}` |
|         5 | 1235 | `		goto Synchronize;` |
|         - | 1236 | `	}` |
|         - | 1237 | `	/* Allocate a new class attribute */` |
|    630515 | 1238 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    630515 | 1239 | `	if( pAttr ){` |
|    630515 | 1240 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|    630515 | 1241 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 1242 | `			return SXERR_ABORT;` |
|         - | 1243 | `		}` |
|    315255 | 1244 | `	}` |
|    630515 | 1245 | `	if( pAttr == 0 ){` |
|       ! 0 | 1246 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 1247 | `		return SXERR_ABORT;` |
|         - | 1248 | `	}` |
|    630515 | 1249 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     17011 | 1250 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      8503 | 1251 | `	}` |
|    630515 | 1252 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|         - | 1253 | `		SySet *pInstrContainer;` |
|    406607 | 1254 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|    406607 | 1255 | `		pGen->pIn++; /*Jump the equal sign */` |
|         - | 1256 | `		{` |
|         - | 1257 | `			/* Delimit the default expression: it ends at the declaration's` |
|         - | 1258 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|         - | 1259 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|         - | 1260 | `			 * compiler would otherwise run into the hook tokens. */` |
|    406607 | 1261 | `			SyToken *pScan = pGen->pIn;` |
|    406607 | 1262 | `			sxi32 iNest = 0;` |
|    896073 | 1263 | `			while( pScan < pGen->pEnd ){` |
|    896073 | 1264 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     53909 | 1265 | `					iNest++;` |
|    869121 | 1266 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|     53909 | 1267 | `					iNest--;` |
|    815217 | 1268 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|    406607 | 1269 | `					break;` |
|         - | 1270 | `				}` |
|    489471 | 1271 | `				pScan++;` |
|         5 | 1272 | `			}` |
|    406607 | 1273 | `			pGen->pEnd = pScan;` |
|         - | 1274 | `		}` |
|         - | 1275 | `		/* Swap bytecode container */` |
|    406607 | 1276 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    406607 | 1277 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|         - | 1278 | `		/* Compile attribute value. The default is a const-expression belonging to` |
|         - | 1279 | `		 * pClass (see iInMemberDefault) — __TRAIT__ in it reads pCurClass. */` |
|    406607 | 1280 | `		pGen->iInMemberDefault++;` |
|    406607 | 1281 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    406607 | 1282 | `		pGen->iInMemberDefault--;` |
|    406607 | 1283 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 | 1284 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|       ! 0 | 1285 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1286 | `				return SXERR_ABORT;` |
|         - | 1287 | `			}` |
|       ! 0 | 1288 | `		}` |
|         - | 1289 | `		/* Emit the done instruction */` |
|    406607 | 1290 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    406607 | 1291 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    406607 | 1292 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|    406607 | 1293 | `		pGen->pEnd = pSavedDefEnd;` |
|    203301 | 1294 | `	}` |
|         - | 1295 | `	/* All done,install the attribute */` |
|    630515 | 1296 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|    630515 | 1297 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1298 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1299 | `		return SXERR_ABORT;` |
|         - | 1300 | `	}` |
|    630515 | 1301 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|         - | 1302 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|         - | 1303 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|        95 | 1304 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|        95 | 1305 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1306 | `			return SXERR_ABORT;` |
|         - | 1307 | `		}` |
|        95 | 1308 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1309 | `			goto Synchronize;` |
|         - | 1310 | `		}` |
|        95 | 1311 | `		SySetRelease(&aUnionAlts);` |
|        95 | 1312 | `		return SXRET_OK;` |
|         - | 1313 | `	}` |
|    630421 | 1314 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
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
|    630421 | 1326 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
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
|    630417 | 1346 | `	SySetRelease(&aUnionAlts);` |
|    630417 | 1347 | `	return SXRET_OK;` |
|         9 | 1348 | `Synchronize:` |
|         - | 1349 | `	/* Synchronize with the first semi-colon */` |
|        55 | 1350 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        37 | 1351 | `		pGen->pIn++;` |
|         3 | 1352 | `	}` |
|        21 | 1353 | `	SySetRelease(&aUnionAlts);` |
|        21 | 1354 | `	return SXERR_CORRUPT;` |
|    315267 | 1355 | `}` |
|         - | 1356 | `/*` |
|         - | 1357 | ` * Compile a class method.` |
|         - | 1358 | ` *` |
|         - | 1359 | ` * Refer to the official documentation for more information` |
|         - | 1360 | ` * on the powerful extension introduced by the PH7 engine` |
|         - | 1361 | ` * to the OO subsystem such as full type hinting,method` |
|         - | 1362 | ` * overloading and many more.` |
|         - | 1363 | ` */` |
|   3090544 | 1364 | `static sxi32 GenStateCompileClassMethod(` |
|         - | 1365 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 1366 | `	sxi32 iProtection,   /* Visibility level */` |
|         - | 1367 | `	sxi32 iFlags,        /* Configuration flags */` |
|         - | 1368 | `	int doBody,          /* TRUE to process method body */` |
|         - | 1369 | `	ph7_class *pClass    /* Class this method belongs */` |
|         - | 1370 | `	)` |
|         5 | 1371 | `{` |
|   3090549 | 1372 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   3090549 | 1373 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|         - | 1374 | `	ph7_class_method *pMeth;` |
|         - | 1375 | `	sxi32 iFuncFlags;` |
|         - | 1376 | `	SyString *pName;` |
|         - | 1377 | `	SyToken *pEnd;` |
|         - | 1378 | `	sxi32 rc;` |
|         - | 1379 | `	/* Extract visibility level */` |
|   3090549 | 1380 | `	iProtection = GetProtectionLevel(iProtection);` |
|   3090549 | 1381 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   3090549 | 1382 | `	iFuncFlags = 0;` |
|   3090549 | 1383 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - | 1384 | `		/* Invalid method name */` |
|       ! 0 | 1385 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|       ! 0 | 1386 | `		if( rc == SXERR_ABORT ){` |
|         - | 1387 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 1388 | `			return SXERR_ABORT;` |
|         - | 1389 | `		}` |
|       ! 0 | 1390 | `		goto Synchronize;` |
|         - | 1391 | `	}` |
|   3090549 | 1392 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - | 1393 | `		/* Return by reference,remember that */` |
|       ! 0 | 1394 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|         - | 1395 | `		/* Jump the '&' token */` |
|       ! 0 | 1396 | `		pGen->pIn++;` |
|       ! 0 | 1397 | `	}` |
|   3090549 | 1398 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - | 1399 | `		/* Invalid method name */` |
|       ! 0 | 1400 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|       ! 0 | 1401 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1402 | `			return SXERR_ABORT;` |
|         - | 1403 | `		}` |
|       ! 0 | 1404 | `		goto Synchronize;` |
|         - | 1405 | `	}` |
|         - | 1406 | `	/* Peek method name */` |
|   3090549 | 1407 | `	pName = &pGen->pIn->sData;` |
|   3090549 | 1408 | `	nLine = pGen->pIn->nLine;` |
|         - | 1409 | `	/* Jump the method name */` |
|   3090549 | 1410 | `	pGen->pIn++;` |
|   3090549 | 1411 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - | 1412 | `		/* Abstract method */` |
|    149129 | 1413 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       ! 0 | 1414 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 1415 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|       ! 0 | 1416 | `				&pClass->sName,pName);` |
|       ! 0 | 1417 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1418 | `				return SXERR_ABORT;` |
|         - | 1419 | `			}` |
|       ! 0 | 1420 | `		}` |
|         - | 1421 | `		/* Assemble method signature only */` |
|    149129 | 1422 | `		doBody = FALSE;` |
|     74562 | 1423 | `	}` |
|   3090549 | 1424 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 1425 | `		/* Syntax error */` |
|       ! 0 | 1426 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|       ! 0 | 1427 | `		if( rc == SXERR_ABORT ){` |
|         - | 1428 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 1429 | `			return SXERR_ABORT;` |
|         - | 1430 | `		}` |
|       ! 0 | 1431 | `		goto Synchronize;` |
|         - | 1432 | `	}` |
|         - | 1433 | `	/* Allocate a new class_method instance */` |
|   3090549 | 1434 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   3090549 | 1435 | `	if( pMeth == 0 ){` |
|       ! 0 | 1436 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1437 | `		return SXERR_ABORT;` |
|         - | 1438 | `	}` |
|   3090549 | 1439 | `	pMeth->sFunc.nLine = nKwLine;` |
|   3090549 | 1440 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|   3090549 | 1441 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 1442 | `		return SXERR_ABORT;` |
|         - | 1443 | `	}` |
|         - | 1444 | `	/* Jump the left parenthesis '(' */` |
|   3090549 | 1445 | `	pGen->pIn++;` |
|   3090549 | 1446 | `	pEnd = 0; /* cc warning */` |
|         - | 1447 | `	/* Delimit the method signature */` |
|   3090549 | 1448 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   3090549 | 1449 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 1450 | `		/* Syntax error */` |
|         3 | 1451 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|         3 | 1452 | `		if( rc == SXERR_ABORT ){` |
|         - | 1453 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 1454 | `			return SXERR_ABORT;` |
|         - | 1455 | `		}` |
|         3 | 1456 | `		goto Synchronize;` |
|         - | 1457 | `	}` |
|         - | 1458 | `	{` |
|   3090547 | 1459 | `		int bIsCtor = 0;` |
|   3090547 | 1460 | `		int bAbstractCtor = 0;` |
|         - | 1461 | `		/* Only __construct is the constructor (PHP-4 class-name constructors removed` |
|         - | 1462 | `		 * in 8.0): a method named like the class is a plain method, so promoted` |
|         - | 1463 | `		 * properties in it are rejected exactly as php does elsewhere. */` |
|   3090542 | 1464 | `		if( pName->nByte == sizeof("__construct") - 1` |
|   1814557 | 1465 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0 ){` |
|    219669 | 1466 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         3 | 1467 | `				bAbstractCtor = 1;` |
|         2 | 1468 | `			}else{` |
|    219667 | 1469 | `				bIsCtor = 1;` |
|         - | 1470 | `			}` |
|    109832 | 1471 | `		}` |
|   3090547 | 1472 | `		if( pGen->pIn < pEnd ){` |
|         - | 1473 | `			/* Collect method arguments */` |
|   1188977 | 1474 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   1188977 | 1475 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1476 | `				return SXERR_ABORT;` |
|         - | 1477 | `			}` |
|    594486 | 1478 | `		}` |
|         - | 1479 | `	}` |
|         - | 1480 | `	/* Point past ')' and parse optional return type ': type' */` |
|   3090547 | 1481 | `	pGen->pIn = &pEnd[1];` |
|         - | 1482 | `	{` |
|   3090547 | 1483 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|   3090547 | 1484 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 | 1485 | `			return SXERR_ABORT;` |
|   3090547 | 1486 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       ! 0 | 1487 | `			goto Synchronize;` |
|         - | 1488 | `		}` |
|         - | 1489 | `	}` |
|         - | 1490 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|         - | 1491 | `	 * property init/typecheck is handled by the generic typed-property path` |
|         - | 1492 | `	 * since we mint real ph7_class_attr entries. */` |
|         - | 1493 | `	{` |
|   3090547 | 1494 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|         - | 1495 | `		sxu32 i;` |
|   4867593 | 1496 | `		for( i = 0; i < nArg; i++ ){` |
|   1777061 | 1497 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|         - | 1498 | `			ph7_class_attr *pAttr;` |
|   1777061 | 1499 | `			sxi32 iAttrFlags = 0;` |
|         - | 1500 | `			int bArgTyped;` |
|   1777061 | 1501 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1776973 | 1502 | `				continue;` |
|         - | 1503 | `			}` |
|         - | 1504 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|         - | 1505 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|         - | 1506 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|        61 | 1507 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|        94 | 1508 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|        93 | 1509 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|         3 | 1510 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 1511 | `					"Cannot declare variadic promoted property");` |
|         3 | 1512 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 1513 | `					return SXERR_ABORT;` |
|         - | 1514 | `				}` |
|         3 | 1515 | `				goto Synchronize;` |
|         - | 1516 | `			}` |
|         - | 1517 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|         - | 1518 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|         - | 1519 | `			 * appear as an alternative of a union type. */` |
|        91 | 1520 | `			if( bArgTyped ){` |
|       128 | 1521 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|        82 | 1522 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|        82 | 1523 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|        41 | 1524 | `					"Property %z::$%z cannot have type %z",nLine);` |
|        87 | 1525 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 1526 | `					return SXERR_ABORT;` |
|        87 | 1527 | `				}else if( rc != SXRET_OK ){` |
|         6 | 1528 | `					goto Synchronize;` |
|         - | 1529 | `				}` |
|        39 | 1530 | `			}` |
|         - | 1531 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|        87 | 1532 | `			if( GenStateExtractProperty(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|         4 | 1533 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 | 1534 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|         3 | 1535 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 1536 | `					return SXERR_ABORT;` |
|         - | 1537 | `				}` |
|         3 | 1538 | `				goto Synchronize;` |
|         - | 1539 | `			}` |
|        85 | 1540 | `			if( bArgTyped ){` |
|        81 | 1541 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        38 | 1542 | `			}` |
|        85 | 1543 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|         3 | 1544 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|         1 | 1545 | `			}` |
|        85 | 1546 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|         8 | 1547 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|         3 | 1548 | `			}` |
|        85 | 1549 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|         - | 1550 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|         - | 1551 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|        26 | 1552 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         4 | 1553 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 | 1554 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|         3 | 1555 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 1556 | `						return SXERR_ABORT;` |
|         - | 1557 | `					}` |
|         3 | 1558 | `					goto Synchronize;` |
|         - | 1559 | `				}` |
|        24 | 1560 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        10 | 1561 | `			}` |
|        83 | 1562 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|         - | 1563 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|         5 | 1564 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 | 1565 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 1566 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|       ! 0 | 1567 | `						&pClass->sName,&pArg->sName);` |
|       ! 0 | 1568 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 1569 | `						return SXERR_ABORT;` |
|         - | 1570 | `					}` |
|       ! 0 | 1571 | `					goto Synchronize;` |
|         - | 1572 | `				}` |
|         5 | 1573 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|         2 | 1574 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|         2 | 1575 | `			}` |
|        83 | 1576 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|        83 | 1577 | `			if( pAttr == 0 ){` |
|       ! 0 | 1578 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1579 | `				return SXERR_ABORT;` |
|         - | 1580 | `			}` |
|        83 | 1581 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|        81 | 1582 | `				pAttr->nType = pArg->nType;` |
|        81 | 1583 | `				pAttr->sClass = pArg->sClass;` |
|        81 | 1584 | `				pAttr->sTypeName = pArg->sTypeName;` |
|        81 | 1585 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|         - | 1586 | `					sxu32 k;` |
|        20 | 1587 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|        14 | 1588 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|        14 | 1589 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|         8 | 1590 | `					}` |
|         3 | 1591 | `				}` |
|        38 | 1592 | `			}` |
|        83 | 1593 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|        83 | 1594 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1595 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1596 | `				return SXERR_ABORT;` |
|         - | 1597 | `			}` |
|        44 | 1598 | `		}` |
|         - | 1599 | `	}` |
|   3090537 | 1600 | `	if( doBody ){` |
|         - | 1601 | `		/* Compile method body */` |
|   2941413 | 1602 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   2941413 | 1603 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1604 | `			return SXERR_ABORT;` |
|         - | 1605 | `		}` |
|         - | 1606 | `		/* The cursor sits just past the body's closing brace */` |
|   2941413 | 1607 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   1470709 | 1608 | `	}else{` |
|         - | 1609 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|    149129 | 1610 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|    149129 | 1611 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|     74562 | 1612 | `		}` |
|         - | 1613 | `		/* Only method signature is allowed */` |
|    149129 | 1614 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|       ! 0 | 1615 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 1616 | `				"Expected ';' after method signature '%z'",pName);` |
|       ! 0 | 1617 | `				if( rc == SXERR_ABORT ){` |
|         - | 1618 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 | 1619 | `					return SXERR_ABORT;` |
|         - | 1620 | `				}` |
|       ! 0 | 1621 | `				return SXERR_CORRUPT;` |
|         - | 1622 | `			}` |
|         - | 1623 | `	}` |
|         - | 1624 | `	/* php: an abstract method in a class that was not DECLARED abstract is a fatal,` |
|         - | 1625 | `	 * and it names the method -- distinct from the "contains N abstract methods"` |
|         - | 1626 | `	 * wording php uses for an unimplemented INHERITED one, which` |
|         - | 1627 | `	 * GenStateCheckAbstractMethods already handles. Interfaces and traits may carry` |
|         - | 1628 | `	 * abstract methods freely. */` |
|   3090532 | 1629 | `	if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT)` |
|   1619833 | 1630 | `	 && (pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|         4 | 1631 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 1632 | `			"Class %z declares abstract method %z() and must therefore be declared abstract",` |
|         1 | 1633 | `			&pClass->sName,pName);` |
|         3 | 1634 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1635 | `			return SXERR_ABORT;` |
|         - | 1636 | `		}` |
|         3 | 1637 | `		return SXRET_OK;` |
|         - | 1638 | `	}` |
|         - | 1639 | `	/* php: two methods of the same name in one class body is a fatal, and the` |
|         - | 1640 | ``	 * comparison is case-INSENSITIVE (hMethod now matches that way), so `f` and`` |
|         - | 1641 | ``	 * `F` collide. php names the offending declaration with the spelling used at`` |
|         - | 1642 | `	 * the SECOND site. */` |
|   3090535 | 1643 | `	if( PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte) != 0 ){` |
|         8 | 1644 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         2 | 1645 | `			"Cannot redeclare %z::%z()",&pClass->sName,pName);` |
|         6 | 1646 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1647 | `			return SXERR_ABORT;` |
|         - | 1648 | `		}` |
|         6 | 1649 | `		return SXRET_OK;` |
|         - | 1650 | `	}` |
|         - | 1651 | `	/* All done,install the method */` |
|   3090531 | 1652 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   3090531 | 1653 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1654 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1655 | `		return SXERR_ABORT;` |
|         - | 1656 | `	}` |
|   3090531 | 1657 | `	return SXRET_OK;` |
|         6 | 1658 | `Synchronize:` |
|         - | 1659 | `	/* Synchronize with the first semi-colon */` |
|        40 | 1660 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        28 | 1661 | `		pGen->pIn++;` |
|         4 | 1662 | `	}` |
|        16 | 1663 | `	return SXERR_CORRUPT;` |
|   1545277 | 1664 | `}` |
|         - | 1665 | `/*` |
|         - | 1666 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|         - | 1667 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|         - | 1668 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|         - | 1669 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|         - | 1670 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|         - | 1671 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|         - | 1672 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|         - | 1673 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|         - | 1674 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|         - | 1675 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|         - | 1676 | `` * implicit `$value` formal.`` |
|         - | 1677 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|         - | 1678 | ` */` |
|         - | 1679 | `/*` |
|         - | 1680 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|         - | 1681 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|         - | 1682 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|         - | 1683 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|         - | 1684 | ` * allowed, excluded from the raw object surfaces.` |
|         - | 1685 | ` */` |
|        94 | 1686 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|         1 | 1687 | `{` |
|         - | 1688 | `	SyToken *p;` |
|       345 | 1689 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|       303 | 1690 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       223 | 1691 | `			continue;` |
|         - | 1692 | `		}` |
|         - | 1693 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|        80 | 1694 | `		if( p + 3 < pEnd` |
|        80 | 1695 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        80 | 1696 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|        73 | 1697 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|        66 | 1698 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|        66 | 1699 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        66 | 1700 | `		 && p[3].sData.nByte == pName->nByte` |
|        60 | 1701 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        51 | 1702 | `			return 1;` |
|         - | 1703 | `		}` |
|         - | 1704 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|         - | 1705 | `		 * hook operates on the shared per-instance backing store, so the` |
|         - | 1706 | `		 * property is backed (php compiles a default alongside it). */` |
|        30 | 1707 | `		if( p > pStart` |
|        26 | 1708 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|        12 | 1709 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         2 | 1710 | `		 && p[1].sData.nByte == pName->nByte` |
|         3 | 1711 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|         3 | 1712 | `			return 1;` |
|         - | 1713 | `		}` |
|        15 | 1714 | `	}` |
|        43 | 1715 | `	return 0;` |
|        48 | 1716 | `}` |
|         - | 1717 | `/*` |
|         - | 1718 | ` * True when p opens php 8.4's parent-hook call form` |
|         - | 1719 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|         - | 1720 | ` */` |
|       990 | 1721 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|         1 | 1722 | `{` |
|      1167 | 1723 | `	return p + 6 < pEnd` |
|       671 | 1724 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       250 | 1725 | `	 && p->sData.nByte == sizeof("parent")-1` |
|        81 | 1726 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|        11 | 1727 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|         8 | 1728 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|         8 | 1729 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 | 1730 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|         8 | 1731 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 | 1732 | `	 && p[5].sData.nByte == 3` |
|         8 | 1733 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|         6 | 1734 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|      1166 | 1735 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|         1 | 1736 | `}` |
|         - | 1737 | `/*` |
|         - | 1738 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|         - | 1739 | ` * hook body into calls of the parent class's synthesized hook method` |
|         - | 1740 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|         - | 1741 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|         - | 1742 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|         - | 1743 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|         - | 1744 | ` * or SXERR_MEM.` |
|         - | 1745 | ` */` |
|         4 | 1746 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|         - | 1747 | `	SyToken *pStart,SyToken *pEnd)` |
|         1 | 1748 | `{` |
|         5 | 1749 | `	SyToken *p = pStart;` |
|        35 | 1750 | `	while( p < pEnd ){` |
|        31 | 1751 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|         - | 1752 | `			SyToken sTok;` |
|         - | 1753 | `			char zName[384];` |
|         - | 1754 | `			sxu32 nName;` |
|         - | 1755 | `			char *zDup;` |
|         - | 1756 | ``			/* `parent` `::` */`` |
|         5 | 1757 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|         5 | 1758 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|         7 | 1759 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|         4 | 1760 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|         5 | 1761 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|         5 | 1762 | `			if( zDup == 0 ){` |
|       ! 0 | 1763 | `				return SXERR_MEM;` |
|         - | 1764 | `			}` |
|         5 | 1765 | `			sTok = p[3]; /* keep the line info of the property name */` |
|         5 | 1766 | `			sTok.nType = PH7_TK_ID;` |
|         5 | 1767 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|         5 | 1768 | `			sTok.pUserData = 0;` |
|         5 | 1769 | `			SySetPut(pCopy,(const void *)&sTok);` |
|         5 | 1770 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|         5 | 1771 | `			continue;` |
|         - | 1772 | `		}` |
|        27 | 1773 | `		SySetPut(pCopy,(const void *)p);` |
|        27 | 1774 | `		p++;` |
|         1 | 1775 | `	}` |
|         5 | 1776 | `	return SXRET_OK;` |
|         3 | 1777 | `}` |
|        94 | 1778 | `PH7_PRIVATE sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|         1 | 1779 | `{` |
|        95 | 1780 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 1781 | `	sxi32 rc;` |
|        95 | 1782 | `	int bRefsSelf = 0;` |
|        95 | 1783 | `	pGen->pIn++; /* Jump '{' */` |
|       253 | 1784 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|         - | 1785 | `		char zHook[384];` |
|         - | 1786 | `		SyString sHookName;` |
|         - | 1787 | `		ph7_class_method *pMeth;` |
|         - | 1788 | `		int bGet;` |
|       159 | 1789 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|       159 | 1790 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        15 | 1791 | `			pGen->pIn++; /* stray ';' between hooks */` |
|        22 | 1792 | `			continue;` |
|         - | 1793 | `		}` |
|       145 | 1794 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - | 1795 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|       ! 0 | 1796 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - | 1797 | `				"By-reference property hooks are not supported for %z::$%z",` |
|       ! 0 | 1798 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 | 1799 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1800 | `				return SXERR_ABORT;` |
|         - | 1801 | `			}` |
|       ! 0 | 1802 | `			return SXERR_CORRUPT;` |
|         - | 1803 | `		}` |
|       145 | 1804 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 1805 | `			goto HookSyntax;` |
|         - | 1806 | `		}` |
|       144 | 1807 | `		if( pGen->pIn->sData.nByte == 3` |
|       145 | 1808 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|        79 | 1809 | `			bGet = 1;` |
|       106 | 1810 | `		}else if( pGen->pIn->sData.nByte == 3` |
|        67 | 1811 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|        67 | 1812 | `			bGet = 0;` |
|        34 | 1813 | `		}else{` |
|       ! 0 | 1814 | `			goto HookSyntax;` |
|         - | 1815 | `		}` |
|       145 | 1816 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|       145 | 1817 | `		sHookName.zString = zHook;` |
|       217 | 1818 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|        72 | 1819 | `			bGet ? "get" : "set",&pAttr->sName);` |
|       145 | 1820 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|         - | 1821 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|         - | 1822 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|         - | 1823 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|         - | 1824 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|         - | 1825 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|        14 | 1826 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|         8 | 1827 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 1828 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - | 1829 | `					"Non-abstract property hook must have a body");` |
|       ! 0 | 1830 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 1831 | `					return SXERR_ABORT;` |
|         - | 1832 | `				}` |
|       ! 0 | 1833 | `				return SXERR_CORRUPT;` |
|         - | 1834 | `			}` |
|        15 | 1835 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - | 1836 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|        15 | 1837 | `			if( pMeth == 0 ){` |
|       ! 0 | 1838 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1839 | `				return SXERR_ABORT;` |
|         - | 1840 | `			}` |
|        15 | 1841 | `			pMeth->sFunc.nLine = nHLine;` |
|        15 | 1842 | `			if( !bGet ){` |
|         - | 1843 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|         - | 1844 | `				 * compatible with concrete set-hook implementations (which` |
|         - | 1845 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|         - | 1846 | `				 * type (php: the abstract set's parameter type IS the property` |
|         - | 1847 | `				 * type), so the override contravariance check accepts a typed` |
|         - | 1848 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|         - | 1849 | `				ph7_vm_func_arg sVArg;` |
|         7 | 1850 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|         7 | 1851 | `				if( zVName == 0 ){` |
|       ! 0 | 1852 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1853 | `					return SXERR_ABORT;` |
|         - | 1854 | `				}` |
|         7 | 1855 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|         7 | 1856 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|         7 | 1857 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         7 | 1858 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         7 | 1859 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|         7 | 1860 | `				sVArg.nType = pAttr->nType;` |
|         7 | 1861 | `				sVArg.sClass = pAttr->sClass;` |
|         7 | 1862 | `				sVArg.sTypeName = pAttr->sTypeName;` |
|         7 | 1863 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       ! 0 | 1864 | `					sVArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|       ! 0 | 1865 | `				}` |
|         7 | 1866 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|         3 | 1867 | `			}` |
|        15 | 1868 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|        15 | 1869 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1870 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1871 | `				return SXERR_ABORT;` |
|         - | 1872 | `			}` |
|        15 | 1873 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        15 | 1874 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|         - | 1875 | `		}` |
|       130 | 1876 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|       131 | 1877 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|         - | 1878 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|       ! 0 | 1879 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - | 1880 | `				"Abstract property hook cannot have body");` |
|       ! 0 | 1881 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1882 | `				return SXERR_ABORT;` |
|         - | 1883 | `			}` |
|       ! 0 | 1884 | `			return SXERR_CORRUPT;` |
|         - | 1885 | `		}` |
|       131 | 1886 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - | 1887 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|       131 | 1888 | `		if( pMeth == 0 ){` |
|       ! 0 | 1889 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1890 | `			return SXERR_ABORT;` |
|         - | 1891 | `		}` |
|       131 | 1892 | `		pMeth->sFunc.nLine = nHLine;` |
|       131 | 1893 | `		if( !bGet ){` |
|         - | 1894 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|        61 | 1895 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        17 | 1896 | `				SyToken *pRp = 0;` |
|        17 | 1897 | `				pGen->pIn++;` |
|        17 | 1898 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|        17 | 1899 | `				if( pRp >= pGen->pEnd ){` |
|       ! 0 | 1900 | `					goto HookSyntax;` |
|         - | 1901 | `				}` |
|        17 | 1902 | `				if( pGen->pIn < pRp ){` |
|        17 | 1903 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|        17 | 1904 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 1905 | `						return SXERR_ABORT;` |
|         - | 1906 | `					}` |
|         8 | 1907 | `				}` |
|        17 | 1908 | `				pGen->pIn = &pRp[1];` |
|         8 | 1909 | `			}` |
|        61 | 1910 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|         - | 1911 | `				/* Implicit $value formal */` |
|         - | 1912 | `				ph7_vm_func_arg sVArg;` |
|        45 | 1913 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        45 | 1914 | `				if( zVName == 0 ){` |
|       ! 0 | 1915 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1916 | `					return SXERR_ABORT;` |
|         - | 1917 | `				}` |
|        45 | 1918 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        45 | 1919 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        45 | 1920 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        45 | 1921 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        45 | 1922 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        45 | 1923 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|        45 | 1924 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        22 | 1925 | `			}` |
|        30 | 1926 | `		}` |
|       165 | 1927 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - | 1928 | `			/* Block body */` |
|        69 | 1929 | `			SyToken *pBodyStart = pGen->pIn;` |
|        69 | 1930 | `			SyToken *pCloser = 0;` |
|        69 | 1931 | `			int bParentCall = 0;` |
|        69 | 1932 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|        69 | 1933 | `			if( pCloser < pGen->pEnd ){` |
|         - | 1934 | `				SyToken *pScan;` |
|       753 | 1935 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|       687 | 1936 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|         3 | 1937 | `						bParentCall = 1;` |
|         3 | 1938 | `						break;` |
|         - | 1939 | `					}` |
|       343 | 1940 | `				}` |
|        34 | 1941 | `			}` |
|        69 | 1942 | `			if( bParentCall ){` |
|         - | 1943 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|         - | 1944 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|         - | 1945 | `				 * hook method), then continue past the original body. */` |
|         - | 1946 | `				SySet sBody;` |
|         3 | 1947 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|         3 | 1948 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 | 1949 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|         3 | 1950 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 1951 | `					SySetRelease(&sBody);` |
|       ! 0 | 1952 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1953 | `					return SXERR_ABORT;` |
|         - | 1954 | `				}` |
|         3 | 1955 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 | 1956 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         3 | 1957 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|         3 | 1958 | `				pGen->pIn = &pCloser[1];` |
|         3 | 1959 | `				pGen->pEnd = pSavedEnd;` |
|         3 | 1960 | `				SySetRelease(&sBody);` |
|         3 | 1961 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 1962 | `					return SXERR_ABORT;` |
|         - | 1963 | `				}` |
|         3 | 1964 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|         2 | 1965 | `			}else{` |
|        67 | 1966 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        67 | 1967 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 1968 | `					return SXERR_ABORT;` |
|         - | 1969 | `				}` |
|        67 | 1970 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|         - | 1971 | `			}` |
|        69 | 1972 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        17 | 1973 | `				bRefsSelf = 1;` |
|         9 | 1974 | `			}` |
|       128 | 1975 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|         - | 1976 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|         - | 1977 | `			GenBlock *pBlock;` |
|         - | 1978 | `			SySet *pInstrContainer;` |
|         - | 1979 | `			SyToken *pBodyStart;` |
|         - | 1980 | `			SyToken *pExprEnd;` |
|        63 | 1981 | `			SyToken *pSavedEnd = 0;` |
|         - | 1982 | `			SySet sBody;` |
|        63 | 1983 | `			int bParentCall = 0;` |
|        63 | 1984 | `			pGen->pIn++; /* Jump '=>' */` |
|        63 | 1985 | `			pBodyStart = pGen->pIn;` |
|         - | 1986 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|         - | 1987 | `			 * would end the enclosing hook list) and rewrite any` |
|         - | 1988 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|         - | 1989 | `			 * method on a token copy. */` |
|         - | 1990 | `			{` |
|        63 | 1991 | `				sxi32 iNest = 0;` |
|        63 | 1992 | `				pExprEnd = pBodyStart;` |
|       355 | 1993 | `				while( pExprEnd < pGen->pEnd ){` |
|       355 | 1994 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         9 | 1995 | `						iNest++;` |
|       351 | 1996 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         9 | 1997 | `						if( iNest <= 0 ){` |
|       ! 0 | 1998 | `							break;` |
|         - | 1999 | `						}` |
|         9 | 2000 | `						iNest--;` |
|       343 | 2001 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|        63 | 2002 | `						break;` |
|         - | 2003 | `					}` |
|       293 | 2004 | `					pExprEnd++;` |
|         1 | 2005 | `				}` |
|         - | 2006 | `			}` |
|         - | 2007 | `			{` |
|         - | 2008 | `				SyToken *pScan;` |
|       335 | 2009 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|       275 | 2010 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|         3 | 2011 | `						bParentCall = 1;` |
|         3 | 2012 | `						break;` |
|         - | 2013 | `					}` |
|       137 | 2014 | `				}` |
|         - | 2015 | `			}` |
|        63 | 2016 | `			if( bParentCall ){` |
|         3 | 2017 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 | 2018 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|         3 | 2019 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 2020 | `					SySetRelease(&sBody);` |
|       ! 0 | 2021 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 2022 | `					return SXERR_ABORT;` |
|         - | 2023 | `				}` |
|         3 | 2024 | `				pSavedEnd = pGen->pEnd;` |
|         3 | 2025 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 | 2026 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         1 | 2027 | `			}` |
|        94 | 2028 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|        62 | 2029 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|        63 | 2030 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 2031 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|       ! 0 | 2032 | `				return SXERR_ABORT;` |
|         - | 2033 | `			}` |
|        63 | 2034 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        63 | 2035 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|        63 | 2036 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|        63 | 2037 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        63 | 2038 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        63 | 2039 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        63 | 2040 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        63 | 2041 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        63 | 2042 | `			if( bParentCall ){` |
|         3 | 2043 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|         3 | 2044 | `				pGen->pEnd = pSavedEnd;` |
|         3 | 2045 | `				SySetRelease(&sBody);` |
|         1 | 2046 | `			}` |
|        63 | 2047 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2048 | `				return SXERR_ABORT;` |
|         - | 2049 | `			}` |
|        63 | 2050 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|        63 | 2051 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        37 | 2052 | `				bRefsSelf = 1;` |
|        18 | 2053 | `			}` |
|        63 | 2054 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        63 | 2055 | `				pGen->pIn++; /* Jump ';' */` |
|        31 | 2056 | `			}` |
|        63 | 2057 | `			if( !bGet ){` |
|         - | 2058 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|         - | 2059 | `				 * the dispatcher consumes the implicit return value — which` |
|         - | 2060 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|         - | 2061 | ``				 * for `$this->NAME = expr`). */`` |
|         3 | 2062 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|         3 | 2063 | `				bRefsSelf = 1;` |
|         1 | 2064 | `			}` |
|        32 | 2065 | `		}else{` |
|       ! 0 | 2066 | `			goto HookSyntax;` |
|         - | 2067 | `		}` |
|       131 | 2068 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       131 | 2069 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 2070 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 2071 | `			return SXERR_ABORT;` |
|         - | 2072 | `		}` |
|       131 | 2073 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|         1 | 2074 | `	}` |
|        95 | 2075 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|       ! 0 | 2076 | `		goto HookSyntax;` |
|         - | 2077 | `	}` |
|        95 | 2078 | `	pGen->pIn++; /* Jump '}' */` |
|        95 | 2079 | `	if( !bRefsSelf ){` |
|         - | 2080 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|         - | 2081 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|         - | 2082 | `		 * a default value (compile fatal, php's exact wording). */` |
|        41 | 2083 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|        41 | 2084 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       ! 0 | 2085 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 2086 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|       ! 0 | 2087 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 | 2088 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2089 | `				return SXERR_ABORT;` |
|         - | 2090 | `			}` |
|       ! 0 | 2091 | `			return SXERR_CORRUPT;` |
|         - | 2092 | `		}` |
|        20 | 2093 | `	}` |
|        95 | 2094 | `	return SXRET_OK;` |
|       ! 0 | 2095 | `HookSyntax:` |
|       ! 0 | 2096 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 2097 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|       ! 0 | 2098 | `		&pClass->sName,&pAttr->sName);` |
|       ! 0 | 2099 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 2100 | `		return SXERR_ABORT;` |
|         - | 2101 | `	}` |
|       ! 0 | 2102 | `	return SXERR_CORRUPT;` |
|        48 | 2103 | `}` |
|         - | 2104 | `/*` |
|         - | 2105 | ` * Compile an object interface.` |
|         - | 2106 | ` *  According to the PHP language reference manual` |
|         - | 2107 | ` *   Object Interfaces:` |
|         - | 2108 | ` *   Object interfaces allow you to create code which specifies which methods` |
|         - | 2109 | ` *   a class must implement, without having to define how these methods are handled.` |
|         - | 2110 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|         - | 2111 | ` *   class, but without any of the methods having their contents defined.` |
|         - | 2112 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|         - | 2113 | ` */` |
|     74668 | 2114 | `PH7_PRIVATE sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|         5 | 2115 | `{` |
|     74673 | 2116 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 2117 | `	ph7_class *pClass,*pBase;` |
|     74673 | 2118 | `	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' */` |
|         - | 2119 | `	SyToken *pEnd,*pTmp;` |
|         - | 2120 | `	SyString *pName;` |
|         - | 2121 | `	sxi32 nKwrd;` |
|         - | 2122 | `	sxi32 rc;` |
|         - | 2123 | `	{` |
|         - | 2124 | `		/* Deferral gate: parent interfaces may need an autoloader` |
|         - | 2125 | `		 * that has not run yet. */` |
|         - | 2126 | `		sxi32 rcDefer;` |
|     74673 | 2127 | `		if( GenStateMaybeDeferClass(pGen,0,PH7_DEFER_KIND_INTERFACE,&rcDefer) ){` |
|         3 | 2128 | `			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - | 2129 | `		}` |
|         - | 2130 | `	}` |
|         - | 2131 | `	/* Jump the 'interface' keyword */` |
|     74671 | 2132 | `	pGen->pIn++;` |
|         - | 2133 | `	/* Extract interface name */` |
|     74671 | 2134 | `	pName = &pGen->pIn->sData;` |
|         - | 2135 | `	/* Advance the stream cursor */` |
|     74671 | 2136 | `	pGen->pIn++;` |
|         - | 2137 | `	/* Build FQN and obtain a raw class */ {` |
|         - | 2138 | `		SyBlob sFQN;` |
|         - | 2139 | `		SyString sFQNStr;` |
|     74671 | 2140 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     74671 | 2141 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     74671 | 2142 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|     74671 | 2143 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     74671 | 2144 | `		SyBlobRelease(&sFQN);` |
|         - | 2145 | `	}` |
|     74671 | 2146 | `	if( pClass == 0 ){` |
|       ! 0 | 2147 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 2148 | `		return SXERR_ABORT;` |
|         - | 2149 | `	}` |
|     74671 | 2150 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     74671 | 2151 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 2152 | `		return SXERR_ABORT;` |
|         - | 2153 | `	}` |
|         - | 2154 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|     74671 | 2155 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|         - | 2156 | `	/* Assume no base class is given */` |
|     74671 | 2157 | `	pBase = 0;` |
|     74671 | 2158 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     29001 | 2159 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     29001 | 2160 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a, c, … */ ){` |
|         - | 2161 | `			/* php lets an interface extend SEVERAL parent interfaces. The first` |
|         - | 2162 | `			 * becomes pBase (single-inheritance chain, hDerived); every extra one` |
|         - | 2163 | `			 * is recorded via PH7_ClassImplement so it lands in aInterface — the` |
|         - | 2164 | `			 * runtime subtype walk (VmInterfaceReaches) follows both. */` |
|     29001 | 2165 | `			pGen->pIn++;` |
|     14499 | 2166 | `			for(;;){` |
|         - | 2167 | `				SyBlob sResolved;` |
|         - | 2168 | `				SyString sBaseName;` |
|         - | 2169 | `				sxu32 nRefLine;` |
|         - | 2170 | `				ph7_class *pParent;` |
|     29003 | 2171 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     29003 | 2172 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     29003 | 2173 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 2174 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 2175 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 2176 | `						"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|       ! 0 | 2177 | `						pName);` |
|       ! 0 | 2178 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 2179 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2180 | `						return SXERR_ABORT;` |
|         - | 2181 | `					}` |
|       ! 0 | 2182 | `					return SXRET_OK;` |
|         - | 2183 | `				}` |
|     43502 | 2184 | `				pParent = PH7_VmExtractClass(pGen->pVm,` |
|     28998 | 2185 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     29003 | 2186 | `				SyStringInitFromBuf(&sBaseName,` |
|         - | 2187 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 2188 | `				/* Only interfaces is allowed */` |
|     29003 | 2189 | `				while( pParent && (pParent->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 2190 | `					pParent = pParent->pNextName;` |
|       ! 0 | 2191 | `				}` |
|     29003 | 2192 | `				if( pParent == 0 ){` |
|       ! 0 | 2193 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 2194 | `						"Nonexistent base interface '%z'",&sBaseName);` |
|       ! 0 | 2195 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2196 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 2197 | `						return SXERR_ABORT;` |
|       ! 0 | 2198 | `					}` |
|     29003 | 2199 | `				}else if( pBase == 0 ){` |
|         - | 2200 | `					/* First parent → single-inheritance base */` |
|     29001 | 2201 | `					pBase = pParent;` |
|     14503 | 2202 | `				}else{` |
|         - | 2203 | `					/* Additional parent → record it in aInterface (+ copy its` |
|         - | 2204 | `					 * constants/method stubs) so instanceof reaches it too. */` |
|         3 | 2205 | `					PH7_ClassImplement(pClass,pParent);` |
|         - | 2206 | `				}` |
|     29003 | 2207 | `				SyBlobRelease(&sResolved);` |
|         - | 2208 | `				/* Continue on a comma-separated list */` |
|     29003 | 2209 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|         3 | 2210 | `					pGen->pIn++;` |
|         3 | 2211 | `					continue;` |
|         - | 2212 | `				}` |
|     29001 | 2213 | `				break;` |
|       ! 0 | 2214 | `			}` |
|     14498 | 2215 | `		}` |
|     14498 | 2216 | `	}` |
|     74671 | 2217 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - | 2218 | `		/* Syntax error */` |
|       ! 0 | 2219 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|       ! 0 | 2220 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 2221 | `		if( rc == SXERR_ABORT ){` |
|         - | 2222 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 2223 | `			return SXERR_ABORT;` |
|         - | 2224 | `		}` |
|       ! 0 | 2225 | `		return SXRET_OK;` |
|         - | 2226 | `	}` |
|     74671 | 2227 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     74671 | 2228 | `	pEnd = 0; /* cc warning */` |
|         - | 2229 | `	/* Delimit the interface body */` |
|     74671 | 2230 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|     74671 | 2231 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 2232 | `		/* Syntax error */` |
|       ! 0 | 2233 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|       ! 0 | 2234 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 2235 | `		if( rc == SXERR_ABORT ){` |
|         - | 2236 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 2237 | `			return SXERR_ABORT;` |
|         - | 2238 | `		}` |
|       ! 0 | 2239 | `		return SXRET_OK;` |
|         - | 2240 | `	}` |
|         - | 2241 | `	/* The delimiter token is the interface body's closing brace */` |
|     74671 | 2242 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 2243 | `	/* Swap token stream */` |
|     74671 | 2244 | `	pTmp = pGen->pEnd;` |
|     74671 | 2245 | `	pGen->pEnd = pEnd;` |
|         - | 2246 | `	/* This interface is now the lexical class for its body (see pCurClass) — a` |
|         - | 2247 | `	 * const default here is not a trait, so __TRAIT__ stays "". */` |
|     74671 | 2248 | `	pGen->pCurClass = pClass;` |
|         - | 2249 | `	/* Start the parse process` |
|         - | 2250 | `	 * Note (According to the PHP reference manual):` |
|         - | 2251 | `	 *  Only constants and function signatures(without body) are allowed.` |
|         - | 2252 | `	 *  Only 'public' visibility is allowed.` |
|         - | 2253 | `	 */` |
|    136731 | 2254 | `	for(;;){` |
|         - | 2255 | `		/* Jump leading/trailing semi-colons */` |
|    472267 | 2256 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    198801 | 2257 | `			pGen->pIn++;` |
|         5 | 2258 | `		}` |
|    273471 | 2259 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 2260 | `			/* End of interface body */` |
|     74667 | 2261 | `			break;` |
|         - | 2262 | `		}` |
|         - | 2263 | `		/* Bind a directly-preceding docblock to this member */` |
|    198809 | 2264 | `		GenStateSetPendingDoc(&(*pGen));` |
|    198809 | 2265 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 2266 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 2267 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|       ! 0 | 2268 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 2269 | `			if( rc == SXERR_ABORT ){` |
|         - | 2270 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 2271 | `				return SXERR_ABORT;` |
|         - | 2272 | `			}` |
|       ! 0 | 2273 | `			goto done;` |
|         - | 2274 | `		}` |
|         - | 2275 | `		/* Extract the current keyword */` |
|    198809 | 2276 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    198809 | 2277 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - | 2278 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|         - | 2279 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|         3 | 2280 | `			const char *zKind = "member";` |
|         3 | 2281 | `			SyString *pMemberName = 0;` |
|         3 | 2282 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|         3 | 2283 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|         3 | 2284 | `				if( nNext == PH7_TKWRD_CONST ){` |
|         3 | 2285 | `					zKind = "constant";` |
|         3 | 2286 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|         3 | 2287 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|         2 | 2288 | `					}` |
|         1 | 2289 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 2290 | `					zKind = "method";` |
|       ! 0 | 2291 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       ! 0 | 2292 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       ! 0 | 2293 | `					}` |
|       ! 0 | 2294 | `				}` |
|         1 | 2295 | `			}` |
|         3 | 2296 | `			if( pMemberName ){` |
|         4 | 2297 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         1 | 2298 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|         2 | 2299 | `			}else{` |
|       ! 0 | 2300 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 2301 | `					"Access type for interface %s must be public",zKind);` |
|         - | 2302 | `			}` |
|         3 | 2303 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2304 | `				return SXERR_ABORT;` |
|         - | 2305 | `			}` |
|         3 | 2306 | `			goto done;` |
|         - | 2307 | `		}` |
|    198807 | 2308 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|       ! 0 | 2309 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 2310 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 | 2311 | `			if( rc == SXERR_ABORT ){` |
|         - | 2312 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 2313 | `				return SXERR_ABORT;` |
|         - | 2314 | `			}` |
|       ! 0 | 2315 | `			goto done;` |
|         - | 2316 | `		}` |
|    198807 | 2317 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|         - | 2318 | `			/* Advance the stream cursor */` |
|    140821 | 2319 | `			pGen->pIn++;` |
|    140816 | 2320 | `			if( pGen->pIn < pGen->pEnd` |
|    140821 | 2321 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|    140816 | 2322 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         - | 2323 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|         - | 2324 | `				 * requirement. The attribute compiler + hook parser handle it` |
|         - | 2325 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|         - | 2326 | `				 * property without hooks is ITS "Interfaces may only include` |
|         - | 2327 | `				 * hooked properties" error). */` |
|       ! 0 | 2328 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 | 2329 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 | 2330 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 2331 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2332 | `						return SXERR_ABORT;` |
|         - | 2333 | `					}` |
|       ! 0 | 2334 | `					goto done;` |
|         - | 2335 | `				}` |
|       ! 0 | 2336 | `				continue;` |
|         - | 2337 | `			}` |
|    140821 | 2338 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|         - | 2339 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|         - | 2340 | `				 * '$' also opens a hooked-property requirement. */` |
|       ! 0 | 2341 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|       ! 0 | 2342 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|       ! 0 | 2343 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|       ! 0 | 2344 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 | 2345 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 | 2346 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 2347 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 2348 | `							return SXERR_ABORT;` |
|         - | 2349 | `						}` |
|       ! 0 | 2350 | `						goto done;` |
|         - | 2351 | `					}` |
|       ! 0 | 2352 | `					continue;` |
|         - | 2353 | `				}` |
|       ! 0 | 2354 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 2355 | `					"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 | 2356 | `				if( rc == SXERR_ABORT ){` |
|         - | 2357 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 | 2358 | `					return SXERR_ABORT;` |
|         - | 2359 | `				}` |
|       ! 0 | 2360 | `				goto done;` |
|         - | 2361 | `			}` |
|    140821 | 2362 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    140821 | 2363 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|         - | 2364 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|         - | 2365 | `				 * hooked-property requirement (PHP 8.4). */` |
|         4 | 2366 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|         5 | 2367 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|         7 | 2368 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|         2 | 2369 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|         5 | 2370 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 2371 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 2372 | `							return SXERR_ABORT;` |
|         - | 2373 | `						}` |
|       ! 0 | 2374 | `						goto done;` |
|         - | 2375 | `					}` |
|         5 | 2376 | `					continue;` |
|         - | 2377 | `				}` |
|       ! 0 | 2378 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 2379 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 | 2380 | `				if( rc == SXERR_ABORT ){` |
|         - | 2381 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 | 2382 | `					return SXERR_ABORT;` |
|         - | 2383 | `				}` |
|       ! 0 | 2384 | `				goto done;` |
|         - | 2385 | `			}` |
|     70406 | 2386 | `		}` |
|    198803 | 2387 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|         - | 2388 | `			/* Parse constant */` |
|     57987 | 2389 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|     57987 | 2390 | `			if( rc != SXRET_OK ){` |
|         3 | 2391 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 2392 | `					return SXERR_ABORT;` |
|         - | 2393 | `				}` |
|         3 | 2394 | `				goto done;` |
|         - | 2395 | `			}` |
|     28995 | 2396 | `		}else{` |
|    140821 | 2397 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|    140821 | 2398 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - | 2399 | `				/* Static method,record that */` |
|     12425 | 2400 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|         - | 2401 | `				/* Advance the stream cursor */` |
|     12425 | 2402 | `				pGen->pIn++;` |
|     12420 | 2403 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     12425 | 2404 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 2405 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 2406 | `							"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 | 2407 | `						if( rc == SXERR_ABORT ){` |
|         - | 2408 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 2409 | `							return SXERR_ABORT;` |
|         - | 2410 | `						}` |
|       ! 0 | 2411 | `						goto done;` |
|         - | 2412 | `				}` |
|      6210 | 2413 | `			}` |
|         - | 2414 | `			/* Process method signature (no body for interface methods) */` |
|    140821 | 2415 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|    140821 | 2416 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 2417 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 2418 | `					return SXERR_ABORT;` |
|         - | 2419 | `				}` |
|       ! 0 | 2420 | `				goto done;` |
|         - | 2421 | `			}` |
|         - | 2422 | `		}` |
|         5 | 2423 | `	}` |
|         - | 2424 | `	/* Reject a php-fatal redeclaration before hoisting the interface */` |
|     74667 | 2425 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|         3 | 2426 | `		return SXERR_ABORT;` |
|         - | 2427 | `	}` |
|         - | 2428 | `	/* Install the interface */` |
|     74665 | 2429 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     74665 | 2430 | `	if( rc == SXRET_OK && pBase ){` |
|         - | 2431 | `		/* Inherit from the base interface */` |
|     29001 | 2432 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     14498 | 2433 | `	}` |
|     74665 | 2434 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 2435 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 2436 | `		return SXERR_ABORT;` |
|         - | 2437 | `	}` |
|     37330 | 2438 | `done:` |
|     74669 | 2439 | `	pGen->pCurClass = pSavedCurClass;` |
|         - | 2440 | `	/* Point beyond the interface body */` |
|     74669 | 2441 | `	pGen->pIn  = &pEnd[1];` |
|     74669 | 2442 | `	pGen->pEnd = pTmp;` |
|     74669 | 2443 | `	return PH7_OK;` |
|     37339 | 2444 | `}` |
|         - | 2445 | `/*` |
|         - | 2446 | ` * Compile a user-defined class.` |
|         - | 2447 | ` * According to the PHP language reference manual` |
|         - | 2448 | ` *  class` |
|         - | 2449 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|         - | 2450 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|         - | 2451 | ` *  of the properties and methods belonging to the class.` |
|         - | 2452 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|         - | 2453 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|         - | 2454 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|         - | 2455 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - | 2456 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|         - | 2457 | ` *  (called "methods").` |
|         - | 2458 | ` */` |
|         - | 2459 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|         - | 2460 | `typedef struct TraitUseEntry TraitUseEntry;` |
|         - | 2461 | `struct TraitUseEntry {` |
|         - | 2462 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|         - | 2463 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|         - | 2464 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|         - | 2465 | `};` |
|         - | 2466 | `/*` |
|         - | 2467 | ` * Validate that methods implementing interface contracts have compatible` |
|         - | 2468 | ` * signatures: public visibility and at least as many parameters as declared.` |
|         - | 2469 | ` */` |
|    482460 | 2470 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 2471 | `{` |
|         - | 2472 | `	ph7_class **apIface;` |
|         - | 2473 | `	sxu32 nIface,i;` |
|         - | 2474 | `	sxi32 rc;` |
|    482465 | 2475 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|       ! 0 | 2476 | `		return SXRET_OK;` |
|         - | 2477 | `	}` |
|    482465 | 2478 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    482465 | 2479 | `	nIface = SySetUsed(&pClass->aInterface);` |
|    934121 | 2480 | `	for(i = 0; i < nIface; i++){` |
|    451661 | 2481 | `		ph7_class *pIface = apIface[i];` |
|         - | 2482 | `		SyHashEntry *pEntry;` |
|    451661 | 2483 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   1329989 | 2484 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|    878333 | 2485 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|         - | 2486 | `			ph7_class_method *pImplMeth;` |
|    878333 | 2487 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|         - | 2488 | `			/* Find the implementing method in the class */` |
|    878333 | 2489 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|    878333 | 2490 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        23 | 2491 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|         - | 2492 | `			}` |
|         - | 2493 | `			/* Check visibility: interface methods must be implemented as public */` |
|    878315 | 2494 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|         4 | 2495 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - | 2496 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|         1 | 2497 | `					&pClass->sName,pMName,&pIface->sName);` |
|         3 | 2498 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 2499 | `					return SXERR_ABORT;` |
|         - | 2500 | `				}` |
|         1 | 2501 | `			}` |
|         - | 2502 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|         - | 2503 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|         - | 2504 | `			 */` |
|         - | 2505 | `			{` |
|    878315 | 2506 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|    878315 | 2507 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|    878315 | 2508 | `				int sigError = 0;` |
|    878315 | 2509 | `				if( nImplArgs < nIfaceArgs ){` |
|         3 | 2510 | `					sigError = 1;` |
|    878314 | 2511 | `				}else if( nImplArgs > nIfaceArgs ){` |
|         - | 2512 | `					/* Extra parameters must all have default values */` |
|      4149 | 2513 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|         - | 2514 | `					sxu32 k;` |
|      8291 | 2515 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|      4149 | 2516 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|         3 | 2517 | `							sigError = 1;` |
|         3 | 2518 | `							break;` |
|         - | 2519 | `						}` |
|      2076 | 2520 | `					}` |
|      2072 | 2521 | `				}` |
|    878315 | 2522 | `				if( sigError ){` |
|         - | 2523 | `					SyBlob sImplSig, sIfaceSig;` |
|         - | 2524 | `					ph7_vm_func_arg *aArgs;` |
|         - | 2525 | `					sxu32 j;` |
|         6 | 2526 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|         6 | 2527 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|         - | 2528 | `					/* Build implementing method signature */` |
|         6 | 2529 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        12 | 2530 | `					for(j = 0; j < nImplArgs; j++){` |
|         8 | 2531 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|         8 | 2532 | `						SyBlobAppend(&sImplSig,"$",1);` |
|         8 | 2533 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 | 2534 | `					}` |
|         - | 2535 | `					/* Build interface method signature */` |
|         6 | 2536 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|        12 | 2537 | `					for(j = 0; j < nIfaceArgs; j++){` |
|         8 | 2538 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|         8 | 2539 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|         8 | 2540 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 | 2541 | `					}` |
|         8 | 2542 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - | 2543 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|         2 | 2544 | `						&pClass->sName,pMName,` |
|         4 | 2545 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|         2 | 2546 | `						&pIface->sName,pMName,` |
|         4 | 2547 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|         6 | 2548 | `					SyBlobRelease(&sImplSig);` |
|         6 | 2549 | `					SyBlobRelease(&sIfaceSig);` |
|         6 | 2550 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2551 | `						return SXERR_ABORT;` |
|         - | 2552 | `					}` |
|         2 | 2553 | `				}` |
|         - | 2554 | `			}` |
|         5 | 2555 | `		}` |
|    225833 | 2556 | `	}` |
|    482465 | 2557 | `	return SXRET_OK;` |
|    241235 | 2558 | `}` |
|         - | 2559 | `/*` |
|         - | 2560 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|         - | 2561 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|         - | 2562 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|         - | 2563 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|         - | 2564 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|         - | 2565 | ` * means that specific hook is still missing.` |
|         - | 2566 | ` */` |
|        38 | 2567 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|         5 | 2568 | `{` |
|         - | 2569 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|         - | 2570 | `	ph7_class_attr *pProp;` |
|        38 | 2571 | `	if( pMName->nByte <= nPfx` |
|        27 | 2572 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|         4 | 2573 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|        36 | 2574 | `		return 0; /* not a hook stub */` |
|         - | 2575 | `	}` |
|         7 | 2576 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|         7 | 2577 | `	return pProp != 0` |
|         6 | 2578 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|         3 | 2579 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|        24 | 2580 | `}` |
|         - | 2581 | `/*` |
|         - | 2582 | ` * Append an abstract member's display name to the message blob, translating a` |
|         - | 2583 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|         - | 2584 | ` */` |
|        16 | 2585 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|         4 | 2586 | `{` |
|         - | 2587 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        16 | 2588 | `	if( pMName->nByte > nPfx` |
|        12 | 2589 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|       ! 0 | 2590 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|       ! 0 | 2591 | `		SyBlobAppend(pMsg,"$",1);` |
|       ! 0 | 2592 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|       ! 0 | 2593 | `		SyBlobAppend(pMsg,"::",2);` |
|       ! 0 | 2594 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|       ! 0 | 2595 | `		return;` |
|         - | 2596 | `	}` |
|        20 | 2597 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|        12 | 2598 | `}` |
|         - | 2599 | `/*` |
|         - | 2600 | ` * Check that a concrete class has no remaining abstract methods.` |
|         - | 2601 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|         - | 2602 | ` */` |
|    482460 | 2603 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 2604 | `{` |
|         - | 2605 | `	ph7_class_method *pMeth;` |
|         - | 2606 | `	SyHashEntry *pEntry;` |
|         - | 2607 | `	sxu32 nAbstract;` |
|         - | 2608 | `	SyBlob sMsg;` |
|         - | 2609 | `	sxi32 rc;` |
|         - | 2610 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|    482465 | 2611 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     20763 | 2612 | `		return SXRET_OK;` |
|         - | 2613 | `	}` |
|         - | 2614 | `	/* Count abstract methods */` |
|    461707 | 2615 | `	nAbstract = 0;` |
|    461707 | 2616 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   6897372 | 2617 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   6204819 | 2618 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   6204819 | 2619 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        27 | 2620 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|         7 | 2621 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 2622 | `			}` |
|        20 | 2623 | `			nAbstract++;` |
|         8 | 2624 | `		}` |
|         5 | 2625 | `	}` |
|    461707 | 2626 | `	if( nAbstract == 0 ){` |
|    461693 | 2627 | `		return SXRET_OK;` |
|         - | 2628 | `	}` |
|         - | 2629 | `	/* Build the error message listing all abstract methods with origins */` |
|        18 | 2630 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        18 | 2631 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|         - | 2632 | `		"be declared abstract or implement the remaining method%s (",` |
|         7 | 2633 | `		&pClass->sName,nAbstract,` |
|         7 | 2634 | `		(nAbstract > 1 ? "s" : ""),` |
|         7 | 2635 | `		(nAbstract > 1 ? "s" : ""));` |
|         - | 2636 | `	/* Second pass: list methods with origins */` |
|         - | 2637 | `	{` |
|        18 | 2638 | `		sxu32 nListed = 0;` |
|        18 | 2639 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|        36 | 2640 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|        22 | 2641 | `			ph7_class *pOrigin = 0;` |
|         - | 2642 | `			SyString *pMName;` |
|        22 | 2643 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        22 | 2644 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|         3 | 2645 | `				continue;` |
|         - | 2646 | `			}` |
|        20 | 2647 | `			pMName = &pMeth->sFunc.sName;` |
|        20 | 2648 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|       ! 0 | 2649 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 2650 | `			}` |
|        20 | 2651 | `			if( nListed > 0 ){` |
|         3 | 2652 | `				SyBlobAppend(&sMsg,", ",2);` |
|         1 | 2653 | `			}` |
|         - | 2654 | `			/* Find the origin of this abstract method.` |
|         - | 2655 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|         - | 2656 | `			 * inheritance chains) take precedence for interface-declared` |
|         - | 2657 | `			 * methods. Abstract class methods only win when the class` |
|         - | 2658 | `			 * itself declared the abstract method (not inherited from` |
|         - | 2659 | `			 * an interface). Trait methods are adopted into the using` |
|         - | 2660 | `			 * class's namespace.` |
|         - | 2661 | `			 */` |
|         - | 2662 | `			{` |
|         - | 2663 | `				ph7_class **apIface;` |
|         - | 2664 | `				ph7_class **apTrait;` |
|         - | 2665 | `				ph7_class *pWalk;` |
|         - | 2666 | `				sxu32 i;` |
|         - | 2667 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|         - | 2668 | `				 * (one that was written in the class body, not inherited from an` |
|         - | 2669 | `				 * interface). PHP attributes origin to the declaring class.` |
|         - | 2670 | `				 */` |
|        20 | 2671 | `				if( pClass->pBase ){` |
|        11 | 2672 | `					pWalk = pClass->pBase;` |
|        19 | 2673 | `					while( pWalk ){` |
|         - | 2674 | `						ph7_class_method *pParentMeth;` |
|        13 | 2675 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|        13 | 2676 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|         - | 2677 | `							/* Exclude methods that came from an interface anywhere` |
|         - | 2678 | `							 * in this class's ancestor chain.` |
|         - | 2679 | `							 */` |
|        13 | 2680 | `							int fromIface = 0;` |
|        13 | 2681 | `							ph7_class *pAnc = pWalk;` |
|        17 | 2682 | `							while( pAnc ){` |
|         - | 2683 | `								ph7_class **apPI;` |
|         - | 2684 | `								sxu32 j;` |
|        15 | 2685 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|        15 | 2686 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|        10 | 2687 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|        10 | 2688 | `										fromIface = 1;` |
|        10 | 2689 | `										break;` |
|         - | 2690 | `									}` |
|       ! 0 | 2691 | `								}` |
|        15 | 2692 | `								if( fromIface ) break;` |
|         6 | 2693 | `								pAnc = pAnc->pBase;` |
|         2 | 2694 | `							}` |
|        13 | 2695 | `							if( !fromIface ){` |
|         3 | 2696 | `								pOrigin = pWalk;` |
|         3 | 2697 | `								break;` |
|         - | 2698 | `							}` |
|         4 | 2699 | `						}` |
|        10 | 2700 | `						pWalk = pWalk->pBase;` |
|         2 | 2701 | `					}` |
|         4 | 2702 | `				}` |
|         - | 2703 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|         - | 2704 | `				 * each interface's own parent chain for the deepest origin.` |
|         - | 2705 | `				 */` |
|        20 | 2706 | `				if( !pOrigin ){` |
|        18 | 2707 | `					pWalk = pClass;` |
|        40 | 2708 | `					while( pWalk && !pOrigin ){` |
|        26 | 2709 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|        26 | 2710 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|        16 | 2711 | `							ph7_class *pIface = apIface[i];` |
|        16 | 2712 | `							ph7_class *pDeepest = 0;` |
|        28 | 2713 | `							while( pIface ){` |
|        16 | 2714 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|        16 | 2715 | `									pDeepest = pIface;` |
|         6 | 2716 | `								}` |
|        16 | 2717 | `								pIface = pIface->pBase;` |
|         4 | 2718 | `							}` |
|        16 | 2719 | `							if( pDeepest ){` |
|        16 | 2720 | `								pOrigin = pDeepest;` |
|        16 | 2721 | `								break;` |
|         - | 2722 | `							}` |
|       ! 0 | 2723 | `						}` |
|        26 | 2724 | `						pWalk = pWalk->pBase;` |
|         4 | 2725 | `					}` |
|         7 | 2726 | `				}` |
|         - | 2727 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|        20 | 2728 | `				if( !pOrigin ){` |
|         3 | 2729 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|         3 | 2730 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|         3 | 2731 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|         3 | 2732 | `							pOrigin = pClass;` |
|         3 | 2733 | `							break;` |
|         - | 2734 | `						}` |
|       ! 0 | 2735 | `					}` |
|         1 | 2736 | `				}` |
|         - | 2737 | `			}` |
|        20 | 2738 | `			if( pOrigin ){` |
|        20 | 2739 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|        12 | 2740 | `			}else{` |
|         - | 2741 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|       ! 0 | 2742 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|         - | 2743 | `			}` |
|        20 | 2744 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|        20 | 2745 | `			nListed++;` |
|         4 | 2746 | `		}` |
|         - | 2747 | `	}` |
|        18 | 2748 | `	SyBlobAppend(&sMsg,")",1);` |
|        25 | 2749 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|        14 | 2750 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|        18 | 2751 | `	SyBlobRelease(&sMsg);` |
|        18 | 2752 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 2753 | `		return SXERR_ABORT;` |
|         - | 2754 | `	}` |
|        18 | 2755 | `	return SXRET_OK;` |
|    241235 | 2756 | `}` |
|         - | 2757 | `/*` |
|         - | 2758 | ` * Parse a class/interface name reference from the current token stream.` |
|         - | 2759 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|         - | 2760 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|         - | 2761 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|         - | 2762 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|         - | 2763 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|         - | 2764 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|         - | 2765 | ` */` |
|   1071798 | 2766 | `PH7_PRIVATE sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|         5 | 2767 | `{` |
|   1071803 | 2768 | `	int isAbsolute = 0;` |
|   1071803 | 2769 | `	SyToken *pStart = pGen->pIn;` |
|         - | 2770 | `	SyBlob sName;` |
|   1071803 | 2771 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      9093 | 2772 | `		isAbsolute = 1;` |
|      9093 | 2773 | `		pGen->pIn++;` |
|      4544 | 2774 | `	}` |
|   1071803 | 2775 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        11 | 2776 | `		pGen->pIn = pStart;` |
|        11 | 2777 | `		return SXERR_INVALID;` |
|         - | 2778 | `	}` |
|   1071795 | 2779 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   1071795 | 2780 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   1071795 | 2781 | `	pGen->pIn++;` |
|   1607847 | 2782 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    536062 | 2783 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       113 | 2784 | `		SyBlobAppend(&sName,"\\",1);` |
|       113 | 2785 | `		pGen->pIn++;` |
|       113 | 2786 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       113 | 2787 | `		pGen->pIn++;` |
|         5 | 2788 | `	}` |
|   1071795 | 2789 | `	if( isAbsolute ){` |
|      9089 | 2790 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      4547 | 2791 | `	}else{` |
|         - | 2792 | `		SyString sRaw;` |
|   1062711 | 2793 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   1062711 | 2794 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|         - | 2795 | `	}` |
|   1071795 | 2796 | `	SyBlobRelease(&sName);` |
|   1071795 | 2797 | `	return SXRET_OK;` |
|    535904 | 2798 | `}` |
|         - | 2799 | `/*` |
|         - | 2800 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|         - | 2801 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|         - | 2802 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|         - | 2803 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|         - | 2804 | ` * either direction cannot run unbounded.` |
|         - | 2805 | ` */` |
|         - | 2806 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    211368 | 2807 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|         5 | 2808 | `{` |
|         - | 2809 | `	ph7_class **apParent;` |
|         - | 2810 | `	sxu32 n;` |
|    559403 | 2811 | `	while( pInterface ){` |
|    356325 | 2812 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|       ! 0 | 2813 | `			return FALSE;` |
|         - | 2814 | `		}` |
|    397746 | 2815 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|     82842 | 2816 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|      8295 | 2817 | `			return TRUE;` |
|         - | 2818 | `		}` |
|    348035 | 2819 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    348037 | 2820 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|         3 | 2821 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|       ! 0 | 2822 | `				return TRUE;` |
|         - | 2823 | `			}` |
|         2 | 2824 | `		}` |
|    348035 | 2825 | `		pInterface = pInterface->pBase;` |
|    348035 | 2826 | `		iDepth++;` |
|         5 | 2827 | `	}` |
|    203083 | 2828 | `	return FALSE;` |
|    105689 | 2829 | `}` |
|    211366 | 2830 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|         5 | 2831 | `{` |
|    211371 | 2832 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|         5 | 2833 | `}` |
|         - | 2834 | `/*` |
|         - | 2835 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|         - | 2836 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|         - | 2837 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|         - | 2838 | ` */` |
|      8290 | 2839 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|         5 | 2840 | `{` |
|      8299 | 2841 | `	while( pBase ){` |
|        10 | 2842 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|         2 | 2843 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|         3 | 2844 | `			return TRUE;` |
|         - | 2845 | `		}` |
|        10 | 2846 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|         6 | 2847 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|         3 | 2848 | `			return TRUE;` |
|         - | 2849 | `		}` |
|         5 | 2850 | `		pBase = pBase->pBase;` |
|         1 | 2851 | `	}` |
|      8291 | 2852 | `	return FALSE;` |
|      4150 | 2853 | `}` |
|         - | 2854 | `/*` |
|         - | 2855 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|         - | 2856 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|         - | 2857 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|         - | 2858 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|         - | 2859 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|         - | 2860 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|         - | 2861 | ` * pClass->aEnumCases for cases().` |
|         - | 2862 | ` */` |
|      8348 | 2863 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 2864 | `{` |
|      8353 | 2865 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 2866 | `	SySet *pInstrContainer;` |
|         - | 2867 | `	ph7_class_attr *pCase;` |
|         - | 2868 | `	SyString *pName;` |
|         - | 2869 | `	sxi32 rc;` |
|      8353 | 2870 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|      8353 | 2871 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 2872 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 2873 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|       ! 0 | 2874 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 2875 | `			return SXERR_ABORT;` |
|         - | 2876 | `		}` |
|       ! 0 | 2877 | `		goto Synchronize;` |
|         - | 2878 | `	}` |
|      8353 | 2879 | `	pName = &pGen->pIn->sData;` |
|         - | 2880 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|      8353 | 2881 | `	if( SyHashGet(&pClass->hConst,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       ! 0 | 2882 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 2883 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|       ! 0 | 2884 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 2885 | `			return SXERR_ABORT;` |
|         - | 2886 | `		}` |
|       ! 0 | 2887 | `		goto Synchronize;` |
|         - | 2888 | `	}` |
|      8353 | 2889 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 2890 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|      8353 | 2891 | `	if( pCase == 0 ){` |
|       ! 0 | 2892 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 2893 | `		return SXERR_ABORT;` |
|         - | 2894 | `	}` |
|      8353 | 2895 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|      8353 | 2896 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 2897 | `		return SXERR_ABORT;` |
|         - | 2898 | `	}` |
|      8353 | 2899 | `	pGen->pIn++; /* Jump the case name */` |
|      8353 | 2900 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|      8335 | 2901 | `		if( pClass->nEnumBacking == 0 ){` |
|         8 | 2902 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         2 | 2903 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|         6 | 2904 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2905 | `				return SXERR_ABORT;` |
|         - | 2906 | `			}` |
|         6 | 2907 | `			goto Synchronize;` |
|         - | 2908 | `		}` |
|      8331 | 2909 | `		pGen->pIn++; /* Jump the equal sign */` |
|         - | 2910 | `		/* Compile the backing value expression into the case's own container` |
|         - | 2911 | `		 * (same technique as class constants). */` |
|      8331 | 2912 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      8331 | 2913 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|      8331 | 2914 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      8331 | 2915 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 | 2916 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 2917 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|       ! 0 | 2918 | `		}` |
|      8331 | 2919 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      8331 | 2920 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      8331 | 2921 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 2922 | `			return SXERR_ABORT;` |
|         - | 2923 | `		}` |
|      4168 | 2924 | `	}else{` |
|        22 | 2925 | `		if( pClass->nEnumBacking != 0 ){` |
|       ! 0 | 2926 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 2927 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|       ! 0 | 2928 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2929 | `				return SXERR_ABORT;` |
|         - | 2930 | `			}` |
|       ! 0 | 2931 | `			goto Synchronize;` |
|         - | 2932 | `		}` |
|         - | 2933 | `	}` |
|      8349 | 2934 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|      8349 | 2935 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 2936 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 2937 | `		return SXERR_ABORT;` |
|         - | 2938 | `	}` |
|      8349 | 2939 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|      8349 | 2940 | `	return SXRET_OK;` |
|         2 | 2941 | `Synchronize:` |
|         - | 2942 | `	/* Synchronize with the first semi-colon */` |
|        14 | 2943 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|        10 | 2944 | `		pGen->pIn++;` |
|         2 | 2945 | `	}` |
|         6 | 2946 | `	return SXERR_CORRUPT;` |
|      4179 | 2947 | `}` |
|         - | 2948 | `/*` |
|         - | 2949 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|         - | 2950 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|         - | 2951 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|         - | 2952 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|         - | 2953 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|         - | 2954 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|         - | 2955 | ` * pointers into it (see the constructor-promotion precedent above).` |
|         - | 2956 | ` */` |
|      4184 | 2957 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 2958 | `{` |
|         - | 2959 | `	SyToken *pSaveIn,*pSaveEnd;` |
|         - | 2960 | `	const char *zBack;` |
|         - | 2961 | `	SySet sToken;` |
|         - | 2962 | `	char *zSrc;` |
|         - | 2963 | `	sxu32 nSrc,nMax;` |
|      4189 | 2964 | `	sxi32 rc = SXRET_OK;` |
|      4189 | 2965 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|      4184 | 2966 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|      4189 | 2967 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|      4189 | 2968 | `	if( zSrc == 0 ){` |
|       ! 0 | 2969 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 2970 | `		return SXERR_ABORT;` |
|         - | 2971 | `	}` |
|      4189 | 2972 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|      4189 | 2973 | `	if( pClass->nEnumBacking != 0 ){` |
|      6245 | 2974 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         - | 2975 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|         - | 2976 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|         - | 2977 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|      2080 | 2978 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|      2085 | 2979 | `	}else{` |
|        41 | 2980 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        12 | 2981 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|         - | 2982 | `	}` |
|      4189 | 2983 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      4189 | 2984 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|      4189 | 2985 | `	pSaveIn = pGen->pIn;` |
|      4189 | 2986 | `	pSaveEnd = pGen->pEnd;` |
|      4189 | 2987 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      4189 | 2988 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|     16693 | 2989 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|     12509 | 2990 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|         5 | 2991 | `	}` |
|      4189 | 2992 | `	pGen->pIn = pSaveIn;` |
|      4189 | 2993 | `	pGen->pEnd = pSaveEnd;` |
|      4189 | 2994 | `	SySetRelease(&sToken);` |
|      4189 | 2995 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|      2097 | 2996 | `}` |
|         - | 2997 | `/*` |
|         - | 2998 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|         - | 2999 | ` * __call/__callStatic/__invoke stay allowed).` |
|         - | 3000 | ` */` |
|         - | 3001 | `static const char *azEnumBannedMagic[] = {` |
|         - | 3002 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|         - | 3003 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|         - | 3004 | `};` |
|         - | 3005 | `/*` |
|         - | 3006 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|         - | 3007 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|         - | 3008 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|         - | 3009 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|         - | 3010 | ` * and before the class is installed.` |
|         - | 3011 | ` */` |
|      4184 | 3012 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|         5 | 3013 | `{` |
|         - | 3014 | `	SyHashEntry *pEntry;` |
|         - | 3015 | `	sxi32 rc;` |
|         - | 3016 | `	sxu32 n;` |
|         - | 3017 | `	/* php: "Enum %s cannot include properties" */` |
|      4189 | 3018 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|      4189 | 3019 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|         3 | 3020 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|         3 | 3021 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|         3 | 3022 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|         1 | 3023 | `				"Enum %z cannot include properties",&pClass->sName);` |
|         3 | 3024 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 3025 | `				return SXERR_ABORT;` |
|         - | 3026 | `			}` |
|         3 | 3027 | `			break;` |
|         - | 3028 | `		}` |
|       ! 0 | 3029 | `	}` |
|         - | 3030 | `	/* php: "Enum %s cannot include magic method %s" */` |
|     58581 | 3031 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|     81588 | 3032 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|     54397 | 3033 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|       ! 0 | 3034 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 3035 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|       ! 0 | 3036 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 3037 | `				return SXERR_ABORT;` |
|         - | 3038 | `			}` |
|       ! 0 | 3039 | `		}` |
|     27201 | 3040 | `	}` |
|         - | 3041 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|         - | 3042 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|         - | 3043 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|         - | 3044 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|         - | 3045 | `	{` |
|         - | 3046 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|         - | 3047 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|         - | 3048 | `		ph7_class_attr *pAttr;` |
|      4189 | 3049 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 3050 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      4189 | 3051 | `		if( pAttr == 0 ){` |
|       ! 0 | 3052 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 3053 | `			return SXERR_ABORT;` |
|         - | 3054 | `		}` |
|      4189 | 3055 | `		pAttr->nType = MEMOBJ_STRING;` |
|      4189 | 3056 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|      4189 | 3057 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|      4189 | 3058 | `		if( pClass->nEnumBacking != 0 ){` |
|      4165 | 3059 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 3060 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      4165 | 3061 | `			if( pAttr == 0 ){` |
|       ! 0 | 3062 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 3063 | `				return SXERR_ABORT;` |
|         - | 3064 | `			}` |
|      4165 | 3065 | `			pAttr->nType = pClass->nEnumBacking;` |
|      4165 | 3066 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|         7 | 3067 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|         4 | 3068 | `			}else{` |
|      4159 | 3069 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|         - | 3070 | `			}` |
|      4165 | 3071 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|      2080 | 3072 | `		}` |
|         - | 3073 | `	}` |
|      4189 | 3074 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|      2097 | 3075 | `}` |
|         - | 3076 | `/*` |
|         - | 3077 | ` * Deferred class declarations (class/anonymous-class extending an` |
|         - | 3078 | ` * autoloaded parent).` |
|         - | 3079 | ` *` |
|         - | 3080 | ` * A class declaration compiles INLINE while its enclosing file compiles, so a` |
|         - | 3081 | ` * parent/interface/trait that an autoloader would provide is unreachable when` |
|         - | 3082 | ` * the autoloader's own spl_autoload_register() statement has not EXECUTED yet` |
|         - | 3083 | `` * (same-file registration, or `new class extends \App\Child {}` anywhere).`` |
|         - | 3084 | ` * php's model has no such problem: a declaration with unresolved dependencies` |
|         - | 3085 | ` * is declared at its EXECUTION point, in statement order, not hoisted.` |
|         - | 3086 | ` *` |
|         - | 3087 | ` * These helpers reproduce that: before compiling a declaration, scan its` |
|         - | 3088 | `` * header (extends/implements) and body (depth-1 trait `use`) for referenced`` |
|         - | 3089 | ` * names and try to resolve each (firing autoload exactly where the normal` |
|         - | 3090 | ` * compile would). If any name is still missing, the WHOLE declaration is` |
|         - | 3091 | `` * captured as re-compilable source — a reconstructed `namespace`/`use`-import/`` |
|         - | 3092 | ` * doc/attribute/modifier prefix plus the declaration's raw text — recorded in` |
|         - | 3093 | ` * a VmDeferredClass, and OP_CLASS_DEFER is emitted at the declaration site.` |
|         - | 3094 | ` * At runtime (VmExecDeferredClass, vm_include.c) the autoloader is live: each` |
|         - | 3095 | `` * recorded name resolves or throws php's catchable `... not found` Error, and`` |
|         - | 3096 | ` * the chunk re-compiles through VmEvalChunk. An anonymous class re-compiles` |
|         - | 3097 | `` * inside `if (false) { new ... }` (installing the class without instantiating`` |
|         - | 3098 | ` * it) under its original synthesized name via pVm->sDeferAnonName; the site's` |
|         - | 3099 | ` * own OP_NEW then instantiates it with the site-compiled arguments.` |
|         - | 3100 | ` *` |
|         - | 3101 | ` * Behavior shifts only for declarations that previously died with the` |
|         - | 3102 | ` * compile-time "Nonexistent base class" fatal: they now follow php — succeed` |
|         - | 3103 | ` * when the autoloader is registered first, or throw php's catchable` |
|         - | 3104 | `` * `Class/Interface/Trait "X" not found` Error at the declaration point.`` |
|         - | 3105 | ` * A deferred declaration's OTHER compile errors (a body syntax error) shift` |
|         - | 3106 | ` * from file-compile time to the declaration's execution — still loud, timing` |
|         - | 3107 | ` * differs from php (recorded).` |
|         - | 3108 | ` */` |
|        84 | 3109 | `static void GenStateDeferEmitUses(SyBlob *pOut,SyHash *pTable,const char *zKind)` |
|         2 | 3110 | `{` |
|         - | 3111 | `	SyHashEntry *pEntry;` |
|        86 | 3112 | `	SyHashResetLoopCursor(pTable);` |
|       128 | 3113 | `	while( (pEntry = SyHashGetNextEntry(pTable)) != 0 ){` |
|       ! 0 | 3114 | `		const char *zFqn = (const char *)pEntry->pUserData;` |
|       ! 0 | 3115 | `		if( zFqn ){` |
|       ! 0 | 3116 | `			SyBlobFormat(pOut,"use %s%s as %.*s;\n",zKind,zFqn,` |
|       ! 0 | 3117 | `				(int)pEntry->nKeyLen,(const char *)pEntry->pKey);` |
|       ! 0 | 3118 | `		}` |
|       ! 0 | 3119 | `	}` |
|        86 | 3120 | `}` |
|         - | 3121 | `/*` |
|         - | 3122 | ` * Parse one class reference at *ppCur (bounded by pEnd) with the SAME` |
|         - | 3123 | ` * namespace/import resolution the real compile uses, and append it to pNames.` |
|         - | 3124 | ` * Advances *ppCur past the reference. Returns SXERR_INVALID on a malformed` |
|         - | 3125 | ` * reference (caller bails out of deferral and lets the normal path report).` |
|         - | 3126 | ` */` |
|    522412 | 3127 | `static sxi32 GenStateDeferRecordRef(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd,` |
|         - | 3128 | `	sxu8 cKind,SySet *pNames)` |
|         5 | 3129 | `{` |
|    522417 | 3130 | `	SyToken *pSavedIn = pGen->pIn;` |
|    522417 | 3131 | `	SyToken *pSavedEnd = pGen->pEnd;` |
|         - | 3132 | `	SyBlob sFqn;` |
|         - | 3133 | `	VmDeferredReq sReq;` |
|         - | 3134 | `	char *zDup;` |
|         - | 3135 | `	sxi32 rc;` |
|    522417 | 3136 | `	SyBlobInit(&sFqn,&pGen->pVm->sAllocator);` |
|    522417 | 3137 | `	pGen->pIn = *ppCur;` |
|    522417 | 3138 | `	pGen->pEnd = pEnd;` |
|    522417 | 3139 | `	rc = GenStateParseClassReference(pGen,&sFqn);` |
|    522417 | 3140 | `	*ppCur = pGen->pIn;` |
|    522417 | 3141 | `	pGen->pIn = pSavedIn;` |
|    522417 | 3142 | `	pGen->pEnd = pSavedEnd;` |
|    522417 | 3143 | `	if( rc != SXRET_OK \|\| SyBlobLength(&sFqn) < 1 ){` |
|         3 | 3144 | `		SyBlobRelease(&sFqn);` |
|         3 | 3145 | `		return SXERR_INVALID;` |
|         - | 3146 | `	}` |
|    783620 | 3147 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    522410 | 3148 | `		(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));` |
|    522415 | 3149 | `	if( zDup == 0 ){` |
|       ! 0 | 3150 | `		SyBlobRelease(&sFqn);` |
|       ! 0 | 3151 | `		return SXERR_INVALID;` |
|         - | 3152 | `	}` |
|    522415 | 3153 | `	SyStringInitFromBuf(&sReq.sName,zDup,SyBlobLength(&sFqn));` |
|    522415 | 3154 | `	sReq.cKind = cKind;` |
|    522415 | 3155 | `	SySetPut(pNames,(const void *)&sReq);` |
|    522415 | 3156 | `	SyBlobRelease(&sFqn);` |
|    522415 | 3157 | `	return SXRET_OK;` |
|    261211 | 3158 | `}` |
|         - | 3159 | `/*` |
|         - | 3160 | ` * Scan the declaration whose keyword pGen->pIn sits on (class/enum/interface/` |
|         - | 3161 | `` * trait, or an anonymous `class(args)`) WITHOUT consuming tokens. Collects`` |
|         - | 3162 | ` * every referenced dependency name, locates the body braces, and filters the` |
|         - | 3163 | ` * collected names down to the UNRESOLVABLE ones (each lookup fires autoload,` |
|         - | 3164 | ` * exactly like the compile it replaces). SXRET_OK with an empty pMissing set` |
|         - | 3165 | ` * means "compile normally"; a non-empty set means "defer". Any structural` |
|         - | 3166 | ` * surprise returns SXERR_INVALID so the normal compile reports it.` |
|         - | 3167 | ` */` |
|    565628 | 3168 | `static sxi32 GenStateScanDeferDeps(ph7_gen_state *pGen,int bAnon,int iSelfKind,` |
|         - | 3169 | `	SySet *pMissing,SyToken **ppBody,SyToken **ppBodyEnd,SyBlob *pSelfFqn)` |
|         5 | 3170 | `{` |
|    565633 | 3171 | `	SyToken *pCur = pGen->pIn; /* on the declaration keyword */` |
|    565633 | 3172 | `	SyToken *pEnd = pGen->pEnd;` |
|         - | 3173 | `	SySet aNames;` |
|    565633 | 3174 | `	sxi32 rc = SXRET_OK;` |
|    565633 | 3175 | `	*ppBody = *ppBodyEnd = 0;` |
|    565633 | 3176 | `	SySetInit(&aNames,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|    565633 | 3177 | `	pCur++; /* Jump the keyword */` |
|    565633 | 3178 | `	if( bAnon ){` |
|        55 | 3179 | `		if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|        10 | 3180 | `			SyToken *pClose = 0;` |
|        10 | 3181 | `			pCur++;` |
|        10 | 3182 | `			PH7_DelimitNestedTokens(pCur,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|        10 | 3183 | `			if( pClose == 0 \|\| pClose >= pEnd ){` |
|       ! 0 | 3184 | `				SySetRelease(&aNames);` |
|       ! 0 | 3185 | `				return SXERR_INVALID;` |
|         - | 3186 | `			}` |
|        10 | 3187 | `			pCur = &pClose[1];` |
|         4 | 3188 | `		}` |
|        30 | 3189 | `	}else{` |
|    565583 | 3190 | `		if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 3191 | `			SySetRelease(&aNames);` |
|       ! 0 | 3192 | `			return SXERR_INVALID;` |
|         - | 3193 | `		}` |
|    565583 | 3194 | `		GenStateBuildFQN(pGen,&pCur->sData,pSelfFqn);` |
|    565583 | 3195 | `		pCur++;` |
|         - | 3196 | `	}` |
|         - | 3197 | ``	/* Header: extends/implements lists up to the '{' (an enum's `: int` backing`` |
|         - | 3198 | `	 * and any stray tokens pass through; malformed headers bail to the normal` |
|         - | 3199 | `	 * path's diagnostics). */` |
|   1013391 | 3200 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_OCB) == 0 ){` |
|    447765 | 3201 | `		int iKind = -1;` |
|    447765 | 3202 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    443601 | 3203 | `			sxi32 nKw = SX_PTR_TO_INT(pCur->pUserData);` |
|    443601 | 3204 | `			if( nKw == PH7_TKWRD_EXTENDS ){` |
|    294319 | 3205 | `				iKind = (iSelfKind == PH7_DEFER_KIND_INTERFACE)` |
|    147157 | 3206 | `					? PH7_DEFER_KIND_INTERFACE : PH7_DEFER_KIND_CLASS;` |
|    296444 | 3207 | `			}else if( nKw == PH7_TKWRD_IMPLEMENTS ){` |
|    145123 | 3208 | `				iKind = PH7_DEFER_KIND_INTERFACE;` |
|     72559 | 3209 | `			}` |
|    221798 | 3210 | `		}` |
|    447765 | 3211 | `		if( iKind < 0 ){` |
|      8333 | 3212 | `			pCur++;` |
|      8333 | 3213 | `			continue;` |
|         - | 3214 | `		}` |
|    439437 | 3215 | `		pCur++; /* Jump extends/implements */` |
|    219716 | 3216 | `		for(;;){` |
|    505693 | 3217 | `			if( GenStateDeferRecordRef(pGen,&pCur,pEnd,(sxu8)iKind,&aNames) != SXRET_OK ){` |
|         3 | 3218 | `				SySetRelease(&aNames);` |
|         3 | 3219 | `				return SXERR_INVALID;` |
|         - | 3220 | `			}` |
|    505691 | 3221 | `			if( pCur < pEnd && (pCur->nType & PH7_TK_COMMA) ){` |
|     66261 | 3222 | `				pCur++;` |
|     66261 | 3223 | `				continue;` |
|         - | 3224 | `			}` |
|    439435 | 3225 | `			break;` |
|       ! 0 | 3226 | `		}` |
|         5 | 3227 | `	}` |
|    565631 | 3228 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 | 3229 | `		SySetRelease(&aNames);` |
|       ! 0 | 3230 | `		return SXERR_INVALID;` |
|         - | 3231 | `	}` |
|    565631 | 3232 | `	*ppBody = pCur;` |
|         - | 3233 | `	{` |
|    565631 | 3234 | `		SyToken *pClose = 0;` |
|    565631 | 3235 | `		PH7_DelimitNestedTokens(&pCur[1],pEnd,PH7_TK_OCB,PH7_TK_CCB,&pClose);` |
|    565631 | 3236 | `		if( pClose == 0 \|\| pClose >= pEnd ){` |
|       ! 0 | 3237 | `			SySetRelease(&aNames);` |
|       ! 0 | 3238 | `			return SXERR_INVALID;` |
|         - | 3239 | `		}` |
|    565631 | 3240 | `		*ppBodyEnd = pClose;` |
|         - | 3241 | `	}` |
|         - | 3242 | ``	/* Body: depth-1 trait `use Name[, Name]` statements. Statement position only`` |
|         - | 3243 | ``	 * (previous token one of '{' '}' ';'), so a closure's `use ($x)` — which`` |
|         - | 3244 | `	 * follows a ')' — never matches. */` |
|         - | 3245 | `	{` |
|    565631 | 3246 | `		SyToken *p = &(*ppBody)[1];` |
|    565631 | 3247 | `		int bStmtPos = 1;` |
|    565631 | 3248 | `		sxi32 iDepth = 1;` |
| 132547155 | 3249 | `		while( p < *ppBodyEnd ){` |
| 131981529 | 3250 | `			if( p->nType & PH7_TK_OCB ){` |
|   5011675 | 3251 | `				iDepth++;` |
|   5011675 | 3252 | `				bStmtPos = 1;` |
|   5011675 | 3253 | `				p++;` |
|   5011675 | 3254 | `				continue;` |
|         - | 3255 | `			}` |
| 126969859 | 3256 | `			if( p->nType & PH7_TK_CCB ){` |
|   5011675 | 3257 | `				iDepth--;` |
|   5011675 | 3258 | `				bStmtPos = 1;` |
|   5011675 | 3259 | `				p++;` |
|   5011675 | 3260 | `				continue;` |
|         - | 3261 | `			}` |
| 121958189 | 3262 | `			if( p->nType & PH7_TK_SEMI ){` |
|   8532383 | 3263 | `				bStmtPos = 1;` |
|   8532383 | 3264 | `				p++;` |
|   8532383 | 3265 | `				continue;` |
|         - | 3266 | `			}` |
| 113425806 | 3267 | `			if( iDepth == 1 && bStmtPos && (p->nType & PH7_TK_KEYWORD)` |
|   4098282 | 3268 | `			 && SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_USE ){` |
|     16715 | 3269 | `				p++;` |
|      8355 | 3270 | `				for(;;){` |
|     16729 | 3271 | `					if( GenStateDeferRecordRef(pGen,&p,*ppBodyEnd,PH7_DEFER_KIND_TRAIT,&aNames) != SXRET_OK ){` |
|       ! 0 | 3272 | `						SySetRelease(&aNames);` |
|       ! 0 | 3273 | `						return SXERR_INVALID;` |
|         - | 3274 | `					}` |
|     16729 | 3275 | `					if( p < *ppBodyEnd && (p->nType & PH7_TK_COMMA) ){` |
|        17 | 3276 | `						p++;` |
|        17 | 3277 | `						continue;` |
|         - | 3278 | `					}` |
|     16715 | 3279 | `					break;` |
|       ! 0 | 3280 | `				}` |
|     16715 | 3281 | `				continue;` |
|         - | 3282 | `			}` |
| 113409101 | 3283 | `			bStmtPos = 0;` |
| 113409101 | 3284 | `			p++;` |
|         5 | 3285 | `		}` |
|         - | 3286 | `	}` |
|         - | 3287 | `	/* Filter: keep only the names that do NOT resolve. The lookup fires the` |
|         - | 3288 | `	 * autoloader exactly where the replaced compile would. */` |
|         - | 3289 | `	{` |
|    565631 | 3290 | `		VmDeferredReq *aReq = (VmDeferredReq *)SySetBasePtr(&aNames);` |
|         - | 3291 | `		sxu32 n;` |
|   1088041 | 3292 | `		for( n = 0 ; n < SySetUsed(&aNames) ; ++n ){` |
|    522415 | 3293 | `			if( PH7_VmExtractClass(pGen->pVm,aReq[n].sName.zString,aReq[n].sName.nByte,FALSE,0) == 0 ){` |
|        34 | 3294 | `				SySetPut(pMissing,(const void *)&aReq[n]);` |
|        16 | 3295 | `			}` |
|    261210 | 3296 | `		}` |
|         - | 3297 | `	}` |
|    565631 | 3298 | `	SySetRelease(&aNames);` |
|    565631 | 3299 | `	return rc;` |
|    282819 | 3300 | `}` |
|         - | 3301 | `/*` |
|         - | 3302 | ` * Capture the declaration as a re-compilable chunk, record it, and emit` |
|         - | 3303 | ` * OP_CLASS_DEFER at the current emission point. On return the statement` |
|         - | 3304 | ` * cursor sits past the declaration's closing '}'. pMissing's entries are` |
|         - | 3305 | ` * COPIED into the record (their name bytes are already allocator-owned).` |
|         - | 3306 | ` */` |
|        28 | 3307 | `static sxi32 GenStateEmitDeferredClass(ph7_gen_state *pGen,sxi32 iFlags,int bAnon,` |
|         - | 3308 | `	SySet *pMissing,SyToken *pBodyEnd,SyBlob *pSelfFqn,const SyString *pAnonName)` |
|         2 | 3309 | `{` |
|        30 | 3310 | `	SyToken *pKw = pGen->pIn; /* the declaration keyword */` |
|         - | 3311 | `	VmDeferredClass *pDefer;` |
|         - | 3312 | `	SyBlob sChunk;` |
|         - | 3313 | `	const char *zFrom;` |
|         - | 3314 | `	const char *zTo;` |
|         - | 3315 | `	char *zDup;` |
|        30 | 3316 | `	SyBlobInit(&sChunk,&pGen->pVm->sAllocator);` |
|         - | 3317 | `	/* The declaration site's compile context is replayed as literal statements:` |
|         - | 3318 | `	 * strict_types first (it must open the chunk), then namespace and the` |
|         - | 3319 | `	 * use-import tables — the runtime re-compile starts in a fresh scope. */` |
|        30 | 3320 | `	if( pGen->bStrictTypes ){` |
|       ! 0 | 3321 | `		SyBlobAppend(&sChunk,"declare(strict_types=1);\n",sizeof("declare(strict_types=1);\n")-1);` |
|       ! 0 | 3322 | `	}` |
|        30 | 3323 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       ! 0 | 3324 | `		SyBlobFormat(&sChunk,"namespace %.*s;\n",` |
|       ! 0 | 3325 | `			(int)SyBlobLength(&pGen->sNamespace),(const char *)SyBlobData(&pGen->sNamespace));` |
|       ! 0 | 3326 | `	}` |
|        30 | 3327 | `	GenStateDeferEmitUses(&sChunk,&pGen->hUseImports,"");` |
|        30 | 3328 | `	GenStateDeferEmitUses(&sChunk,&pGen->hUseFuncImports,"function ");` |
|        30 | 3329 | `	GenStateDeferEmitUses(&sChunk,&pGen->hUseConstImports,"const ");` |
|         - | 3330 | `	/* Doc-comment and attribute groups precede the keyword in the raw source,` |
|         - | 3331 | `	 * outside the captured span — re-emit them from the trivia sidecar. */` |
|        30 | 3332 | `	if( !bAnon && pGen->sPendingDoc.nByte > 0 ){` |
|       ! 0 | 3333 | `		SyBlobAppend(&sChunk,pGen->sPendingDoc.zString,pGen->sPendingDoc.nByte);` |
|       ! 0 | 3334 | `		SyBlobAppend(&sChunk,"\n",1);` |
|       ! 0 | 3335 | `	}` |
|         - | 3336 | `	{` |
|         - | 3337 | `		ph7_trivia *aT;` |
|         - | 3338 | `		sxu32 nT,n;` |
|        30 | 3339 | `		if( bAnon ){` |
|         - | 3340 | ``			/* `new #[A] class` trivia is keyed to the 'class' token */`` |
|         7 | 3341 | `			SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|         7 | 3342 | `			aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|         7 | 3343 | `			nT = SySetUsed(&pGen->aTrivia);` |
|         7 | 3344 | `			if( pGen->pTokenSet && pKw >= pBase && pKw < &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         7 | 3345 | `				sxu32 nIdx = (sxu32)(pKw - pBase);` |
|         7 | 3346 | `				for( n = 0 ; n < nT ; ++n ){` |
|       ! 0 | 3347 | `					if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|       ! 0 | 3348 | `						SyBlobFormat(&sChunk,"#[%.*s]\n",(int)aT[n].sText.nByte,aT[n].sText.zString);` |
|       ! 0 | 3349 | `					}` |
|       ! 0 | 3350 | `				}` |
|         3 | 3351 | `			}` |
|         4 | 3352 | `		}else{` |
|        24 | 3353 | `			aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|        24 | 3354 | `			nT = SySetUsed(&pGen->aPendingAttrs);` |
|        24 | 3355 | `			for( n = 0 ; n < nT ; ++n ){` |
|       ! 0 | 3356 | `				if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|       ! 0 | 3357 | `					SyBlobFormat(&sChunk,"#[%.*s]\n",(int)aT[n].sText.nByte,aT[n].sText.zString);` |
|       ! 0 | 3358 | `				}` |
|       ! 0 | 3359 | `			}` |
|         - | 3360 | `		}` |
|         - | 3361 | `	}` |
|         - | 3362 | `	/* Pad the prefix with newlines so the declaration keyword sits on its` |
|         - | 3363 | `	 * ORIGINAL line inside the chunk — runtime diagnostics from the deferred` |
|         - | 3364 | `	 * compile then report the source's real line. Best-effort: a prefix` |
|         - | 3365 | `	 * already longer than the declaration line skips the padding. */` |
|         - | 3366 | `	{` |
|        30 | 3367 | `		const char *zScan = (const char *)SyBlobData(&sChunk);` |
|        30 | 3368 | `		sxu32 nHave = 0;` |
|         - | 3369 | `		sxu32 nScan;` |
|        30 | 3370 | `		for( nScan = 0 ; nScan < SyBlobLength(&sChunk) ; ++nScan ){` |
|       ! 0 | 3371 | `			if( zScan[nScan] == '\n' ){` |
|       ! 0 | 3372 | `				nHave++;` |
|       ! 0 | 3373 | `			}` |
|       ! 0 | 3374 | `		}` |
|       304 | 3375 | `		while( nHave + 1 < pKw->nLine ){` |
|       276 | 3376 | `			SyBlobAppend(&sChunk,"\n",1);` |
|       276 | 3377 | `			nHave++;` |
|         2 | 3378 | `		}` |
|         - | 3379 | `	}` |
|        30 | 3380 | `	if( bAnon ){` |
|         - | 3381 | ``		/* `if (false) { new class <header-minus-args> { body } ; }` — installs`` |
|         - | 3382 | `		 * the class at the chunk's compile, never instantiates it. */` |
|         7 | 3383 | `		SyToken *pAfterArgs = &pKw[1];` |
|         7 | 3384 | `		SyBlobAppend(&sChunk,"if (false) { new ",sizeof("if (false) { new ")-1);` |
|         7 | 3385 | `		SyBlobAppend(&sChunk,pKw->sData.zString,pKw->sData.nByte);` |
|         7 | 3386 | `		if( pAfterArgs < pGen->pEnd && (pAfterArgs->nType & PH7_TK_LPAREN) ){` |
|         3 | 3387 | `			SyToken *pClose = 0;` |
|         3 | 3388 | `			PH7_DelimitNestedTokens(&pAfterArgs[1],pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|         3 | 3389 | `			if( pClose == 0 \|\| pClose >= pGen->pEnd ){` |
|       ! 0 | 3390 | `				SyBlobRelease(&sChunk);` |
|       ! 0 | 3391 | `				return SXERR_INVALID;` |
|         - | 3392 | `			}` |
|         3 | 3393 | `			pAfterArgs = &pClose[1];` |
|         1 | 3394 | `		}` |
|         7 | 3395 | `		if( pAfterArgs < pBodyEnd ){` |
|         7 | 3396 | `			SyBlobAppend(&sChunk," ",1);` |
|         7 | 3397 | `			zFrom = pAfterArgs->sData.zString;` |
|         7 | 3398 | `			zTo = pBodyEnd->sData.zString + pBodyEnd->sData.nByte;` |
|         7 | 3399 | `			SyBlobAppend(&sChunk,zFrom,(sxu32)(zTo - zFrom));` |
|         3 | 3400 | `		}` |
|         7 | 3401 | `		SyBlobAppend(&sChunk,"; }",sizeof("; }")-1);` |
|         4 | 3402 | `	}else{` |
|         - | 3403 | `		/* Modifiers were consumed before this compiler ran; reconstruct them` |
|         - | 3404 | ``		 * (an enum's implicit `final` must NOT be spelled out). */`` |
|        22 | 3405 | `		if( (iFlags & PH7_CLASS_ENUM) == 0` |
|        21 | 3406 | `		 && (pKw->nType & PH7_TK_KEYWORD)` |
|        22 | 3407 | `		 && SX_PTR_TO_INT(pKw->pUserData) == PH7_TKWRD_CLASS ){` |
|        16 | 3408 | `			if( iFlags & PH7_CLASS_FINAL ){` |
|         3 | 3409 | `				SyBlobAppend(&sChunk,"final ",sizeof("final ")-1);` |
|         1 | 3410 | `			}` |
|        16 | 3411 | `			if( iFlags & PH7_CLASS_ABSTRACT ){` |
|       ! 0 | 3412 | `				SyBlobAppend(&sChunk,"abstract ",sizeof("abstract ")-1);` |
|       ! 0 | 3413 | `			}` |
|        16 | 3414 | `			if( iFlags & PH7_CLASS_READONLY ){` |
|       ! 0 | 3415 | `				SyBlobAppend(&sChunk,"readonly ",sizeof("readonly ")-1);` |
|       ! 0 | 3416 | `			}` |
|         7 | 3417 | `		}` |
|        24 | 3418 | `		zFrom = pKw->sData.zString;` |
|        24 | 3419 | `		zTo = pBodyEnd->sData.zString + pBodyEnd->sData.nByte;` |
|        24 | 3420 | `		SyBlobAppend(&sChunk,zFrom,(sxu32)(zTo - zFrom));` |
|         - | 3421 | `	}` |
|        30 | 3422 | `	pDefer = (VmDeferredClass *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmDeferredClass));` |
|        30 | 3423 | `	if( pDefer == 0 ){` |
|       ! 0 | 3424 | `		SyBlobRelease(&sChunk);` |
|       ! 0 | 3425 | `		PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 3426 | `		return SXERR_ABORT;` |
|         - | 3427 | `	}` |
|        30 | 3428 | `	SyZero(pDefer,sizeof(VmDeferredClass));` |
|        44 | 3429 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        28 | 3430 | `		(const char *)SyBlobData(&sChunk),SyBlobLength(&sChunk));` |
|        30 | 3431 | `	SyBlobRelease(&sChunk);` |
|        30 | 3432 | `	if( zDup == 0 ){` |
|       ! 0 | 3433 | `		PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 3434 | `		return SXERR_ABORT;` |
|         - | 3435 | `	}` |
|        30 | 3436 | `	SyStringInitFromBuf(&pDefer->sText,zDup,SyStrlen(zDup));` |
|        30 | 3437 | `	if( bAnon ){` |
|         7 | 3438 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pAnonName->zString,pAnonName->nByte);` |
|         7 | 3439 | `		if( zDup == 0 ){` |
|       ! 0 | 3440 | `			PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 3441 | `			return SXERR_ABORT;` |
|         - | 3442 | `		}` |
|         7 | 3443 | `		SyStringInitFromBuf(&pDefer->sAnonName,zDup,pAnonName->nByte);` |
|         7 | 3444 | `		pDefer->sSelfName = pDefer->sAnonName;` |
|         4 | 3445 | `	}else{` |
|        35 | 3446 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        22 | 3447 | `			(const char *)SyBlobData(pSelfFqn),SyBlobLength(pSelfFqn));` |
|        24 | 3448 | `		if( zDup == 0 ){` |
|       ! 0 | 3449 | `			PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 3450 | `			return SXERR_ABORT;` |
|         - | 3451 | `		}` |
|        24 | 3452 | `		SyStringInitFromBuf(&pDefer->sSelfName,zDup,SyBlobLength(pSelfFqn));` |
|         - | 3453 | `	}` |
|        30 | 3454 | `	SySetInit(&pDefer->aRequired,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|         - | 3455 | `	{` |
|        30 | 3456 | `		VmDeferredReq *aReq = (VmDeferredReq *)SySetBasePtr(pMissing);` |
|         - | 3457 | `		sxu32 n;` |
|        62 | 3458 | `		for( n = 0 ; n < SySetUsed(pMissing) ; ++n ){` |
|        34 | 3459 | `			SySetPut(&pDefer->aRequired,(const void *)&aReq[n]);` |
|        18 | 3460 | `		}` |
|         - | 3461 | `	}` |
|        30 | 3462 | `	pDefer->nLine = pKw->nLine;` |
|        30 | 3463 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLASS_DEFER,0,0,(void *)pDefer,0);` |
|         - | 3464 | `	/* Skip the declaration: the statement cursor lands past its '}' */` |
|        30 | 3465 | `	pGen->pIn = &pBodyEnd[1];` |
|        30 | 3466 | `	return SXRET_OK;` |
|        16 | 3467 | `}` |
|         - | 3468 | `/*` |
|         - | 3469 | ` * Deferral gate shared by the named-declaration compilers: scan the` |
|         - | 3470 | ` * declaration at pGen->pIn; when a dependency is missing, capture + emit the` |
|         - | 3471 | ` * deferred record and return TRUE (the caller returns immediately — the` |
|         - | 3472 | ` * declaration compiles at execution time). FALSE means compile normally.` |
|         - | 3473 | ` * *pRc carries SXERR_ABORT out of the capture path.` |
|         - | 3474 | ` */` |
|    565578 | 3475 | `static int GenStateMaybeDeferClass(ph7_gen_state *pGen,sxi32 iFlags,int iSelfKind,sxi32 *pRc)` |
|         5 | 3476 | `{` |
|         - | 3477 | `	SySet aMissing;` |
|    565583 | 3478 | `	SyToken *pBody = 0;` |
|    565583 | 3479 | `	SyToken *pBodyEnd = 0;` |
|         - | 3480 | `	SyBlob sSelfFqn;` |
|    565583 | 3481 | `	int bDefer = 0;` |
|    565583 | 3482 | `	*pRc = SXRET_OK;` |
|    565583 | 3483 | `	SySetInit(&aMissing,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|    565583 | 3484 | `	SyBlobInit(&sSelfFqn,&pGen->pVm->sAllocator);` |
|    565578 | 3485 | `	if( GenStateScanDeferDeps(pGen,0,iSelfKind,&aMissing,&pBody,&pBodyEnd,&sSelfFqn) == SXRET_OK` |
|    565582 | 3486 | `	 && SySetUsed(&aMissing) > 0 ){` |
|        24 | 3487 | `		*pRc = GenStateEmitDeferredClass(pGen,iFlags,0,&aMissing,pBodyEnd,&sSelfFqn,0);` |
|        24 | 3488 | `		bDefer = 1;` |
|        11 | 3489 | `	}` |
|    565583 | 3490 | `	SySetRelease(&aMissing);` |
|    565583 | 3491 | `	SyBlobRelease(&sSelfFqn);` |
|    565583 | 3492 | `	return bDefer;` |
|         5 | 3493 | `}` |
|         - | 3494 | `/*` |
|         - | 3495 | ``  * Apply a declaration body's collected `use Trait[, Trait] [{ resolution }]` `` |
|         - | 3496 | ` * entries to pClass — plain application when no resolution block is present,` |
|         - | 3497 | ` * otherwise the two-pass insteadof/as machinery. Shared by the CLASS body and` |
|         - | 3498 | ` * (since the adaptation-block port) the TRAIT body compiler. Returns the last` |
|         - | 3499 | ` * application status (non-OK = out of memory at a copy site).` |
|         - | 3500 | ` */` |
|    490886 | 3501 | `static sxi32 GenStateApplyTraitUses(ph7_gen_state *pGen,ph7_class *pClass,SySet *pUseEntries)` |
|         5 | 3502 | `{` |
|    490891 | 3503 | `	sxi32 rc = SXRET_OK;` |
|         - | 3504 | `	{` |
|         - | 3505 | `		TraitUseEntry *apUse;` |
|         - | 3506 | `		sxu32 nU;` |
|    490891 | 3507 | `		apUse = (TraitUseEntry *)SySetBasePtr(pUseEntries);` |
|    507589 | 3508 | `		for( nU = 0 ; nU < SySetUsed(pUseEntries) ; nU++ ){` |
|     16703 | 3509 | `			TraitUseEntry *pUse = &apUse[nU];` |
|     16703 | 3510 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|     16703 | 3511 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|     16703 | 3512 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|         - | 3513 | `			sxu32 nT;` |
|     16703 | 3514 | `			if( !hasResolution ){` |
|         - | 3515 | `				/* No conflict resolution block: use standard trait application */` |
|     33357 | 3516 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|     16685 | 3517 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|     16685 | 3518 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 3519 | `						break;` |
|         - | 3520 | `					}` |
|      8345 | 3521 | `				}` |
|      8341 | 3522 | `			}else{` |
|         - | 3523 | `				/* With resolution block: copy attributes, record traits,` |
|         - | 3524 | `				 * then use the block to resolve method conflicts.` |
|         - | 3525 | `				 */` |
|         - | 3526 | `				SyToken *pR;` |
|        61 | 3527 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        35 | 3528 | `					ph7_class *pTR = apTrait[nT];` |
|         - | 3529 | `					ph7_class_attr *pAR;` |
|         - | 3530 | `					SyHashEntry *pER;` |
|         - | 3531 | `					SyString *pNR;` |
|        35 | 3532 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|        51 | 3533 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|       ! 0 | 3534 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|       ! 0 | 3535 | `						pNR = &pAR->sName;` |
|       ! 0 | 3536 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       ! 0 | 3537 | `							SyHashInsertTail(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|       ! 0 | 3538 | `						}` |
|       ! 0 | 3539 | `					}` |
|         - | 3540 | `					/* Trait constants (PHP 8.2) live in the separate hConst namespace */` |
|        35 | 3541 | `					SyHashResetLoopCursor(&pTR->hConst);` |
|        51 | 3542 | `					while((pER = SyHashGetNextEntry(&pTR->hConst)) != 0 ){` |
|       ! 0 | 3543 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|       ! 0 | 3544 | `						pNR = &pAR->sName;` |
|       ! 0 | 3545 | `						if( SyHashGet(&pClass->hConst,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       ! 0 | 3546 | `							SyHashInsertTail(&pClass->hConst,(const void *)pNR->zString,pNR->nByte,pAR);` |
|       ! 0 | 3547 | `						}` |
|       ! 0 | 3548 | `					}` |
|        35 | 3549 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|        19 | 3550 | `				}` |
|         - | 3551 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|        29 | 3552 | `				pR = pUse->pResolvStart;` |
|        65 | 3553 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 3554 | `					SyString sTrait,sMethod;` |
|         - | 3555 | `					ph7_class *pSrcTrait;` |
|         - | 3556 | `					ph7_class_method *pMeth;` |
|         - | 3557 | `					sxi32 nRKwrd;` |
|       101 | 3558 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        65 | 3559 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        39 | 3560 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        39 | 3561 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        39 | 3562 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        39 | 3563 | `					sMethod = pR->sData;` |
|        39 | 3564 | `					pR++;` |
|        39 | 3565 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        16 | 3566 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        16 | 3567 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        16 | 3568 | `							sTrait = sMethod;` |
|        16 | 3569 | `							pR++;` |
|        16 | 3570 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        16 | 3571 | `							sMethod = pR->sData;` |
|        16 | 3572 | `							pR++;` |
|         7 | 3573 | `						}` |
|         7 | 3574 | `					}` |
|        39 | 3575 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 3576 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 3577 | `						continue;` |
|         - | 3578 | `					}` |
|        39 | 3579 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        39 | 3580 | `					pR++;` |
|        39 | 3581 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|        10 | 3582 | `						pSrcTrait = 0;` |
|        12 | 3583 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        12 | 3584 | `							SyString *pTN = &apTrait[nT]->sName;` |
|        17 | 3585 | `							if( pTN->nByte >= sTrait.nByte &&` |
|        10 | 3586 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        10 | 3587 | `								pSrcTrait = apTrait[nT];` |
|        10 | 3588 | `								break;` |
|         - | 3589 | `							}` |
|         2 | 3590 | `						}` |
|        10 | 3591 | `						if( pSrcTrait ){` |
|        10 | 3592 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        10 | 3593 | `							if( pMeth ){` |
|        10 | 3594 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|        10 | 3595 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|        10 | 3596 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|         4 | 3597 | `								}` |
|         4 | 3598 | `							}` |
|         4 | 3599 | `						}` |
|         4 | 3600 | `					}` |
|        81 | 3601 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 3602 | `				}` |
|         - | 3603 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|        61 | 3604 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         - | 3605 | `					ph7_class_method *pMR;` |
|         - | 3606 | `					SyHashEntry *pER;` |
|         - | 3607 | `					SyString *pNR;` |
|        35 | 3608 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|        97 | 3609 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|        49 | 3610 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|        49 | 3611 | `						pNR = &pMR->sFunc.sName;` |
|        49 | 3612 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|        33 | 3613 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|        15 | 3614 | `						}` |
|         3 | 3615 | `					}` |
|        19 | 3616 | `				}` |
|         - | 3617 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|        29 | 3618 | `				pR = pUse->pResolvStart;` |
|        65 | 3619 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 3620 | `					SyString sTrait,sMethod,sAlias;` |
|         - | 3621 | `					ph7_class *pSrcTrait;` |
|         - | 3622 | `					ph7_class_method *pMeth;` |
|        65 | 3623 | `					int hasQual = 0;` |
|         - | 3624 | `					sxi32 nRKwrd;` |
|       101 | 3625 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        65 | 3626 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        39 | 3627 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        39 | 3628 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        39 | 3629 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|        39 | 3630 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        39 | 3631 | `					sMethod = pR->sData;` |
|        39 | 3632 | `					pR++;` |
|        39 | 3633 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        16 | 3634 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        16 | 3635 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        16 | 3636 | `							sTrait = sMethod;` |
|        16 | 3637 | `							hasQual = 1;` |
|        16 | 3638 | `							pR++;` |
|        16 | 3639 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        16 | 3640 | `							sMethod = pR->sData;` |
|        16 | 3641 | `							pR++;` |
|         7 | 3642 | `						}` |
|         7 | 3643 | `					}` |
|        39 | 3644 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 3645 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 3646 | `						continue;` |
|         - | 3647 | `					}` |
|        39 | 3648 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        39 | 3649 | `					pR++;` |
|        39 | 3650 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|        31 | 3651 | `						sxi32 iNewVis = -1;` |
|        31 | 3652 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|        10 | 3653 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|        10 | 3654 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|        10 | 3655 | `								iNewVis = nAK;` |
|        10 | 3656 | `								pR++;` |
|         4 | 3657 | `							}` |
|         4 | 3658 | `						}` |
|        31 | 3659 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|        29 | 3660 | `							sAlias = pR->sData;` |
|        29 | 3661 | `							pR++;` |
|        13 | 3662 | `						}` |
|        31 | 3663 | `						pMeth = 0;` |
|        31 | 3664 | `						if( hasQual ){` |
|         8 | 3665 | `							pSrcTrait = 0;` |
|        14 | 3666 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        14 | 3667 | `								SyString *pTN = &apTrait[nT]->sName;` |
|        20 | 3668 | `								if( pTN->nByte >= sTrait.nByte &&` |
|        12 | 3669 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         8 | 3670 | `									pSrcTrait = apTrait[nT];` |
|         8 | 3671 | `									break;` |
|         - | 3672 | `								}` |
|         5 | 3673 | `							}` |
|         8 | 3674 | `							if( pSrcTrait ){` |
|         8 | 3675 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         3 | 3676 | `							}` |
|         5 | 3677 | `						}else{` |
|        25 | 3678 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|         - | 3679 | `						}` |
|        31 | 3680 | `						if( pMeth ){` |
|         - | 3681 | `							/* php: a method declared in the class BODY wins over a trait alias` |
|         - | 3682 | ``							 * of the same name (e.g. an explicit __construct over `init as`` |
|         - | 3683 | ``							 * __construct`). If pClass already declares sAlias ITSELF — an own`` |
|         - | 3684 | `							 * method, sFunc.pUserData == pClass — keep it: SyHashInsert is LIFO,` |
|         - | 3685 | `							 * so an unconditional insert would shadow the class method at lookup` |
|         - | 3686 | ``							 * and `new` would run the alias. A name held only by another trait is`` |
|         - | 3687 | `							 * a genuine conflict resolved by the insteadof pass above. */` |
|        31 | 3688 | `							int bClassWins = 0;` |
|        31 | 3689 | `							if( sAlias.nByte > 0 ){` |
|        29 | 3690 | `								ph7_class_method *pOwn = PH7_ClassExtractMethod(pClass,sAlias.zString,sAlias.nByte);` |
|        29 | 3691 | `								bClassWins = (pOwn && pOwn->sFunc.pUserData == pClass);` |
|        13 | 3692 | `							}` |
|        41 | 3693 | `							if( sAlias.nByte > 0 && !bClassWins ){` |
|         - | 3694 | `								/* Create a shallow copy of the method struct for the alias` |
|         - | 3695 | `								 * so it can carry its own visibility without affecting the original.` |
|         - | 3696 | `								 */` |
|         - | 3697 | `								ph7_class_method *pAlias;` |
|         - | 3698 | `								char *zAliasDup;` |
|        23 | 3699 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        23 | 3700 | `								if( pAlias ){` |
|        23 | 3701 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|        23 | 3702 | `									if( iNewVis >= 0 ){` |
|         8 | 3703 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         6 | 3704 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 3705 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         3 | 3706 | `									}` |
|        23 | 3707 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        23 | 3708 | `									if( zAliasDup ){` |
|        23 | 3709 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|        10 | 3710 | `									}` |
|        13 | 3711 | `								}` |
|        20 | 3712 | `							}else if( sAlias.nByte == 0 && iNewVis >= 0 ){` |
|         - | 3713 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|         - | 3714 | `								ph7_class_method *pCopy;` |
|         3 | 3715 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|         3 | 3716 | `								if( pCopy ){` |
|         3 | 3717 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|         3 | 3718 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|         3 | 3719 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 3720 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 3721 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         - | 3722 | `									/* Replace the method in the class hash */` |
|         3 | 3723 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|         3 | 3724 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|         1 | 3725 | `								}` |
|         1 | 3726 | `							}` |
|        14 | 3727 | `						}` |
|        14 | 3728 | `						SXUNUSED(hasQual);` |
|        14 | 3729 | `					}` |
|        47 | 3730 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 3731 | `				}` |
|         - | 3732 | `			}` |
|     16703 | 3733 | `			SySetRelease(&pUse->aTraits);` |
|      8354 | 3734 | `		}` |
|         - | 3735 | `	}` |
|    490891 | 3736 | `	return rc;` |
|         5 | 3737 | `}` |
|         - | 3738 | `/*` |
|         - | 3739 | ` * Compile a class declaration, named or anonymous.` |
|         - | 3740 | ` *` |
|         - | 3741 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|         - | 3742 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|         - | 3743 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|         - | 3744 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|         - | 3745 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|         - | 3746 | ` * implements, body, install) is shared by both paths.` |
|         - | 3747 | ` */` |
|    482530 | 3748 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|         - | 3749 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|         5 | 3750 | `{` |
|    482535 | 3751 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 3752 | `	ph7_class *pClass,*pBase;` |
|    482535 | 3753 | `	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' (enclosing class for a nested anon) */` |
|         - | 3754 | `	SyToken *pEnd,*pTmp;` |
|         - | 3755 | `	sxi32 iProtection;` |
|         - | 3756 | `	SySet aInterfaces;` |
|         - | 3757 | `	SySet aUseEntries;` |
|         - | 3758 | `	sxi32 iAttrflags;` |
|         - | 3759 | `	SyString *pName;` |
|         - | 3760 | `	sxi32 nKwrd;` |
|         - | 3761 | `	sxi32 rc;` |
|    482535 | 3762 | `	if( pAnonName == 0 ){` |
|         - | 3763 | `		/* Deferral gate: an unresolvable parent/interface/trait —` |
|         - | 3764 | `		 * its autoloader has not RUN yet — re-compiles this declaration at its` |
|         - | 3765 | `		 * execution point instead of dying on "Nonexistent base class". */` |
|         - | 3766 | `		sxi32 rcDefer;` |
|    482491 | 3767 | `		if( GenStateMaybeDeferClass(pGen,iFlags,PH7_DEFER_KIND_CLASS,&rcDefer) ){` |
|        18 | 3768 | `			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - | 3769 | `		}` |
|    241235 | 3770 | `	}` |
|         - | 3771 | `	/* Jump the 'class' keyword */` |
|    482519 | 3772 | `	pGen->pIn++;` |
|    482519 | 3773 | `	if( pAnonName ){` |
|         - | 3774 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|         - | 3775 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|         - | 3776 | `		 * then use the synthesized name. */` |
|        49 | 3777 | `		*ppArgStart = *ppArgEnd = 0;` |
|        49 | 3778 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         7 | 3779 | `			pGen->pIn++; /* Jump '(' */` |
|         7 | 3780 | `			*ppArgStart = pGen->pIn;` |
|        10 | 3781 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|         3 | 3782 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|         7 | 3783 | `			pGen->pIn = *ppArgEnd;` |
|         7 | 3784 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|         3 | 3785 | `		}` |
|        49 | 3786 | `		pName = pAnonName;` |
|        49 | 3787 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|        27 | 3788 | `	}else{` |
|    482475 | 3789 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - | 3790 | `			/* Syntax error */` |
|       ! 0 | 3791 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|       ! 0 | 3792 | `			if( rc == SXERR_ABORT ){` |
|         - | 3793 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 3794 | `				return SXERR_ABORT;` |
|         - | 3795 | `			}` |
|         - | 3796 | `			/* Synchronize with the first semi-colon or curly braces */` |
|       ! 0 | 3797 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|       ! 0 | 3798 | `				pGen->pIn++;` |
|       ! 0 | 3799 | `			}` |
|       ! 0 | 3800 | `			return SXRET_OK;` |
|         - | 3801 | `		}` |
|         - | 3802 | `		/* Extract class name */` |
|    482475 | 3803 | `		pName = &pGen->pIn->sData;` |
|         - | 3804 | `		/* Advance the stream cursor */` |
|    482475 | 3805 | `		pGen->pIn++;` |
|         - | 3806 | `		/* Build FQN and obtain a raw class */ {` |
|         - | 3807 | `			SyBlob sFQN;` |
|         - | 3808 | `			SyString sFQNStr;` |
|    482475 | 3809 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    482475 | 3810 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|    482475 | 3811 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    482475 | 3812 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    482475 | 3813 | `			SyBlobRelease(&sFQN);` |
|         - | 3814 | `		}` |
|         - | 3815 | `	}` |
|    482519 | 3816 | `	if( pClass == 0 ){` |
|       ! 0 | 3817 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 3818 | `		return SXERR_ABORT;` |
|         - | 3819 | `	}` |
|    482514 | 3820 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|      4193 | 3821 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|         - | 3822 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|      4167 | 3823 | `		pGen->pIn++; /* Jump ':' */` |
|      4162 | 3824 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      4167 | 3825 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|         7 | 3826 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|         7 | 3827 | `			pGen->pIn++;` |
|      4160 | 3828 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      4161 | 3829 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|      4159 | 3830 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|      4159 | 3831 | `			pGen->pIn++;` |
|      2082 | 3832 | `		}else{` |
|         3 | 3833 | `			SyToken *pTok = pGen->pIn;` |
|         3 | 3834 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|         4 | 3835 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|         1 | 3836 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|         3 | 3837 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 3838 | `				return SXERR_ABORT;` |
|         - | 3839 | `			}` |
|         3 | 3840 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|         3 | 3841 | `				pGen->pIn++; /* Skip the bogus type token */` |
|         1 | 3842 | `			}` |
|         - | 3843 | `		}` |
|      2081 | 3844 | `	}` |
|    482519 | 3845 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    482519 | 3846 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 3847 | `		return SXERR_ABORT;` |
|         - | 3848 | `	}` |
|         - | 3849 | `	/* implemented interfaces and per-use-statement trait containers */` |
|    482519 | 3850 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    482519 | 3851 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|         - | 3852 | `	/* Assume a standalone class */` |
|    482519 | 3853 | `	pBase = 0;` |
|    482519 | 3854 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    393841 | 3855 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    393841 | 3856 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|         - | 3857 | `			SyBlob sResolved;` |
|         - | 3858 | `			SyString sBaseName;` |
|         - | 3859 | `			sxu32 nRefLine;` |
|    265309 | 3860 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|         - | 3861 | `				/* php parse-fatals here (enums have no inheritance) */` |
|       ! 0 | 3862 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 3863 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|       ! 0 | 3864 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 3865 | `					return SXERR_ABORT;` |
|         - | 3866 | `				}` |
|       ! 0 | 3867 | `			}` |
|    265309 | 3868 | `			pGen->pIn++; /* Advance past 'extends' */` |
|    265309 | 3869 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    265309 | 3870 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    265309 | 3871 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         3 | 3872 | `				SyBlobRelease(&sResolved);` |
|         4 | 3873 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 3874 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|         1 | 3875 | `					pName);` |
|         3 | 3876 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|         3 | 3877 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 3878 | `					return SXERR_ABORT;` |
|         - | 3879 | `				}` |
|         3 | 3880 | `				return SXRET_OK;` |
|         - | 3881 | `			}` |
|    397958 | 3882 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    265302 | 3883 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    265307 | 3884 | `			SyStringInitFromBuf(&sBaseName,` |
|         - | 3885 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 3886 | `			/* Interfaces are not allowed */` |
|    265307 | 3887 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|       ! 0 | 3888 | `				pBase = pBase->pNextName;` |
|       ! 0 | 3889 | `			}` |
|    265307 | 3890 | `			if( pBase == 0 ){` |
|       ! 0 | 3891 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 3892 | `					"Nonexistent base class '%z'",&sBaseName);` |
|       ! 0 | 3893 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 3894 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 3895 | `					return SXERR_ABORT;` |
|         - | 3896 | `				}` |
|       ! 0 | 3897 | `			}else{` |
|    265307 | 3898 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|         4 | 3899 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 | 3900 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|         3 | 3901 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 3902 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 3903 | `						return SXERR_ABORT;` |
|         - | 3904 | `					}` |
|         3 | 3905 | `					pBase = 0; /* Never inherit from an enum */` |
|    265306 | 3906 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|       ! 0 | 3907 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 3908 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|       ! 0 | 3909 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 3910 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 3911 | `						return SXERR_ABORT;` |
|         - | 3912 | `					}` |
|       ! 0 | 3913 | `				}` |
|         - | 3914 | `			}` |
|    265307 | 3915 | `			SyBlobRelease(&sResolved);` |
|    265307 | 3916 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|       ! 0 | 3917 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|       ! 0 | 3918 | `			}` |
|    132651 | 3919 | `		}` |
|    393839 | 3920 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|         - | 3921 | `			ph7_class *pInterface;` |
|         - | 3922 | `			/* Interface implementation */` |
|    145117 | 3923 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    138810 | 3924 | `			for(;;){` |
|         - | 3925 | `				SyBlob sResolved;` |
|         - | 3926 | `				SyString sIntName;` |
|         - | 3927 | `				sxu32 nRefLine;` |
|    211371 | 3928 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    211371 | 3929 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    211371 | 3930 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 3931 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 3932 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 3933 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|       ! 0 | 3934 | `						pName);` |
|       ! 0 | 3935 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 3936 | `						return SXERR_ABORT;` |
|         - | 3937 | `					}` |
|       ! 0 | 3938 | `					break;` |
|         - | 3939 | `				}` |
|    422737 | 3940 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    211366 | 3941 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    211371 | 3942 | `				SyStringInitFromBuf(&sIntName,` |
|         - | 3943 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 3944 | `				/* Only interfaces are allowed */` |
|    211371 | 3945 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 3946 | `					pInterface = pInterface->pNextName;` |
|       ! 0 | 3947 | `				}` |
|    211371 | 3948 | `				if( pInterface == 0 ){` |
|       ! 0 | 3949 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 3950 | `						"Nonexistent base interface '%z'",&sIntName);` |
|       ! 0 | 3951 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 3952 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 3953 | `						return SXERR_ABORT;` |
|         - | 3954 | `					}` |
|       ! 0 | 3955 | `				}else{` |
|         - | 3956 | `					/* Reject user classes that try to implement Throwable` |
|         - | 3957 | `					 * directly (or via an interface that extends Throwable)` |
|         - | 3958 | `					 * unless they already extend Exception or Error.` |
|         - | 3959 | `					 * Exception and Error themselves are compiled from the` |
|         - | 3960 | `					 * built-in library and are exempt by FQN — a namespaced` |
|         - | 3961 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    211371 | 3962 | `					SyString *pFqn = &pClass->sName;` |
|    211371 | 3963 | `					int bIsExceptionOrError =` |
|    109827 | 3964 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    319123 | 3965 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    209303 | 3966 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|      4154 | 3967 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    215511 | 3968 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|     12438 | 3969 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|      4143 | 3970 | `						!bIsExceptionOrError ){` |
|        12 | 3971 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 3972 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|         3 | 3973 | `							&pClass->sName);` |
|         9 | 3974 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 3975 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 3976 | `							return SXERR_ABORT;` |
|         - | 3977 | `						}` |
|         - | 3978 | `						/* Skip registration so the follow-up abstract-method` |
|         - | 3979 | `						 * check does not produce a duplicate fatal. */` |
|         6 | 3980 | `					}else{` |
|    211365 | 3981 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|         - | 3982 | `					}` |
|         - | 3983 | `				}` |
|    211371 | 3984 | `				SyBlobRelease(&sResolved);` |
|    211371 | 3985 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     72561 | 3986 | `					break;` |
|         - | 3987 | `				}` |
|     66259 | 3988 | `				pGen->pIn++;/* Jump the comma */` |
|         5 | 3989 | `			}` |
|     72556 | 3990 | `		}` |
|    196917 | 3991 | `	}` |
|    482517 | 3992 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - | 3993 | `		/* Syntax error */` |
|       ! 0 | 3994 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|       ! 0 | 3995 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 3996 | `		if( rc == SXERR_ABORT ){` |
|         - | 3997 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 3998 | `			return SXERR_ABORT;` |
|         - | 3999 | `		}` |
|       ! 0 | 4000 | `		return SXRET_OK;` |
|         - | 4001 | `	}` |
|    482517 | 4002 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    482517 | 4003 | `	pEnd = 0; /* cc warning */` |
|         - | 4004 | `	/* Delimit the class body */` |
|    482517 | 4005 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    482517 | 4006 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 4007 | `		/* Syntax error */` |
|       ! 0 | 4008 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|       ! 0 | 4009 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 4010 | `		if( rc == SXERR_ABORT ){` |
|         - | 4011 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 4012 | `			return SXERR_ABORT;` |
|         - | 4013 | `		}` |
|       ! 0 | 4014 | `		return SXRET_OK;` |
|         - | 4015 | `	}` |
|         - | 4016 | `	/* The delimiter token is the class body's closing brace */` |
|    482517 | 4017 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 4018 | `	/* Swap token stream */` |
|    482517 | 4019 | `	pTmp = pGen->pEnd;` |
|    482517 | 4020 | `	pGen->pEnd = pEnd;` |
|         - | 4021 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|    482517 | 4022 | `	pClass->iFlags \|= iFlags;` |
|         - | 4023 | `	/* This class/enum is now the lexical class for its body — see pCurClass. */` |
|    482517 | 4024 | `	pGen->pCurClass = pClass;` |
|         - | 4025 | `	/* Start the parse process */` |
|   1807212 | 4026 | `	for(;;){` |
|         - | 4027 | `		/* Jump leading/trailing semi-colons */` |
|   5190605 | 4028 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    945507 | 4029 | `			pGen->pIn++;` |
|         5 | 4030 | `		}` |
|   4245103 | 4031 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 4032 | `			/* End of class body */` |
|    482471 | 4033 | `			break;` |
|         - | 4034 | `		}` |
|         - | 4035 | `		/* Bind a directly-preceding docblock to this member */` |
|   3762637 | 4036 | `		GenStateSetPendingDoc(&(*pGen));` |
|   3762632 | 4037 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   1881321 | 4038 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|       ! 0 | 4039 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 4040 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 4041 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 4042 | `			if( rc == SXERR_ABORT ){` |
|         - | 4043 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 4044 | `				return SXERR_ABORT;` |
|         - | 4045 | `			}` |
|       ! 0 | 4046 | `			goto done;` |
|         - | 4047 | `		}` |
|         - | 4048 | `		/* Assume public visibility */` |
|   3762637 | 4049 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   3762637 | 4050 | `		iAttrflags = 0;` |
|         - | 4051 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|         - | 4052 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|         - | 4053 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|         - | 4054 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|   3762637 | 4055 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 4056 | `			int bMod = 0;` |
|       ! 0 | 4057 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 4058 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|         - | 4059 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|         - | 4060 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|         - | 4061 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|         - | 4062 | `			 * that the generic keyword dispatch would misread as a method. */` |
|       ! 0 | 4063 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       ! 0 | 4064 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 | 4065 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|       ! 0 | 4066 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|       ! 0 | 4067 | `			}` |
|       ! 0 | 4068 | `			if( !bMod ){` |
|       ! 0 | 4069 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 4070 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 4071 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 4072 | `						return SXERR_ABORT;` |
|         - | 4073 | `					}` |
|       ! 0 | 4074 | `					goto done;` |
|         - | 4075 | `				}` |
|       ! 0 | 4076 | `				continue;` |
|         - | 4077 | `			}` |
|       ! 0 | 4078 | `		}` |
|   3762637 | 4079 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 4080 | `			/* Extract the current keyword */` |
|   3762637 | 4081 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   3762637 | 4082 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|         - | 4083 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|      8353 | 4084 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|      8353 | 4085 | `				if( rc != SXRET_OK ){` |
|         6 | 4086 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 4087 | `						return SXERR_ABORT;` |
|         - | 4088 | `					}` |
|         6 | 4089 | `					goto done;` |
|         - | 4090 | `				}` |
|      8349 | 4091 | `				continue;` |
|         - | 4092 | `			}` |
|   3754289 | 4093 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 4094 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|         - | 4095 | `				TraitUseEntry sUse;` |
|     16691 | 4096 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|     16691 | 4097 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|     16691 | 4098 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      8353 | 4099 | `				for(;;){` |
|         - | 4100 | `					ph7_class *pTrait;` |
|         - | 4101 | `					SyBlob sResolved;` |
|         - | 4102 | `					SyString sTraitName;` |
|     16701 | 4103 | `					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|         - | 4104 | `					/* A trait name is a full class reference: it may be qualified or` |
|         - | 4105 | ``					 * fully-qualified (`use Foo\Bar\T;`, `use \Foo\Bar\T;`) — the generated`` |
|         - | 4106 | `					 * PHPUnit mocks name their traits absolutely. Parse it with the shared` |
|         - | 4107 | `					 * class-reference reader (handles the leading '\', every '\'-segment and` |
|         - | 4108 | `					 * namespace/import resolution) instead of a single-identifier read, which` |
|         - | 4109 | `					 * choked on the first '\'. */` |
|     16701 | 4110 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     16701 | 4111 | `					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 4112 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 4113 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|       ! 0 | 4114 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|       ! 0 | 4115 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 4116 | `							return SXERR_ABORT;` |
|         - | 4117 | `						}` |
|       ! 0 | 4118 | `						break;` |
|         - | 4119 | `					}` |
|     33397 | 4120 | `					pTrait = PH7_VmExtractClass(pGen->pVm,` |
|     16696 | 4121 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     16701 | 4122 | `					SyStringInitFromBuf(&sTraitName,` |
|         - | 4123 | `						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 4124 | `					/* Only traits are allowed */` |
|     16701 | 4125 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 4126 | `						pTrait = pTrait->pNextName;` |
|       ! 0 | 4127 | `					}` |
|     16701 | 4128 | `					if( pTrait == 0 ){` |
|       ! 0 | 4129 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|         - | 4130 | `							"'%z' is not a trait",&sTraitName);` |
|       ! 0 | 4131 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 4132 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 4133 | `							return SXERR_ABORT;` |
|         - | 4134 | `						}` |
|       ! 0 | 4135 | `					}else{` |
|     16701 | 4136 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|         - | 4137 | `					}` |
|     16701 | 4138 | `					SyBlobRelease(&sResolved);` |
|         - | 4139 | `					/* GenStateParseClassReference already advanced past the whole name —` |
|         - | 4140 | `					 * continue only across a comma-separated trait list. */` |
|     16701 | 4141 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      8348 | 4142 | `						break;` |
|         - | 4143 | `					}` |
|        13 | 4144 | `					pGen->pIn++; /* Jump the comma */` |
|         3 | 4145 | `				}` |
|         - | 4146 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|     16691 | 4147 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - | 4148 | `					SyToken *pBlock;` |
|        25 | 4149 | `					pGen->pIn++; /* Jump '{' */` |
|        25 | 4150 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|        25 | 4151 | `					sUse.pResolvStart = pGen->pIn;` |
|        25 | 4152 | `					sUse.pResolvEnd = pBlock;` |
|        25 | 4153 | `					if( pBlock < pGen->pEnd ){` |
|        25 | 4154 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|        14 | 4155 | `					}else{` |
|       ! 0 | 4156 | `						pGen->pIn = pGen->pEnd;` |
|         - | 4157 | `					}` |
|        11 | 4158 | `				}` |
|     16691 | 4159 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|         - | 4160 | `				/* The semicolon will be consumed by the outer loop */` |
|     16691 | 4161 | `				continue;` |
|         - | 4162 | `			}` |
|   3737603 | 4163 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - | 4164 | `				int nSetTok;` |
|   3157343 | 4165 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   3157343 | 4166 | `				if( nSetVis ){` |
|         - | 4167 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|         - | 4168 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|         3 | 4169 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 4170 | `					pGen->pIn += nSetTok;` |
|         2 | 4171 | `				}else{` |
|   3157341 | 4172 | `					iProtection = nKwrd;` |
|   3157341 | 4173 | `					pGen->pIn++; /* Jump the visibility token */` |
|         - | 4174 | `					/* Optional asymmetric set-visibility after the read` |
|         - | 4175 | ``					 * visibility: `public private(set) int $x`. */`` |
|   3157341 | 4176 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   3157341 | 4177 | `					if( nSetVis ){` |
|         9 | 4178 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         9 | 4179 | `						pGen->pIn += nSetTok;` |
|         4 | 4180 | `					}` |
|         - | 4181 | `				}` |
|         - | 4182 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|         - | 4183 | ``				 * `public private(set) readonly int $x`. */`` |
|   3157343 | 4184 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|        31 | 4185 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|        31 | 4186 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        13 | 4187 | `				}` |
|   3157338 | 4188 | `				if( pGen->pIn >= pGen->pEnd` |
|   3157343 | 4189 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 4190 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 4191 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 4192 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 4193 | `					if( rc == SXERR_ABORT ){` |
|         - | 4194 | `						/* Error count limit reached,abort immediately */` |
|       ! 0 | 4195 | `						return SXERR_ABORT;` |
|         - | 4196 | `					}` |
|       ! 0 | 4197 | `					goto done;` |
|         - | 4198 | `				}` |
|   3157343 | 4199 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 4200 | `					/* Attribute declaration (untyped) */` |
|    559625 | 4201 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    559625 | 4202 | `					if( rc != SXRET_OK ){` |
|        10 | 4203 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 4204 | `							return SXERR_ABORT;` |
|         - | 4205 | `						}` |
|        10 | 4206 | `						goto done;` |
|         - | 4207 | `					}` |
|    568088 | 4208 | `					continue;` |
|         - | 4209 | `				}` |
|   2597723 | 4210 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 4211 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     16953 | 4212 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     16953 | 4213 | `					if( rc != SXRET_OK ){` |
|         8 | 4214 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 4215 | `							return SXERR_ABORT;` |
|         - | 4216 | `						}` |
|         8 | 4217 | `						goto done;` |
|         - | 4218 | `					}` |
|     16947 | 4219 | `					continue;` |
|         - | 4220 | `				}` |
|         - | 4221 | `				/* Extract the keyword */` |
|   2580775 | 4222 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1290385 | 4223 | `			}` |
|   3161035 | 4224 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|         - | 4225 | `				/* Process constant declaration */` |
|    306649 | 4226 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|    306649 | 4227 | `				if( rc != SXRET_OK ){` |
|        15 | 4228 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 4229 | `						return SXERR_ABORT;` |
|         - | 4230 | `					}` |
|        15 | 4231 | `					goto done;` |
|         - | 4232 | `				}` |
|    153321 | 4233 | `			}else{` |
|   2854391 | 4234 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - | 4235 | `					/* Static method or attribute,record that */` |
|    107895 | 4236 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    107895 | 4237 | `					pGen->pIn++; /* Jump the static keyword */` |
|    107895 | 4238 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 4239 | `						int nSetTok;` |
|     78859 | 4240 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|     78859 | 4241 | `						if( nSetVis ){` |
|         - | 4242 | ``							/* `static private(set) int $x` — read side stays public */`` |
|         3 | 4243 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 4244 | `							pGen->pIn += nSetTok;` |
|         2 | 4245 | `						}else{` |
|         - | 4246 | `							/* Extract the keyword */` |
|     78857 | 4247 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     78857 | 4248 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 4249 | `								iProtection = nKwrd;` |
|       ! 0 | 4250 | `								pGen->pIn++; /* Jump the visibility token */` |
|       ! 0 | 4251 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|       ! 0 | 4252 | `								if( nSetVis ){` |
|       ! 0 | 4253 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|       ! 0 | 4254 | `									pGen->pIn += nSetTok;` |
|       ! 0 | 4255 | `								}` |
|       ! 0 | 4256 | `							}` |
|         - | 4257 | `						}` |
|     39427 | 4258 | `					}` |
|         - | 4259 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|         - | 4260 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|         - | 4261 | `					 * than a generic "expecting method" parse error. */` |
|    107895 | 4262 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 4263 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 4264 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       ! 0 | 4265 | `					}` |
|    107890 | 4266 | `					if( pGen->pIn >= pGen->pEnd` |
|    107895 | 4267 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 4268 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 4269 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|       ! 0 | 4270 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 4271 | `						if( rc == SXERR_ABORT ){` |
|         - | 4272 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 4273 | `							return SXERR_ABORT;` |
|         - | 4274 | `						}` |
|       ! 0 | 4275 | `						goto done;` |
|         - | 4276 | `					}` |
|    107895 | 4277 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 4278 | `						/* Attribute declaration */` |
|     29035 | 4279 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     29035 | 4280 | `						if( rc != SXRET_OK ){` |
|         3 | 4281 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 4282 | `								return SXERR_ABORT;` |
|         - | 4283 | `							}` |
|         3 | 4284 | `							goto done;` |
|         - | 4285 | `						}` |
|     29033 | 4286 | `						continue;` |
|         - | 4287 | `					}` |
|     78865 | 4288 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 4289 | `						/* Typed static attribute declaration */` |
|        49 | 4290 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        49 | 4291 | `						if( rc != SXRET_OK ){` |
|         3 | 4292 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 4293 | `								return SXERR_ABORT;` |
|         - | 4294 | `							}` |
|         3 | 4295 | `							goto done;` |
|         - | 4296 | `						}` |
|        47 | 4297 | `						continue;` |
|         - | 4298 | `					}` |
|         - | 4299 | `					/* Extract the keyword */` |
|     78821 | 4300 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2785909 | 4301 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         - | 4302 | `					/* Abstract method,record that.` |
|         - | 4303 | `					 * PHL used to also mark the whole CLASS abstract here, silently` |
|         - | 4304 | ``					 * promoting `class C{abstract function m();}` -- which php rejects`` |
|         - | 4305 | `					 * outright -- into a valid abstract class. That promotion is why` |
|         - | 4306 | `					 * GenStateCheckAbstractMethods never fired for it: by the time the` |
|         - | 4307 | `					 * check ran, the class looked declared-abstract. The declaration is` |
|         - | 4308 | `					 * now diagnosed where the method name is known (see the install` |
|         - | 4309 | `					 * site), so the class flag stays what the SOURCE said. */` |
|      8313 | 4310 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         - | 4311 | `					/* Advance the stream cursor */` |
|      8313 | 4312 | `					pGen->pIn++;` |
|      8313 | 4313 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      8313 | 4314 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      8313 | 4315 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      8311 | 4316 | `							iProtection = nKwrd;` |
|      8311 | 4317 | `							pGen->pIn++; /* Jump the visibility token */` |
|      4153 | 4318 | `						}` |
|      4154 | 4319 | `					}` |
|      8313 | 4320 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      8308 | 4321 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 4322 | `							/* Static method */` |
|       ! 0 | 4323 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 4324 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 4325 | `					}` |
|      8313 | 4326 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      8308 | 4327 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|         - | 4328 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|         - | 4329 | `							 * HOOKED property declaration. Route anything that is not a` |
|         - | 4330 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|         - | 4331 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|         - | 4332 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|         6 | 4333 | `							if( pGen->pIn < pGen->pEnd` |
|         7 | 4334 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|         3 | 4335 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         7 | 4336 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         7 | 4337 | `								if( rc != SXRET_OK ){` |
|       ! 0 | 4338 | `									if( rc == SXERR_ABORT ){` |
|       ! 0 | 4339 | `										return SXERR_ABORT;` |
|         - | 4340 | `									}` |
|       ! 0 | 4341 | `									goto done;` |
|         - | 4342 | `								}` |
|         7 | 4343 | `								continue;` |
|         - | 4344 | `							}` |
|       ! 0 | 4345 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 4346 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|       ! 0 | 4347 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 4348 | `							if( rc == SXERR_ABORT ){` |
|         - | 4349 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 4350 | `								return SXERR_ABORT;` |
|         - | 4351 | `							}` |
|       ! 0 | 4352 | `							goto done;` |
|         - | 4353 | `					}` |
|      8307 | 4354 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   2742344 | 4355 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|         - | 4356 | `					/* final method ,record that */` |
|        24 | 4357 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|        24 | 4358 | `					pGen->pIn++; /* Jump the final keyword */` |
|        24 | 4359 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 4360 | `						/* Extract the keyword */` |
|        24 | 4361 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        24 | 4362 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        13 | 4363 | `							iProtection = nKwrd;` |
|        13 | 4364 | `							pGen->pIn++; /* Jump the visibility token */` |
|         5 | 4365 | `						}` |
|        10 | 4366 | `					}` |
|        24 | 4367 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        20 | 4368 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|         - | 4369 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|         - | 4370 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|         - | 4371 | `							 * child class is compiled (PH7_ClassInherit). */` |
|        16 | 4372 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|        16 | 4373 | `							if( rc != SXRET_OK ){` |
|       ! 0 | 4374 | `								if( rc == SXERR_ABORT ){` |
|       ! 0 | 4375 | `									return SXERR_ABORT;` |
|         - | 4376 | `								}` |
|       ! 0 | 4377 | `								goto done;` |
|         - | 4378 | `							}` |
|        16 | 4379 | `							continue;` |
|         - | 4380 | `					}` |
|         9 | 4381 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         6 | 4382 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 4383 | `							/* Static method */` |
|       ! 0 | 4384 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 4385 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 4386 | `					}` |
|         9 | 4387 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         6 | 4388 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 4389 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 4390 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|       ! 0 | 4391 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 4392 | `							if( rc == SXERR_ABORT ){` |
|         - | 4393 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 4394 | `								return SXERR_ABORT;` |
|         - | 4395 | `							}` |
|       ! 0 | 4396 | `							goto done;` |
|         - | 4397 | `					}` |
|         9 | 4398 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         3 | 4399 | `				}` |
|   2825297 | 4400 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 4401 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 4402 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|       ! 0 | 4403 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 4404 | `						if( rc == SXERR_ABORT ){` |
|         - | 4405 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 4406 | `							return SXERR_ABORT;` |
|         - | 4407 | `						}` |
|       ! 0 | 4408 | `						goto done;` |
|         - | 4409 | `				}` |
|   2825297 | 4410 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|         7 | 4411 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|         7 | 4412 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|       ! 0 | 4413 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 4414 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 4415 | `						if( rc == SXERR_ABORT ){` |
|         - | 4416 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 4417 | `							return SXERR_ABORT;` |
|         - | 4418 | `						}` |
|       ! 0 | 4419 | `						goto done;` |
|         - | 4420 | `					}` |
|         - | 4421 | `					/* Attribute declaration */` |
|         7 | 4422 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         4 | 4423 | `				}else{` |
|         - | 4424 | `					/* Process method declaration */` |
|   2825291 | 4425 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 4426 | `				}` |
|   2825297 | 4427 | `				if( rc != SXRET_OK ){` |
|        16 | 4428 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 4429 | `						return SXERR_ABORT;` |
|         - | 4430 | `					}` |
|        16 | 4431 | `					goto done;` |
|         - | 4432 | `				}` |
|         - | 4433 | `			}` |
|   1565961 | 4434 | `		}else{` |
|         - | 4435 | `			/* Attribute declaration */` |
|       ! 0 | 4436 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 4437 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 4438 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 4439 | `					return SXERR_ABORT;` |
|         - | 4440 | `				}` |
|       ! 0 | 4441 | `				goto done;` |
|         - | 4442 | `			}` |
|         - | 4443 | `		}` |
|         5 | 4444 | `	}` |
|         - | 4445 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|         - | 4446 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|         - | 4447 | `	 */` |
|    482471 | 4448 | `	rc = GenStateApplyTraitUses(&(*pGen),pClass,&aUseEntries);` |
|    482471 | 4449 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 4450 | `		SySetRelease(&aUseEntries);` |
|       ! 0 | 4451 | `		SySetRelease(&aInterfaces);` |
|       ! 0 | 4452 | `		return SXERR_ABORT;` |
|         - | 4453 | `	}` |
|    482471 | 4454 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 4455 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|         - | 4456 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|      4189 | 4457 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|      4189 | 4458 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 4459 | `			SySetRelease(&aUseEntries);` |
|       ! 0 | 4460 | `			SySetRelease(&aInterfaces);` |
|       ! 0 | 4461 | `			return SXERR_ABORT;` |
|         - | 4462 | `		}` |
|      2092 | 4463 | `	}` |
|         - | 4464 | `	/* Reject a php-fatal redeclaration before hoisting the class */` |
|    482471 | 4465 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|         9 | 4466 | `		return SXERR_ABORT;` |
|         - | 4467 | `	}` |
|         - | 4468 | `	/* Install the class */` |
|    482465 | 4469 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    482465 | 4470 | `	if( rc == SXRET_OK ){` |
|         - | 4471 | `		ph7_class **apInterface;` |
|         - | 4472 | `		sxu32 n;` |
|    482465 | 4473 | `		if( pBase ){` |
|         - | 4474 | `			/* Inherit from base class and mark as a subclass */` |
|    265305 | 4475 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|    132650 | 4476 | `		}` |
|    482465 | 4477 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|    693825 | 4478 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|         - | 4479 | `			/* Implements one or more interface */` |
|    211365 | 4480 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    211365 | 4481 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 4482 | `				break;` |
|         - | 4483 | `			}` |
|    105685 | 4484 | `		}` |
|         - | 4485 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|         - | 4486 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|    482465 | 4487 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|      4187 | 4488 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|      4187 | 4489 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 4490 | `				pIntf = pIntf->pNextName;` |
|       ! 0 | 4491 | `			}` |
|      4187 | 4492 | `			if( pIntf ){` |
|      4187 | 4493 | `				PH7_ClassImplement(pClass,pIntf);` |
|      2091 | 4494 | `			}` |
|      4187 | 4495 | `			if( pClass->nEnumBacking != 0 ){` |
|      4165 | 4496 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|      4165 | 4497 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 4498 | `					pIntf = pIntf->pNextName;` |
|       ! 0 | 4499 | `				}` |
|      4165 | 4500 | `				if( pIntf ){` |
|      4165 | 4501 | `					PH7_ClassImplement(pClass,pIntf);` |
|      2080 | 4502 | `				}` |
|      2080 | 4503 | `			}` |
|      2091 | 4504 | `		}` |
|         - | 4505 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|         - | 4506 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|    482460 | 4507 | `		if( rc == SXRET_OK` |
|    482460 | 4508 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|    482465 | 4509 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|    244381 | 4510 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|         - | 4511 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|    244381 | 4512 | `			if( pStringable ){` |
|    244381 | 4513 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    244381 | 4514 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|         - | 4515 | `				sxu32 i;` |
|    244381 | 4516 | `				int bAlready = 0;` |
|    294065 | 4517 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|     62111 | 4518 | `					if( apImpl[i] == pStringable ){` |
|     12427 | 4519 | `						bAlready = 1;` |
|     12427 | 4520 | `						break;` |
|         - | 4521 | `					}` |
|     24847 | 4522 | `				}` |
|    244381 | 4523 | `				if( !bAlready ){` |
|    231959 | 4524 | `					PH7_ClassImplement(pClass,pStringable);` |
|    115977 | 4525 | `				}` |
|    122188 | 4526 | `			}` |
|    122188 | 4527 | `		}` |
|         - | 4528 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|    482465 | 4529 | `		if( rc == SXRET_OK ){` |
|    482465 | 4530 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|    482465 | 4531 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 4532 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 4533 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 4534 | `				return SXERR_ABORT;` |
|         - | 4535 | `			}` |
|    241230 | 4536 | `		}` |
|         - | 4537 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|    482465 | 4538 | `		if( rc == SXRET_OK ){` |
|    482465 | 4539 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|    482465 | 4540 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 4541 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 4542 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 4543 | `				return SXERR_ABORT;` |
|         - | 4544 | `			}` |
|    241230 | 4545 | `		}` |
|    241230 | 4546 | `	}` |
|    482465 | 4547 | `	SySetRelease(&aUseEntries);` |
|    482465 | 4548 | `	SySetRelease(&aInterfaces);` |
|    482465 | 4549 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 4550 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 4551 | `		return SXERR_ABORT;` |
|         - | 4552 | `	}` |
|    241230 | 4553 | `done:` |
|    482511 | 4554 | `	pGen->pCurClass = pSavedCurClass;` |
|         - | 4555 | `	/* Point beyond the class body */` |
|    482511 | 4556 | `	pGen->pIn = &pEnd[1];` |
|    482511 | 4557 | `	pGen->pEnd = pTmp;` |
|    482511 | 4558 | `	return PH7_OK;` |
|    241270 | 4559 | `}` |
|         - | 4560 | `/* Compile a named class declaration (the common case). */` |
|    482486 | 4561 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|         5 | 4562 | `{` |
|    482491 | 4563 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|         5 | 4564 | `}` |
|         - | 4565 | `/*` |
|         - | 4566 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|         - | 4567 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|         - | 4568 | ` * compile + install the class body once (at compile time, like every other` |
|         - | 4569 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|         - | 4570 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|         - | 4571 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|         - | 4572 | ` */` |
|        50 | 4573 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 | 4574 | `{` |
|         - | 4575 | `	char zName[128];         /* Synthesized class name */` |
|         - | 4576 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|         - | 4577 | `	SyString sName;` |
|         - | 4578 | `	SyToken *pArgStart,*pArgEnd;` |
|        55 | 4579 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|         - | 4580 | `	                              * is keyed to this 'class' token */` |
|         - | 4581 | `	ph7_value *pObj;` |
|        55 | 4582 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 4583 | `	sxu32 nIdx,nLen;` |
|         - | 4584 | `	sxi32 nArg,rc;` |
|        25 | 4585 | `	SXUNUSED(iCompileFlag);` |
|        55 | 4586 | `	if( pGen->pVm->sDeferAnonName.nByte > 0 ){` |
|         - | 4587 | `		/* Deferred re-compile (VmExecDeferredClass): install under the SAME` |
|         - | 4588 | `		 * synthesized name the original site's OP_NEW loads. One-shot. */` |
|         5 | 4589 | `		sName = pGen->pVm->sDeferAnonName;` |
|         5 | 4590 | `		nLen = sName.nByte;` |
|         5 | 4591 | `		pGen->pVm->sDeferAnonName.zString = 0;` |
|         5 | 4592 | `		pGen->pVm->sDeferAnonName.nByte = 0;` |
|         3 | 4593 | `	}else{` |
|         - | 4594 | `		/* Generate a unique anonymous-class name (collision-checked) */` |
|        51 | 4595 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|        51 | 4596 | `		while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 | 4597 | `			nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       ! 0 | 4598 | `		}` |
|        51 | 4599 | `		SyStringInitFromBuf(&sName,zName,nLen);` |
|         - | 4600 | `	}` |
|         - | 4601 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|         - | 4602 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|         - | 4603 | `	 * delimited construct; GenStateCompileClassEx restores both on success.` |
|         - | 4604 | ``	 * Deferral gate: `new class extends \App\Child {}` where the`` |
|         - | 4605 | `	 * parent's autoloader has not RUN yet — capture the class for a runtime` |
|         - | 4606 | `	 * re-compile and keep only the site's argument/OP_NEW emission here. */` |
|        55 | 4607 | `	pArgStart = pArgEnd = 0;` |
|         - | 4608 | `	{` |
|         - | 4609 | `		SySet aMissing;` |
|        55 | 4610 | `		SyToken *pBody = 0;` |
|        55 | 4611 | `		SyToken *pBodyEnd = 0;` |
|         - | 4612 | `		SyBlob sSelfFqn;` |
|        55 | 4613 | `		int bDeferred = 0;` |
|        55 | 4614 | `		SySetInit(&aMissing,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|        55 | 4615 | `		SyBlobInit(&sSelfFqn,&pGen->pVm->sAllocator);` |
|        50 | 4616 | `		if( GenStateScanDeferDeps(pGen,1,PH7_DEFER_KIND_CLASS,&aMissing,&pBody,&pBodyEnd,&sSelfFqn) == SXRET_OK` |
|        55 | 4617 | `		 && SySetUsed(&aMissing) > 0 ){` |
|         7 | 4618 | `			if( &pTokKw[1] < pGen->pEnd && (pTokKw[1].nType & PH7_TK_LPAREN) ){` |
|         3 | 4619 | `				SyToken *pClose = 0;` |
|         3 | 4620 | `				PH7_DelimitNestedTokens(&pTokKw[2],pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|         3 | 4621 | `				if( pClose && pClose < pGen->pEnd ){` |
|         3 | 4622 | `					pArgStart = &pTokKw[2];` |
|         3 | 4623 | `					pArgEnd = pClose;` |
|         1 | 4624 | `				}` |
|         1 | 4625 | `			}` |
|         7 | 4626 | `			rc = GenStateEmitDeferredClass(pGen,0,1,&aMissing,pBodyEnd,0,&sName);` |
|         7 | 4627 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 4628 | `				SySetRelease(&aMissing);` |
|       ! 0 | 4629 | `				SyBlobRelease(&sSelfFqn);` |
|       ! 0 | 4630 | `				return SXERR_ABORT;` |
|         - | 4631 | `			}` |
|         7 | 4632 | `			bDeferred = ( rc == SXRET_OK );` |
|         3 | 4633 | `		}` |
|        55 | 4634 | `		SySetRelease(&aMissing);` |
|        55 | 4635 | `		SyBlobRelease(&sSelfFqn);` |
|        55 | 4636 | `		if( !bDeferred ){` |
|        49 | 4637 | `			rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|        49 | 4638 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 4639 | `				return rc;` |
|         - | 4640 | `			}` |
|         - | 4641 | `			{` |
|         - | 4642 | ``				/* Expression-position attributes (`new #[A] class {…}`) */`` |
|        49 | 4643 | `				ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,sName.zString,nLen,FALSE,0);` |
|        44 | 4644 | `				if( pAnonClass` |
|        49 | 4645 | `				 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 4646 | `					return SXERR_ABORT;` |
|         - | 4647 | `				}` |
|         - | 4648 | `			}` |
|        22 | 4649 | `		}` |
|         - | 4650 | `	}` |
|         - | 4651 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|         - | 4652 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|        55 | 4653 | `	nArg = 0;` |
|        55 | 4654 | `	if( pArgStart < pArgEnd ){` |
|        10 | 4655 | `		SyToken *pSavedIn = pGen->pIn;` |
|        10 | 4656 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|         - | 4657 | `		SyToken *pArgNext;` |
|        10 | 4658 | `		pGen->pIn = pArgStart;` |
|        10 | 4659 | `		pGen->pEnd = pArgEnd;` |
|        18 | 4660 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|        10 | 4661 | `			if( pGen->pIn < pArgNext ){` |
|        10 | 4662 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|        10 | 4663 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 4664 | `					pGen->pIn = pSavedIn;` |
|       ! 0 | 4665 | `					pGen->pEnd = pSavedEnd;` |
|       ! 0 | 4666 | `					return SXERR_ABORT;` |
|         - | 4667 | `				}` |
|        10 | 4668 | `				nArg++;` |
|         4 | 4669 | `			}` |
|        10 | 4670 | `			pGen->pIn = &pArgNext[1];` |
|         2 | 4671 | `		}` |
|        10 | 4672 | `		pGen->pIn = pSavedIn;` |
|        10 | 4673 | `		pGen->pEnd = pSavedEnd;` |
|         4 | 4674 | `	}` |
|         - | 4675 | `	/* Load the synthesized class name */` |
|        55 | 4676 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        55 | 4677 | `	if( pObj == 0 ){` |
|       ! 0 | 4678 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 4679 | `		return SXERR_ABORT;` |
|         - | 4680 | `	}` |
|        55 | 4681 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|        55 | 4682 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - | 4683 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|        55 | 4684 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        55 | 4685 | `	return SXRET_OK;` |
|        30 | 4686 | `}` |
|         - | 4687 | `/*` |
|         - | 4688 | ` * Compile a user-defined abstract class.` |
|         - | 4689 | ` *  According to the PHP language reference manual` |
|         - | 4690 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|         - | 4691 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|         - | 4692 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|         - | 4693 | ` *   the method's signature - they cannot define the implementation.` |
|         - | 4694 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|         - | 4695 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|         - | 4696 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|         - | 4697 | ` *   method is defined as protected, the function implementation must be defined as either` |
|         - | 4698 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|         - | 4699 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|         - | 4700 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|         - | 4701 | ` *   could differ.` |
|         - | 4702 | ` */` |
|         - | 4703 | `/*` |
|         - | 4704 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|         - | 4705 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|         - | 4706 | ` * receives the corresponding PH7_CLASS_* bit.` |
|         - | 4707 | ` */` |
|  15470848 | 4708 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|         5 | 4709 | `{` |
|  15470853 | 4710 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|   9259969 | 4711 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|   9259969 | 4712 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|   9201951 | 4713 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|   4580214 | 4714 | `	}` |
|  15371317 | 4715 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  15371257 | 4716 | `	return FALSE;` |
|   7735429 | 4717 | `}` |
|         - | 4718 | `/*` |
|         - | 4719 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|         - | 4720 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|         - | 4721 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|         - | 4722 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|         - | 4723 | ` */` |
|  15371252 | 4724 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|         5 | 4725 | `{` |
|  15371257 | 4726 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  15371257 | 4727 | `	sxi32 iFlags = 0,iFlag;` |
|  15470853 | 4728 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|     99601 | 4729 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|         5 | 4730 | `			pDup = pIn;` |
|         2 | 4731 | `		}` |
|     99601 | 4732 | `		iFlags \|= iFlag;` |
|     99601 | 4733 | `		pIn++;` |
|         5 | 4734 | `	}` |
|  15371257 | 4735 | `	*ppIn = pIn;` |
|  15371257 | 4736 | `	if( ppDup ){ *ppDup = pDup; }` |
|  15371257 | 4737 | `	return iFlags;` |
|         5 | 4738 | `}` |
|         - | 4739 | `/*` |
|         - | 4740 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|         - | 4741 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|         - | 4742 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|         - | 4743 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|         - | 4744 | `` * `readonly`) to their existing handlers.`` |
|         - | 4745 | ` */` |
|  15325604 | 4746 | `PH7_PRIVATE int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|         5 | 4747 | `{` |
|  15325609 | 4748 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|   7716737 | 4749 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  15352570 | 4750 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|         5 | 4751 | `}` |
|         - | 4752 | `/*` |
|         - | 4753 | ` * Compile a class declaration carrying one or more leading modifiers` |
|         - | 4754 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|         - | 4755 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|         - | 4756 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|         - | 4757 | `` * `abstract`+`final` pair, like PHP.`` |
|         - | 4758 | ` */` |
|     45648 | 4759 | `PH7_PRIVATE sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|         5 | 4760 | `{` |
|         - | 4761 | `	SyToken *pDup;` |
|     45653 | 4762 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|         - | 4763 | `	sxi32 rc;` |
|     45653 | 4764 | `	if( pDup ){` |
|         4 | 4765 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|         2 | 4766 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|         3 | 4767 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 4768 | `			return SXERR_ABORT;` |
|         - | 4769 | `		}` |
|         1 | 4770 | `	}` |
|     45648 | 4771 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|     22829 | 4772 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|         3 | 4773 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 4774 | `			"Cannot use the final modifier on an abstract class");` |
|         3 | 4775 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 4776 | `			return SXERR_ABORT;` |
|         - | 4777 | `		}` |
|         1 | 4778 | `	}` |
|     45653 | 4779 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|     22829 | 4780 | `}` |
|         - | 4781 | `/*` |
|         - | 4782 | ` * Compile a user-defined trait.` |
|         - | 4783 | ` *  Traits are similar to classes, but only intended to group functionality` |
|         - | 4784 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|         - | 4785 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|         - | 4786 | ` */` |
|      8424 | 4787 | `PH7_PRIVATE sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|         5 | 4788 | `{` |
|      8429 | 4789 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 4790 | `	ph7_class *pClass;` |
|      8429 | 4791 | `	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' */` |
|         - | 4792 | `	SyToken *pEnd,*pTmp;` |
|         - | 4793 | `	sxi32 iProtection;` |
|         - | 4794 | `	sxi32 iAttrflags;` |
|         - | 4795 | ``	SySet aUseEntries; /* trait-body `use` statements (incl. adaptation blocks) */`` |
|         - | 4796 | `	SyString *pName;` |
|         - | 4797 | `	sxi32 nKwrd;` |
|         - | 4798 | `	sxi32 rc;` |
|         - | 4799 | `	{` |
|         - | 4800 | `		/* Deferral gate: a used trait may need an autoloader that` |
|         - | 4801 | `		 * has not run yet. */` |
|         - | 4802 | `		sxi32 rcDefer;` |
|      8429 | 4803 | `		if( GenStateMaybeDeferClass(pGen,0,PH7_DEFER_KIND_TRAIT,&rcDefer) ){` |
|         6 | 4804 | `			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - | 4805 | `		}` |
|         - | 4806 | `	}` |
|      8425 | 4807 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|         - | 4808 | `	/* Jump the 'trait' keyword */` |
|      8425 | 4809 | `	pGen->pIn++;` |
|      8425 | 4810 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 4811 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|       ! 0 | 4812 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 4813 | `			return SXERR_ABORT;` |
|         - | 4814 | `		}` |
|       ! 0 | 4815 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|       ! 0 | 4816 | `			pGen->pIn++;` |
|       ! 0 | 4817 | `		}` |
|       ! 0 | 4818 | `		return SXRET_OK;` |
|         - | 4819 | `	}` |
|         - | 4820 | `	/* Extract trait name */` |
|      8425 | 4821 | `	pName = &pGen->pIn->sData;` |
|      8425 | 4822 | `	pGen->pIn++;` |
|         - | 4823 | `	/* Build FQN and obtain a raw class */ {` |
|         - | 4824 | `		SyBlob sFQN;` |
|         - | 4825 | `		SyString sFQNStr;` |
|      8425 | 4826 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      8425 | 4827 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      8425 | 4828 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      8425 | 4829 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      8425 | 4830 | `		SyBlobRelease(&sFQN);` |
|         - | 4831 | `	}` |
|      8425 | 4832 | `	if( pClass == 0 ){` |
|       ! 0 | 4833 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 4834 | `		return SXERR_ABORT;` |
|         - | 4835 | `	}` |
|      8425 | 4836 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|      8425 | 4837 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 4838 | `		return SXERR_ABORT;` |
|         - | 4839 | `	}` |
|         - | 4840 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      8425 | 4841 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 | 4842 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|       ! 0 | 4843 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 4844 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 4845 | `			return SXERR_ABORT;` |
|         - | 4846 | `		}` |
|       ! 0 | 4847 | `		return SXRET_OK;` |
|         - | 4848 | `	}` |
|      8425 | 4849 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      8425 | 4850 | `	pEnd = 0;` |
|      8425 | 4851 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      8425 | 4852 | `	if( pEnd >= pGen->pEnd ){` |
|       ! 0 | 4853 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|       ! 0 | 4854 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 4855 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 4856 | `			return SXERR_ABORT;` |
|         - | 4857 | `		}` |
|       ! 0 | 4858 | `		return SXRET_OK;` |
|         - | 4859 | `	}` |
|         - | 4860 | `	/* The delimiter token is the trait body's closing brace */` |
|      8425 | 4861 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 4862 | `	/* Swap token stream */` |
|      8425 | 4863 | `	pTmp = pGen->pEnd;` |
|      8425 | 4864 | `	pGen->pEnd = pEnd;` |
|         - | 4865 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|      8425 | 4866 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|         - | 4867 | `	/* This trait is now the lexical class for its body, so a property/parameter` |
|         - | 4868 | `	 * default here resolves __TRAIT__ to it (see pCurClass). */` |
|      8425 | 4869 | `	pGen->pCurClass = pClass;` |
|         - | 4870 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|     60179 | 4871 | `	for(;;){` |
|    170121 | 4872 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     24885 | 4873 | `			pGen->pIn++;` |
|         5 | 4874 | `		}` |
|    145241 | 4875 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      8425 | 4876 | `			break;` |
|         - | 4877 | `		}` |
|         - | 4878 | `		/* Bind a directly-preceding docblock to this member */` |
|    136821 | 4879 | `		GenStateSetPendingDoc(&(*pGen));` |
|    136821 | 4880 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|       ! 0 | 4881 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 4882 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 4883 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 4884 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 4885 | `				return SXERR_ABORT;` |
|         - | 4886 | `			}` |
|       ! 0 | 4887 | `			goto done;` |
|         - | 4888 | `		}` |
|    136821 | 4889 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|    136821 | 4890 | `		iAttrflags = 0;` |
|    136821 | 4891 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|    136821 | 4892 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    136821 | 4893 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 4894 | `				/* Trait uses another trait: use T[, T2] [{ resolution }]; A trait` |
|         - | 4895 | `				 * name is a full class reference — qualified or fully-qualified` |
|         - | 4896 | ``				 * (`use Foo\T;`, `use \Foo\T;`) — so parse it with the shared`` |
|         - | 4897 | `				 * class-reference reader like the CLASS body's trait-use does` |
|         - | 4898 | `				 * (the old single-identifier read choked on the leading '\'),` |
|         - | 4899 | `				 * and collect a TraitUseEntry so an adaptation block` |
|         - | 4900 | `				 * (insteadof/as) applies through the same shared machinery. */` |
|         - | 4901 | `				TraitUseEntry sUse;` |
|        16 | 4902 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|        16 | 4903 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|        16 | 4904 | `				pGen->pIn++; /* Jump 'use' */` |
|        10 | 4905 | `				for(;;){` |
|         - | 4906 | `					ph7_class *pUsedTrait;` |
|         - | 4907 | `					SyBlob sResolved;` |
|         - | 4908 | `					SyString sUsedName;` |
|        20 | 4909 | `					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|        20 | 4910 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        20 | 4911 | `					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 4912 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 4913 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|       ! 0 | 4914 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|       ! 0 | 4915 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 4916 | `							return SXERR_ABORT;` |
|         - | 4917 | `						}` |
|       ! 0 | 4918 | `						break;` |
|         - | 4919 | `					}` |
|        36 | 4920 | `					pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|        16 | 4921 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|        20 | 4922 | `					SyStringInitFromBuf(&sUsedName,` |
|         - | 4923 | `						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        20 | 4924 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 4925 | `						pUsedTrait = pUsedTrait->pNextName;` |
|       ! 0 | 4926 | `					}` |
|        20 | 4927 | `					if( pUsedTrait == 0 ){` |
|       ! 0 | 4928 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|         - | 4929 | `							"'%z' is not a trait",&sUsedName);` |
|       ! 0 | 4930 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 4931 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 4932 | `							return SXERR_ABORT;` |
|         - | 4933 | `						}` |
|       ! 0 | 4934 | `					}else{` |
|        20 | 4935 | `						SySetPut(&sUse.aTraits,(const void *)&pUsedTrait);` |
|         - | 4936 | `					}` |
|        20 | 4937 | `					SyBlobRelease(&sResolved);` |
|        20 | 4938 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|        10 | 4939 | `						break;` |
|         - | 4940 | `					}` |
|         6 | 4941 | `					pGen->pIn++;` |
|         2 | 4942 | `				}` |
|         - | 4943 | `				/* Optional adaptation block (conflict resolution) */` |
|        16 | 4944 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - | 4945 | `					SyToken *pBlock;` |
|         5 | 4946 | `					pGen->pIn++; /* Jump '{' */` |
|         5 | 4947 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|         5 | 4948 | `					sUse.pResolvStart = pGen->pIn;` |
|         5 | 4949 | `					sUse.pResolvEnd = pBlock;` |
|         5 | 4950 | `					if( pBlock < pGen->pEnd ){` |
|         5 | 4951 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|         3 | 4952 | `					}else{` |
|       ! 0 | 4953 | `						pGen->pIn = pGen->pEnd;` |
|         - | 4954 | `					}` |
|         2 | 4955 | `				}` |
|        16 | 4956 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|        16 | 4957 | `				continue;` |
|         - | 4958 | `			}` |
|    136809 | 4959 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|    136791 | 4960 | `				iProtection = nKwrd;` |
|    136791 | 4961 | `				pGen->pIn++;` |
|         - | 4962 | ``				/* Optional `readonly` after the visibility (PHP 8.1): `private readonly T`` |
|         - | 4963 | ``				 * $x` — the generated PHPUnit runtime traits (StubApi, …) declare their`` |
|         - | 4964 | `				 * readonly state this way. Mirrors the class-body attribute parser. */` |
|    136791 | 4965 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|         5 | 4966 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|         5 | 4967 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|         2 | 4968 | `				}` |
|    136786 | 4969 | `				if( pGen->pIn >= pGen->pEnd` |
|    136791 | 4970 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 4971 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 4972 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 4973 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 4974 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 4975 | `						return SXERR_ABORT;` |
|         - | 4976 | `					}` |
|       ! 0 | 4977 | `					goto done;` |
|         - | 4978 | `				}` |
|    136791 | 4979 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     24861 | 4980 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     24861 | 4981 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 4982 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 4983 | `							return SXERR_ABORT;` |
|         - | 4984 | `						}` |
|       ! 0 | 4985 | `						goto done;` |
|         - | 4986 | `					}` |
|     24861 | 4987 | `					continue;` |
|         - | 4988 | `				}` |
|    111935 | 4989 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         9 | 4990 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         9 | 4991 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 4992 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 4993 | `							return SXERR_ABORT;` |
|         - | 4994 | `						}` |
|       ! 0 | 4995 | `						goto done;` |
|         - | 4996 | `					}` |
|         9 | 4997 | `					continue;` |
|         - | 4998 | `				}` |
|    111927 | 4999 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     55961 | 5000 | `			}` |
|    111945 | 5001 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       ! 0 | 5002 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 5003 | `					"Traits cannot have constants");` |
|       ! 0 | 5004 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 5005 | `					return SXERR_ABORT;` |
|         - | 5006 | `				}` |
|       ! 0 | 5007 | `				goto done;` |
|       ! 0 | 5008 | `			}else{` |
|    111945 | 5009 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|      8297 | 5010 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      8297 | 5011 | `					pGen->pIn++;` |
|      8297 | 5012 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      8295 | 5013 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      8295 | 5014 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 5015 | `							iProtection = nKwrd;` |
|       ! 0 | 5016 | `							pGen->pIn++;` |
|       ! 0 | 5017 | `						}` |
|      4145 | 5018 | `					}` |
|      8292 | 5019 | `					if( pGen->pIn >= pGen->pEnd` |
|      8297 | 5020 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 5021 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 5022 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|       ! 0 | 5023 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 5024 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 5025 | `							return SXERR_ABORT;` |
|         - | 5026 | `						}` |
|       ! 0 | 5027 | `						goto done;` |
|         - | 5028 | `					}` |
|      8297 | 5029 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         3 | 5030 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         3 | 5031 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 5032 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 5033 | `								return SXERR_ABORT;` |
|         - | 5034 | `							}` |
|       ! 0 | 5035 | `							goto done;` |
|         - | 5036 | `						}` |
|         3 | 5037 | `						continue;` |
|         - | 5038 | `					}` |
|      8295 | 5039 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       ! 0 | 5040 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 5041 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 5042 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 5043 | `								return SXERR_ABORT;` |
|         - | 5044 | `							}` |
|       ! 0 | 5045 | `							goto done;` |
|         - | 5046 | `						}` |
|       ! 0 | 5047 | `						continue;` |
|         - | 5048 | `					}` |
|      8295 | 5049 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    107798 | 5050 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         9 | 5051 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         9 | 5052 | `					pGen->pIn++;` |
|         9 | 5053 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         9 | 5054 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         9 | 5055 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         9 | 5056 | `							iProtection = nKwrd;` |
|         9 | 5057 | `							pGen->pIn++;` |
|         3 | 5058 | `						}` |
|         3 | 5059 | `					}` |
|         9 | 5060 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         6 | 5061 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 5062 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 5063 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|       ! 0 | 5064 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 5065 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 5066 | `							return SXERR_ABORT;` |
|         - | 5067 | `						}` |
|       ! 0 | 5068 | `						goto done;` |
|         - | 5069 | `					}` |
|         9 | 5070 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         3 | 5071 | `				}` |
|    111943 | 5072 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 5073 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 5074 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|       ! 0 | 5075 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 5076 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 5077 | `						return SXERR_ABORT;` |
|         - | 5078 | `					}` |
|       ! 0 | 5079 | `					goto done;` |
|         - | 5080 | `				}` |
|    111943 | 5081 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       ! 0 | 5082 | `					pGen->pIn++;` |
|       ! 0 | 5083 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 | 5084 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 5085 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 5086 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 5087 | `							return SXERR_ABORT;` |
|         - | 5088 | `						}` |
|       ! 0 | 5089 | `						goto done;` |
|         - | 5090 | `					}` |
|       ! 0 | 5091 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 5092 | `				}else{` |
|    111943 | 5093 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 5094 | `				}` |
|    111943 | 5095 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 5096 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 5097 | `						return SXERR_ABORT;` |
|         - | 5098 | `					}` |
|       ! 0 | 5099 | `					goto done;` |
|         - | 5100 | `				}` |
|         - | 5101 | `			}` |
|     55974 | 5102 | `		}else{` |
|       ! 0 | 5103 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 5104 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 5105 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 5106 | `					return SXERR_ABORT;` |
|         - | 5107 | `				}` |
|       ! 0 | 5108 | `				goto done;` |
|         - | 5109 | `			}` |
|         - | 5110 | `		}` |
|         5 | 5111 | `	}` |
|         - | 5112 | ``	/* Apply the collected `use` entries (incl. adaptation blocks) through the`` |
|         - | 5113 | `	 * machinery shared with the class-body compiler. */` |
|      8425 | 5114 | `	rc = GenStateApplyTraitUses(&(*pGen),pClass,&aUseEntries);` |
|      8425 | 5115 | `	SySetRelease(&aUseEntries);` |
|      8425 | 5116 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 5117 | `		return SXERR_ABORT;` |
|         - | 5118 | `	}` |
|         - | 5119 | `	/* Reject a php-fatal redeclaration before hoisting the trait */` |
|      8425 | 5120 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|         3 | 5121 | `		return SXERR_ABORT;` |
|         - | 5122 | `	}` |
|         - | 5123 | `	/* Install the trait */` |
|      8423 | 5124 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      8423 | 5125 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 5126 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 5127 | `		return SXERR_ABORT;` |
|         - | 5128 | `	}` |
|      4209 | 5129 | `done:` |
|      8423 | 5130 | `	pGen->pCurClass = pSavedCurClass;` |
|         - | 5131 | `	/* Point beyond the trait body */` |
|      8423 | 5132 | `	pGen->pIn = &pEnd[1];` |
|      8423 | 5133 | `	pGen->pEnd = pTmp;` |
|      8423 | 5134 | `	return PH7_OK;` |
|      4217 | 5135 | `}` |
|         - | 5136 | `/*` |
|         - | 5137 | ` * Compile a user-defined class.` |
|         - | 5138 | ` *  According to the PHP language reference manual` |
|         - | 5139 | ` *   Basic class definitions begin with the keyword class, followed` |
|         - | 5140 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|         - | 5141 | ` *   the definitions of the properties and methods belonging to the class.` |
|         - | 5142 | ` *   A class may contain its own constants, variables (called "properties")` |
|         - | 5143 | ` *   and functions (called "methods").` |
|         - | 5144 | ` */` |
|    432648 | 5145 | `PH7_PRIVATE sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|         5 | 5146 | `{` |
|         - | 5147 | `	sxi32 rc;` |
|    432653 | 5148 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|    432653 | 5149 | `	return rc;` |
|         5 | 5150 | `}` |
|         - | 5151 | `/*` |
|         - | 5152 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|         - | 5153 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|         - | 5154 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|         - | 5155 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|         - | 5156 | `` * meaning; `enum Name` can never start a valid expression.`` |
|         - | 5157 | ` */` |
|  15271676 | 5158 | `PH7_PRIVATE int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|         5 | 5159 | `{` |
|  15506989 | 5160 | `	return (pIn->nType & PH7_TK_ID)` |
|   7871146 | 5161 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    245832 | 5162 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  15506984 | 5163 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|         5 | 5164 | `}` |
|         - | 5165 | `/*` |
|         - | 5166 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|         - | 5167 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|         - | 5168 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|         - | 5169 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|         - | 5170 | ` */` |
|      4190 | 5171 | `PH7_PRIVATE sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|         5 | 5172 | `{` |
|      4195 | 5173 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|         5 | 5174 | `}` |
|         - | 5175 |  |
