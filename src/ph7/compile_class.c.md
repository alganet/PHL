# src/ph7/compile_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2149/2824 lines (76.10%)

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
|        - |   10 | ` *    Class/OO compilation: classes, interfaces, traits, enums, anonymous` |
|        - |   11 | ` *    classes, class constants, typed properties, property hooks and methods.` |
|        - |   12 | ` * Status:` |
|        - |   13 | ` *    Stable.` |
|        - |   14 | ` */` |
|       10 |   15 | `static const char * GenStateClassKind(const ph7_class *pClass)` |
|        3 |   16 | `{` |
|       13 |   17 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){ return "interface"; }` |
|       11 |   18 | `	if( pClass->iFlags & PH7_CLASS_TRAIT ){ return "trait"; }` |
|        9 |   19 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){ return "enum"; }` |
|        6 |   20 | `	return "class";` |
|        8 |   21 | `}` |
|        - |   22 | `/*` |
|        - |   23 | ` * Guard a class/interface/trait/enum about to be installed. Returns SXERR_ABORT` |
|        - |   24 | ` * (after emitting the fatal) if it redeclares an already-bound type; otherwise` |
|        - |   25 | ` * marks it bound (when unconditional & top-level) and returns SXRET_OK.` |
|        - |   26 | ` */` |
|   532300 |   27 | `static sxi32 GenStateGuardClassRedeclaration(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |   28 | `{` |
|        - |   29 | `	SyHashEntry *pEntry;` |
|   532305 |   30 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|       26 |   31 | `		return SXRET_OK; /* conditional/nested: keep hoisting */` |
|        - |   32 | `	}` |
|   532283 |   33 | `	pClass->iFlags \|= PH7_CLASS_BOUND;` |
|   532283 |   34 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|   530133 |   35 | `		return SXRET_OK; /* the prelude installs each builtin exactly once */` |
|        - |   36 | `	}` |
|     2155 |   37 | `	pEntry = SyHashGet(&pGen->pVm->hClass,(const void *)pClass->sName.zString,pClass->sName.nByte);` |
|     2155 |   38 | `	if( pEntry ){` |
|       16 |   39 | `		ph7_class *pPrev = (ph7_class *)pEntry->pUserData;` |
|       18 |   40 | `		while( pPrev ){` |
|       16 |   41 | `			if( pPrev->iFlags & PH7_CLASS_BOUND ){` |
|        - |   42 | `				/* php names the entity by the PREVIOUS declaration's kind and omits` |
|        - |   43 | `				 * the "(previously declared in ...)" clause for internal symbols. */` |
|       13 |   44 | `				if( pPrev->sFile.nByte > 0 ){` |
|       15 |   45 | `					PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|        - |   46 | `						"Cannot redeclare %s %z (previously declared in %.*s:%u)",` |
|        4 |   47 | `						GenStateClassKind(pPrev),&pClass->sName,` |
|        4 |   48 | `						pPrev->sFile.nByte,pPrev->sFile.zString,pPrev->nLine);` |
|        7 |   49 | `				}else{` |
|        4 |   50 | `					PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|        1 |   51 | `						"Cannot redeclare %s %z",GenStateClassKind(pPrev),&pClass->sName);` |
|        - |   52 | `				}` |
|       13 |   53 | `				return SXERR_ABORT;` |
|        - |   54 | `			}` |
|        3 |   55 | `			pPrev = pPrev->pNextName;` |
|        1 |   56 | `		}` |
|        1 |   57 | `	}` |
|     2145 |   58 | `	return SXRET_OK;` |
|   266155 |   59 | `}` |
|        - |   60 | `/*` |
|        - |   61 | ` * Extract the visibility level associated with a given keyword.` |
|        - |   62 | ` * According to the PHP language reference manual` |
|        - |   63 | ` *  Visibility:` |
|        - |   64 | ` *  The visibility of a property or method can be defined by prefixing` |
|        - |   65 | ` *  the declaration with the keywords public, protected or private.` |
|        - |   66 | ` *  Class members declared public can be accessed everywhere.` |
|        - |   67 | ` *  Members declared protected can be accessed only within the class` |
|        - |   68 | ` *  itself and by inherited and parent classes. Members declared as private` |
|        - |   69 | ` *  may only be accessed by the class that defines the member.` |
|        - |   70 | ` */` |
|  3846640 |   71 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|        5 |   72 | `{` |
|  3846645 |   73 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|   280887 |   74 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  3565763 |   75 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   222293 |   76 | `		return PH7_CLASS_PROT_PROTECTED;` |
|        - |   77 | `	}` |
|        - |   78 | `	/* Assume public by default */` |
|  3343475 |   79 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  1923325 |   80 | `}` |
|        - |   81 | `/*` |
|        - |   82 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|        - |   83 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|        - |   84 | ` * token immediately followed by '='. Anything else with a leading type token` |
|        - |   85 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|        - |   86 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|        - |   87 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|        - |   88 | ` */` |
|   343232 |   89 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|        5 |   90 | `{` |
|        - |   91 | `	SyToken *p0, *p1;` |
|   343237 |   92 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |   93 | `		return 0;` |
|        - |   94 | `	}` |
|   343237 |   95 | `	p0 = pGen->pIn;` |
|        - |   96 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|   343237 |   97 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|      ! 0 |   98 | `		return 1;` |
|        - |   99 | `	}` |
|   343237 |  100 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|        5 |  101 | `		return 1;` |
|        - |  102 | `	}` |
|        - |  103 | `	/* A name-like first token begins a type only when followed by another` |
|        - |  104 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|        - |  105 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|   343233 |  106 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|   343233 |  107 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|   343233 |  108 | `		if( p1 ){` |
|   343233 |  109 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|       34 |  110 | `				return 1;` |
|        - |  111 | `			}` |
|   343203 |  112 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|        5 |  113 | `				return 1;` |
|        - |  114 | `			}` |
|   171597 |  115 | `		}` |
|   171597 |  116 | `	}` |
|   343199 |  117 | `	return 0;` |
|   171621 |  118 | `}` |
|        - |  119 | `/*` |
|        - |  120 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|        - |  121 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|        - |  122 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|        - |  123 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|        - |  124 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|        - |  125 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|        - |  126 | ` * Peek only; never consumes tokens.` |
|        - |  127 | ` */` |
|       24 |  128 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|        4 |  129 | `{` |
|       28 |  130 | `	SyToken *p = pGen->pIn;` |
|       39 |  131 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       20 |  132 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|        3 |  133 | `		p++; /* skip leading unary sign(s) */` |
|        1 |  134 | `	}` |
|       28 |  135 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|       23 |  136 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|        - |  137 | `	}` |
|        6 |  138 | `	p++;` |
|        - |  139 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|        6 |  140 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|       16 |  141 | `}` |
|        - |  142 | `/*` |
|        - |  143 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|        - |  144 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|        - |  145 | `` * `$o->new`), not a `new` expression.`` |
|        - |  146 | ` */` |
|      110 |  147 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|        3 |  148 | `{` |
|        - |  149 | `	sxi32 iOp;` |
|      113 |  150 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|       11 |  151 | `		return 0;` |
|        - |  152 | `	}` |
|      103 |  153 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|      103 |  154 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|       58 |  155 | `}` |
|        - |  156 | `/*` |
|        - |  157 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|        - |  158 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|        - |  159 | ` * interface-constant and (instance/static) property-default initializers` |
|        - |  160 | ` * ("New expressions are not supported in this context") while still allowing it` |
|        - |  161 | ` * in global constants, parameter defaults and static-local initializers (which` |
|        - |  162 | ` * are compiled by different functions and left untouched). The scan is` |
|        - |  163 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|        - |  164 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|        - |  165 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|        - |  166 | ` *` |
|        - |  167 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|        - |  168 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|        - |  169 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|        - |  170 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|        - |  171 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|        - |  172 | ` */` |
|   726012 |  173 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|        5 |  174 | `{` |
|   726017 |  175 | `	SyToken *p = pGen->pIn;` |
|   726017 |  176 | `	int iDepth = 0;` |
|  1917449 |  177 | `	while( p < pGen->pEnd ){` |
|  1917449 |  178 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   725965 |  179 | `			break; /* end of this initializer */` |
|        - |  180 | `		}` |
|  1191484 |  181 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   603563 |  182 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|    15631 |  183 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  184 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|        - |  185 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|        - |  186 | `			 * expression. */` |
|        6 |  187 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|        6 |  188 | `			p++;` |
|        6 |  189 | `			if( bArrow ){` |
|        - |  190 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|        - |  191 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|        3 |  192 | `				int iBase = iDepth;` |
|       17 |  193 | `				while( p < pGen->pEnd ){` |
|       17 |  194 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        5 |  195 | `						iDepth++;` |
|       15 |  196 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        5 |  197 | `						if( iDepth <= iBase ){` |
|      ! 0 |  198 | `							break; /* closes an enclosing group, not the fn's own */` |
|        - |  199 | `						}` |
|        5 |  200 | `						iDepth--;` |
|       11 |  201 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|        3 |  202 | `						break;` |
|        - |  203 | `					}` |
|       15 |  204 | `					p++;` |
|        1 |  205 | `				}` |
|        2 |  206 | `			}else{` |
|        - |  207 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|        - |  208 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|        - |  209 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|        - |  210 | `				 * then skip the balanced brace block. */` |
|        3 |  211 | `				int iLocal = 0;` |
|        7 |  212 | `				while( p < pGen->pEnd ){` |
|        7 |  213 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|        3 |  214 | `						break; /* body brace */` |
|        - |  215 | `					}` |
|        5 |  216 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        3 |  217 | `						iLocal++;` |
|        4 |  218 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        3 |  219 | `						if( iLocal > 0 ){` |
|        3 |  220 | `							iLocal--;` |
|        1 |  221 | `						}` |
|        1 |  222 | `					}` |
|        5 |  223 | `					p++;` |
|        1 |  224 | `				}` |
|        3 |  225 | `				if( p < pGen->pEnd ){` |
|        3 |  226 | `					int iBrace = 0; /* p is on the body '{' */` |
|       17 |  227 | `					while( p < pGen->pEnd ){` |
|       17 |  228 | `						if( p->nType & PH7_TK_OCB ){` |
|        3 |  229 | `							iBrace++;` |
|       16 |  230 | `						}else if( p->nType & PH7_TK_CCB ){` |
|        3 |  231 | `							iBrace--;` |
|        3 |  232 | `							if( iBrace == 0 ){` |
|        3 |  233 | `								p++;` |
|        3 |  234 | `								break;` |
|        - |  235 | `							}` |
|      ! 0 |  236 | `						}` |
|       15 |  237 | `						p++;` |
|        1 |  238 | `					}` |
|        1 |  239 | `				}` |
|        - |  240 | `			}` |
|        6 |  241 | `			continue;` |
|        - |  242 | `		}` |
|  1191485 |  243 | `		if( p->nType & PH7_TK_OCB ){` |
|       45 |  244 | `			if( iDepth == 0 ){` |
|        - |  245 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|        - |  246 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|        - |  247 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|        - |  248 | `				 * is legal — don't scan into it. */` |
|       45 |  249 | `				break;` |
|        - |  250 | `			}` |
|      ! 0 |  251 | `			iDepth++;` |
|  1191441 |  252 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|    50779 |  253 | `			iDepth++;` |
|  1166054 |  254 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|    50777 |  255 | `			if( iDepth > 0 ){` |
|    50777 |  256 | `				iDepth--;` |
|    25386 |  257 | `			}` |
|  1115281 |  258 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|   392799 |  259 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|        - |  260 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|        - |  261 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|        - |  262 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|       11 |  263 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|       11 |  264 | `				return 1;` |
|        - |  265 | `			}` |
|      ! 0 |  266 | `		}` |
|  1191433 |  267 | `		p++;` |
|        5 |  268 | `	}` |
|   726009 |  269 | `	return 0;` |
|   363011 |  270 | `}` |
|        - |  271 | `/*` |
|        - |  272 | ` * Return TRUE if the constant expression starting at the current token performs a` |
|        - |  273 | ` * FUNCTION CALL, which php rejects with "Constant expression contains invalid` |
|        - |  274 | `` * operations" in every constant-expression context (global `const`, class/interface`` |
|        - |  275 | ` * constants, property defaults, parameter defaults, attribute arguments).` |
|        - |  276 | ` *` |
|        - |  277 | ` * Shares GenStateInitHasNewExpr's walk: depth-aware so a nested call is caught` |
|        - |  278 | `` * (`[1, f()]`) and an inner comma does not end the scan, and skipping any`` |
|        - |  279 | `` * `function`/`fn` construct outright — a call inside a closure body runs when the`` |
|        - |  280 | ` * closure is invoked, so php allows it.` |
|        - |  281 | ` *` |
|        - |  282 | ` * Deliberately NOT rejected, because php accepts them:` |
|        - |  283 | `` *   - first-class callables, `strlen(...)` — the parens hold only the ellipsis;`` |
|        - |  284 | ``  *   - `new X(...)` — constructor calls are legal in the contexts that allow `new` `` |
|        - |  285 | ` *     at all, and GenStateInitHasNewExpr owns the contexts that do not;` |
|        - |  286 | ` *   - anything inside a ternary. php FOLDS a constant condition and only rejects a` |
|        - |  287 | `` *     call that survives, so `true ? 1 : f()` is legal while `false ? 1 : f()` is`` |
|        - |  288 | ` *     not. PHL does not constant-fold here, so rather than risk rejecting valid` |
|        - |  289 | `` *     code this scan skips an initializer containing a depth-0 `?` entirely. The`` |
|        - |  290 | ` *     residual is a call hiding in a TAKEN ternary branch, which stays accepted.` |
|        - |  291 | ` */` |
|   726062 |  292 | `PH7_PRIVATE int PH7_GenStateInitHasCallExpr(ph7_gen_state *pGen)` |
|        5 |  293 | `{` |
|   726067 |  294 | `	SyToken *p = pGen->pIn;` |
|   726067 |  295 | `	int iDepth = 0;` |
|        - |  296 | `	/* Conservative ternary bail-out (see the note above). */` |
|        - |  297 | `	{` |
|   726067 |  298 | `		SyToken *q = pGen->pIn;` |
|   726067 |  299 | `		int iQd = 0;` |
|  1918797 |  300 | `		while( q < pGen->pEnd ){` |
|  1918759 |  301 | `			if( iQd == 0 && (q->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   726023 |  302 | `				break;` |
|        - |  303 | `			}` |
|  1192741 |  304 | `			if( q->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    50915 |  305 | `				iQd++;` |
|  1167286 |  306 | `			}else if( q->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|    50915 |  307 | `				if( iQd > 0 ){ iQd--; }` |
|  1116376 |  308 | `			}else if( (q->nType & PH7_TK_OP) && q->pUserData` |
|   393025 |  309 | `				&& ((const ph7_expr_op *)q->pUserData)->iOp == EXPR_OP_QUESTY ){` |
|        8 |  310 | `				return 0;` |
|        - |  311 | `			}` |
|  1192735 |  312 | `			q++;` |
|        5 |  313 | `		}` |
|        - |  314 | `	}` |
|  1917643 |  315 | `	while( p < pGen->pEnd ){` |
|  1917643 |  316 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   726017 |  317 | `			break; /* end of this initializer */` |
|        - |  318 | `		}` |
|  1191626 |  319 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   603634 |  320 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|    15631 |  321 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  322 | `			/* A call inside a closure/arrow-fn is deferred to call time: skip the` |
|        - |  323 | `			 * whole construct. Delegating to the sibling scanner is not possible` |
|        - |  324 | ``			 * (it reports `new`), so mirror its bracket walk. */`` |
|        6 |  325 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|        6 |  326 | `			int iBase = iDepth;` |
|        6 |  327 | `			p++;` |
|        6 |  328 | `			if( bArrow ){` |
|       17 |  329 | `				while( p < pGen->pEnd ){` |
|       17 |  330 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        5 |  331 | `						iDepth++;` |
|       15 |  332 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        5 |  333 | `						if( iDepth <= iBase ){` |
|      ! 0 |  334 | `							break;` |
|        - |  335 | `						}` |
|        5 |  336 | `						iDepth--;` |
|       11 |  337 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|        3 |  338 | `						break;` |
|        - |  339 | `					}` |
|       15 |  340 | `					p++;` |
|        1 |  341 | `				}` |
|        2 |  342 | `			}else{` |
|        3 |  343 | `				int iLocal = 0;` |
|        7 |  344 | `				while( p < pGen->pEnd ){` |
|        7 |  345 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|        3 |  346 | `						break;` |
|        - |  347 | `					}` |
|        5 |  348 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        3 |  349 | `						iLocal++;` |
|        4 |  350 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        3 |  351 | `						if( iLocal > 0 ){ iLocal--; }` |
|        1 |  352 | `					}` |
|        5 |  353 | `					p++;` |
|        1 |  354 | `				}` |
|        3 |  355 | `				if( p < pGen->pEnd ){` |
|        3 |  356 | `					int iBrace = 0;` |
|       17 |  357 | `					while( p < pGen->pEnd ){` |
|       17 |  358 | `						if( p->nType & PH7_TK_OCB ){` |
|        3 |  359 | `							iBrace++;` |
|       16 |  360 | `						}else if( p->nType & PH7_TK_CCB ){` |
|        3 |  361 | `							iBrace--;` |
|        3 |  362 | `							if( iBrace == 0 ){` |
|        3 |  363 | `								p++;` |
|        3 |  364 | `								break;` |
|        - |  365 | `							}` |
|      ! 0 |  366 | `						}` |
|       15 |  367 | `						p++;` |
|        1 |  368 | `					}` |
|        1 |  369 | `				}` |
|        - |  370 | `			}` |
|        6 |  371 | `			continue;` |
|        - |  372 | `		}` |
|  1191627 |  373 | `		if( p->nType & PH7_TK_OCB ){` |
|       43 |  374 | `			if( iDepth == 0 ){` |
|       43 |  375 | `				break; /* property-hook list: the default expression ends here */` |
|        - |  376 | `			}` |
|      ! 0 |  377 | `			iDepth++;` |
|  1191585 |  378 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|        - |  379 | `			/* A '(' directly after a NAME is a call. A name here is an identifier` |
|        - |  380 | ``			 * token; `new X(` is excluded by looking for the `new` operator, and`` |
|        - |  381 | ``			 * `f(...)` (first-class callable) by peeking for a lone ellipsis. */`` |
|    50794 |  382 | `			if( (p->nType & PH7_TK_LPAREN) && p > pGen->pIn` |
|    15622 |  383 | `				&& (p[-1].nType & PH7_TK_ID)` |
|     7823 |  384 | `				&& !((p[-1].nType & PH7_TK_OP) && p[-1].pUserData` |
|      ! 0 |  385 | `					&& ((const ph7_expr_op *)p[-1].pUserData)->iOp == EXPR_OP_NEW) ){` |
|       18 |  386 | `				int bNewCtor = 0;` |
|       18 |  387 | `				SyToken *q = &p[-1];` |
|        - |  388 | ``				/* Walk back over a qualified name (A\B, A::b, $o->m) to a `new`. */`` |
|       18 |  389 | `				while( q > pGen->pIn && (q[-1].nType & (PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) ){` |
|       10 |  390 | `					if( (q[-1].nType & PH7_TK_OP) && q[-1].pUserData` |
|       14 |  391 | `						&& ((const ph7_expr_op *)q[-1].pUserData)->iOp == EXPR_OP_NEW ){` |
|       14 |  392 | `						bNewCtor = 1;` |
|       14 |  393 | `						break;` |
|        - |  394 | `					}` |
|      ! 0 |  395 | `					if( !GenStateTokenIsMemberOp(&q[-1]) && (q[-1].nType & PH7_TK_NSSEP) == 0 ){` |
|      ! 0 |  396 | `						break;` |
|        - |  397 | `					}` |
|      ! 0 |  398 | `					q--;` |
|      ! 0 |  399 | `				}` |
|       14 |  400 | `				if( !bNewCtor` |
|       13 |  401 | `					&& !(&p[1] < pGen->pEnd && (p[1].nType & PH7_TK_ELLIPSIS)) ){` |
|        3 |  402 | `					return 1;` |
|        - |  403 | `				}` |
|        6 |  404 | `			}` |
|    50797 |  405 | `			iDepth++;` |
|  1166187 |  406 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|    50797 |  407 | `			if( iDepth > 0 ){` |
|    50797 |  408 | `				iDepth--;` |
|    25396 |  409 | `			}` |
|    25396 |  410 | `		}` |
|  1191583 |  411 | `		p++;` |
|        5 |  412 | `	}` |
|   726059 |  413 | `	return 0;` |
|   363036 |  414 | `}` |
|        - |  415 | `/*` |
|        - |  416 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|        - |  417 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|        - |  418 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|        - |  419 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|        - |  420 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|        - |  421 | ` * share the same backing.` |
|        - |  422 | ` */` |
|    15986 |  423 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|        - |  424 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|        5 |  425 | `{` |
|    15991 |  426 | `	pAttr->nType = nType;` |
|    15991 |  427 | `	pAttr->sClass = *pClass;` |
|    15991 |  428 | `	pAttr->sTypeName = *pTypeName;` |
|    15991 |  429 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  430 | `		sxu32 i;` |
|       72 |  431 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|       50 |  432 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|       50 |  433 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|       27 |  434 | `		}` |
|       11 |  435 | `	}` |
|    15991 |  436 | `}` |
|   343232 |  437 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  438 | `{` |
|   343237 |  439 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  440 | `	SySet *pInstrContainer;` |
|        - |  441 | `	ph7_class_attr *pCons;` |
|        - |  442 | `	SyString *pName;` |
|        - |  443 | `	sxi32 rc;` |
|   343237 |  444 | `	sxu32 nType = 0;` |
|        - |  445 | `	SyString sTypeClass;` |
|        - |  446 | `	SyString sTypeText;` |
|        - |  447 | `	SySet aUnionAlts;` |
|   343237 |  448 | `	sxi32 iTypeFlags = 0;` |
|   343237 |  449 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   343237 |  450 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   343237 |  451 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  452 | `	/* Extract visibility level */` |
|   343237 |  453 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  454 | `	/* Mark as constant */` |
|   343237 |  455 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|   343237 |  456 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        - |  457 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|        - |  458 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|   343256 |  459 | `	if( GenStateClassConstHasType(pGen) ){` |
|       61 |  460 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|       38 |  461 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|        - |  462 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|        - |  463 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|        - |  464 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|        - |  465 | `		 * and success paths release. */` |
|       42 |  466 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  467 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  468 | `			goto Synchronize;` |
|       42 |  469 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  470 | `			return SXERR_ABORT;` |
|       42 |  471 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 |  472 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  473 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|      ! 0 |  474 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  475 | `				return SXERR_ABORT;` |
|        - |  476 | `			}` |
|      ! 0 |  477 | `			goto Synchronize;` |
|        - |  478 | `		}` |
|       42 |  479 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       19 |  480 | `	}` |
|   171616 |  481 | `loop:` |
|   343239 |  482 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - |  483 | `		/* Invalid constant name */` |
|      ! 0 |  484 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|      ! 0 |  485 | `		if( rc == SXERR_ABORT ){` |
|        - |  486 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  487 | `			return SXERR_ABORT;` |
|        - |  488 | `		}` |
|      ! 0 |  489 | `		goto Synchronize;` |
|        - |  490 | `	}` |
|        - |  491 | `	/* Peek constant name */` |
|   343239 |  492 | `	pName = &pGen->pIn->sData;` |
|        - |  493 | `	/* Make sure the constant name isn't reserved */` |
|   343239 |  494 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  495 | `		/* Reserved constant name */` |
|      ! 0 |  496 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|      ! 0 |  497 | `		if( rc == SXERR_ABORT ){` |
|        - |  498 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  499 | `			return SXERR_ABORT;` |
|        - |  500 | `		}` |
|      ! 0 |  501 | `		goto Synchronize;` |
|        - |  502 | `	}` |
|        - |  503 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|   343239 |  504 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       61 |  505 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|       38 |  506 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       19 |  507 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|       42 |  508 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  509 | `			return SXERR_ABORT;` |
|       42 |  510 | `		}else if( rc != SXRET_OK ){` |
|        3 |  511 | `			goto Synchronize;` |
|        - |  512 | `		}` |
|       18 |  513 | `	}` |
|        - |  514 | `	/* Advance the stream cursor */` |
|   343237 |  515 | `	pGen->pIn++;` |
|   343237 |  516 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  517 | `		/* Invalid declaration */` |
|      ! 0 |  518 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|      ! 0 |  519 | `		if( rc == SXERR_ABORT ){` |
|        - |  520 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  521 | `			return SXERR_ABORT;` |
|        - |  522 | `		}` |
|      ! 0 |  523 | `		goto Synchronize;` |
|        - |  524 | `	}` |
|   343237 |  525 | `	pGen->pIn++; /* Jump the equal sign */` |
|        - |  526 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|        - |  527 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|        - |  528 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|        - |  529 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|   343232 |  530 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|       39 |  531 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|        8 |  532 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  533 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|        2 |  534 | `			&pClass->sName,pName,&sTypeText);` |
|        6 |  535 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  536 | `			return SXERR_ABORT;` |
|        - |  537 | `		}` |
|        6 |  538 | `		goto Synchronize;` |
|        - |  539 | `	}` |
|        - |  540 | `	/* php: a constant expression may not CALL anything. Same rule as the global` |
|        - |  541 | ``	 * `const` path in compile_stmt.c. */`` |
|   343233 |  542 | `	if( PH7_GenStateInitHasCallExpr(pGen) ){` |
|      ! 0 |  543 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  544 | `			"Constant expression contains invalid operations");` |
|      ! 0 |  545 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  546 | `			return SXERR_ABORT;` |
|        - |  547 | `		}` |
|      ! 0 |  548 | `		goto Synchronize;` |
|        - |  549 | `	}` |
|        - |  550 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|        - |  551 | `	 * constant initializer ("New expressions are not supported in this context").` |
|        - |  552 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|   343233 |  553 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|        6 |  554 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  555 | `			"New expressions are not supported in this context");` |
|        6 |  556 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  557 | `			return SXERR_ABORT;` |
|        - |  558 | `		}` |
|        6 |  559 | `		goto Synchronize;` |
|        - |  560 | `	}` |
|        - |  561 | `	/* php: a class constant may not be redefined in the same class body. The` |
|        - |  562 | `	 * property path already guarded this; the constant path did not, so` |
|        - |  563 | ``	 * `class C{const X=1; const X=2;}` silently kept one of them. */`` |
|   343229 |  564 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|      ! 0 |  565 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  566 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|      ! 0 |  567 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  568 | `			return SXERR_ABORT;` |
|        - |  569 | `		}` |
|      ! 0 |  570 | `		goto Synchronize;` |
|        - |  571 | `	}` |
|        - |  572 | `	/* Allocate a new class attribute */` |
|   343229 |  573 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   343229 |  574 | `	if( pCons ){` |
|   343229 |  575 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|   343229 |  576 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  577 | `			return SXERR_ABORT;` |
|        - |  578 | `		}` |
|   171612 |  579 | `	}` |
|   343229 |  580 | `	if( pCons == 0 ){` |
|      ! 0 |  581 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  582 | `		return SXERR_ABORT;` |
|        - |  583 | `	}` |
|   343229 |  584 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       35 |  585 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       16 |  586 | `	}` |
|        - |  587 | `	/* Swap bytecode container */` |
|   343229 |  588 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   343229 |  589 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|        - |  590 | `	/* Compile constant value.` |
|        - |  591 | `	 */` |
|   343229 |  592 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   343229 |  593 | `	if( rc == SXERR_EMPTY ){` |
|        3 |  594 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|        3 |  595 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  596 | `			return SXERR_ABORT;` |
|        - |  597 | `		}` |
|        1 |  598 | `	}` |
|        - |  599 | `	/* Emit the done instruction */` |
|   343229 |  600 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   343229 |  601 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   343229 |  602 | `	if( rc == SXERR_ABORT ){` |
|        - |  603 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  604 | `		return SXERR_ABORT;` |
|        - |  605 | `	}` |
|        - |  606 | `	/* All done,install the constant */` |
|   343229 |  607 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|   343229 |  608 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  609 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  610 | `		return SXERR_ABORT;` |
|        - |  611 | `	}` |
|   343229 |  612 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  613 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|        3 |  614 | `		pGen->pIn++; /* Jump the comma */` |
|        3 |  615 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 |  616 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 |  617 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 |  618 | `				pTok--;` |
|      ! 0 |  619 | `			}` |
|      ! 0 |  620 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  621 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|      ! 0 |  622 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 |  623 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  624 | `				return SXERR_ABORT;` |
|        - |  625 | `			}` |
|      ! 0 |  626 | `		}else{` |
|        3 |  627 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|        3 |  628 | `				goto loop;` |
|        - |  629 | `			}` |
|        - |  630 | `		}` |
|      ! 0 |  631 | `	}` |
|   343227 |  632 | `	SySetRelease(&aUnionAlts);` |
|   343227 |  633 | `	return SXRET_OK;` |
|        5 |  634 | `Synchronize:` |
|       13 |  635 | `	SySetRelease(&aUnionAlts);` |
|        - |  636 | `	/* Synchronize with the first semi-colon */` |
|       45 |  637 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       35 |  638 | `		pGen->pIn++;` |
|        3 |  639 | `	}` |
|       13 |  640 | `	return SXERR_CORRUPT;` |
|   171621 |  641 | `}` |
|        - |  642 | `/*` |
|        - |  643 | ` * complie a class attribute or Properties in the PHP jargon.` |
|        - |  644 | ` * According to the PHP language reference manual` |
|        - |  645 | ` *  Properties` |
|        - |  646 | ` *  Class member variables are called "properties". You may also see them referred` |
|        - |  647 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|        - |  648 | ` *  of this reference we will use "properties". They are defined by using one` |
|        - |  649 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|        - |  650 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|        - |  651 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|        - |  652 | ` *  and must not depend on run-time information in order to be evaluated.` |
|        - |  653 | ` * Symisc eXtension.` |
|        - |  654 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|        - |  655 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  656 | ` *  Example:` |
|        - |  657 | ` *   class Test{` |
|        - |  658 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - |  659 | ` *   };` |
|        - |  660 | ` *   var_dump(TEST::myVar);` |
|        - |  661 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - |  662 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - |  663 | ` */` |
|        - |  664 | `/*` |
|        - |  665 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|        - |  666 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|        - |  667 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|        - |  668 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|        - |  669 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|        - |  670 | ` */` |
|  2633136 |  671 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|        5 |  672 | `{` |
|  2633141 |  673 | `	SyToken *p = pStart;` |
|  2633141 |  674 | `	int bFirst = 1;` |
|  2633141 |  675 | `	if( p >= pEnd ) return 0;` |
|        - |  676 | ``	/* Optional nullable `?` shorthand. */`` |
|  2633141 |  677 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|       41 |  678 | `		p++;` |
|       41 |  679 | `		if( p >= pEnd ) return 0;` |
|       19 |  680 | `	}` |
|        - |  681 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|        - |  682 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|        - |  683 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|        - |  684 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|  1316568 |  685 | `	for(;;){` |
|  2633161 |  686 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|        - |  687 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|        3 |  688 | `			p++;` |
|        9 |  689 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|        3 |  690 | `			if( p >= pEnd ) return 0;` |
|        3 |  691 | `			p++; /* skip ')' */` |
|        2 |  692 | `		}else{` |
|        - |  693 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|        - |  694 | ``			 * then any `&`-joined intersection members. */`` |
|  2633159 |  695 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  2633159 |  696 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  697 | `				return 0;` |
|        - |  698 | `			}` |
|        - |  699 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|        - |  700 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|        - |  701 | `			 * may still appear at the initial dispatch site). */` |
|  2633159 |  702 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  2633103 |  703 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  2633098 |  704 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|   125238 |  705 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  2617193 |  706 | `					return 0;` |
|        - |  707 | `				}` |
|     7955 |  708 | `			}` |
|    15971 |  709 | `			p++;` |
|    15973 |  710 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  711 | `				p += 2;` |
|        1 |  712 | `			}` |
|    23952 |  713 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|    15974 |  714 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  715 | `				p++; /* skip '&' */` |
|        3 |  716 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|        3 |  717 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|        3 |  718 | `				p++;` |
|        3 |  719 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      ! 0 |  720 | `					p += 2;` |
|      ! 0 |  721 | `				}` |
|        1 |  722 | `			}` |
|        - |  723 | `		}` |
|    15973 |  724 | `		bFirst = 0;` |
|    15968 |  725 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       25 |  726 | `			&& p->sData.zString[0] == '\|' ){` |
|       24 |  727 | ``			p++; /* next `\|`-separated part */`` |
|       24 |  728 | `			continue;` |
|        - |  729 | `		}` |
|    15953 |  730 | `		break;` |
|      ! 0 |  731 | `	}` |
|    15953 |  732 | `	if( p >= pEnd ) return 0;` |
|    15953 |  733 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|  1316573 |  734 | `}` |
|        - |  735 |  |
|        - |  736 | `/*` |
|        - |  737 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|        - |  738 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|        - |  739 | ` * if not). Recognized forms:` |
|        - |  740 | ` *   ?Type, array, bool, int, float, string, object,` |
|        - |  741 | ` *   self, parent, \Ns\ClassName, ClassName` |
|        - |  742 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|        - |  743 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|        - |  744 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|        - |  745 | ` * on unrecoverable error.` |
|        - |  746 | ` *` |
|        - |  747 | ` * When a type is parsed:` |
|        - |  748 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|        - |  749 | ` *   *pClass is set to the class name (for class types)` |
|        - |  750 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|        - |  751 | ` *   *pTypeText is set to the original text span of the type` |
|        - |  752 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|        - |  753 | ` */` |
|    15958 |  754 | `static sxi32 GenStateParsePropertyType(` |
|        - |  755 | `	ph7_gen_state *pGen,` |
|        - |  756 | `	sxu32 *pnType,` |
|        - |  757 | `	SyString *pClass,` |
|        - |  758 | `	sxi32 *piTypeFlags,` |
|        - |  759 | `	SyString *pTypeText,` |
|        - |  760 | `	SySet *pAlts` |
|        5 |  761 | `){` |
|    15963 |  762 | `	sxi32 iFlags = 0;` |
|        - |  763 | `	sxi32 rc;` |
|    15963 |  764 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  765 | `		return SXRET_OK;` |
|        - |  766 | `	}` |
|        - |  767 | `	/* If the first token is '$', there's no type */` |
|    15963 |  768 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      ! 0 |  769 | `		return SXRET_OK;` |
|        - |  770 | `	}` |
|    15963 |  771 | `	rc = GenStateParseUnionTypeDecl(` |
|     7979 |  772 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|        - |  773 | `		PH7_CLASS_ATTR_NULLABLE,` |
|        - |  774 | `		PH7_CLASS_ATTR_UNION,` |
|        - |  775 | `		/* bAllowVoid */ 0,` |
|    15958 |  776 | `		pGen->pIn->nLine);` |
|    15963 |  777 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  778 | `		return rc;` |
|        - |  779 | `	}` |
|        - |  780 | `	/* Verify next token is '$' (start of property name) */` |
|    15963 |  781 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  782 | `		return SXERR_SYNTAX;` |
|        - |  783 | `	}` |
|    15963 |  784 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|    15963 |  785 | `	return SXRET_OK;` |
|     7984 |  786 | `}` |
|        - |  787 |  |
|        - |  788 | `/*` |
|        - |  789 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|        - |  790 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|        - |  791 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|        - |  792 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|        - |  793 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|        - |  794 | ` * by the type parser itself before reaching here.` |
|        - |  795 | ` *` |
|        - |  796 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|        - |  797 | ` * use in the error message.` |
|        - |  798 | ` */` |
|    16136 |  799 | `static int GenStateIsDisallowedPropertyAtom(` |
|        - |  800 | `	sxu32 nType,` |
|        - |  801 | `	const SyString *pClass,` |
|        - |  802 | `	const char **pzName,` |
|        - |  803 | `	sxu32 *pnName)` |
|        5 |  804 | `{` |
|        - |  805 | `	const char *z;` |
|        - |  806 | `	sxu32 n;` |
|    16141 |  807 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|    16075 |  808 | `		return 0;` |
|        - |  809 | `	}` |
|       70 |  810 | `	z = pClass->zString;` |
|       70 |  811 | `	n = pClass->nByte;` |
|       70 |  812 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|        8 |  813 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|        - |  814 | `	}` |
|        - |  815 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|        - |  816 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|        - |  817 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|       64 |  818 | `	return 0;` |
|     8073 |  819 | `}` |
|        - |  820 |  |
|        - |  821 | `/*` |
|        - |  822 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|        - |  823 | ` * constant) — the main atom plus any union alternatives — against the` |
|        - |  824 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|        - |  825 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|        - |  826 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|        - |  827 | ` * type T" vs "Class constant C::X cannot have type T").` |
|        - |  828 | ` *` |
|        - |  829 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|        - |  830 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|        - |  831 | ` */` |
|    16074 |  832 | `PH7_PRIVATE sxi32 GenStateValidateMemberType(` |
|        - |  833 | `	ph7_gen_state *pGen,` |
|        - |  834 | `	ph7_class *pClass,` |
|        - |  835 | `	const SyString *pMemberName,` |
|        - |  836 | `	sxu32 nType,` |
|        - |  837 | `	const SyString *pTypeClass,` |
|        - |  838 | `	const SyString *pTypeText,` |
|        - |  839 | `	SySet *pUnionAlts,` |
|        - |  840 | `	const char *zErrFmt,` |
|        - |  841 | `	sxu32 nLine)` |
|        5 |  842 | `{` |
|    16079 |  843 | `	const char *zBad = 0;` |
|    16079 |  844 | `	sxu32 nBad = 0;` |
|        - |  845 | `	SyString sFallback;` |
|        - |  846 | `	const SyString *pBad;` |
|        - |  847 | `	sxi32 rc;` |
|    16079 |  848 | `	int bDisallowed = 0;` |
|    16079 |  849 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|        5 |  850 | `		bDisallowed = 1;` |
|    16077 |  851 | `	}else if( pUnionAlts ){` |
|        - |  852 | `		sxu32 i;` |
|       94 |  853 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|       66 |  854 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|       66 |  855 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|        3 |  856 | `				bDisallowed = 1;` |
|        3 |  857 | `				break;` |
|        - |  858 | `			}` |
|       34 |  859 | `		}` |
|       15 |  860 | `	}` |
|    16079 |  861 | `	if( !bDisallowed ){` |
|    16073 |  862 | `		return SXRET_OK;` |
|        - |  863 | `	}` |
|        - |  864 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|        - |  865 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|        - |  866 | `	 * canonical spelling if the type text is unavailable. */` |
|        8 |  867 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|        8 |  868 | `		pBad = pTypeText;` |
|        5 |  869 | `	}else{` |
|      ! 0 |  870 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|      ! 0 |  871 | `		pBad = &sFallback;` |
|        - |  872 | `	}` |
|       11 |  873 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        3 |  874 | `		zErrFmt,` |
|        3 |  875 | `		&pClass->sName,pMemberName,pBad);` |
|        8 |  876 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  877 | `		return SXERR_ABORT;` |
|        - |  878 | `	}` |
|        8 |  879 | `	return SXERR_SYNTAX;` |
|     8042 |  880 | `}` |
|        - |  881 | `/*` |
|        - |  882 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|        - |  883 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|        - |  884 | ` * matched as a plain identifier in the class-member modifier position rather` |
|        - |  885 | ` * than promoted to a lexer keyword.` |
|        - |  886 | ` */` |
| 23631546 |  887 | `PH7_PRIVATE int GenStateIsReadonly(SyToken *pTok)` |
|        5 |  888 | `{` |
| 23858490 |  889 | `	return (pTok->nType & PH7_TK_ID)` |
| 12042712 |  890 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 23858485 |  891 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|        5 |  892 | `}` |
|        - |  893 | `/*` |
|        - |  894 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|        - |  895 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|        - |  896 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|        - |  897 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|        - |  898 | ` */` |
|  8485884 |  899 | `PH7_PRIVATE sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|        5 |  900 | `{` |
|  8485889 |  901 | `	*pnTok = 0;` |
|  8485884 |  902 | `	if( &pTok[3] < pEnd` |
|  7933465 |  903 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|  6470078 |  904 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|  2779563 |  905 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       16 |  906 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|       16 |  907 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|       21 |  908 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|       17 |  909 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|       17 |  910 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|       17 |  911 | `			*pnTok = 4;` |
|       17 |  912 | `			return nKw;` |
|        - |  913 | `		}` |
|      ! 0 |  914 | `	}` |
|  8485873 |  915 | `	return 0;` |
|  4242947 |  916 | `}` |
|        - |  917 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|       16 |  918 | `PH7_PRIVATE sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|        1 |  919 | `{` |
|       17 |  920 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|       13 |  921 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|        - |  922 | `	}` |
|        5 |  923 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|        3 |  924 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|        - |  925 | `	}` |
|        3 |  926 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|        9 |  927 | `}` |
|   593610 |  928 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  929 | `{` |
|   593615 |  930 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  931 | `	ph7_class_attr *pAttr;` |
|        - |  932 | `	SyString *pName;` |
|        - |  933 | `	sxi32 rc;` |
|   593615 |  934 | `	sxu32 nType = 0;` |
|        - |  935 | `	SyString sTypeClass;` |
|        - |  936 | `	SyString sTypeText;` |
|        - |  937 | `	SySet aUnionAlts;` |
|   593615 |  938 | `	sxi32 iTypeFlags = 0;` |
|   593615 |  939 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   593615 |  940 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   593615 |  941 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  942 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|        - |  943 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|        - |  944 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   593615 |  945 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|       21 |  946 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        9 |  947 | `	}` |
|        - |  948 | `	/* Extract visibility level */` |
|   593615 |  949 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  950 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   601594 |  951 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|    15963 |  952 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|    15963 |  953 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  954 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  955 | `			goto Synchronize;` |
|    15963 |  956 | `		}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  957 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  958 | `				"Invalid property type or declaration near '%z'",` |
|      ! 0 |  959 | `				&pGen->pIn->sData);` |
|      ! 0 |  960 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  961 | `				return SXERR_ABORT;` |
|        - |  962 | `			}` |
|      ! 0 |  963 | `			goto Synchronize;` |
|    15963 |  964 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  965 | `			return SXERR_ABORT;` |
|        - |  966 | `		}` |
|     7979 |  967 | `	}` |
|      ! 0 |  968 | `loop:` |
|   593619 |  969 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  970 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|      ! 0 |  971 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  972 | `			return SXERR_ABORT;` |
|        - |  973 | `		}` |
|      ! 0 |  974 | `		goto Synchronize;` |
|        - |  975 | `	}` |
|   593619 |  976 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   593619 |  977 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        - |  978 | `		/* Invalid attribute name */` |
|      ! 0 |  979 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|      ! 0 |  980 | `		if( rc == SXERR_ABORT ){` |
|        - |  981 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  982 | `			return SXERR_ABORT;` |
|        - |  983 | `		}` |
|      ! 0 |  984 | `		goto Synchronize;` |
|        - |  985 | `	}` |
|        - |  986 | `	/* Peek attribute name */` |
|   593619 |  987 | `	pName = &pGen->pIn->sData;` |
|        - |  988 | `	/* Advance the stream cursor */` |
|   593619 |  989 | `	pGen->pIn++;` |
|   593619 |  990 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|        - |  991 | `		/* Invalid declaration */` |
|        3 |  992 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|        3 |  993 | `		if( rc == SXERR_ABORT ){` |
|        - |  994 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  995 | `			return SXERR_ABORT;` |
|        - |  996 | `		}` |
|        3 |  997 | `		goto Synchronize;` |
|        - |  998 | `	}` |
|        - |  999 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|        - | 1000 | `	 * the read visibility must not be narrower than the set visibility. */` |
|   593617 | 1001 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|       13 | 1002 | `		const char *zAvErr = 0;` |
|       19 | 1003 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|       10 | 1004 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|        2 | 1005 | `			: PH7_CLASS_PROT_PUBLIC;` |
|       13 | 1006 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 | 1007 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|       13 | 1008 | `		}else if( iProtection > iSetLevel ){` |
|      ! 0 | 1009 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|      ! 0 | 1010 | `		}` |
|       13 | 1011 | `		if( zAvErr ){` |
|      ! 0 | 1012 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|      ! 0 | 1013 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1014 | `				return SXERR_ABORT;` |
|        - | 1015 | `			}` |
|      ! 0 | 1016 | `			goto Synchronize;` |
|        - | 1017 | `		}` |
|        6 | 1018 | `	}` |
|        - | 1019 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|        - | 1020 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   593617 | 1021 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       51 | 1022 | `		const char *zRoErr = 0;` |
|       51 | 1023 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        3 | 1024 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|       50 | 1025 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        6 | 1026 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|       47 | 1027 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|        6 | 1028 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|        2 | 1029 | `		}` |
|       51 | 1030 | `		if( zRoErr ){` |
|       13 | 1031 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|       13 | 1032 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1033 | `				return SXERR_ABORT;` |
|        - | 1034 | `			}` |
|       13 | 1035 | `			goto Synchronize;` |
|        - | 1036 | `		}` |
|       18 | 1037 | `	}` |
|        - | 1038 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|        - | 1039 | `	 * type atom or any union alternative. void/never are already rejected` |
|        - | 1040 | `	 * by the type parser. */` |
|   593607 | 1041 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|    23939 | 1042 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|        - | 1043 | `			&sTypeText,` |
|    15956 | 1044 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|     7978 | 1045 | `			"Property %z::$%z cannot have type %z",nLine);` |
|    15961 | 1046 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1047 | `			return SXERR_ABORT;` |
|    15961 | 1048 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 | 1049 | `			goto Synchronize;` |
|        - | 1050 | `		}` |
|     7978 | 1051 | `	}` |
|        - | 1052 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   593607 | 1053 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|        4 | 1054 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1055 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|        3 | 1056 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1057 | `			return SXERR_ABORT;` |
|        - | 1058 | `		}` |
|        3 | 1059 | `		goto Synchronize;` |
|        - | 1060 | `	}` |
|        - | 1061 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|        - | 1062 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|        - | 1063 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|        - | 1064 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|        - | 1065 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|        - | 1066 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|        - | 1067 | `	/* php: a property default may not CALL anything either. */` |
|   593605 | 1068 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && PH7_GenStateInitHasCallExpr(pGen) ){` |
|      ! 0 | 1069 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1070 | `			"Constant expression contains invalid operations");` |
|      ! 0 | 1071 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1072 | `			return SXERR_ABORT;` |
|        - | 1073 | `		}` |
|      ! 0 | 1074 | `		goto Synchronize;` |
|        - | 1075 | `	}` |
|   593605 | 1076 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|        6 | 1077 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1078 | `			"New expressions are not supported in this context");` |
|        6 | 1079 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1080 | `			return SXERR_ABORT;` |
|        - | 1081 | `		}` |
|        6 | 1082 | `		goto Synchronize;` |
|        - | 1083 | `	}` |
|        - | 1084 | `	/* Allocate a new class attribute */` |
|   593601 | 1085 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   593601 | 1086 | `	if( pAttr ){` |
|   593601 | 1087 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|   593601 | 1088 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 1089 | `			return SXERR_ABORT;` |
|        - | 1090 | `		}` |
|   296798 | 1091 | `	}` |
|   593601 | 1092 | `	if( pAttr == 0 ){` |
|      ! 0 | 1093 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1094 | `		return SXERR_ABORT;` |
|        - | 1095 | `	}` |
|   593601 | 1096 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|    15959 | 1097 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|     7977 | 1098 | `	}` |
|   593601 | 1099 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|        - | 1100 | `		SySet *pInstrContainer;` |
|   382785 | 1101 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|   382785 | 1102 | `		pGen->pIn++; /*Jump the equal sign */` |
|        - | 1103 | `		{` |
|        - | 1104 | `			/* Delimit the default expression: it ends at the declaration's` |
|        - | 1105 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|        - | 1106 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|        - | 1107 | `			 * compiler would otherwise run into the hook tokens. */` |
|   382785 | 1108 | `			SyToken *pScan = pGen->pIn;` |
|   382785 | 1109 | `			sxi32 iNest = 0;` |
|   844087 | 1110 | `			while( pScan < pGen->pEnd ){` |
|   844087 | 1111 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|    50767 | 1112 | `					iNest++;` |
|   818706 | 1113 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|    50767 | 1114 | `					iNest--;` |
|   767944 | 1115 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|   382785 | 1116 | `					break;` |
|        - | 1117 | `				}` |
|   461307 | 1118 | `				pScan++;` |
|        5 | 1119 | `			}` |
|   382785 | 1120 | `			pGen->pEnd = pScan;` |
|        - | 1121 | `		}` |
|        - | 1122 | `		/* Swap bytecode container */` |
|   382785 | 1123 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   382785 | 1124 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|        - | 1125 | `		/* Compile attribute value.` |
|        - | 1126 | `		 */` |
|   382785 | 1127 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   382785 | 1128 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 1129 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|      ! 0 | 1130 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1131 | `				return SXERR_ABORT;` |
|        - | 1132 | `			}` |
|      ! 0 | 1133 | `		}` |
|        - | 1134 | `		/* Emit the done instruction */` |
|   382785 | 1135 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   382785 | 1136 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   382785 | 1137 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|   382785 | 1138 | `		pGen->pEnd = pSavedDefEnd;` |
|   191390 | 1139 | `	}` |
|        - | 1140 | `	/* All done,install the attribute */` |
|   593601 | 1141 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   593601 | 1142 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1143 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1144 | `		return SXERR_ABORT;` |
|        - | 1145 | `	}` |
|   593601 | 1146 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|        - | 1147 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|        - | 1148 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|       95 | 1149 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|       95 | 1150 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1151 | `			return SXERR_ABORT;` |
|        - | 1152 | `		}` |
|       95 | 1153 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1154 | `			goto Synchronize;` |
|        - | 1155 | `		}` |
|       95 | 1156 | `		SySetRelease(&aUnionAlts);` |
|       95 | 1157 | `		return SXRET_OK;` |
|        - | 1158 | `	}` |
|   593507 | 1159 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - | 1160 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|        - | 1161 | `		 * wording differs per declaration site) */` |
|      ! 0 | 1162 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 1163 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|        - | 1164 | `				? "Interfaces may only include hooked properties"` |
|        - | 1165 | `				: "Only hooked properties may be declared abstract");` |
|      ! 0 | 1166 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1167 | `			return SXERR_ABORT;` |
|        - | 1168 | `		}` |
|      ! 0 | 1169 | `		goto Synchronize;` |
|        - | 1170 | `	}` |
|   593507 | 1171 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - | 1172 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|        5 | 1173 | `		pGen->pIn++; /* Jump the comma */` |
|        5 | 1174 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 | 1175 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 | 1176 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 | 1177 | `				pTok--;` |
|      ! 0 | 1178 | `			}` |
|      ! 0 | 1179 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 1180 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|      ! 0 | 1181 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 | 1182 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1183 | `				return SXERR_ABORT;` |
|        - | 1184 | `			}` |
|      ! 0 | 1185 | `		}else{` |
|        5 | 1186 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        5 | 1187 | `				goto loop;` |
|        - | 1188 | `			}` |
|        - | 1189 | `		}` |
|      ! 0 | 1190 | `	}` |
|   593503 | 1191 | `	SySetRelease(&aUnionAlts);` |
|   593503 | 1192 | `	return SXRET_OK;` |
|        9 | 1193 | `Synchronize:` |
|        - | 1194 | `	/* Synchronize with the first semi-colon */` |
|       56 | 1195 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       38 | 1196 | `		pGen->pIn++;` |
|        4 | 1197 | `	}` |
|       22 | 1198 | `	SySetRelease(&aUnionAlts);` |
|       22 | 1199 | `	return SXERR_CORRUPT;` |
|   296810 | 1200 | `}` |
|        - | 1201 | `/*` |
|        - | 1202 | ` * Compile a class method.` |
|        - | 1203 | ` *` |
|        - | 1204 | ` * Refer to the official documentation for more information` |
|        - | 1205 | ` * on the powerful extension introduced by the PH7 engine` |
|        - | 1206 | ` * to the OO subsystem such as full type hinting,method` |
|        - | 1207 | ` * overloading and many more.` |
|        - | 1208 | ` */` |
|  2909798 | 1209 | `static sxi32 GenStateCompileClassMethod(` |
|        - | 1210 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 1211 | `	sxi32 iProtection,   /* Visibility level */` |
|        - | 1212 | `	sxi32 iFlags,        /* Configuration flags */` |
|        - | 1213 | `	int doBody,          /* TRUE to process method body */` |
|        - | 1214 | `	ph7_class *pClass    /* Class this method belongs */` |
|        - | 1215 | `	)` |
|        5 | 1216 | `{` |
|  2909803 | 1217 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  2909803 | 1218 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|        - | 1219 | `	ph7_class_method *pMeth;` |
|        - | 1220 | `	sxi32 iFuncFlags;` |
|        - | 1221 | `	SyString *pName;` |
|        - | 1222 | `	SyToken *pEnd;` |
|        - | 1223 | `	sxi32 rc;` |
|        - | 1224 | `	/* Extract visibility level */` |
|  2909803 | 1225 | `	iProtection = GetProtectionLevel(iProtection);` |
|  2909803 | 1226 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  2909803 | 1227 | `	iFuncFlags = 0;` |
|  2909803 | 1228 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1229 | `		/* Invalid method name */` |
|      ! 0 | 1230 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|      ! 0 | 1231 | `		if( rc == SXERR_ABORT ){` |
|        - | 1232 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1233 | `			return SXERR_ABORT;` |
|        - | 1234 | `		}` |
|      ! 0 | 1235 | `		goto Synchronize;` |
|        - | 1236 | `	}` |
|  2909803 | 1237 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - | 1238 | `		/* Return by reference,remember that */` |
|      ! 0 | 1239 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|        - | 1240 | `		/* Jump the '&' token */` |
|      ! 0 | 1241 | `		pGen->pIn++;` |
|      ! 0 | 1242 | `	}` |
|  2909803 | 1243 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 1244 | `		/* Invalid method name */` |
|      ! 0 | 1245 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|      ! 0 | 1246 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1247 | `			return SXERR_ABORT;` |
|        - | 1248 | `		}` |
|      ! 0 | 1249 | `		goto Synchronize;` |
|        - | 1250 | `	}` |
|        - | 1251 | `	/* Peek method name */` |
|  2909803 | 1252 | `	pName = &pGen->pIn->sData;` |
|  2909803 | 1253 | `	nLine = pGen->pIn->nLine;` |
|        - | 1254 | `	/* Jump the method name */` |
|  2909803 | 1255 | `	pGen->pIn++;` |
|  2909803 | 1256 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - | 1257 | `		/* Abstract method */` |
|   140409 | 1258 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|      ! 0 | 1259 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1260 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|      ! 0 | 1261 | `				&pClass->sName,pName);` |
|      ! 0 | 1262 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1263 | `				return SXERR_ABORT;` |
|        - | 1264 | `			}` |
|      ! 0 | 1265 | `		}` |
|        - | 1266 | `		/* Assemble method signature only */` |
|   140409 | 1267 | `		doBody = FALSE;` |
|    70202 | 1268 | `	}` |
|  2909803 | 1269 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1270 | `		/* Syntax error */` |
|      ! 0 | 1271 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|      ! 0 | 1272 | `		if( rc == SXERR_ABORT ){` |
|        - | 1273 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1274 | `			return SXERR_ABORT;` |
|        - | 1275 | `		}` |
|      ! 0 | 1276 | `		goto Synchronize;` |
|        - | 1277 | `	}` |
|        - | 1278 | `	/* Allocate a new class_method instance */` |
|  2909803 | 1279 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  2909803 | 1280 | `	if( pMeth == 0 ){` |
|      ! 0 | 1281 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1282 | `		return SXERR_ABORT;` |
|        - | 1283 | `	}` |
|  2909803 | 1284 | `	pMeth->sFunc.nLine = nKwLine;` |
|  2909803 | 1285 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|  2909803 | 1286 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 1287 | `		return SXERR_ABORT;` |
|        - | 1288 | `	}` |
|        - | 1289 | `	/* Jump the left parenthesis '(' */` |
|  2909803 | 1290 | `	pGen->pIn++;` |
|  2909803 | 1291 | `	pEnd = 0; /* cc warning */` |
|        - | 1292 | `	/* Delimit the method signature */` |
|  2909803 | 1293 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  2909803 | 1294 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 1295 | `		/* Syntax error */` |
|        3 | 1296 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|        3 | 1297 | `		if( rc == SXERR_ABORT ){` |
|        - | 1298 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1299 | `			return SXERR_ABORT;` |
|        - | 1300 | `		}` |
|        3 | 1301 | `		goto Synchronize;` |
|        - | 1302 | `	}` |
|        - | 1303 | `	{` |
|  2909801 | 1304 | `		int bIsCtor = 0;` |
|  2909801 | 1305 | `		int bAbstractCtor = 0;` |
|  2909796 | 1306 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  1708439 | 1307 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  2806391 | 1308 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   206825 | 1309 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        3 | 1310 | `				bAbstractCtor = 1;` |
|        2 | 1311 | `			}else{` |
|   206823 | 1312 | `				bIsCtor = 1;` |
|        - | 1313 | `			}` |
|   103410 | 1314 | `		}` |
|  2909801 | 1315 | `		if( pGen->pIn < pEnd ){` |
|        - | 1316 | `			/* Collect method arguments */` |
|  1119471 | 1317 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|  1119471 | 1318 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1319 | `				return SXERR_ABORT;` |
|        - | 1320 | `			}` |
|   559733 | 1321 | `		}` |
|        - | 1322 | `	}` |
|        - | 1323 | `	/* Point past ')' and parse optional return type ': type' */` |
|  2909801 | 1324 | `	pGen->pIn = &pEnd[1];` |
|        - | 1325 | `	{` |
|  2909801 | 1326 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  2909801 | 1327 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 | 1328 | `			return SXERR_ABORT;` |
|  2909801 | 1329 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|      ! 0 | 1330 | `			goto Synchronize;` |
|        - | 1331 | `		}` |
|        - | 1332 | `	}` |
|        - | 1333 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|        - | 1334 | `	 * property init/typecheck is handled by the generic typed-property path` |
|        - | 1335 | `	 * since we mint real ph7_class_attr entries. */` |
|        - | 1336 | `	{` |
|  2909801 | 1337 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|        - | 1338 | `		sxu32 i;` |
|  4582977 | 1339 | `		for( i = 0; i < nArg; i++ ){` |
|  1673191 | 1340 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|        - | 1341 | `			ph7_class_attr *pAttr;` |
|  1673191 | 1342 | `			sxi32 iAttrFlags = 0;` |
|        - | 1343 | `			int bArgTyped;` |
|  1673191 | 1344 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  1673105 | 1345 | `				continue;` |
|        - | 1346 | `			}` |
|        - | 1347 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|        - | 1348 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|        - | 1349 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|       60 | 1350 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|       92 | 1351 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|       91 | 1352 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        3 | 1353 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1354 | `					"Cannot declare variadic promoted property");` |
|        3 | 1355 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1356 | `					return SXERR_ABORT;` |
|        - | 1357 | `				}` |
|        3 | 1358 | `				goto Synchronize;` |
|        - | 1359 | `			}` |
|        - | 1360 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|        - | 1361 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|        - | 1362 | `			 * appear as an alternative of a union type. */` |
|       89 | 1363 | `			if( bArgTyped ){` |
|      125 | 1364 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|       80 | 1365 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|       80 | 1366 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|       40 | 1367 | `					"Property %z::$%z cannot have type %z",nLine);` |
|       85 | 1368 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1369 | `					return SXERR_ABORT;` |
|       85 | 1370 | `				}else if( rc != SXRET_OK ){` |
|        6 | 1371 | `					goto Synchronize;` |
|        - | 1372 | `				}` |
|       38 | 1373 | `			}` |
|        - | 1374 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|       85 | 1375 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|        4 | 1376 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1377 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|        3 | 1378 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1379 | `					return SXERR_ABORT;` |
|        - | 1380 | `				}` |
|        3 | 1381 | `				goto Synchronize;` |
|        - | 1382 | `			}` |
|       83 | 1383 | `			if( bArgTyped ){` |
|       79 | 1384 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       37 | 1385 | `			}` |
|       83 | 1386 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|        3 | 1387 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|        1 | 1388 | `			}` |
|       83 | 1389 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|        8 | 1390 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|        3 | 1391 | `			}` |
|       83 | 1392 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|        - | 1393 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|        - | 1394 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|       26 | 1395 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        4 | 1396 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1397 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|        3 | 1398 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1399 | `						return SXERR_ABORT;` |
|        - | 1400 | `					}` |
|        3 | 1401 | `					goto Synchronize;` |
|        - | 1402 | `				}` |
|       24 | 1403 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       10 | 1404 | `			}` |
|       81 | 1405 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|        - | 1406 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|        5 | 1407 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 | 1408 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1409 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|      ! 0 | 1410 | `						&pClass->sName,&pArg->sName);` |
|      ! 0 | 1411 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1412 | `						return SXERR_ABORT;` |
|        - | 1413 | `					}` |
|      ! 0 | 1414 | `					goto Synchronize;` |
|        - | 1415 | `				}` |
|        5 | 1416 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|        2 | 1417 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|        2 | 1418 | `			}` |
|       81 | 1419 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|       81 | 1420 | `			if( pAttr == 0 ){` |
|      ! 0 | 1421 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1422 | `				return SXERR_ABORT;` |
|        - | 1423 | `			}` |
|       81 | 1424 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|       79 | 1425 | `				pAttr->nType = pArg->nType;` |
|       79 | 1426 | `				pAttr->sClass = pArg->sClass;` |
|       79 | 1427 | `				pAttr->sTypeName = pArg->sTypeName;` |
|       79 | 1428 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|        - | 1429 | `					sxu32 k;` |
|       20 | 1430 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|       14 | 1431 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|       14 | 1432 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|        8 | 1433 | `					}` |
|        3 | 1434 | `				}` |
|       37 | 1435 | `			}` |
|       81 | 1436 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|       81 | 1437 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1438 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1439 | `				return SXERR_ABORT;` |
|        - | 1440 | `			}` |
|       43 | 1441 | `		}` |
|        - | 1442 | `	}` |
|  2909791 | 1443 | `	if( doBody ){` |
|        - | 1444 | `		/* Compile method body */` |
|  2769387 | 1445 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  2769387 | 1446 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1447 | `			return SXERR_ABORT;` |
|        - | 1448 | `		}` |
|        - | 1449 | `		/* The cursor sits just past the body's closing brace */` |
|  2769387 | 1450 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|  1384696 | 1451 | `	}else{` |
|        - | 1452 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|   140409 | 1453 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|   140409 | 1454 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|    70202 | 1455 | `		}` |
|        - | 1456 | `		/* Only method signature is allowed */` |
|   140409 | 1457 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|      ! 0 | 1458 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 1459 | `				"Expected ';' after method signature '%z'",pName);` |
|      ! 0 | 1460 | `				if( rc == SXERR_ABORT ){` |
|        - | 1461 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 | 1462 | `					return SXERR_ABORT;` |
|        - | 1463 | `				}` |
|      ! 0 | 1464 | `				return SXERR_CORRUPT;` |
|        - | 1465 | `			}` |
|        - | 1466 | `	}` |
|        - | 1467 | `	/* php: an abstract method in a class that was not DECLARED abstract is a fatal,` |
|        - | 1468 | `	 * and it names the method -- distinct from the "contains N abstract methods"` |
|        - | 1469 | `	 * wording php uses for an unimplemented INHERITED one, which` |
|        - | 1470 | `	 * GenStateCheckAbstractMethods already handles. Interfaces and traits may carry` |
|        - | 1471 | `	 * abstract methods freely. */` |
|  2909786 | 1472 | `	if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT)` |
|  1525100 | 1473 | `	 && (pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|        4 | 1474 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1475 | `			"Class %z declares abstract method %z() and must therefore be declared abstract",` |
|        1 | 1476 | `			&pClass->sName,pName);` |
|        3 | 1477 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1478 | `			return SXERR_ABORT;` |
|        - | 1479 | `		}` |
|        3 | 1480 | `		return SXRET_OK;` |
|        - | 1481 | `	}` |
|        - | 1482 | `	/* php: two methods of the same name in one class body is a fatal, and the` |
|        - | 1483 | ``	 * comparison is case-INSENSITIVE (hMethod now matches that way), so `f` and`` |
|        - | 1484 | ``	 * `F` collide. php names the offending declaration with the spelling used at`` |
|        - | 1485 | `	 * the SECOND site. */` |
|  2909789 | 1486 | `	if( PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte) != 0 ){` |
|        8 | 1487 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        2 | 1488 | `			"Cannot redeclare %z::%z()",&pClass->sName,pName);` |
|        6 | 1489 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1490 | `			return SXERR_ABORT;` |
|        - | 1491 | `		}` |
|        6 | 1492 | `		return SXRET_OK;` |
|        - | 1493 | `	}` |
|        - | 1494 | `	/* All done,install the method */` |
|  2909785 | 1495 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  2909785 | 1496 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1497 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1498 | `		return SXERR_ABORT;` |
|        - | 1499 | `	}` |
|  2909785 | 1500 | `	return SXRET_OK;` |
|        6 | 1501 | `Synchronize:` |
|        - | 1502 | `	/* Synchronize with the first semi-colon */` |
|       40 | 1503 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       28 | 1504 | `		pGen->pIn++;` |
|        4 | 1505 | `	}` |
|       16 | 1506 | `	return SXERR_CORRUPT;` |
|  1454904 | 1507 | `}` |
|        - | 1508 | `/*` |
|        - | 1509 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|        - | 1510 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|        - | 1511 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|        - | 1512 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|        - | 1513 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|        - | 1514 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|        - | 1515 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|        - | 1516 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|        - | 1517 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|        - | 1518 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|        - | 1519 | `` * implicit `$value` formal.`` |
|        - | 1520 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|        - | 1521 | ` */` |
|        - | 1522 | `/*` |
|        - | 1523 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|        - | 1524 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|        - | 1525 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|        - | 1526 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|        - | 1527 | ` * allowed, excluded from the raw object surfaces.` |
|        - | 1528 | ` */` |
|       94 | 1529 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|        1 | 1530 | `{` |
|        - | 1531 | `	SyToken *p;` |
|      345 | 1532 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|      303 | 1533 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      223 | 1534 | `			continue;` |
|        - | 1535 | `		}` |
|        - | 1536 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|       80 | 1537 | `		if( p + 3 < pEnd` |
|       80 | 1538 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       80 | 1539 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|       73 | 1540 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|       66 | 1541 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|       66 | 1542 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       66 | 1543 | `		 && p[3].sData.nByte == pName->nByte` |
|       60 | 1544 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       51 | 1545 | `			return 1;` |
|        - | 1546 | `		}` |
|        - | 1547 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|        - | 1548 | `		 * hook operates on the shared per-instance backing store, so the` |
|        - | 1549 | `		 * property is backed (php compiles a default alongside it). */` |
|       30 | 1550 | `		if( p > pStart` |
|       26 | 1551 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|       12 | 1552 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        2 | 1553 | `		 && p[1].sData.nByte == pName->nByte` |
|        3 | 1554 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        3 | 1555 | `			return 1;` |
|        - | 1556 | `		}` |
|       15 | 1557 | `	}` |
|       43 | 1558 | `	return 0;` |
|       48 | 1559 | `}` |
|        - | 1560 | `/*` |
|        - | 1561 | ` * True when p opens php 8.4's parent-hook call form` |
|        - | 1562 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|        - | 1563 | ` */` |
|      990 | 1564 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|        1 | 1565 | `{` |
|     1167 | 1566 | `	return p + 6 < pEnd` |
|      671 | 1567 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|      250 | 1568 | `	 && p->sData.nByte == sizeof("parent")-1` |
|       81 | 1569 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|       11 | 1570 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|        8 | 1571 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|        8 | 1572 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        8 | 1573 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|        8 | 1574 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        8 | 1575 | `	 && p[5].sData.nByte == 3` |
|        8 | 1576 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|        6 | 1577 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|     1166 | 1578 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|        1 | 1579 | `}` |
|        - | 1580 | `/*` |
|        - | 1581 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|        - | 1582 | ` * hook body into calls of the parent class's synthesized hook method` |
|        - | 1583 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|        - | 1584 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|        - | 1585 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|        - | 1586 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|        - | 1587 | ` * or SXERR_MEM.` |
|        - | 1588 | ` */` |
|        4 | 1589 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|        - | 1590 | `	SyToken *pStart,SyToken *pEnd)` |
|        1 | 1591 | `{` |
|        5 | 1592 | `	SyToken *p = pStart;` |
|       35 | 1593 | `	while( p < pEnd ){` |
|       31 | 1594 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|        - | 1595 | `			SyToken sTok;` |
|        - | 1596 | `			char zName[384];` |
|        - | 1597 | `			sxu32 nName;` |
|        - | 1598 | `			char *zDup;` |
|        - | 1599 | ``			/* `parent` `::` */`` |
|        5 | 1600 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|        5 | 1601 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|        7 | 1602 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|        4 | 1603 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|        5 | 1604 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|        5 | 1605 | `			if( zDup == 0 ){` |
|      ! 0 | 1606 | `				return SXERR_MEM;` |
|        - | 1607 | `			}` |
|        5 | 1608 | `			sTok = p[3]; /* keep the line info of the property name */` |
|        5 | 1609 | `			sTok.nType = PH7_TK_ID;` |
|        5 | 1610 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|        5 | 1611 | `			sTok.pUserData = 0;` |
|        5 | 1612 | `			SySetPut(pCopy,(const void *)&sTok);` |
|        5 | 1613 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|        5 | 1614 | `			continue;` |
|        - | 1615 | `		}` |
|       27 | 1616 | `		SySetPut(pCopy,(const void *)p);` |
|       27 | 1617 | `		p++;` |
|        1 | 1618 | `	}` |
|        5 | 1619 | `	return SXRET_OK;` |
|        3 | 1620 | `}` |
|       94 | 1621 | `PH7_PRIVATE sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 | 1622 | `{` |
|       95 | 1623 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 1624 | `	sxi32 rc;` |
|       95 | 1625 | `	int bRefsSelf = 0;` |
|       95 | 1626 | `	pGen->pIn++; /* Jump '{' */` |
|      253 | 1627 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|        - | 1628 | `		char zHook[384];` |
|        - | 1629 | `		SyString sHookName;` |
|        - | 1630 | `		ph7_class_method *pMeth;` |
|        - | 1631 | `		int bGet;` |
|      159 | 1632 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|      159 | 1633 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       15 | 1634 | `			pGen->pIn++; /* stray ';' between hooks */` |
|       22 | 1635 | `			continue;` |
|        - | 1636 | `		}` |
|      145 | 1637 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - | 1638 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|      ! 0 | 1639 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 1640 | `				"By-reference property hooks are not supported for %z::$%z",` |
|      ! 0 | 1641 | `				&pClass->sName,&pAttr->sName);` |
|      ! 0 | 1642 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1643 | `				return SXERR_ABORT;` |
|        - | 1644 | `			}` |
|      ! 0 | 1645 | `			return SXERR_CORRUPT;` |
|        - | 1646 | `		}` |
|      145 | 1647 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 1648 | `			goto HookSyntax;` |
|        - | 1649 | `		}` |
|      144 | 1650 | `		if( pGen->pIn->sData.nByte == 3` |
|      145 | 1651 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|       79 | 1652 | `			bGet = 1;` |
|      106 | 1653 | `		}else if( pGen->pIn->sData.nByte == 3` |
|       67 | 1654 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|       67 | 1655 | `			bGet = 0;` |
|       34 | 1656 | `		}else{` |
|      ! 0 | 1657 | `			goto HookSyntax;` |
|        - | 1658 | `		}` |
|      145 | 1659 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|      145 | 1660 | `		sHookName.zString = zHook;` |
|      217 | 1661 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|       72 | 1662 | `			bGet ? "get" : "set",&pAttr->sName);` |
|      145 | 1663 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|        - | 1664 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|        - | 1665 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|        - | 1666 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|        - | 1667 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|        - | 1668 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|       14 | 1669 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|        8 | 1670 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 1671 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 1672 | `					"Non-abstract property hook must have a body");` |
|      ! 0 | 1673 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1674 | `					return SXERR_ABORT;` |
|        - | 1675 | `				}` |
|      ! 0 | 1676 | `				return SXERR_CORRUPT;` |
|        - | 1677 | `			}` |
|       15 | 1678 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|        - | 1679 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|       15 | 1680 | `			if( pMeth == 0 ){` |
|      ! 0 | 1681 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1682 | `				return SXERR_ABORT;` |
|        - | 1683 | `			}` |
|       15 | 1684 | `			pMeth->sFunc.nLine = nHLine;` |
|       15 | 1685 | `			if( !bGet ){` |
|        - | 1686 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|        - | 1687 | `				 * compatible with concrete set-hook implementations (which` |
|        - | 1688 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|        - | 1689 | `				 * type (php: the abstract set's parameter type IS the property` |
|        - | 1690 | `				 * type), so the override contravariance check accepts a typed` |
|        - | 1691 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|        - | 1692 | `				ph7_vm_func_arg sVArg;` |
|        7 | 1693 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        7 | 1694 | `				if( zVName == 0 ){` |
|      ! 0 | 1695 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1696 | `					return SXERR_ABORT;` |
|        - | 1697 | `				}` |
|        7 | 1698 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        7 | 1699 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        7 | 1700 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        7 | 1701 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        7 | 1702 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        7 | 1703 | `				sVArg.nType = pAttr->nType;` |
|        7 | 1704 | `				sVArg.sClass = pAttr->sClass;` |
|        7 | 1705 | `				sVArg.sTypeName = pAttr->sTypeName;` |
|        7 | 1706 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|      ! 0 | 1707 | `					sVArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|      ! 0 | 1708 | `				}` |
|        7 | 1709 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        3 | 1710 | `			}` |
|       15 | 1711 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       15 | 1712 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1713 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1714 | `				return SXERR_ABORT;` |
|        - | 1715 | `			}` |
|       15 | 1716 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|       15 | 1717 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|        - | 1718 | `		}` |
|      130 | 1719 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|      131 | 1720 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|        - | 1721 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|      ! 0 | 1722 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 1723 | `				"Abstract property hook cannot have body");` |
|      ! 0 | 1724 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1725 | `				return SXERR_ABORT;` |
|        - | 1726 | `			}` |
|      ! 0 | 1727 | `			return SXERR_CORRUPT;` |
|        - | 1728 | `		}` |
|      131 | 1729 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|        - | 1730 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|      131 | 1731 | `		if( pMeth == 0 ){` |
|      ! 0 | 1732 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1733 | `			return SXERR_ABORT;` |
|        - | 1734 | `		}` |
|      131 | 1735 | `		pMeth->sFunc.nLine = nHLine;` |
|      131 | 1736 | `		if( !bGet ){` |
|        - | 1737 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|       61 | 1738 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       17 | 1739 | `				SyToken *pRp = 0;` |
|       17 | 1740 | `				pGen->pIn++;` |
|       17 | 1741 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|       17 | 1742 | `				if( pRp >= pGen->pEnd ){` |
|      ! 0 | 1743 | `					goto HookSyntax;` |
|        - | 1744 | `				}` |
|       17 | 1745 | `				if( pGen->pIn < pRp ){` |
|       17 | 1746 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|       17 | 1747 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1748 | `						return SXERR_ABORT;` |
|        - | 1749 | `					}` |
|        8 | 1750 | `				}` |
|       17 | 1751 | `				pGen->pIn = &pRp[1];` |
|        8 | 1752 | `			}` |
|       61 | 1753 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|        - | 1754 | `				/* Implicit $value formal */` |
|        - | 1755 | `				ph7_vm_func_arg sVArg;` |
|       45 | 1756 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|       45 | 1757 | `				if( zVName == 0 ){` |
|      ! 0 | 1758 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1759 | `					return SXERR_ABORT;` |
|        - | 1760 | `				}` |
|       45 | 1761 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|       45 | 1762 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|       45 | 1763 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       45 | 1764 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       45 | 1765 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|       45 | 1766 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|       45 | 1767 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|       22 | 1768 | `			}` |
|       30 | 1769 | `		}` |
|      165 | 1770 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - | 1771 | `			/* Block body */` |
|       69 | 1772 | `			SyToken *pBodyStart = pGen->pIn;` |
|       69 | 1773 | `			SyToken *pCloser = 0;` |
|       69 | 1774 | `			int bParentCall = 0;` |
|       69 | 1775 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|       69 | 1776 | `			if( pCloser < pGen->pEnd ){` |
|        - | 1777 | `				SyToken *pScan;` |
|      753 | 1778 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|      687 | 1779 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|        3 | 1780 | `						bParentCall = 1;` |
|        3 | 1781 | `						break;` |
|        - | 1782 | `					}` |
|      343 | 1783 | `				}` |
|       34 | 1784 | `			}` |
|       69 | 1785 | `			if( bParentCall ){` |
|        - | 1786 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|        - | 1787 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|        - | 1788 | `				 * hook method), then continue past the original body. */` |
|        - | 1789 | `				SySet sBody;` |
|        3 | 1790 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|        3 | 1791 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        3 | 1792 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|        3 | 1793 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1794 | `					SySetRelease(&sBody);` |
|      ! 0 | 1795 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1796 | `					return SXERR_ABORT;` |
|        - | 1797 | `				}` |
|        3 | 1798 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|        3 | 1799 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|        3 | 1800 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        3 | 1801 | `				pGen->pIn = &pCloser[1];` |
|        3 | 1802 | `				pGen->pEnd = pSavedEnd;` |
|        3 | 1803 | `				SySetRelease(&sBody);` |
|        3 | 1804 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1805 | `					return SXERR_ABORT;` |
|        - | 1806 | `				}` |
|        3 | 1807 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|        2 | 1808 | `			}else{` |
|       67 | 1809 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|       67 | 1810 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1811 | `					return SXERR_ABORT;` |
|        - | 1812 | `				}` |
|       67 | 1813 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|        - | 1814 | `			}` |
|       69 | 1815 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|       17 | 1816 | `				bRefsSelf = 1;` |
|        9 | 1817 | `			}` |
|      128 | 1818 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|        - | 1819 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|        - | 1820 | `			GenBlock *pBlock;` |
|        - | 1821 | `			SySet *pInstrContainer;` |
|        - | 1822 | `			SyToken *pBodyStart;` |
|        - | 1823 | `			SyToken *pExprEnd;` |
|       63 | 1824 | `			SyToken *pSavedEnd = 0;` |
|        - | 1825 | `			SySet sBody;` |
|       63 | 1826 | `			int bParentCall = 0;` |
|       63 | 1827 | `			pGen->pIn++; /* Jump '=>' */` |
|       63 | 1828 | `			pBodyStart = pGen->pIn;` |
|        - | 1829 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|        - | 1830 | `			 * would end the enclosing hook list) and rewrite any` |
|        - | 1831 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|        - | 1832 | `			 * method on a token copy. */` |
|        - | 1833 | `			{` |
|       63 | 1834 | `				sxi32 iNest = 0;` |
|       63 | 1835 | `				pExprEnd = pBodyStart;` |
|      355 | 1836 | `				while( pExprEnd < pGen->pEnd ){` |
|      355 | 1837 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        9 | 1838 | `						iNest++;` |
|      351 | 1839 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        9 | 1840 | `						if( iNest <= 0 ){` |
|      ! 0 | 1841 | `							break;` |
|        - | 1842 | `						}` |
|        9 | 1843 | `						iNest--;` |
|      343 | 1844 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|       63 | 1845 | `						break;` |
|        - | 1846 | `					}` |
|      293 | 1847 | `					pExprEnd++;` |
|        1 | 1848 | `				}` |
|        - | 1849 | `			}` |
|        - | 1850 | `			{` |
|        - | 1851 | `				SyToken *pScan;` |
|      335 | 1852 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|      275 | 1853 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|        3 | 1854 | `						bParentCall = 1;` |
|        3 | 1855 | `						break;` |
|        - | 1856 | `					}` |
|      137 | 1857 | `				}` |
|        - | 1858 | `			}` |
|       63 | 1859 | `			if( bParentCall ){` |
|        3 | 1860 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        3 | 1861 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|        3 | 1862 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1863 | `					SySetRelease(&sBody);` |
|      ! 0 | 1864 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1865 | `					return SXERR_ABORT;` |
|        - | 1866 | `				}` |
|        3 | 1867 | `				pSavedEnd = pGen->pEnd;` |
|        3 | 1868 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|        3 | 1869 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|        1 | 1870 | `			}` |
|       94 | 1871 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       62 | 1872 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|       63 | 1873 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1874 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|      ! 0 | 1875 | `				return SXERR_ABORT;` |
|        - | 1876 | `			}` |
|       63 | 1877 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       63 | 1878 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|       63 | 1879 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|       63 | 1880 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       63 | 1881 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       63 | 1882 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       63 | 1883 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       63 | 1884 | `			GenStateLeaveBlock(&(*pGen),0);` |
|       63 | 1885 | `			if( bParentCall ){` |
|        3 | 1886 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|        3 | 1887 | `				pGen->pEnd = pSavedEnd;` |
|        3 | 1888 | `				SySetRelease(&sBody);` |
|        1 | 1889 | `			}` |
|       63 | 1890 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1891 | `				return SXERR_ABORT;` |
|        - | 1892 | `			}` |
|       63 | 1893 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|       63 | 1894 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|       37 | 1895 | `				bRefsSelf = 1;` |
|       18 | 1896 | `			}` |
|       63 | 1897 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       63 | 1898 | `				pGen->pIn++; /* Jump ';' */` |
|       31 | 1899 | `			}` |
|       63 | 1900 | `			if( !bGet ){` |
|        - | 1901 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|        - | 1902 | `				 * the dispatcher consumes the implicit return value — which` |
|        - | 1903 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|        - | 1904 | ``				 * for `$this->NAME = expr`). */`` |
|        3 | 1905 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|        3 | 1906 | `				bRefsSelf = 1;` |
|        1 | 1907 | `			}` |
|       32 | 1908 | `		}else{` |
|      ! 0 | 1909 | `			goto HookSyntax;` |
|        - | 1910 | `		}` |
|      131 | 1911 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|      131 | 1912 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1913 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1914 | `			return SXERR_ABORT;` |
|        - | 1915 | `		}` |
|      131 | 1916 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        1 | 1917 | `	}` |
|       95 | 1918 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|      ! 0 | 1919 | `		goto HookSyntax;` |
|        - | 1920 | `	}` |
|       95 | 1921 | `	pGen->pIn++; /* Jump '}' */` |
|       95 | 1922 | `	if( !bRefsSelf ){` |
|        - | 1923 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|        - | 1924 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|        - | 1925 | `		 * a default value (compile fatal, php's exact wording). */` |
|       41 | 1926 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|       41 | 1927 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      ! 0 | 1928 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1929 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|      ! 0 | 1930 | `				&pClass->sName,&pAttr->sName);` |
|      ! 0 | 1931 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1932 | `				return SXERR_ABORT;` |
|        - | 1933 | `			}` |
|      ! 0 | 1934 | `			return SXERR_CORRUPT;` |
|        - | 1935 | `		}` |
|       20 | 1936 | `	}` |
|       95 | 1937 | `	return SXRET_OK;` |
|      ! 0 | 1938 | `HookSyntax:` |
|      ! 0 | 1939 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1940 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|      ! 0 | 1941 | `		&pClass->sName,&pAttr->sName);` |
|      ! 0 | 1942 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1943 | `		return SXERR_ABORT;` |
|        - | 1944 | `	}` |
|      ! 0 | 1945 | `	return SXERR_CORRUPT;` |
|       48 | 1946 | `}` |
|        - | 1947 | `/*` |
|        - | 1948 | ` * Compile an object interface.` |
|        - | 1949 | ` *  According to the PHP language reference manual` |
|        - | 1950 | ` *   Object Interfaces:` |
|        - | 1951 | ` *   Object interfaces allow you to create code which specifies which methods` |
|        - | 1952 | ` *   a class must implement, without having to define how these methods are handled.` |
|        - | 1953 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - | 1954 | ` *   class, but without any of the methods having their contents defined.` |
|        - | 1955 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|        - | 1956 | ` */` |
|    70288 | 1957 | `PH7_PRIVATE sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|        5 | 1958 | `{` |
|    70293 | 1959 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 1960 | `	ph7_class *pClass,*pBase;` |
|        - | 1961 | `	SyToken *pEnd,*pTmp;` |
|        - | 1962 | `	SyString *pName;` |
|        - | 1963 | `	sxi32 nKwrd;` |
|        - | 1964 | `	sxi32 rc;` |
|        - | 1965 | `	/* Jump the 'interface' keyword */` |
|    70293 | 1966 | `	pGen->pIn++;` |
|        - | 1967 | `	/* Extract interface name */` |
|    70293 | 1968 | `	pName = &pGen->pIn->sData;` |
|        - | 1969 | `	/* Advance the stream cursor */` |
|    70293 | 1970 | `	pGen->pIn++;` |
|        - | 1971 | `	/* Build FQN and obtain a raw class */ {` |
|        - | 1972 | `		SyBlob sFQN;` |
|        - | 1973 | `		SyString sFQNStr;` |
|    70293 | 1974 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    70293 | 1975 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    70293 | 1976 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    70293 | 1977 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    70293 | 1978 | `		SyBlobRelease(&sFQN);` |
|        - | 1979 | `	}` |
|    70293 | 1980 | `	if( pClass == 0 ){` |
|      ! 0 | 1981 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1982 | `		return SXERR_ABORT;` |
|        - | 1983 | `	}` |
|    70293 | 1984 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    70293 | 1985 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 1986 | `		return SXERR_ABORT;` |
|        - | 1987 | `	}` |
|        - | 1988 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|    70293 | 1989 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|        - | 1990 | `	/* Assume no base class is given */` |
|    70293 | 1991 | `	pBase = 0;` |
|    70293 | 1992 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    27303 | 1993 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    27303 | 1994 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a, c, … */ ){` |
|        - | 1995 | `			/* php lets an interface extend SEVERAL parent interfaces. The first` |
|        - | 1996 | `			 * becomes pBase (single-inheritance chain, hDerived); every extra one` |
|        - | 1997 | `			 * is recorded via PH7_ClassImplement so it lands in aInterface — the` |
|        - | 1998 | `			 * runtime subtype walk (VmInterfaceReaches) follows both. */` |
|    27303 | 1999 | `			pGen->pIn++;` |
|    13650 | 2000 | `			for(;;){` |
|        - | 2001 | `				SyBlob sResolved;` |
|        - | 2002 | `				SyString sBaseName;` |
|        - | 2003 | `				sxu32 nRefLine;` |
|        - | 2004 | `				ph7_class *pParent;` |
|    27305 | 2005 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    27305 | 2006 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    27305 | 2007 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 2008 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 2009 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2010 | `						"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|      ! 0 | 2011 | `						pName);` |
|      ! 0 | 2012 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 2013 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2014 | `						return SXERR_ABORT;` |
|        - | 2015 | `					}` |
|      ! 0 | 2016 | `					return SXRET_OK;` |
|        - | 2017 | `				}` |
|    40955 | 2018 | `				pParent = PH7_VmExtractClass(pGen->pVm,` |
|    27300 | 2019 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    27305 | 2020 | `				SyStringInitFromBuf(&sBaseName,` |
|        - | 2021 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 2022 | `				/* Only interfaces is allowed */` |
|    27305 | 2023 | `				while( pParent && (pParent->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 2024 | `					pParent = pParent->pNextName;` |
|      ! 0 | 2025 | `				}` |
|    27305 | 2026 | `				if( pParent == 0 ){` |
|      ! 0 | 2027 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 2028 | `						"Nonexistent base interface '%z'",&sBaseName);` |
|      ! 0 | 2029 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2030 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 2031 | `						return SXERR_ABORT;` |
|      ! 0 | 2032 | `					}` |
|    27305 | 2033 | `				}else if( pBase == 0 ){` |
|        - | 2034 | `					/* First parent → single-inheritance base */` |
|    27303 | 2035 | `					pBase = pParent;` |
|    13654 | 2036 | `				}else{` |
|        - | 2037 | `					/* Additional parent → record it in aInterface (+ copy its` |
|        - | 2038 | `					 * constants/method stubs) so instanceof reaches it too. */` |
|        3 | 2039 | `					PH7_ClassImplement(pClass,pParent);` |
|        - | 2040 | `				}` |
|    27305 | 2041 | `				SyBlobRelease(&sResolved);` |
|        - | 2042 | `				/* Continue on a comma-separated list */` |
|    27305 | 2043 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 | 2044 | `					pGen->pIn++;` |
|        3 | 2045 | `					continue;` |
|        - | 2046 | `				}` |
|    27303 | 2047 | `				break;` |
|      ! 0 | 2048 | `			}` |
|    13649 | 2049 | `		}` |
|    13649 | 2050 | `	}` |
|    70293 | 2051 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - | 2052 | `		/* Syntax error */` |
|      ! 0 | 2053 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|      ! 0 | 2054 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 2055 | `		if( rc == SXERR_ABORT ){` |
|        - | 2056 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 2057 | `			return SXERR_ABORT;` |
|        - | 2058 | `		}` |
|      ! 0 | 2059 | `		return SXRET_OK;` |
|        - | 2060 | `	}` |
|    70293 | 2061 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    70293 | 2062 | `	pEnd = 0; /* cc warning */` |
|        - | 2063 | `	/* Delimit the interface body */` |
|    70293 | 2064 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    70293 | 2065 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 2066 | `		/* Syntax error */` |
|      ! 0 | 2067 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|      ! 0 | 2068 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 2069 | `		if( rc == SXERR_ABORT ){` |
|        - | 2070 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 2071 | `			return SXERR_ABORT;` |
|        - | 2072 | `		}` |
|      ! 0 | 2073 | `		return SXRET_OK;` |
|        - | 2074 | `	}` |
|        - | 2075 | `	/* The delimiter token is the interface body's closing brace */` |
|    70293 | 2076 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 2077 | `	/* Swap token stream */` |
|    70293 | 2078 | `	pTmp = pGen->pEnd;` |
|    70293 | 2079 | `	pGen->pEnd = pEnd;` |
|        - | 2080 | `	/* Start the parse process` |
|        - | 2081 | `	 * Note (According to the PHP reference manual):` |
|        - | 2082 | `	 *  Only constants and function signatures(without body) are allowed.` |
|        - | 2083 | `	 *  Only 'public' visibility is allowed.` |
|        - | 2084 | `	 */` |
|   128729 | 2085 | `	for(;;){` |
|        - | 2086 | `		/* Jump leading/trailing semi-colons */` |
|   444637 | 2087 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   187175 | 2088 | `			pGen->pIn++;` |
|        5 | 2089 | `		}` |
|   257467 | 2090 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 2091 | `			/* End of interface body */` |
|    70289 | 2092 | `			break;` |
|        - | 2093 | `		}` |
|        - | 2094 | `		/* Bind a directly-preceding docblock to this member */` |
|   187183 | 2095 | `		GenStateSetPendingDoc(&(*pGen));` |
|   187183 | 2096 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 2097 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 2098 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|      ! 0 | 2099 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 2100 | `			if( rc == SXERR_ABORT ){` |
|        - | 2101 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 2102 | `				return SXERR_ABORT;` |
|        - | 2103 | `			}` |
|      ! 0 | 2104 | `			goto done;` |
|        - | 2105 | `		}` |
|        - | 2106 | `		/* Extract the current keyword */` |
|   187183 | 2107 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   187183 | 2108 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        - | 2109 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|        - | 2110 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|        3 | 2111 | `			const char *zKind = "member";` |
|        3 | 2112 | `			SyString *pMemberName = 0;` |
|        3 | 2113 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|        3 | 2114 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|        3 | 2115 | `				if( nNext == PH7_TKWRD_CONST ){` |
|        3 | 2116 | `					zKind = "constant";` |
|        3 | 2117 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|        3 | 2118 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|        2 | 2119 | `					}` |
|        1 | 2120 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 2121 | `					zKind = "method";` |
|      ! 0 | 2122 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|      ! 0 | 2123 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|      ! 0 | 2124 | `					}` |
|      ! 0 | 2125 | `				}` |
|        1 | 2126 | `			}` |
|        3 | 2127 | `			if( pMemberName ){` |
|        4 | 2128 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        1 | 2129 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|        2 | 2130 | `			}else{` |
|      ! 0 | 2131 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2132 | `					"Access type for interface %s must be public",zKind);` |
|        - | 2133 | `			}` |
|        3 | 2134 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2135 | `				return SXERR_ABORT;` |
|        - | 2136 | `			}` |
|        3 | 2137 | `			goto done;` |
|        - | 2138 | `		}` |
|   187181 | 2139 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|      ! 0 | 2140 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2141 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 | 2142 | `			if( rc == SXERR_ABORT ){` |
|        - | 2143 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 2144 | `				return SXERR_ABORT;` |
|        - | 2145 | `			}` |
|      ! 0 | 2146 | `			goto done;` |
|        - | 2147 | `		}` |
|   187181 | 2148 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|        - | 2149 | `			/* Advance the stream cursor */` |
|   132591 | 2150 | `			pGen->pIn++;` |
|   132586 | 2151 | `			if( pGen->pIn < pGen->pEnd` |
|   132591 | 2152 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|   132586 | 2153 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|        - | 2154 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|        - | 2155 | `				 * requirement. The attribute compiler + hook parser handle it` |
|        - | 2156 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|        - | 2157 | `				 * property without hooks is ITS "Interfaces may only include` |
|        - | 2158 | `				 * hooked properties" error). */` |
|      ! 0 | 2159 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|      ! 0 | 2160 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|      ! 0 | 2161 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 2162 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2163 | `						return SXERR_ABORT;` |
|        - | 2164 | `					}` |
|      ! 0 | 2165 | `					goto done;` |
|        - | 2166 | `				}` |
|      ! 0 | 2167 | `				continue;` |
|        - | 2168 | `			}` |
|   132591 | 2169 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|        - | 2170 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|        - | 2171 | `				 * '$' also opens a hooked-property requirement. */` |
|      ! 0 | 2172 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|      ! 0 | 2173 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|      ! 0 | 2174 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|      ! 0 | 2175 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|      ! 0 | 2176 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|      ! 0 | 2177 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 2178 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 2179 | `							return SXERR_ABORT;` |
|        - | 2180 | `						}` |
|      ! 0 | 2181 | `						goto done;` |
|        - | 2182 | `					}` |
|      ! 0 | 2183 | `					continue;` |
|        - | 2184 | `				}` |
|      ! 0 | 2185 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2186 | `					"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 | 2187 | `				if( rc == SXERR_ABORT ){` |
|        - | 2188 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 | 2189 | `					return SXERR_ABORT;` |
|        - | 2190 | `				}` |
|      ! 0 | 2191 | `				goto done;` |
|        - | 2192 | `			}` |
|   132591 | 2193 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   132591 | 2194 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|        - | 2195 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|        - | 2196 | `				 * hooked-property requirement (PHP 8.4). */` |
|        4 | 2197 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|        5 | 2198 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|        7 | 2199 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|        2 | 2200 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|        5 | 2201 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 2202 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 2203 | `							return SXERR_ABORT;` |
|        - | 2204 | `						}` |
|      ! 0 | 2205 | `						goto done;` |
|        - | 2206 | `					}` |
|        5 | 2207 | `					continue;` |
|        - | 2208 | `				}` |
|      ! 0 | 2209 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2210 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 | 2211 | `				if( rc == SXERR_ABORT ){` |
|        - | 2212 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 | 2213 | `					return SXERR_ABORT;` |
|        - | 2214 | `				}` |
|      ! 0 | 2215 | `				goto done;` |
|        - | 2216 | `			}` |
|    66291 | 2217 | `		}` |
|   187177 | 2218 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|        - | 2219 | `			/* Parse constant */` |
|    54591 | 2220 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|    54591 | 2221 | `			if( rc != SXRET_OK ){` |
|        3 | 2222 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2223 | `					return SXERR_ABORT;` |
|        - | 2224 | `				}` |
|        3 | 2225 | `				goto done;` |
|        - | 2226 | `			}` |
|    27297 | 2227 | `		}else{` |
|   132591 | 2228 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   132591 | 2229 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - | 2230 | `				/* Static method,record that */` |
|    11699 | 2231 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|        - | 2232 | `				/* Advance the stream cursor */` |
|    11699 | 2233 | `				pGen->pIn++;` |
|    11694 | 2234 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    11699 | 2235 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 2236 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2237 | `							"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 | 2238 | `						if( rc == SXERR_ABORT ){` |
|        - | 2239 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 2240 | `							return SXERR_ABORT;` |
|        - | 2241 | `						}` |
|      ! 0 | 2242 | `						goto done;` |
|        - | 2243 | `				}` |
|     5847 | 2244 | `			}` |
|        - | 2245 | `			/* Process method signature (no body for interface methods) */` |
|   132591 | 2246 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   132591 | 2247 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2248 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2249 | `					return SXERR_ABORT;` |
|        - | 2250 | `				}` |
|      ! 0 | 2251 | `				goto done;` |
|        - | 2252 | `			}` |
|        - | 2253 | `		}` |
|        5 | 2254 | `	}` |
|        - | 2255 | `	/* Reject a php-fatal redeclaration before hoisting the interface */` |
|    70289 | 2256 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|        3 | 2257 | `		return SXERR_ABORT;` |
|        - | 2258 | `	}` |
|        - | 2259 | `	/* Install the interface */` |
|    70287 | 2260 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    70287 | 2261 | `	if( rc == SXRET_OK && pBase ){` |
|        - | 2262 | `		/* Inherit from the base interface */` |
|    27303 | 2263 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    13649 | 2264 | `	}` |
|    70287 | 2265 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2266 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2267 | `		return SXERR_ABORT;` |
|        - | 2268 | `	}` |
|    35141 | 2269 | `done:` |
|        - | 2270 | `	/* Point beyond the interface body */` |
|    70291 | 2271 | `	pGen->pIn  = &pEnd[1];` |
|    70291 | 2272 | `	pGen->pEnd = pTmp;` |
|    70291 | 2273 | `	return PH7_OK;` |
|    35149 | 2274 | `}` |
|        - | 2275 | `/*` |
|        - | 2276 | ` * Compile a user-defined class.` |
|        - | 2277 | ` * According to the PHP language reference manual` |
|        - | 2278 | ` *  class` |
|        - | 2279 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|        - | 2280 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|        - | 2281 | ` *  of the properties and methods belonging to the class.` |
|        - | 2282 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|        - | 2283 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|        - | 2284 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|        - | 2285 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - | 2286 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|        - | 2287 | ` *  (called "methods").` |
|        - | 2288 | ` */` |
|        - | 2289 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|        - | 2290 | `typedef struct TraitUseEntry TraitUseEntry;` |
|        - | 2291 | `struct TraitUseEntry {` |
|        - | 2292 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|        - | 2293 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|        - | 2294 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|        - | 2295 | `};` |
|        - | 2296 | `/*` |
|        - | 2297 | ` * Validate that methods implementing interface contracts have compatible` |
|        - | 2298 | ` * signatures: public visibility and at least as many parameters as declared.` |
|        - | 2299 | ` */` |
|   454114 | 2300 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 2301 | `{` |
|        - | 2302 | `	ph7_class **apIface;` |
|        - | 2303 | `	sxu32 nIface,i;` |
|        - | 2304 | `	sxi32 rc;` |
|   454119 | 2305 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      ! 0 | 2306 | `		return SXRET_OK;` |
|        - | 2307 | `	}` |
|   454119 | 2308 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   454119 | 2309 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   879355 | 2310 | `	for(i = 0; i < nIface; i++){` |
|   425241 | 2311 | `		ph7_class *pIface = apIface[i];` |
|        - | 2312 | `		SyHashEntry *pEntry;` |
|   425241 | 2313 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  1252223 | 2314 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   826987 | 2315 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - | 2316 | `			ph7_class_method *pImplMeth;` |
|   826987 | 2317 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|        - | 2318 | `			/* Find the implementing method in the class */` |
|   826987 | 2319 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   826987 | 2320 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       23 | 2321 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|        - | 2322 | `			}` |
|        - | 2323 | `			/* Check visibility: interface methods must be implemented as public */` |
|   826969 | 2324 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        4 | 2325 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - | 2326 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|        1 | 2327 | `					&pClass->sName,pMName,&pIface->sName);` |
|        3 | 2328 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2329 | `					return SXERR_ABORT;` |
|        - | 2330 | `				}` |
|        1 | 2331 | `			}` |
|        - | 2332 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|        - | 2333 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|        - | 2334 | `			 */` |
|        - | 2335 | `			{` |
|   826969 | 2336 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   826969 | 2337 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   826969 | 2338 | `				int sigError = 0;` |
|   826969 | 2339 | `				if( nImplArgs < nIfaceArgs ){` |
|        3 | 2340 | `					sigError = 1;` |
|   826968 | 2341 | `				}else if( nImplArgs > nIfaceArgs ){` |
|        - | 2342 | `					/* Extra parameters must all have default values */` |
|     3907 | 2343 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        - | 2344 | `					sxu32 k;` |
|     7807 | 2345 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|     3907 | 2346 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|        3 | 2347 | `							sigError = 1;` |
|        3 | 2348 | `							break;` |
|        - | 2349 | `						}` |
|     1955 | 2350 | `					}` |
|     1951 | 2351 | `				}` |
|   826969 | 2352 | `				if( sigError ){` |
|        - | 2353 | `					SyBlob sImplSig, sIfaceSig;` |
|        - | 2354 | `					ph7_vm_func_arg *aArgs;` |
|        - | 2355 | `					sxu32 j;` |
|        6 | 2356 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|        6 | 2357 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|        - | 2358 | `					/* Build implementing method signature */` |
|        6 | 2359 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       12 | 2360 | `					for(j = 0; j < nImplArgs; j++){` |
|        8 | 2361 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|        8 | 2362 | `						SyBlobAppend(&sImplSig,"$",1);` |
|        8 | 2363 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 | 2364 | `					}` |
|        - | 2365 | `					/* Build interface method signature */` |
|        6 | 2366 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|       12 | 2367 | `					for(j = 0; j < nIfaceArgs; j++){` |
|        8 | 2368 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|        8 | 2369 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|        8 | 2370 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 | 2371 | `					}` |
|        8 | 2372 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - | 2373 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|        2 | 2374 | `						&pClass->sName,pMName,` |
|        4 | 2375 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|        2 | 2376 | `						&pIface->sName,pMName,` |
|        4 | 2377 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|        6 | 2378 | `					SyBlobRelease(&sImplSig);` |
|        6 | 2379 | `					SyBlobRelease(&sIfaceSig);` |
|        6 | 2380 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2381 | `						return SXERR_ABORT;` |
|        - | 2382 | `					}` |
|        2 | 2383 | `				}` |
|        - | 2384 | `			}` |
|        5 | 2385 | `		}` |
|   212623 | 2386 | `	}` |
|   454119 | 2387 | `	return SXRET_OK;` |
|   227062 | 2388 | `}` |
|        - | 2389 | `/*` |
|        - | 2390 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|        - | 2391 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|        - | 2392 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|        - | 2393 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|        - | 2394 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|        - | 2395 | ` * means that specific hook is still missing.` |
|        - | 2396 | ` */` |
|       38 | 2397 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|        5 | 2398 | `{` |
|        - | 2399 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        - | 2400 | `	ph7_class_attr *pProp;` |
|       38 | 2401 | `	if( pMName->nByte <= nPfx` |
|       27 | 2402 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|        4 | 2403 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|       36 | 2404 | `		return 0; /* not a hook stub */` |
|        - | 2405 | `	}` |
|        7 | 2406 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|        7 | 2407 | `	return pProp != 0` |
|        6 | 2408 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|        3 | 2409 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|       24 | 2410 | `}` |
|        - | 2411 | `/*` |
|        - | 2412 | ` * Append an abstract member's display name to the message blob, translating a` |
|        - | 2413 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|        - | 2414 | ` */` |
|       16 | 2415 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|        4 | 2416 | `{` |
|        - | 2417 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|       16 | 2418 | `	if( pMName->nByte > nPfx` |
|       12 | 2419 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|      ! 0 | 2420 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|      ! 0 | 2421 | `		SyBlobAppend(pMsg,"$",1);` |
|      ! 0 | 2422 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|      ! 0 | 2423 | `		SyBlobAppend(pMsg,"::",2);` |
|      ! 0 | 2424 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|      ! 0 | 2425 | `		return;` |
|        - | 2426 | `	}` |
|       20 | 2427 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|       12 | 2428 | `}` |
|        - | 2429 | `/*` |
|        - | 2430 | ` * Check that a concrete class has no remaining abstract methods.` |
|        - | 2431 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|        - | 2432 | ` */` |
|   454114 | 2433 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 2434 | `{` |
|        - | 2435 | `	ph7_class_method *pMeth;` |
|        - | 2436 | `	SyHashEntry *pEntry;` |
|        - | 2437 | `	sxu32 nAbstract;` |
|        - | 2438 | `	SyBlob sMsg;` |
|        - | 2439 | `	sxi32 rc;` |
|        - | 2440 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   454119 | 2441 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|    19547 | 2442 | `		return SXRET_OK;` |
|        - | 2443 | `	}` |
|        - | 2444 | `	/* Count abstract methods */` |
|   434577 | 2445 | `	nAbstract = 0;` |
|   434577 | 2446 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  6493903 | 2447 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  5842045 | 2448 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  5842045 | 2449 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       27 | 2450 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|        7 | 2451 | `				continue; /* hook requirement met by a plain property (php) */` |
|        - | 2452 | `			}` |
|       20 | 2453 | `			nAbstract++;` |
|        8 | 2454 | `		}` |
|        5 | 2455 | `	}` |
|   434577 | 2456 | `	if( nAbstract == 0 ){` |
|   434563 | 2457 | `		return SXRET_OK;` |
|        - | 2458 | `	}` |
|        - | 2459 | `	/* Build the error message listing all abstract methods with origins */` |
|       18 | 2460 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       18 | 2461 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|        - | 2462 | `		"be declared abstract or implement the remaining method%s (",` |
|        7 | 2463 | `		&pClass->sName,nAbstract,` |
|        7 | 2464 | `		(nAbstract > 1 ? "s" : ""),` |
|        7 | 2465 | `		(nAbstract > 1 ? "s" : ""));` |
|        - | 2466 | `	/* Second pass: list methods with origins */` |
|        - | 2467 | `	{` |
|       18 | 2468 | `		sxu32 nListed = 0;` |
|       18 | 2469 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|       36 | 2470 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|       22 | 2471 | `			ph7_class *pOrigin = 0;` |
|        - | 2472 | `			SyString *pMName;` |
|       22 | 2473 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|       22 | 2474 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        3 | 2475 | `				continue;` |
|        - | 2476 | `			}` |
|       20 | 2477 | `			pMName = &pMeth->sFunc.sName;` |
|       20 | 2478 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|      ! 0 | 2479 | `				continue; /* hook requirement met by a plain property (php) */` |
|        - | 2480 | `			}` |
|       20 | 2481 | `			if( nListed > 0 ){` |
|        3 | 2482 | `				SyBlobAppend(&sMsg,", ",2);` |
|        1 | 2483 | `			}` |
|        - | 2484 | `			/* Find the origin of this abstract method.` |
|        - | 2485 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|        - | 2486 | `			 * inheritance chains) take precedence for interface-declared` |
|        - | 2487 | `			 * methods. Abstract class methods only win when the class` |
|        - | 2488 | `			 * itself declared the abstract method (not inherited from` |
|        - | 2489 | `			 * an interface). Trait methods are adopted into the using` |
|        - | 2490 | `			 * class's namespace.` |
|        - | 2491 | `			 */` |
|        - | 2492 | `			{` |
|        - | 2493 | `				ph7_class **apIface;` |
|        - | 2494 | `				ph7_class **apTrait;` |
|        - | 2495 | `				ph7_class *pWalk;` |
|        - | 2496 | `				sxu32 i;` |
|        - | 2497 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|        - | 2498 | `				 * (one that was written in the class body, not inherited from an` |
|        - | 2499 | `				 * interface). PHP attributes origin to the declaring class.` |
|        - | 2500 | `				 */` |
|       20 | 2501 | `				if( pClass->pBase ){` |
|       10 | 2502 | `					pWalk = pClass->pBase;` |
|       18 | 2503 | `					while( pWalk ){` |
|        - | 2504 | `						ph7_class_method *pParentMeth;` |
|       12 | 2505 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|       12 | 2506 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        - | 2507 | `							/* Exclude methods that came from an interface anywhere` |
|        - | 2508 | `							 * in this class's ancestor chain.` |
|        - | 2509 | `							 */` |
|       12 | 2510 | `							int fromIface = 0;` |
|       12 | 2511 | `							ph7_class *pAnc = pWalk;` |
|       16 | 2512 | `							while( pAnc ){` |
|        - | 2513 | `								ph7_class **apPI;` |
|        - | 2514 | `								sxu32 j;` |
|       14 | 2515 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|       14 | 2516 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       10 | 2517 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       10 | 2518 | `										fromIface = 1;` |
|       10 | 2519 | `										break;` |
|        - | 2520 | `									}` |
|      ! 0 | 2521 | `								}` |
|       14 | 2522 | `								if( fromIface ) break;` |
|        5 | 2523 | `								pAnc = pAnc->pBase;` |
|        1 | 2524 | `							}` |
|       12 | 2525 | `							if( !fromIface ){` |
|        3 | 2526 | `								pOrigin = pWalk;` |
|        3 | 2527 | `								break;` |
|        - | 2528 | `							}` |
|        4 | 2529 | `						}` |
|       10 | 2530 | `						pWalk = pWalk->pBase;` |
|        2 | 2531 | `					}` |
|        4 | 2532 | `				}` |
|        - | 2533 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|        - | 2534 | `				 * each interface's own parent chain for the deepest origin.` |
|        - | 2535 | `				 */` |
|       20 | 2536 | `				if( !pOrigin ){` |
|       18 | 2537 | `					pWalk = pClass;` |
|       40 | 2538 | `					while( pWalk && !pOrigin ){` |
|       26 | 2539 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|       26 | 2540 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|       16 | 2541 | `							ph7_class *pIface = apIface[i];` |
|       16 | 2542 | `							ph7_class *pDeepest = 0;` |
|       28 | 2543 | `							while( pIface ){` |
|       16 | 2544 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|       16 | 2545 | `									pDeepest = pIface;` |
|        6 | 2546 | `								}` |
|       16 | 2547 | `								pIface = pIface->pBase;` |
|        4 | 2548 | `							}` |
|       16 | 2549 | `							if( pDeepest ){` |
|       16 | 2550 | `								pOrigin = pDeepest;` |
|       16 | 2551 | `								break;` |
|        - | 2552 | `							}` |
|      ! 0 | 2553 | `						}` |
|       26 | 2554 | `						pWalk = pWalk->pBase;` |
|        4 | 2555 | `					}` |
|        7 | 2556 | `				}` |
|        - | 2557 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|       20 | 2558 | `				if( !pOrigin ){` |
|        3 | 2559 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        3 | 2560 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|        3 | 2561 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|        3 | 2562 | `							pOrigin = pClass;` |
|        3 | 2563 | `							break;` |
|        - | 2564 | `						}` |
|      ! 0 | 2565 | `					}` |
|        1 | 2566 | `				}` |
|        - | 2567 | `			}` |
|       20 | 2568 | `			if( pOrigin ){` |
|       20 | 2569 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|       12 | 2570 | `			}else{` |
|        - | 2571 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|      ! 0 | 2572 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|        - | 2573 | `			}` |
|       20 | 2574 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|       20 | 2575 | `			nListed++;` |
|        4 | 2576 | `		}` |
|        - | 2577 | `	}` |
|       18 | 2578 | `	SyBlobAppend(&sMsg,")",1);` |
|       25 | 2579 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|       14 | 2580 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|       18 | 2581 | `	SyBlobRelease(&sMsg);` |
|       18 | 2582 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2583 | `		return SXERR_ABORT;` |
|        - | 2584 | `	}` |
|       18 | 2585 | `	return SXRET_OK;` |
|   227062 | 2586 | `}` |
|        - | 2587 | `/*` |
|        - | 2588 | ` * Parse a class/interface name reference from the current token stream.` |
|        - | 2589 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|        - | 2590 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|        - | 2591 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|        - | 2592 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|        - | 2593 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|        - | 2594 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|        - | 2595 | ` */` |
|   516894 | 2596 | `PH7_PRIVATE sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|        5 | 2597 | `{` |
|   516899 | 2598 | `	int isAbsolute = 0;` |
|   516899 | 2599 | `	SyToken *pStart = pGen->pIn;` |
|        - | 2600 | `	SyBlob sName;` |
|   516899 | 2601 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|     4553 | 2602 | `		isAbsolute = 1;` |
|     4553 | 2603 | `		pGen->pIn++;` |
|     2274 | 2604 | `	}` |
|   516899 | 2605 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        9 | 2606 | `		pGen->pIn = pStart;` |
|        9 | 2607 | `		return SXERR_INVALID;` |
|        - | 2608 | `	}` |
|   516893 | 2609 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   516893 | 2610 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   516893 | 2611 | `	pGen->pIn++;` |
|   775371 | 2612 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   258488 | 2613 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       28 | 2614 | `		SyBlobAppend(&sName,"\\",1);` |
|       28 | 2615 | `		pGen->pIn++;` |
|       28 | 2616 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       28 | 2617 | `		pGen->pIn++;` |
|        2 | 2618 | `	}` |
|   516893 | 2619 | `	if( isAbsolute ){` |
|     4551 | 2620 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     2278 | 2621 | `	}else{` |
|        - | 2622 | `		SyString sRaw;` |
|   512347 | 2623 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   512347 | 2624 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|        - | 2625 | `	}` |
|   516893 | 2626 | `	SyBlobRelease(&sName);` |
|   516893 | 2627 | `	return SXRET_OK;` |
|   258452 | 2628 | `}` |
|        - | 2629 | `/*` |
|        - | 2630 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|        - | 2631 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|        - | 2632 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|        - | 2633 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|        - | 2634 | ` * either direction cannot run unbounded.` |
|        - | 2635 | ` */` |
|        - | 2636 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|   199010 | 2637 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|        5 | 2638 | `{` |
|        - | 2639 | `	ph7_class **apParent;` |
|        - | 2640 | `	sxu32 n;` |
|   526699 | 2641 | `	while( pInterface ){` |
|   335495 | 2642 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|      ! 0 | 2643 | `			return FALSE;` |
|        - | 2644 | `		}` |
|   374495 | 2645 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    78000 | 2646 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|     7811 | 2647 | `			return TRUE;` |
|        - | 2648 | `		}` |
|   327689 | 2649 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|   327691 | 2650 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|        3 | 2651 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|      ! 0 | 2652 | `				return TRUE;` |
|        - | 2653 | `			}` |
|        2 | 2654 | `		}` |
|   327689 | 2655 | `		pInterface = pInterface->pBase;` |
|   327689 | 2656 | `		iDepth++;` |
|        5 | 2657 | `	}` |
|   191209 | 2658 | `	return FALSE;` |
|    99510 | 2659 | `}` |
|   199008 | 2660 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|        5 | 2661 | `{` |
|   199013 | 2662 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|        5 | 2663 | `}` |
|        - | 2664 | `/*` |
|        - | 2665 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|        - | 2666 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|        - | 2667 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|        - | 2668 | ` */` |
|     7806 | 2669 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|        5 | 2670 | `{` |
|     7815 | 2671 | `	while( pBase ){` |
|       10 | 2672 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|        2 | 2673 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|        3 | 2674 | `			return TRUE;` |
|        - | 2675 | `		}` |
|       10 | 2676 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|        6 | 2677 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|        3 | 2678 | `			return TRUE;` |
|        - | 2679 | `		}` |
|        5 | 2680 | `		pBase = pBase->pBase;` |
|        1 | 2681 | `	}` |
|     7807 | 2682 | `	return FALSE;` |
|     3908 | 2683 | `}` |
|        - | 2684 | `/*` |
|        - | 2685 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|        - | 2686 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|        - | 2687 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|        - | 2688 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|        - | 2689 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|        - | 2690 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|        - | 2691 | ` * pClass->aEnumCases for cases().` |
|        - | 2692 | ` */` |
|     7850 | 2693 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 2694 | `{` |
|     7855 | 2695 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2696 | `	SySet *pInstrContainer;` |
|        - | 2697 | `	ph7_class_attr *pCase;` |
|        - | 2698 | `	SyString *pName;` |
|        - | 2699 | `	sxi32 rc;` |
|     7855 | 2700 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|     7855 | 2701 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2702 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 2703 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|      ! 0 | 2704 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2705 | `			return SXERR_ABORT;` |
|        - | 2706 | `		}` |
|      ! 0 | 2707 | `		goto Synchronize;` |
|        - | 2708 | `	}` |
|     7855 | 2709 | `	pName = &pGen->pIn->sData;` |
|        - | 2710 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|     7855 | 2711 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      ! 0 | 2712 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2713 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|      ! 0 | 2714 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2715 | `			return SXERR_ABORT;` |
|        - | 2716 | `		}` |
|      ! 0 | 2717 | `		goto Synchronize;` |
|        - | 2718 | `	}` |
|     7855 | 2719 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 2720 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|     7855 | 2721 | `	if( pCase == 0 ){` |
|      ! 0 | 2722 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2723 | `		return SXERR_ABORT;` |
|        - | 2724 | `	}` |
|     7855 | 2725 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|     7855 | 2726 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 2727 | `		return SXERR_ABORT;` |
|        - | 2728 | `	}` |
|     7855 | 2729 | `	pGen->pIn++; /* Jump the case name */` |
|     7855 | 2730 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|     7841 | 2731 | `		if( pClass->nEnumBacking == 0 ){` |
|        8 | 2732 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        2 | 2733 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|        6 | 2734 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2735 | `				return SXERR_ABORT;` |
|        - | 2736 | `			}` |
|        6 | 2737 | `			goto Synchronize;` |
|        - | 2738 | `		}` |
|     7837 | 2739 | `		pGen->pIn++; /* Jump the equal sign */` |
|        - | 2740 | `		/* Compile the backing value expression into the case's own container` |
|        - | 2741 | `		 * (same technique as class constants). */` |
|     7837 | 2742 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     7837 | 2743 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|     7837 | 2744 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     7837 | 2745 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2746 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 2747 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|      ! 0 | 2748 | `		}` |
|     7837 | 2749 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|     7837 | 2750 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     7837 | 2751 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2752 | `			return SXERR_ABORT;` |
|        - | 2753 | `		}` |
|     3921 | 2754 | `	}else{` |
|       17 | 2755 | `		if( pClass->nEnumBacking != 0 ){` |
|      ! 0 | 2756 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 2757 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|      ! 0 | 2758 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2759 | `				return SXERR_ABORT;` |
|        - | 2760 | `			}` |
|      ! 0 | 2761 | `			goto Synchronize;` |
|        - | 2762 | `		}` |
|        - | 2763 | `	}` |
|     7851 | 2764 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|     7851 | 2765 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2766 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2767 | `		return SXERR_ABORT;` |
|        - | 2768 | `	}` |
|     7851 | 2769 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|     7851 | 2770 | `	return SXRET_OK;` |
|        2 | 2771 | `Synchronize:` |
|        - | 2772 | `	/* Synchronize with the first semi-colon */` |
|       14 | 2773 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       10 | 2774 | `		pGen->pIn++;` |
|        2 | 2775 | `	}` |
|        6 | 2776 | `	return SXERR_CORRUPT;` |
|     3930 | 2777 | `}` |
|        - | 2778 | `/*` |
|        - | 2779 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|        - | 2780 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|        - | 2781 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|        - | 2782 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|        - | 2783 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|        - | 2784 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|        - | 2785 | ` * pointers into it (see the constructor-promotion precedent above).` |
|        - | 2786 | ` */` |
|     3930 | 2787 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 2788 | `{` |
|        - | 2789 | `	SyToken *pSaveIn,*pSaveEnd;` |
|        - | 2790 | `	const char *zBack;` |
|        - | 2791 | `	SySet sToken;` |
|        - | 2792 | `	char *zSrc;` |
|        - | 2793 | `	sxu32 nSrc,nMax;` |
|     3935 | 2794 | `	sxi32 rc = SXRET_OK;` |
|     3935 | 2795 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|     3930 | 2796 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|     3935 | 2797 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|     3935 | 2798 | `	if( zSrc == 0 ){` |
|      ! 0 | 2799 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2800 | `		return SXERR_ABORT;` |
|        - | 2801 | `	}` |
|     3935 | 2802 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|     3935 | 2803 | `	if( pClass->nEnumBacking != 0 ){` |
|     5873 | 2804 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        - | 2805 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|        - | 2806 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|        - | 2807 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|     1956 | 2808 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|     1961 | 2809 | `	}else{` |
|       30 | 2810 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        9 | 2811 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|        - | 2812 | `	}` |
|     3935 | 2813 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|     3935 | 2814 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|     3935 | 2815 | `	pSaveIn = pGen->pIn;` |
|     3935 | 2816 | `	pSaveEnd = pGen->pEnd;` |
|     3935 | 2817 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|     3935 | 2818 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|    15689 | 2819 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|    11759 | 2820 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|        5 | 2821 | `	}` |
|     3935 | 2822 | `	pGen->pIn = pSaveIn;` |
|     3935 | 2823 | `	pGen->pEnd = pSaveEnd;` |
|     3935 | 2824 | `	SySetRelease(&sToken);` |
|     3935 | 2825 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|     1970 | 2826 | `}` |
|        - | 2827 | `/*` |
|        - | 2828 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|        - | 2829 | ` * __call/__callStatic/__invoke stay allowed).` |
|        - | 2830 | ` */` |
|        - | 2831 | `static const char *azEnumBannedMagic[] = {` |
|        - | 2832 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|        - | 2833 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|        - | 2834 | `};` |
|        - | 2835 | `/*` |
|        - | 2836 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|        - | 2837 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|        - | 2838 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|        - | 2839 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|        - | 2840 | ` * and before the class is installed.` |
|        - | 2841 | ` */` |
|     3930 | 2842 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|        5 | 2843 | `{` |
|        - | 2844 | `	SyHashEntry *pEntry;` |
|        - | 2845 | `	sxi32 rc;` |
|        - | 2846 | `	sxu32 n;` |
|        - | 2847 | `	/* php: "Enum %s cannot include properties" */` |
|     3935 | 2848 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    11787 | 2849 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     7859 | 2850 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     7859 | 2851 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        3 | 2852 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|        1 | 2853 | `				"Enum %z cannot include properties",&pClass->sName);` |
|        3 | 2854 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2855 | `				return SXERR_ABORT;` |
|        - | 2856 | `			}` |
|        3 | 2857 | `			break;` |
|        - | 2858 | `		}` |
|        5 | 2859 | `	}` |
|        - | 2860 | `	/* php: "Enum %s cannot include magic method %s" */` |
|    55025 | 2861 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|    76635 | 2862 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|    51095 | 2863 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|      ! 0 | 2864 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 2865 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|      ! 0 | 2866 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2867 | `				return SXERR_ABORT;` |
|        - | 2868 | `			}` |
|      ! 0 | 2869 | `		}` |
|    25550 | 2870 | `	}` |
|        - | 2871 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|        - | 2872 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|        - | 2873 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|        - | 2874 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|        - | 2875 | `	{` |
|        - | 2876 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|        - | 2877 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|        - | 2878 | `		ph7_class_attr *pAttr;` |
|     3935 | 2879 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 2880 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|     3935 | 2881 | `		if( pAttr == 0 ){` |
|      ! 0 | 2882 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2883 | `			return SXERR_ABORT;` |
|        - | 2884 | `		}` |
|     3935 | 2885 | `		pAttr->nType = MEMOBJ_STRING;` |
|     3935 | 2886 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|     3935 | 2887 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|     3935 | 2888 | `		if( pClass->nEnumBacking != 0 ){` |
|     3917 | 2889 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 2890 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|     3917 | 2891 | `			if( pAttr == 0 ){` |
|      ! 0 | 2892 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2893 | `				return SXERR_ABORT;` |
|        - | 2894 | `			}` |
|     3917 | 2895 | `			pAttr->nType = pClass->nEnumBacking;` |
|     3917 | 2896 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        7 | 2897 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|        4 | 2898 | `			}else{` |
|     3911 | 2899 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|        - | 2900 | `			}` |
|     3917 | 2901 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|     1956 | 2902 | `		}` |
|        - | 2903 | `	}` |
|     3935 | 2904 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|     1970 | 2905 | `}` |
|        - | 2906 | `/*` |
|        - | 2907 | ` * Compile a class declaration, named or anonymous.` |
|        - | 2908 | ` *` |
|        - | 2909 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|        - | 2910 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|        - | 2911 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|        - | 2912 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|        - | 2913 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|        - | 2914 | ` * implements, body, install) is shared by both paths.` |
|        - | 2915 | ` */` |
|   454164 | 2916 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|        - | 2917 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|        5 | 2918 | `{` |
|   454169 | 2919 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2920 | `	ph7_class *pClass,*pBase;` |
|        - | 2921 | `	SyToken *pEnd,*pTmp;` |
|        - | 2922 | `	sxi32 iProtection;` |
|        - | 2923 | `	SySet aInterfaces;` |
|        - | 2924 | `	SySet aUseEntries;` |
|        - | 2925 | `	sxi32 iAttrflags;` |
|        - | 2926 | `	SyString *pName;` |
|        - | 2927 | `	sxi32 nKwrd;` |
|        - | 2928 | `	sxi32 rc;` |
|        - | 2929 | `	/* Jump the 'class' keyword */` |
|   454169 | 2930 | `	pGen->pIn++;` |
|   454169 | 2931 | `	if( pAnonName ){` |
|        - | 2932 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|        - | 2933 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|        - | 2934 | `		 * then use the synthesized name. */` |
|       34 | 2935 | `		*ppArgStart = *ppArgEnd = 0;` |
|       34 | 2936 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        7 | 2937 | `			pGen->pIn++; /* Jump '(' */` |
|        7 | 2938 | `			*ppArgStart = pGen->pIn;` |
|       10 | 2939 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|        3 | 2940 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|        7 | 2941 | `			pGen->pIn = *ppArgEnd;` |
|        7 | 2942 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|        3 | 2943 | `		}` |
|       34 | 2944 | `		pName = pAnonName;` |
|       34 | 2945 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|       19 | 2946 | `	}else{` |
|   454139 | 2947 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - | 2948 | `			/* Syntax error */` |
|      ! 0 | 2949 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|      ! 0 | 2950 | `			if( rc == SXERR_ABORT ){` |
|        - | 2951 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 2952 | `				return SXERR_ABORT;` |
|        - | 2953 | `			}` |
|        - | 2954 | `			/* Synchronize with the first semi-colon or curly braces */` |
|      ! 0 | 2955 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|      ! 0 | 2956 | `				pGen->pIn++;` |
|      ! 0 | 2957 | `			}` |
|      ! 0 | 2958 | `			return SXRET_OK;` |
|        - | 2959 | `		}` |
|        - | 2960 | `		/* Extract class name */` |
|   454139 | 2961 | `		pName = &pGen->pIn->sData;` |
|        - | 2962 | `		/* Advance the stream cursor */` |
|   454139 | 2963 | `		pGen->pIn++;` |
|        - | 2964 | `		/* Build FQN and obtain a raw class */ {` |
|        - | 2965 | `			SyBlob sFQN;` |
|        - | 2966 | `			SyString sFQNStr;` |
|   454139 | 2967 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   454139 | 2968 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|   454139 | 2969 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   454139 | 2970 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   454139 | 2971 | `			SyBlobRelease(&sFQN);` |
|        - | 2972 | `		}` |
|        - | 2973 | `	}` |
|   454169 | 2974 | `	if( pClass == 0 ){` |
|      ! 0 | 2975 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2976 | `		return SXERR_ABORT;` |
|        - | 2977 | `	}` |
|   454164 | 2978 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|     3939 | 2979 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|        - | 2980 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|     3919 | 2981 | `		pGen->pIn++; /* Jump ':' */` |
|     3914 | 2982 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     3919 | 2983 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|        7 | 2984 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|        7 | 2985 | `			pGen->pIn++;` |
|     3912 | 2986 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     3913 | 2987 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|     3911 | 2988 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|     3911 | 2989 | `			pGen->pIn++;` |
|     1958 | 2990 | `		}else{` |
|        3 | 2991 | `			SyToken *pTok = pGen->pIn;` |
|        3 | 2992 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|        4 | 2993 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|        1 | 2994 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|        3 | 2995 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2996 | `				return SXERR_ABORT;` |
|        - | 2997 | `			}` |
|        3 | 2998 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|        3 | 2999 | `				pGen->pIn++; /* Skip the bogus type token */` |
|        1 | 3000 | `			}` |
|        - | 3001 | `		}` |
|     1957 | 3002 | `	}` |
|   454169 | 3003 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|   454169 | 3004 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 3005 | `		return SXERR_ABORT;` |
|        - | 3006 | `	}` |
|        - | 3007 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   454169 | 3008 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   454169 | 3009 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|        - | 3010 | `	/* Assume a standalone class */` |
|   454169 | 3011 | `	pBase = 0;` |
|   454169 | 3012 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   370807 | 3013 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   370807 | 3014 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|        - | 3015 | `			SyBlob sResolved;` |
|        - | 3016 | `			SyString sBaseName;` |
|        - | 3017 | `			sxu32 nRefLine;` |
|   249789 | 3018 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|        - | 3019 | `				/* php parse-fatals here (enums have no inheritance) */` |
|      ! 0 | 3020 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 3021 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|      ! 0 | 3022 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3023 | `					return SXERR_ABORT;` |
|        - | 3024 | `				}` |
|      ! 0 | 3025 | `			}` |
|   249789 | 3026 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   249789 | 3027 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   249789 | 3028 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   249789 | 3029 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        3 | 3030 | `				SyBlobRelease(&sResolved);` |
|        4 | 3031 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 3032 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|        1 | 3033 | `					pName);` |
|        3 | 3034 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|        3 | 3035 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3036 | `					return SXERR_ABORT;` |
|        - | 3037 | `				}` |
|        3 | 3038 | `				return SXRET_OK;` |
|        - | 3039 | `			}` |
|   374678 | 3040 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   249782 | 3041 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   249787 | 3042 | `			SyStringInitFromBuf(&sBaseName,` |
|        - | 3043 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 3044 | `			/* Interfaces are not allowed */` |
|   249787 | 3045 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|      ! 0 | 3046 | `				pBase = pBase->pNextName;` |
|      ! 0 | 3047 | `			}` |
|   249787 | 3048 | `			if( pBase == 0 ){` |
|      ! 0 | 3049 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 3050 | `					"Nonexistent base class '%z'",&sBaseName);` |
|      ! 0 | 3051 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3052 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 3053 | `					return SXERR_ABORT;` |
|        - | 3054 | `				}` |
|      ! 0 | 3055 | `			}else{` |
|   249787 | 3056 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|        4 | 3057 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 3058 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|        3 | 3059 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3060 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 3061 | `						return SXERR_ABORT;` |
|        - | 3062 | `					}` |
|        3 | 3063 | `					pBase = 0; /* Never inherit from an enum */` |
|   249786 | 3064 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|      ! 0 | 3065 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 3066 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|      ! 0 | 3067 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3068 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 3069 | `						return SXERR_ABORT;` |
|        - | 3070 | `					}` |
|      ! 0 | 3071 | `				}` |
|        - | 3072 | `			}` |
|   249787 | 3073 | `			SyBlobRelease(&sResolved);` |
|   249787 | 3074 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 | 3075 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|      ! 0 | 3076 | `			}` |
|   124891 | 3077 | `		}` |
|   370805 | 3078 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|        - | 3079 | `			ph7_class *pInterface;` |
|        - | 3080 | `			/* Interface implementation */` |
|   136631 | 3081 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   130695 | 3082 | `			for(;;){` |
|        - | 3083 | `				SyBlob sResolved;` |
|        - | 3084 | `				SyString sIntName;` |
|        - | 3085 | `				sxu32 nRefLine;` |
|   199013 | 3086 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   199013 | 3087 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   199013 | 3088 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 3089 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 3090 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 3091 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|      ! 0 | 3092 | `						pName);` |
|      ! 0 | 3093 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3094 | `						return SXERR_ABORT;` |
|        - | 3095 | `					}` |
|      ! 0 | 3096 | `					break;` |
|        - | 3097 | `				}` |
|   398021 | 3098 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|   199008 | 3099 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   199013 | 3100 | `				SyStringInitFromBuf(&sIntName,` |
|        - | 3101 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 3102 | `				/* Only interfaces are allowed */` |
|   199013 | 3103 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 3104 | `					pInterface = pInterface->pNextName;` |
|      ! 0 | 3105 | `				}` |
|   199013 | 3106 | `				if( pInterface == 0 ){` |
|      ! 0 | 3107 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 3108 | `						"Nonexistent base interface '%z'",&sIntName);` |
|      ! 0 | 3109 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3110 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 3111 | `						return SXERR_ABORT;` |
|        - | 3112 | `					}` |
|      ! 0 | 3113 | `				}else{` |
|        - | 3114 | `					/* Reject user classes that try to implement Throwable` |
|        - | 3115 | `					 * directly (or via an interface that extends Throwable)` |
|        - | 3116 | `					 * unless they already extend Exception or Error.` |
|        - | 3117 | `					 * Exception and Error themselves are compiled from the` |
|        - | 3118 | `					 * built-in library and are exempt by FQN — a namespaced` |
|        - | 3119 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|   199013 | 3120 | `					SyString *pFqn = &pClass->sName;` |
|   199013 | 3121 | `					int bIsExceptionOrError =` |
|   103406 | 3122 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   300465 | 3123 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|   197066 | 3124 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|     3912 | 3125 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   202911 | 3126 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    11712 | 3127 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|     3901 | 3128 | `						!bIsExceptionOrError ){` |
|       12 | 3129 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3130 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|        3 | 3131 | `							&pClass->sName);` |
|        9 | 3132 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3133 | `							SyBlobRelease(&sResolved);` |
|      ! 0 | 3134 | `							return SXERR_ABORT;` |
|        - | 3135 | `						}` |
|        - | 3136 | `						/* Skip registration so the follow-up abstract-method` |
|        - | 3137 | `						 * check does not produce a duplicate fatal. */` |
|        6 | 3138 | `					}else{` |
|   199007 | 3139 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|        - | 3140 | `					}` |
|        - | 3141 | `				}` |
|   199013 | 3142 | `				SyBlobRelease(&sResolved);` |
|   199013 | 3143 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    68318 | 3144 | `					break;` |
|        - | 3145 | `				}` |
|    62387 | 3146 | `				pGen->pIn++;/* Jump the comma */` |
|        5 | 3147 | `			}` |
|    68313 | 3148 | `		}` |
|   185400 | 3149 | `	}` |
|   454167 | 3150 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - | 3151 | `		/* Syntax error */` |
|      ! 0 | 3152 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|      ! 0 | 3153 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 3154 | `		if( rc == SXERR_ABORT ){` |
|        - | 3155 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3156 | `			return SXERR_ABORT;` |
|        - | 3157 | `		}` |
|      ! 0 | 3158 | `		return SXRET_OK;` |
|        - | 3159 | `	}` |
|   454167 | 3160 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   454167 | 3161 | `	pEnd = 0; /* cc warning */` |
|        - | 3162 | `	/* Delimit the class body */` |
|   454167 | 3163 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   454167 | 3164 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 3165 | `		/* Syntax error */` |
|      ! 0 | 3166 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|      ! 0 | 3167 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 3168 | `		if( rc == SXERR_ABORT ){` |
|        - | 3169 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3170 | `			return SXERR_ABORT;` |
|        - | 3171 | `		}` |
|      ! 0 | 3172 | `		return SXRET_OK;` |
|        - | 3173 | `	}` |
|        - | 3174 | `	/* The delimiter token is the class body's closing brace */` |
|   454167 | 3175 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 3176 | `	/* Swap token stream */` |
|   454167 | 3177 | `	pTmp = pGen->pEnd;` |
|   454167 | 3178 | `	pGen->pEnd = pEnd;` |
|        - | 3179 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|   454167 | 3180 | `	pClass->iFlags \|= iFlags;` |
|        - | 3181 | `	/* Start the parse process */` |
|  1701446 | 3182 | `	for(;;){` |
|        - | 3183 | `		/* Jump leading/trailing semi-colons */` |
|  4886661 | 3184 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   890059 | 3185 | `			pGen->pIn++;` |
|        5 | 3186 | `		}` |
|  3996607 | 3187 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3188 | `			/* End of class body */` |
|   454125 | 3189 | `			break;` |
|        - | 3190 | `		}` |
|        - | 3191 | `		/* Bind a directly-preceding docblock to this member */` |
|  3542487 | 3192 | `		GenStateSetPendingDoc(&(*pGen));` |
|  3542482 | 3193 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  1771246 | 3194 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|      ! 0 | 3195 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3196 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 | 3197 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 3198 | `			if( rc == SXERR_ABORT ){` |
|        - | 3199 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 3200 | `				return SXERR_ABORT;` |
|        - | 3201 | `			}` |
|      ! 0 | 3202 | `			goto done;` |
|        - | 3203 | `		}` |
|        - | 3204 | `		/* Assume public visibility */` |
|  3542487 | 3205 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  3542487 | 3206 | `		iAttrflags = 0;` |
|        - | 3207 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|        - | 3208 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|        - | 3209 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|        - | 3210 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  3542487 | 3211 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 3212 | `			int bMod = 0;` |
|      ! 0 | 3213 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 | 3214 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        - | 3215 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|        - | 3216 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|        - | 3217 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|        - | 3218 | `			 * that the generic keyword dispatch would misread as a method. */` |
|      ! 0 | 3219 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      ! 0 | 3220 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 | 3221 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|      ! 0 | 3222 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|      ! 0 | 3223 | `			}` |
|      ! 0 | 3224 | `			if( !bMod ){` |
|      ! 0 | 3225 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 3226 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 3227 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3228 | `						return SXERR_ABORT;` |
|        - | 3229 | `					}` |
|      ! 0 | 3230 | `					goto done;` |
|        - | 3231 | `				}` |
|      ! 0 | 3232 | `				continue;` |
|        - | 3233 | `			}` |
|      ! 0 | 3234 | `		}` |
|  3542487 | 3235 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 3236 | `			/* Extract the current keyword */` |
|  3542487 | 3237 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  3542487 | 3238 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|        - | 3239 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|     7855 | 3240 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|     7855 | 3241 | `				if( rc != SXRET_OK ){` |
|        6 | 3242 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3243 | `						return SXERR_ABORT;` |
|        - | 3244 | `					}` |
|        6 | 3245 | `					goto done;` |
|        - | 3246 | `				}` |
|     7851 | 3247 | `				continue;` |
|        - | 3248 | `			}` |
|  3534637 | 3249 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 3250 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|        - | 3251 | `				TraitUseEntry sUse;` |
|    15687 | 3252 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    15687 | 3253 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|    15687 | 3254 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|     7849 | 3255 | `				for(;;){` |
|        - | 3256 | `					ph7_class *pTrait;` |
|        - | 3257 | `					SyBlob sResolved;` |
|        - | 3258 | `					SyString sTraitName;` |
|    15695 | 3259 | `					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|        - | 3260 | `					/* A trait name is a full class reference: it may be qualified or` |
|        - | 3261 | ``					 * fully-qualified (`use Foo\Bar\T;`, `use \Foo\Bar\T;`) — the generated`` |
|        - | 3262 | `					 * PHPUnit mocks name their traits absolutely. Parse it with the shared` |
|        - | 3263 | `					 * class-reference reader (handles the leading '\', every '\'-segment and` |
|        - | 3264 | `					 * namespace/import resolution) instead of a single-identifier read, which` |
|        - | 3265 | `					 * choked on the first '\'. */` |
|    15695 | 3266 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    15695 | 3267 | `					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 3268 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 3269 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|      ! 0 | 3270 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|      ! 0 | 3271 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3272 | `							return SXERR_ABORT;` |
|        - | 3273 | `						}` |
|      ! 0 | 3274 | `						break;` |
|        - | 3275 | `					}` |
|    31385 | 3276 | `					pTrait = PH7_VmExtractClass(pGen->pVm,` |
|    15690 | 3277 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    15695 | 3278 | `					SyStringInitFromBuf(&sTraitName,` |
|        - | 3279 | `						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 3280 | `					/* Only traits are allowed */` |
|    15695 | 3281 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 3282 | `						pTrait = pTrait->pNextName;` |
|      ! 0 | 3283 | `					}` |
|    15695 | 3284 | `					if( pTrait == 0 ){` |
|      ! 0 | 3285 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|        - | 3286 | `							"'%z' is not a trait",&sTraitName);` |
|      ! 0 | 3287 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3288 | `							SyBlobRelease(&sResolved);` |
|      ! 0 | 3289 | `							return SXERR_ABORT;` |
|        - | 3290 | `						}` |
|      ! 0 | 3291 | `					}else{` |
|    15695 | 3292 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|        - | 3293 | `					}` |
|    15695 | 3294 | `					SyBlobRelease(&sResolved);` |
|        - | 3295 | `					/* GenStateParseClassReference already advanced past the whole name —` |
|        - | 3296 | `					 * continue only across a comma-separated trait list. */` |
|    15695 | 3297 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     7846 | 3298 | `						break;` |
|        - | 3299 | `					}` |
|       10 | 3300 | `					pGen->pIn++; /* Jump the comma */` |
|        2 | 3301 | `				}` |
|        - | 3302 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|    15687 | 3303 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - | 3304 | `					SyToken *pBlock;` |
|       12 | 3305 | `					pGen->pIn++; /* Jump '{' */` |
|       12 | 3306 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       12 | 3307 | `					sUse.pResolvStart = pGen->pIn;` |
|       12 | 3308 | `					sUse.pResolvEnd = pBlock;` |
|       12 | 3309 | `					if( pBlock < pGen->pEnd ){` |
|       12 | 3310 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|        7 | 3311 | `					}else{` |
|      ! 0 | 3312 | `						pGen->pIn = pGen->pEnd;` |
|        - | 3313 | `					}` |
|        5 | 3314 | `				}` |
|    15687 | 3315 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|        - | 3316 | `				/* The semicolon will be consumed by the outer loop */` |
|    15687 | 3317 | `				continue;` |
|        - | 3318 | `			}` |
|  3518955 | 3319 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        - | 3320 | `				int nSetTok;` |
|  2972695 | 3321 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|  2972695 | 3322 | `				if( nSetVis ){` |
|        - | 3323 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|        - | 3324 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|        3 | 3325 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        3 | 3326 | `					pGen->pIn += nSetTok;` |
|        2 | 3327 | `				}else{` |
|  2972693 | 3328 | `					iProtection = nKwrd;` |
|  2972693 | 3329 | `					pGen->pIn++; /* Jump the visibility token */` |
|        - | 3330 | `					/* Optional asymmetric set-visibility after the read` |
|        - | 3331 | ``					 * visibility: `public private(set) int $x`. */`` |
|  2972693 | 3332 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|  2972693 | 3333 | `					if( nSetVis ){` |
|        9 | 3334 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        9 | 3335 | `						pGen->pIn += nSetTok;` |
|        4 | 3336 | `					}` |
|        - | 3337 | `				}` |
|        - | 3338 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|        - | 3339 | ``				 * `public private(set) readonly int $x`. */`` |
|  2972695 | 3340 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       28 | 3341 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       28 | 3342 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       12 | 3343 | `				}` |
|  2972690 | 3344 | `				if( pGen->pIn >= pGen->pEnd` |
|  2972695 | 3345 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 3346 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3347 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 | 3348 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 3349 | `					if( rc == SXERR_ABORT ){` |
|        - | 3350 | `						/* Error count limit reached,abort immediately */` |
|      ! 0 | 3351 | `						return SXERR_ABORT;` |
|        - | 3352 | `					}` |
|      ! 0 | 3353 | `					goto done;` |
|        - | 3354 | `				}` |
|  2972695 | 3355 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 3356 | `					/* Attribute declaration (untyped) */` |
|   526923 | 3357 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   526923 | 3358 | `					if( rc != SXRET_OK ){` |
|       11 | 3359 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3360 | `							return SXERR_ABORT;` |
|        - | 3361 | `						}` |
|       11 | 3362 | `						goto done;` |
|        - | 3363 | `					}` |
|   534874 | 3364 | `					continue;` |
|        - | 3365 | `				}` |
|  2445777 | 3366 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 3367 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|    15929 | 3368 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    15929 | 3369 | `					if( rc != SXRET_OK ){` |
|        8 | 3370 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3371 | `							return SXERR_ABORT;` |
|        - | 3372 | `						}` |
|        8 | 3373 | `						goto done;` |
|        - | 3374 | `					}` |
|    15923 | 3375 | `					continue;` |
|        - | 3376 | `				}` |
|        - | 3377 | `				/* Extract the keyword */` |
|  2429853 | 3378 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1214924 | 3379 | `			}` |
|  2976113 | 3380 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|        - | 3381 | `				/* Process constant declaration */` |
|   288639 | 3382 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|   288639 | 3383 | `				if( rc != SXRET_OK ){` |
|       11 | 3384 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3385 | `						return SXERR_ABORT;` |
|        - | 3386 | `					}` |
|       11 | 3387 | `					goto done;` |
|        - | 3388 | `				}` |
|   144318 | 3389 | `			}else{` |
|  2687479 | 3390 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - | 3391 | `					/* Static method or attribute,record that */` |
|   101549 | 3392 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|   101549 | 3393 | `					pGen->pIn++; /* Jump the static keyword */` |
|   101549 | 3394 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 3395 | `						int nSetTok;` |
|    74221 | 3396 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|    74221 | 3397 | `						if( nSetVis ){` |
|        - | 3398 | ``							/* `static private(set) int $x` — read side stays public */`` |
|        3 | 3399 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        3 | 3400 | `							pGen->pIn += nSetTok;` |
|        2 | 3401 | `						}else{` |
|        - | 3402 | `							/* Extract the keyword */` |
|    74219 | 3403 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    74219 | 3404 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 3405 | `								iProtection = nKwrd;` |
|      ! 0 | 3406 | `								pGen->pIn++; /* Jump the visibility token */` |
|      ! 0 | 3407 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|      ! 0 | 3408 | `								if( nSetVis ){` |
|      ! 0 | 3409 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|      ! 0 | 3410 | `									pGen->pIn += nSetTok;` |
|      ! 0 | 3411 | `								}` |
|      ! 0 | 3412 | `							}` |
|        - | 3413 | `						}` |
|    37108 | 3414 | `					}` |
|        - | 3415 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|        - | 3416 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|        - | 3417 | `					 * than a generic "expecting method" parse error. */` |
|   101549 | 3418 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 3419 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 | 3420 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|      ! 0 | 3421 | `					}` |
|   101544 | 3422 | `					if( pGen->pIn >= pGen->pEnd` |
|   101549 | 3423 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 3424 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3425 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|      ! 0 | 3426 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 3427 | `						if( rc == SXERR_ABORT ){` |
|        - | 3428 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 3429 | `							return SXERR_ABORT;` |
|        - | 3430 | `						}` |
|      ! 0 | 3431 | `						goto done;` |
|        - | 3432 | `					}` |
|   101549 | 3433 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 3434 | `						/* Attribute declaration */` |
|    27329 | 3435 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    27329 | 3436 | `						if( rc != SXRET_OK ){` |
|        3 | 3437 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 3438 | `								return SXERR_ABORT;` |
|        - | 3439 | `							}` |
|        3 | 3440 | `							goto done;` |
|        - | 3441 | `						}` |
|    27327 | 3442 | `						continue;` |
|        - | 3443 | `					}` |
|    74225 | 3444 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 3445 | `						/* Typed static attribute declaration */` |
|       19 | 3446 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       19 | 3447 | `						if( rc != SXRET_OK ){` |
|        3 | 3448 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 3449 | `								return SXERR_ABORT;` |
|        - | 3450 | `							}` |
|        3 | 3451 | `							goto done;` |
|        - | 3452 | `						}` |
|       17 | 3453 | `						continue;` |
|        - | 3454 | `					}` |
|        - | 3455 | `					/* Extract the keyword */` |
|    74209 | 3456 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  2623037 | 3457 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        - | 3458 | `					/* Abstract method,record that.` |
|        - | 3459 | `					 * PHL used to also mark the whole CLASS abstract here, silently` |
|        - | 3460 | ``					 * promoting `class C{abstract function m();}` -- which php rejects`` |
|        - | 3461 | `					 * outright -- into a valid abstract class. That promotion is why` |
|        - | 3462 | `					 * GenStateCheckAbstractMethods never fired for it: by the time the` |
|        - | 3463 | `					 * check ran, the class looked declared-abstract. The declaration is` |
|        - | 3464 | `					 * now diagnosed where the method name is known (see the install` |
|        - | 3465 | `					 * site), so the class flag stays what the SOURCE said. */` |
|     7823 | 3466 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        - | 3467 | `					/* Advance the stream cursor */` |
|     7823 | 3468 | `					pGen->pIn++;` |
|     7823 | 3469 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     7823 | 3470 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     7823 | 3471 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     7821 | 3472 | `							iProtection = nKwrd;` |
|     7821 | 3473 | `							pGen->pIn++; /* Jump the visibility token */` |
|     3908 | 3474 | `						}` |
|     3909 | 3475 | `					}` |
|     7823 | 3476 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     7818 | 3477 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 3478 | `							/* Static method */` |
|      ! 0 | 3479 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 3480 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 3481 | `					}` |
|     7823 | 3482 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|     7818 | 3483 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|        - | 3484 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|        - | 3485 | `							 * HOOKED property declaration. Route anything that is not a` |
|        - | 3486 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|        - | 3487 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|        - | 3488 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|        6 | 3489 | `							if( pGen->pIn < pGen->pEnd` |
|        7 | 3490 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|        3 | 3491 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|        7 | 3492 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        7 | 3493 | `								if( rc != SXRET_OK ){` |
|      ! 0 | 3494 | `									if( rc == SXERR_ABORT ){` |
|      ! 0 | 3495 | `										return SXERR_ABORT;` |
|        - | 3496 | `									}` |
|      ! 0 | 3497 | `									goto done;` |
|        - | 3498 | `								}` |
|        7 | 3499 | `								continue;` |
|        - | 3500 | `							}` |
|      ! 0 | 3501 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3502 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|      ! 0 | 3503 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 3504 | `							if( rc == SXERR_ABORT ){` |
|        - | 3505 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 3506 | `								return SXERR_ABORT;` |
|        - | 3507 | `							}` |
|      ! 0 | 3508 | `							goto done;` |
|        - | 3509 | `					}` |
|     7817 | 3510 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  2582023 | 3511 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|        - | 3512 | `					/* final method ,record that */` |
|       21 | 3513 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       21 | 3514 | `					pGen->pIn++; /* Jump the final keyword */` |
|       21 | 3515 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 3516 | `						/* Extract the keyword */` |
|       21 | 3517 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       21 | 3518 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       10 | 3519 | `							iProtection = nKwrd;` |
|       10 | 3520 | `							pGen->pIn++; /* Jump the visibility token */` |
|        4 | 3521 | `						}` |
|        9 | 3522 | `					}` |
|       21 | 3523 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       18 | 3524 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|        - | 3525 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|        - | 3526 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|        - | 3527 | `							 * child class is compiled (PH7_ClassInherit). */` |
|       14 | 3528 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|       14 | 3529 | `							if( rc != SXRET_OK ){` |
|      ! 0 | 3530 | `								if( rc == SXERR_ABORT ){` |
|      ! 0 | 3531 | `									return SXERR_ABORT;` |
|        - | 3532 | `								}` |
|      ! 0 | 3533 | `								goto done;` |
|        - | 3534 | `							}` |
|       14 | 3535 | `							continue;` |
|        - | 3536 | `					}` |
|        8 | 3537 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        6 | 3538 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 3539 | `							/* Static method */` |
|      ! 0 | 3540 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 3541 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 3542 | `					}` |
|        8 | 3543 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        6 | 3544 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 3545 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3546 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|      ! 0 | 3547 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 3548 | `							if( rc == SXERR_ABORT ){` |
|        - | 3549 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 3550 | `								return SXERR_ABORT;` |
|        - | 3551 | `							}` |
|      ! 0 | 3552 | `							goto done;` |
|        - | 3553 | `					}` |
|        8 | 3554 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        3 | 3555 | `				}` |
|  2660121 | 3556 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 3557 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3558 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|      ! 0 | 3559 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 3560 | `						if( rc == SXERR_ABORT ){` |
|        - | 3561 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 3562 | `							return SXERR_ABORT;` |
|        - | 3563 | `						}` |
|      ! 0 | 3564 | `						goto done;` |
|        - | 3565 | `				}` |
|  2660121 | 3566 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|        7 | 3567 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|        7 | 3568 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|      ! 0 | 3569 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3570 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 3571 | `						if( rc == SXERR_ABORT ){` |
|        - | 3572 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 3573 | `							return SXERR_ABORT;` |
|        - | 3574 | `						}` |
|      ! 0 | 3575 | `						goto done;` |
|        - | 3576 | `					}` |
|        - | 3577 | `					/* Attribute declaration */` |
|        7 | 3578 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        4 | 3579 | `				}else{` |
|        - | 3580 | `					/* Process method declaration */` |
|  2660115 | 3581 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 3582 | `				}` |
|  2660121 | 3583 | `				if( rc != SXRET_OK ){` |
|       16 | 3584 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3585 | `						return SXERR_ABORT;` |
|        - | 3586 | `					}` |
|       16 | 3587 | `					goto done;` |
|        - | 3588 | `				}` |
|        - | 3589 | `			}` |
|  1474370 | 3590 | `		}else{` |
|        - | 3591 | `			/* Attribute declaration */` |
|      ! 0 | 3592 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 3593 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 3594 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3595 | `					return SXERR_ABORT;` |
|        - | 3596 | `				}` |
|      ! 0 | 3597 | `				goto done;` |
|        - | 3598 | `			}` |
|        - | 3599 | `		}` |
|        5 | 3600 | `	}` |
|        - | 3601 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|        - | 3602 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|        - | 3603 | `	 */` |
|        - | 3604 | `	{` |
|        - | 3605 | `		TraitUseEntry *apUse;` |
|        - | 3606 | `		sxu32 nU;` |
|   454125 | 3607 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   469807 | 3608 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|    15687 | 3609 | `			TraitUseEntry *pUse = &apUse[nU];` |
|    15687 | 3610 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|    15687 | 3611 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|    15687 | 3612 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|        - | 3613 | `			sxu32 nT;` |
|    15687 | 3614 | `			if( !hasResolution ){` |
|        - | 3615 | `				/* No conflict resolution block: use standard trait application */` |
|    31355 | 3616 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|    15683 | 3617 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|    15683 | 3618 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 3619 | `						break;` |
|        - | 3620 | `					}` |
|     7844 | 3621 | `				}` |
|     7841 | 3622 | `			}else{` |
|        - | 3623 | `				/* With resolution block: copy attributes, record traits,` |
|        - | 3624 | `				 * then use the block to resolve method conflicts.` |
|        - | 3625 | `				 */` |
|        - | 3626 | `				SyToken *pR;` |
|       24 | 3627 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       14 | 3628 | `					ph7_class *pTR = apTrait[nT];` |
|        - | 3629 | `					ph7_class_attr *pAR;` |
|        - | 3630 | `					SyHashEntry *pER;` |
|        - | 3631 | `					SyString *pNR;` |
|       14 | 3632 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|       20 | 3633 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|      ! 0 | 3634 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|      ! 0 | 3635 | `						pNR = &pAR->sName;` |
|      ! 0 | 3636 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      ! 0 | 3637 | `							SyHashInsertTail(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|      ! 0 | 3638 | `						}` |
|      ! 0 | 3639 | `					}` |
|       14 | 3640 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|        8 | 3641 | `				}` |
|        - | 3642 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       12 | 3643 | `				pR = pUse->pResolvStart;` |
|       26 | 3644 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 3645 | `					SyString sTrait,sMethod;` |
|        - | 3646 | `					ph7_class *pSrcTrait;` |
|        - | 3647 | `					ph7_class_method *pMeth;` |
|        - | 3648 | `					sxi32 nRKwrd;` |
|       40 | 3649 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       26 | 3650 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       16 | 3651 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       16 | 3652 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       16 | 3653 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       16 | 3654 | `					sMethod = pR->sData;` |
|       16 | 3655 | `					pR++;` |
|       16 | 3656 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 3657 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 3658 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 3659 | `							sTrait = sMethod;` |
|        7 | 3660 | `							pR++;` |
|        7 | 3661 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 3662 | `							sMethod = pR->sData;` |
|        7 | 3663 | `							pR++;` |
|        3 | 3664 | `						}` |
|        3 | 3665 | `					}` |
|       16 | 3666 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 3667 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 3668 | `						continue;` |
|        - | 3669 | `					}` |
|       16 | 3670 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       16 | 3671 | `					pR++;` |
|       16 | 3672 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|        5 | 3673 | `						pSrcTrait = 0;` |
|        7 | 3674 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        7 | 3675 | `							SyString *pTN = &apTrait[nT]->sName;` |
|       10 | 3676 | `							if( pTN->nByte >= sTrait.nByte &&` |
|        6 | 3677 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        5 | 3678 | `								pSrcTrait = apTrait[nT];` |
|        5 | 3679 | `								break;` |
|        - | 3680 | `							}` |
|        2 | 3681 | `						}` |
|        5 | 3682 | `						if( pSrcTrait ){` |
|        5 | 3683 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        5 | 3684 | `							if( pMeth ){` |
|        5 | 3685 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|        5 | 3686 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|        5 | 3687 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|        2 | 3688 | `								}` |
|        2 | 3689 | `							}` |
|        2 | 3690 | `						}` |
|        2 | 3691 | `					}` |
|       34 | 3692 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        2 | 3693 | `				}` |
|        - | 3694 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|       24 | 3695 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        - | 3696 | `					ph7_class_method *pMR;` |
|        - | 3697 | `					SyHashEntry *pER;` |
|        - | 3698 | `					SyString *pNR;` |
|       14 | 3699 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|       40 | 3700 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|       22 | 3701 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|       22 | 3702 | `						pNR = &pMR->sFunc.sName;` |
|       22 | 3703 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       13 | 3704 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|        6 | 3705 | `						}` |
|        2 | 3706 | `					}` |
|        8 | 3707 | `				}` |
|        - | 3708 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       12 | 3709 | `				pR = pUse->pResolvStart;` |
|       26 | 3710 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 3711 | `					SyString sTrait,sMethod,sAlias;` |
|        - | 3712 | `					ph7_class *pSrcTrait;` |
|        - | 3713 | `					ph7_class_method *pMeth;` |
|       26 | 3714 | `					int hasQual = 0;` |
|        - | 3715 | `					sxi32 nRKwrd;` |
|       40 | 3716 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       26 | 3717 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       16 | 3718 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       16 | 3719 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       16 | 3720 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|       16 | 3721 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       16 | 3722 | `					sMethod = pR->sData;` |
|       16 | 3723 | `					pR++;` |
|       16 | 3724 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 3725 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 3726 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 3727 | `							sTrait = sMethod;` |
|        7 | 3728 | `							hasQual = 1;` |
|        7 | 3729 | `							pR++;` |
|        7 | 3730 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 3731 | `							sMethod = pR->sData;` |
|        7 | 3732 | `							pR++;` |
|        3 | 3733 | `						}` |
|        3 | 3734 | `					}` |
|       16 | 3735 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 3736 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 3737 | `						continue;` |
|        - | 3738 | `					}` |
|       16 | 3739 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       16 | 3740 | `					pR++;` |
|       16 | 3741 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       12 | 3742 | `						sxi32 iNewVis = -1;` |
|       12 | 3743 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|        7 | 3744 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|        7 | 3745 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|        7 | 3746 | `								iNewVis = nAK;` |
|        7 | 3747 | `								pR++;` |
|        3 | 3748 | `							}` |
|        3 | 3749 | `						}` |
|       12 | 3750 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       10 | 3751 | `							sAlias = pR->sData;` |
|       10 | 3752 | `							pR++;` |
|        4 | 3753 | `						}` |
|       12 | 3754 | `						pMeth = 0;` |
|       12 | 3755 | `						if( hasQual ){` |
|        3 | 3756 | `							pSrcTrait = 0;` |
|        5 | 3757 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        5 | 3758 | `								SyString *pTN = &apTrait[nT]->sName;` |
|        7 | 3759 | `								if( pTN->nByte >= sTrait.nByte &&` |
|        4 | 3760 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        3 | 3761 | `									pSrcTrait = apTrait[nT];` |
|        3 | 3762 | `									break;` |
|        - | 3763 | `								}` |
|        2 | 3764 | `							}` |
|        3 | 3765 | `							if( pSrcTrait ){` |
|        3 | 3766 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        1 | 3767 | `							}` |
|        2 | 3768 | `						}else{` |
|        9 | 3769 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|        - | 3770 | `						}` |
|       12 | 3771 | `						if( pMeth ){` |
|       12 | 3772 | `							if( sAlias.nByte > 0 ){` |
|        - | 3773 | `								/* Create a shallow copy of the method struct for the alias` |
|        - | 3774 | `								 * so it can carry its own visibility without affecting the original.` |
|        - | 3775 | `								 */` |
|        - | 3776 | `								ph7_class_method *pAlias;` |
|        - | 3777 | `								char *zAliasDup;` |
|       10 | 3778 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       10 | 3779 | `								if( pAlias ){` |
|       10 | 3780 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       10 | 3781 | `									if( iNewVis >= 0 ){` |
|        5 | 3782 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 3783 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 3784 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        2 | 3785 | `									}` |
|       10 | 3786 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       10 | 3787 | `									if( zAliasDup ){` |
|       10 | 3788 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|        4 | 3789 | `									}` |
|        6 | 3790 | `								}` |
|        7 | 3791 | `							}else if( iNewVis >= 0 ){` |
|        - | 3792 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|        - | 3793 | `								ph7_class_method *pCopy;` |
|        3 | 3794 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        3 | 3795 | `								if( pCopy ){` |
|        3 | 3796 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|        3 | 3797 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|        3 | 3798 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 3799 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 3800 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        - | 3801 | `									/* Replace the method in the class hash */` |
|        3 | 3802 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|        3 | 3803 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|        1 | 3804 | `								}` |
|        1 | 3805 | `							}` |
|        5 | 3806 | `						}` |
|        5 | 3807 | `						SXUNUSED(hasQual);` |
|        5 | 3808 | `					}` |
|       20 | 3809 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        2 | 3810 | `				}` |
|        - | 3811 | `			}` |
|    15687 | 3812 | `			SySetRelease(&pUse->aTraits);` |
|     7846 | 3813 | `		}` |
|        - | 3814 | `	}` |
|   454125 | 3815 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - | 3816 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|        - | 3817 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|     3935 | 3818 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|     3935 | 3819 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3820 | `			SySetRelease(&aUseEntries);` |
|      ! 0 | 3821 | `			SySetRelease(&aInterfaces);` |
|      ! 0 | 3822 | `			return SXERR_ABORT;` |
|        - | 3823 | `		}` |
|     1965 | 3824 | `	}` |
|        - | 3825 | `	/* Reject a php-fatal redeclaration before hoisting the class */` |
|   454125 | 3826 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|        9 | 3827 | `		return SXERR_ABORT;` |
|        - | 3828 | `	}` |
|        - | 3829 | `	/* Install the class */` |
|   454119 | 3830 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   454119 | 3831 | `	if( rc == SXRET_OK ){` |
|        - | 3832 | `		ph7_class **apInterface;` |
|        - | 3833 | `		sxu32 n;` |
|   454119 | 3834 | `		if( pBase ){` |
|        - | 3835 | `			/* Inherit from base class and mark as a subclass */` |
|   249785 | 3836 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   124890 | 3837 | `		}` |
|   454119 | 3838 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   653121 | 3839 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|        - | 3840 | `			/* Implements one or more interface */` |
|   199007 | 3841 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|   199007 | 3842 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 3843 | `				break;` |
|        - | 3844 | `			}` |
|    99506 | 3845 | `		}` |
|        - | 3846 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|        - | 3847 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|   454119 | 3848 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|     3933 | 3849 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|     3933 | 3850 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 3851 | `				pIntf = pIntf->pNextName;` |
|      ! 0 | 3852 | `			}` |
|     3933 | 3853 | `			if( pIntf ){` |
|     3933 | 3854 | `				PH7_ClassImplement(pClass,pIntf);` |
|     1964 | 3855 | `			}` |
|     3933 | 3856 | `			if( pClass->nEnumBacking != 0 ){` |
|     3917 | 3857 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|     3917 | 3858 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 3859 | `					pIntf = pIntf->pNextName;` |
|      ! 0 | 3860 | `				}` |
|     3917 | 3861 | `				if( pIntf ){` |
|     3917 | 3862 | `					PH7_ClassImplement(pClass,pIntf);` |
|     1956 | 3863 | `				}` |
|     1956 | 3864 | `			}` |
|     1964 | 3865 | `		}` |
|        - | 3866 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|        - | 3867 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|   454114 | 3868 | `		if( rc == SXRET_OK` |
|   454114 | 3869 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|   454119 | 3870 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   230095 | 3871 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|        - | 3872 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   230095 | 3873 | `			if( pStringable ){` |
|   230095 | 3874 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   230095 | 3875 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|        - | 3876 | `				sxu32 i;` |
|   230095 | 3877 | `				int bAlready = 0;` |
|   276875 | 3878 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    58481 | 3879 | `					if( apImpl[i] == pStringable ){` |
|    11701 | 3880 | `						bAlready = 1;` |
|    11701 | 3881 | `						break;` |
|        - | 3882 | `					}` |
|    23395 | 3883 | `				}` |
|   230095 | 3884 | `				if( !bAlready ){` |
|   218399 | 3885 | `					PH7_ClassImplement(pClass,pStringable);` |
|   109197 | 3886 | `				}` |
|   115045 | 3887 | `			}` |
|   115045 | 3888 | `		}` |
|        - | 3889 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   454119 | 3890 | `		if( rc == SXRET_OK ){` |
|   454119 | 3891 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   454119 | 3892 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 3893 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 3894 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 3895 | `				return SXERR_ABORT;` |
|        - | 3896 | `			}` |
|   227057 | 3897 | `		}` |
|        - | 3898 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   454119 | 3899 | `		if( rc == SXRET_OK ){` |
|   454119 | 3900 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   454119 | 3901 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 3902 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 3903 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 3904 | `				return SXERR_ABORT;` |
|        - | 3905 | `			}` |
|   227057 | 3906 | `		}` |
|   227057 | 3907 | `	}` |
|   454119 | 3908 | `	SySetRelease(&aUseEntries);` |
|   454119 | 3909 | `	SySetRelease(&aInterfaces);` |
|   454119 | 3910 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3911 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3912 | `		return SXERR_ABORT;` |
|        - | 3913 | `	}` |
|   227057 | 3914 | `done:` |
|        - | 3915 | `	/* Point beyond the class body */` |
|   454161 | 3916 | `	pGen->pIn = &pEnd[1];` |
|   454161 | 3917 | `	pGen->pEnd = pTmp;` |
|   454161 | 3918 | `	return PH7_OK;` |
|   227087 | 3919 | `}` |
|        - | 3920 | `/* Compile a named class declaration (the common case). */` |
|   454134 | 3921 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|        5 | 3922 | `{` |
|   454139 | 3923 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|        5 | 3924 | `}` |
|        - | 3925 | `/*` |
|        - | 3926 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|        - | 3927 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|        - | 3928 | ` * compile + install the class body once (at compile time, like every other` |
|        - | 3929 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|        - | 3930 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|        - | 3931 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|        - | 3932 | ` */` |
|       30 | 3933 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 | 3934 | `{` |
|        - | 3935 | `	char zName[128];         /* Synthesized class name */` |
|        - | 3936 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|        - | 3937 | `	SyString sName;` |
|        - | 3938 | `	SyToken *pArgStart,*pArgEnd;` |
|       34 | 3939 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|        - | 3940 | `	                              * is keyed to this 'class' token */` |
|        - | 3941 | `	ph7_value *pObj;` |
|       34 | 3942 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3943 | `	sxu32 nIdx,nLen;` |
|        - | 3944 | `	sxi32 nArg,rc;` |
|       15 | 3945 | `	SXUNUSED(iCompileFlag);` |
|        - | 3946 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|       34 | 3947 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       34 | 3948 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 | 3949 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      ! 0 | 3950 | `	}` |
|       34 | 3951 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - | 3952 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|        - | 3953 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|        - | 3954 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|       34 | 3955 | `	pArgStart = pArgEnd = 0;` |
|       34 | 3956 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|       34 | 3957 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3958 | `		return rc;` |
|        - | 3959 | `	}` |
|        - | 3960 | `	{` |
|        - | 3961 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|       34 | 3962 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|       30 | 3963 | `		if( pAnonClass` |
|       34 | 3964 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 3965 | `			return SXERR_ABORT;` |
|        - | 3966 | `		}` |
|        - | 3967 | `	}` |
|        - | 3968 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|        - | 3969 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|       34 | 3970 | `	nArg = 0;` |
|       34 | 3971 | `	if( pArgStart < pArgEnd ){` |
|        7 | 3972 | `		SyToken *pSavedIn = pGen->pIn;` |
|        7 | 3973 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 3974 | `		SyToken *pArgNext;` |
|        7 | 3975 | `		pGen->pIn = pArgStart;` |
|        7 | 3976 | `		pGen->pEnd = pArgEnd;` |
|       13 | 3977 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|        7 | 3978 | `			if( pGen->pIn < pArgNext ){` |
|        7 | 3979 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|        7 | 3980 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3981 | `					pGen->pIn = pSavedIn;` |
|      ! 0 | 3982 | `					pGen->pEnd = pSavedEnd;` |
|      ! 0 | 3983 | `					return SXERR_ABORT;` |
|        - | 3984 | `				}` |
|        7 | 3985 | `				nArg++;` |
|        3 | 3986 | `			}` |
|        7 | 3987 | `			pGen->pIn = &pArgNext[1];` |
|        1 | 3988 | `		}` |
|        7 | 3989 | `		pGen->pIn = pSavedIn;` |
|        7 | 3990 | `		pGen->pEnd = pSavedEnd;` |
|        3 | 3991 | `	}` |
|        - | 3992 | `	/* Load the synthesized class name */` |
|       34 | 3993 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       34 | 3994 | `	if( pObj == 0 ){` |
|      ! 0 | 3995 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 3996 | `		return SXERR_ABORT;` |
|        - | 3997 | `	}` |
|       34 | 3998 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       34 | 3999 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - | 4000 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|       34 | 4001 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       34 | 4002 | `	return SXRET_OK;` |
|       19 | 4003 | `}` |
|        - | 4004 | `/*` |
|        - | 4005 | ` * Compile a user-defined abstract class.` |
|        - | 4006 | ` *  According to the PHP language reference manual` |
|        - | 4007 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|        - | 4008 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|        - | 4009 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|        - | 4010 | ` *   the method's signature - they cannot define the implementation.` |
|        - | 4011 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|        - | 4012 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|        - | 4013 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|        - | 4014 | ` *   method is defined as protected, the function implementation must be defined as either` |
|        - | 4015 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|        - | 4016 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|        - | 4017 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|        - | 4018 | ` *   could differ.` |
|        - | 4019 | ` */` |
|        - | 4020 | `/*` |
|        - | 4021 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|        - | 4022 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|        - | 4023 | ` * receives the corresponding PH7_CLASS_* bit.` |
|        - | 4024 | ` */` |
| 14513500 | 4025 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|        5 | 4026 | `{` |
| 14513505 | 4027 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  8686617 | 4028 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  8686617 | 4029 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  8631999 | 4030 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  4296454 | 4031 | `	}` |
| 14419801 | 4032 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
| 14419741 | 4033 | `	return FALSE;` |
|  7256755 | 4034 | `}` |
|        - | 4035 | `/*` |
|        - | 4036 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|        - | 4037 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|        - | 4038 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|        - | 4039 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|        - | 4040 | ` */` |
| 14419736 | 4041 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|        5 | 4042 | `{` |
| 14419741 | 4043 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
| 14419741 | 4044 | `	sxi32 iFlags = 0,iFlag;` |
| 14513505 | 4045 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    93769 | 4046 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|        5 | 4047 | `			pDup = pIn;` |
|        2 | 4048 | `		}` |
|    93769 | 4049 | `		iFlags \|= iFlag;` |
|    93769 | 4050 | `		pIn++;` |
|        5 | 4051 | `	}` |
| 14419741 | 4052 | `	*ppIn = pIn;` |
| 14419741 | 4053 | `	if( ppDup ){ *ppDup = pDup; }` |
| 14419741 | 4054 | `	return iFlags;` |
|        5 | 4055 | `}` |
|        - | 4056 | `/*` |
|        - | 4057 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|        - | 4058 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|        - | 4059 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|        - | 4060 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|        - | 4061 | `` * `readonly`) to their existing handlers.`` |
|        - | 4062 | ` */` |
| 14376762 | 4063 | `PH7_PRIVATE int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|        5 | 4064 | `{` |
| 14376767 | 4065 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  7239158 | 4066 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
| 14402149 | 4067 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|        5 | 4068 | `}` |
|        - | 4069 | `/*` |
|        - | 4070 | ` * Compile a class declaration carrying one or more leading modifiers` |
|        - | 4071 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|        - | 4072 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|        - | 4073 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|        - | 4074 | `` * `abstract`+`final` pair, like PHP.`` |
|        - | 4075 | ` */` |
|    42974 | 4076 | `PH7_PRIVATE sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|        5 | 4077 | `{` |
|        - | 4078 | `	SyToken *pDup;` |
|    42979 | 4079 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|        - | 4080 | `	sxi32 rc;` |
|    42979 | 4081 | `	if( pDup ){` |
|        4 | 4082 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|        2 | 4083 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|        3 | 4084 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4085 | `			return SXERR_ABORT;` |
|        - | 4086 | `		}` |
|        1 | 4087 | `	}` |
|    42974 | 4088 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    21492 | 4089 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|        3 | 4090 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4091 | `			"Cannot use the final modifier on an abstract class");` |
|        3 | 4092 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4093 | `			return SXERR_ABORT;` |
|        - | 4094 | `		}` |
|        1 | 4095 | `	}` |
|    42979 | 4096 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    21492 | 4097 | `}` |
|        - | 4098 | `/*` |
|        - | 4099 | ` * Compile a user-defined trait.` |
|        - | 4100 | ` *  Traits are similar to classes, but only intended to group functionality` |
|        - | 4101 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|        - | 4102 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|        - | 4103 | ` */` |
|     7896 | 4104 | `PH7_PRIVATE sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|        5 | 4105 | `{` |
|     7901 | 4106 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 4107 | `	ph7_class *pClass;` |
|        - | 4108 | `	SyToken *pEnd,*pTmp;` |
|        - | 4109 | `	sxi32 iProtection;` |
|        - | 4110 | `	sxi32 iAttrflags;` |
|        - | 4111 | `	SyString *pName;` |
|        - | 4112 | `	sxi32 nKwrd;` |
|        - | 4113 | `	sxi32 rc;` |
|        - | 4114 | `	/* Jump the 'trait' keyword */` |
|     7901 | 4115 | `	pGen->pIn++;` |
|     7901 | 4116 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 4117 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|      ! 0 | 4118 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4119 | `			return SXERR_ABORT;` |
|        - | 4120 | `		}` |
|      ! 0 | 4121 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|      ! 0 | 4122 | `			pGen->pIn++;` |
|      ! 0 | 4123 | `		}` |
|      ! 0 | 4124 | `		return SXRET_OK;` |
|        - | 4125 | `	}` |
|        - | 4126 | `	/* Extract trait name */` |
|     7901 | 4127 | `	pName = &pGen->pIn->sData;` |
|     7901 | 4128 | `	pGen->pIn++;` |
|        - | 4129 | `	/* Build FQN and obtain a raw class */ {` |
|        - | 4130 | `		SyBlob sFQN;` |
|        - | 4131 | `		SyString sFQNStr;` |
|     7901 | 4132 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     7901 | 4133 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     7901 | 4134 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|     7901 | 4135 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     7901 | 4136 | `		SyBlobRelease(&sFQN);` |
|        - | 4137 | `	}` |
|     7901 | 4138 | `	if( pClass == 0 ){` |
|      ! 0 | 4139 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4140 | `		return SXERR_ABORT;` |
|        - | 4141 | `	}` |
|     7901 | 4142 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     7901 | 4143 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 4144 | `		return SXERR_ABORT;` |
|        - | 4145 | `	}` |
|        - | 4146 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|     7901 | 4147 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 | 4148 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|      ! 0 | 4149 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 4150 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4151 | `			return SXERR_ABORT;` |
|        - | 4152 | `		}` |
|      ! 0 | 4153 | `		return SXRET_OK;` |
|        - | 4154 | `	}` |
|     7901 | 4155 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     7901 | 4156 | `	pEnd = 0;` |
|     7901 | 4157 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|     7901 | 4158 | `	if( pEnd >= pGen->pEnd ){` |
|      ! 0 | 4159 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|      ! 0 | 4160 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 4161 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4162 | `			return SXERR_ABORT;` |
|        - | 4163 | `		}` |
|      ! 0 | 4164 | `		return SXRET_OK;` |
|        - | 4165 | `	}` |
|        - | 4166 | `	/* The delimiter token is the trait body's closing brace */` |
|     7901 | 4167 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 4168 | `	/* Swap token stream */` |
|     7901 | 4169 | `	pTmp = pGen->pEnd;` |
|     7901 | 4170 | `	pGen->pEnd = pEnd;` |
|        - | 4171 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|     7901 | 4172 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|        - | 4173 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|    56622 | 4174 | `	for(;;){` |
|   160087 | 4175 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|    23427 | 4176 | `			pGen->pIn++;` |
|        5 | 4177 | `		}` |
|   136665 | 4178 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     7901 | 4179 | `			break;` |
|        - | 4180 | `		}` |
|        - | 4181 | `		/* Bind a directly-preceding docblock to this member */` |
|   128769 | 4182 | `		GenStateSetPendingDoc(&(*pGen));` |
|   128769 | 4183 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|      ! 0 | 4184 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4185 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 4186 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 4187 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4188 | `				return SXERR_ABORT;` |
|        - | 4189 | `			}` |
|      ! 0 | 4190 | `			goto done;` |
|        - | 4191 | `		}` |
|   128769 | 4192 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   128769 | 4193 | `		iAttrflags = 0;` |
|   128769 | 4194 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   128769 | 4195 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   128769 | 4196 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 4197 | `				/* Trait uses another trait: use OtherTrait; */` |
|        5 | 4198 | `				pGen->pIn++; /* Jump 'use' */` |
|        2 | 4199 | `				for(;;){` |
|        - | 4200 | `					ph7_class *pUsedTrait;` |
|        - | 4201 | `					SyString *pUsedName;` |
|        5 | 4202 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 4203 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 4204 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|      ! 0 | 4205 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4206 | `							return SXERR_ABORT;` |
|        - | 4207 | `						}` |
|      ! 0 | 4208 | `						break;` |
|        - | 4209 | `					}` |
|        5 | 4210 | `					pUsedName = &pGen->pIn->sData;` |
|        - | 4211 | `					{` |
|        - | 4212 | `						SyBlob sResolved;` |
|        5 | 4213 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        5 | 4214 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|        7 | 4215 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|        4 | 4216 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|        5 | 4217 | `						SyBlobRelease(&sResolved);` |
|        - | 4218 | `					}` |
|        5 | 4219 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 4220 | `						pUsedTrait = pUsedTrait->pNextName;` |
|      ! 0 | 4221 | `					}` |
|        5 | 4222 | `					if( pUsedTrait == 0 ){` |
|        4 | 4223 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        1 | 4224 | `							"'%z' is not a trait",pUsedName);` |
|        3 | 4225 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4226 | `							return SXERR_ABORT;` |
|        - | 4227 | `						}` |
|        2 | 4228 | `					}else{` |
|        3 | 4229 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|        - | 4230 | `					}` |
|        5 | 4231 | `					pGen->pIn++;` |
|        5 | 4232 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|        3 | 4233 | `						break;` |
|        - | 4234 | `					}` |
|      ! 0 | 4235 | `					pGen->pIn++;` |
|      ! 0 | 4236 | `				}` |
|        5 | 4237 | `				continue;` |
|        - | 4238 | `			}` |
|   128765 | 4239 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|   128747 | 4240 | `				iProtection = nKwrd;` |
|   128747 | 4241 | `				pGen->pIn++;` |
|        - | 4242 | ``				/* Optional `readonly` after the visibility (PHP 8.1): `private readonly T`` |
|        - | 4243 | ``				 * $x` — the generated PHPUnit runtime traits (StubApi, …) declare their`` |
|        - | 4244 | `				 * readonly state this way. Mirrors the class-body attribute parser. */` |
|   128747 | 4245 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|        5 | 4246 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|        5 | 4247 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        2 | 4248 | `				}` |
|   128742 | 4249 | `				if( pGen->pIn >= pGen->pEnd` |
|   128747 | 4250 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 4251 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4252 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 4253 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 4254 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 4255 | `						return SXERR_ABORT;` |
|        - | 4256 | `					}` |
|      ! 0 | 4257 | `					goto done;` |
|        - | 4258 | `				}` |
|   128747 | 4259 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|    23407 | 4260 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    23407 | 4261 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 4262 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4263 | `							return SXERR_ABORT;` |
|        - | 4264 | `						}` |
|      ! 0 | 4265 | `						goto done;` |
|        - | 4266 | `					}` |
|    23407 | 4267 | `					continue;` |
|        - | 4268 | `				}` |
|   105345 | 4269 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        9 | 4270 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        9 | 4271 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 4272 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4273 | `							return SXERR_ABORT;` |
|        - | 4274 | `						}` |
|      ! 0 | 4275 | `						goto done;` |
|        - | 4276 | `					}` |
|        9 | 4277 | `					continue;` |
|        - | 4278 | `				}` |
|   105337 | 4279 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    52666 | 4280 | `			}` |
|   105355 | 4281 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|      ! 0 | 4282 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4283 | `					"Traits cannot have constants");` |
|      ! 0 | 4284 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 4285 | `					return SXERR_ABORT;` |
|        - | 4286 | `				}` |
|      ! 0 | 4287 | `				goto done;` |
|      ! 0 | 4288 | `			}else{` |
|   105355 | 4289 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|     7811 | 4290 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     7811 | 4291 | `					pGen->pIn++;` |
|     7811 | 4292 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     7809 | 4293 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     7809 | 4294 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 4295 | `							iProtection = nKwrd;` |
|      ! 0 | 4296 | `							pGen->pIn++;` |
|      ! 0 | 4297 | `						}` |
|     3902 | 4298 | `					}` |
|     7806 | 4299 | `					if( pGen->pIn >= pGen->pEnd` |
|     7811 | 4300 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 4301 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4302 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|      ! 0 | 4303 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 4304 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4305 | `							return SXERR_ABORT;` |
|        - | 4306 | `						}` |
|      ! 0 | 4307 | `						goto done;` |
|        - | 4308 | `					}` |
|     7811 | 4309 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        3 | 4310 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        3 | 4311 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 4312 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 4313 | `								return SXERR_ABORT;` |
|        - | 4314 | `							}` |
|      ! 0 | 4315 | `							goto done;` |
|        - | 4316 | `						}` |
|        3 | 4317 | `						continue;` |
|        - | 4318 | `					}` |
|     7809 | 4319 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|      ! 0 | 4320 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 4321 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 4322 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 4323 | `								return SXERR_ABORT;` |
|        - | 4324 | `							}` |
|      ! 0 | 4325 | `							goto done;` |
|        - | 4326 | `						}` |
|      ! 0 | 4327 | `						continue;` |
|        - | 4328 | `					}` |
|     7809 | 4329 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   101451 | 4330 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        9 | 4331 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        9 | 4332 | `					pGen->pIn++;` |
|        9 | 4333 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        9 | 4334 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        9 | 4335 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        9 | 4336 | `							iProtection = nKwrd;` |
|        9 | 4337 | `							pGen->pIn++;` |
|        3 | 4338 | `						}` |
|        3 | 4339 | `					}` |
|        9 | 4340 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        6 | 4341 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 4342 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4343 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|      ! 0 | 4344 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 4345 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4346 | `							return SXERR_ABORT;` |
|        - | 4347 | `						}` |
|      ! 0 | 4348 | `						goto done;` |
|        - | 4349 | `					}` |
|        9 | 4350 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        3 | 4351 | `				}` |
|   105353 | 4352 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 4353 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4354 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|      ! 0 | 4355 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 4356 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 4357 | `						return SXERR_ABORT;` |
|        - | 4358 | `					}` |
|      ! 0 | 4359 | `					goto done;` |
|        - | 4360 | `				}` |
|   105353 | 4361 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|      ! 0 | 4362 | `					pGen->pIn++;` |
|      ! 0 | 4363 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 4364 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4365 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 4366 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4367 | `							return SXERR_ABORT;` |
|        - | 4368 | `						}` |
|      ! 0 | 4369 | `						goto done;` |
|        - | 4370 | `					}` |
|      ! 0 | 4371 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 4372 | `				}else{` |
|   105353 | 4373 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 4374 | `				}` |
|   105353 | 4375 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 4376 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 4377 | `						return SXERR_ABORT;` |
|        - | 4378 | `					}` |
|      ! 0 | 4379 | `					goto done;` |
|        - | 4380 | `				}` |
|        - | 4381 | `			}` |
|    52679 | 4382 | `		}else{` |
|      ! 0 | 4383 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 4384 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 4385 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 4386 | `					return SXERR_ABORT;` |
|        - | 4387 | `				}` |
|      ! 0 | 4388 | `				goto done;` |
|        - | 4389 | `			}` |
|        - | 4390 | `		}` |
|        5 | 4391 | `	}` |
|        - | 4392 | `	/* Reject a php-fatal redeclaration before hoisting the trait */` |
|     7901 | 4393 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|        3 | 4394 | `		return SXERR_ABORT;` |
|        - | 4395 | `	}` |
|        - | 4396 | `	/* Install the trait */` |
|     7899 | 4397 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     7899 | 4398 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 4399 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4400 | `		return SXERR_ABORT;` |
|        - | 4401 | `	}` |
|     3947 | 4402 | `done:` |
|        - | 4403 | `	/* Point beyond the trait body */` |
|     7899 | 4404 | `	pGen->pIn = &pEnd[1];` |
|     7899 | 4405 | `	pGen->pEnd = pTmp;` |
|     7899 | 4406 | `	return PH7_OK;` |
|     3953 | 4407 | `}` |
|        - | 4408 | `/*` |
|        - | 4409 | ` * Compile a user-defined class.` |
|        - | 4410 | ` *  According to the PHP language reference manual` |
|        - | 4411 | ` *   Basic class definitions begin with the keyword class, followed` |
|        - | 4412 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|        - | 4413 | ` *   the definitions of the properties and methods belonging to the class.` |
|        - | 4414 | ` *   A class may contain its own constants, variables (called "properties")` |
|        - | 4415 | ` *   and functions (called "methods").` |
|        - | 4416 | ` */` |
|   407226 | 4417 | `PH7_PRIVATE sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|        5 | 4418 | `{` |
|        - | 4419 | `	sxi32 rc;` |
|   407231 | 4420 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   407231 | 4421 | `	return rc;` |
|        5 | 4422 | `}` |
|        - | 4423 | `/*` |
|        - | 4424 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|        - | 4425 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|        - | 4426 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|        - | 4427 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|        - | 4428 | `` * meaning; `enum Name` can never start a valid expression.`` |
|        - | 4429 | ` */` |
| 14325992 | 4430 | `PH7_PRIVATE int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|        5 | 4431 | `{` |
| 14543070 | 4432 | `	return (pIn->nType & PH7_TK_ID)` |
|  7380069 | 4433 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|   226965 | 4434 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
| 14543065 | 4435 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|        5 | 4436 | `}` |
|        - | 4437 | `/*` |
|        - | 4438 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|        - | 4439 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|        - | 4440 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|        - | 4441 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|        - | 4442 | ` */` |
|     3934 | 4443 | `PH7_PRIVATE sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|        5 | 4444 | `{` |
|     3939 | 4445 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|        5 | 4446 | `}` |
|        - | 4447 |  |
