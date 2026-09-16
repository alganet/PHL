# src/ph7/compile_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2138/2806 lines (76.19%)

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
|   530662 |   27 | `static sxi32 GenStateGuardClassRedeclaration(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |   28 | `{` |
|        - |   29 | `	SyHashEntry *pEntry;` |
|   530667 |   30 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|       26 |   31 | `		return SXRET_OK; /* conditional/nested: keep hoisting */` |
|        - |   32 | `	}` |
|   530645 |   33 | `	pClass->iFlags \|= PH7_CLASS_BOUND;` |
|   530645 |   34 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|   528501 |   35 | `		return SXRET_OK; /* the prelude installs each builtin exactly once */` |
|        - |   36 | `	}` |
|     2149 |   37 | `	pEntry = SyHashGet(&pGen->pVm->hClass,(const void *)pClass->sName.zString,pClass->sName.nByte);` |
|     2149 |   38 | `	if( pEntry ){` |
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
|     2139 |   58 | `	return SXRET_OK;` |
|   265336 |   59 | `}` |
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
|  3834798 |   71 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|        5 |   72 | `{` |
|  3834803 |   73 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|   280023 |   74 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  3554785 |   75 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   221609 |   76 | `		return PH7_CLASS_PROT_PROTECTED;` |
|        - |   77 | `	}` |
|        - |   78 | `	/* Assume public by default */` |
|  3333181 |   79 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  1917404 |   80 | `}` |
|        - |   81 | `/*` |
|        - |   82 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|        - |   83 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|        - |   84 | ` * token immediately followed by '='. Anything else with a leading type token` |
|        - |   85 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|        - |   86 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|        - |   87 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|        - |   88 | ` */` |
|   342176 |   89 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|        5 |   90 | `{` |
|        - |   91 | `	SyToken *p0, *p1;` |
|   342181 |   92 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |   93 | `		return 0;` |
|        - |   94 | `	}` |
|   342181 |   95 | `	p0 = pGen->pIn;` |
|        - |   96 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|   342181 |   97 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|      ! 0 |   98 | `		return 1;` |
|        - |   99 | `	}` |
|   342181 |  100 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|        5 |  101 | `		return 1;` |
|        - |  102 | `	}` |
|        - |  103 | `	/* A name-like first token begins a type only when followed by another` |
|        - |  104 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|        - |  105 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|   342177 |  106 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|   342177 |  107 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|   342177 |  108 | `		if( p1 ){` |
|   342177 |  109 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|       34 |  110 | `				return 1;` |
|        - |  111 | `			}` |
|   342147 |  112 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|        5 |  113 | `				return 1;` |
|        - |  114 | `			}` |
|   171069 |  115 | `		}` |
|   171069 |  116 | `	}` |
|   342143 |  117 | `	return 0;` |
|   171093 |  118 | `}` |
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
|        4 |  148 | `{` |
|        - |  149 | `	sxi32 iOp;` |
|      114 |  150 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|       11 |  151 | `		return 0;` |
|        - |  152 | `	}` |
|      104 |  153 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|      104 |  154 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|       59 |  155 | `}` |
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
|   723780 |  173 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|        5 |  174 | `{` |
|   723785 |  175 | `	SyToken *p = pGen->pIn;` |
|   723785 |  176 | `	int iDepth = 0;` |
|  1911593 |  177 | `	while( p < pGen->pEnd ){` |
|  1911593 |  178 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   723733 |  179 | `			break; /* end of this initializer */` |
|        - |  180 | `		}` |
|  1187860 |  181 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   601727 |  182 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|    15583 |  183 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
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
|  1187861 |  243 | `		if( p->nType & PH7_TK_OCB ){` |
|       45 |  244 | `			if( iDepth == 0 ){` |
|        - |  245 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|        - |  246 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|        - |  247 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|        - |  248 | `				 * is legal — don't scan into it. */` |
|       45 |  249 | `				break;` |
|        - |  250 | `			}` |
|      ! 0 |  251 | `			iDepth++;` |
|  1187817 |  252 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|    50623 |  253 | `			iDepth++;` |
|  1162508 |  254 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|    50621 |  255 | `			if( iDepth > 0 ){` |
|    50621 |  256 | `				iDepth--;` |
|    25308 |  257 | `			}` |
|  1111891 |  258 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|   391611 |  259 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|        - |  260 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|        - |  261 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|        - |  262 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|       11 |  263 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|       11 |  264 | `				return 1;` |
|        - |  265 | `			}` |
|      ! 0 |  266 | `		}` |
|  1187809 |  267 | `		p++;` |
|        5 |  268 | `	}` |
|   723777 |  269 | `	return 0;` |
|   361895 |  270 | `}` |
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
|   723830 |  292 | `PH7_PRIVATE int PH7_GenStateInitHasCallExpr(ph7_gen_state *pGen)` |
|        5 |  293 | `{` |
|   723835 |  294 | `	SyToken *p = pGen->pIn;` |
|   723835 |  295 | `	int iDepth = 0;` |
|        - |  296 | `	/* Conservative ternary bail-out (see the note above). */` |
|        - |  297 | `	{` |
|   723835 |  298 | `		SyToken *q = pGen->pIn;` |
|   723835 |  299 | `		int iQd = 0;` |
|  1912941 |  300 | `		while( q < pGen->pEnd ){` |
|  1912903 |  301 | `			if( iQd == 0 && (q->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   723791 |  302 | `				break;` |
|        - |  303 | `			}` |
|  1189117 |  304 | `			if( q->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    50759 |  305 | `				iQd++;` |
|  1163740 |  306 | `			}else if( q->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|    50759 |  307 | `				if( iQd > 0 ){ iQd--; }` |
|  1112986 |  308 | `			}else if( (q->nType & PH7_TK_OP) && q->pUserData` |
|   391837 |  309 | `				&& ((const ph7_expr_op *)q->pUserData)->iOp == EXPR_OP_QUESTY ){` |
|        8 |  310 | `				return 0;` |
|        - |  311 | `			}` |
|  1189111 |  312 | `			q++;` |
|        5 |  313 | `		}` |
|        - |  314 | `	}` |
|  1911787 |  315 | `	while( p < pGen->pEnd ){` |
|  1911787 |  316 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   723785 |  317 | `			break; /* end of this initializer */` |
|        - |  318 | `		}` |
|  1188002 |  319 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   601798 |  320 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|    15583 |  321 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
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
|  1188003 |  373 | `		if( p->nType & PH7_TK_OCB ){` |
|       43 |  374 | `			if( iDepth == 0 ){` |
|       43 |  375 | `				break; /* property-hook list: the default expression ends here */` |
|        - |  376 | `			}` |
|      ! 0 |  377 | `			iDepth++;` |
|  1187961 |  378 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|        - |  379 | `			/* A '(' directly after a NAME is a call. A name here is an identifier` |
|        - |  380 | ``			 * token; `new X(` is excluded by looking for the `new` operator, and`` |
|        - |  381 | ``			 * `f(...)` (first-class callable) by peeking for a lone ellipsis. */`` |
|    50638 |  382 | `			if( (p->nType & PH7_TK_LPAREN) && p > pGen->pIn` |
|    15574 |  383 | `				&& (p[-1].nType & PH7_TK_ID)` |
|     7799 |  384 | `				&& !((p[-1].nType & PH7_TK_OP) && p[-1].pUserData` |
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
|    50641 |  405 | `			iDepth++;` |
|  1162641 |  406 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|    50641 |  407 | `			if( iDepth > 0 ){` |
|    50641 |  408 | `				iDepth--;` |
|    25318 |  409 | `			}` |
|    25318 |  410 | `		}` |
|  1187959 |  411 | `		p++;` |
|        5 |  412 | `	}` |
|   723827 |  413 | `	return 0;` |
|   361920 |  414 | `}` |
|        - |  415 | `/*` |
|        - |  416 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|        - |  417 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|        - |  418 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|        - |  419 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|        - |  420 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|        - |  421 | ` * share the same backing.` |
|        - |  422 | ` */` |
|    15938 |  423 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|        - |  424 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|        5 |  425 | `{` |
|    15943 |  426 | `	pAttr->nType = nType;` |
|    15943 |  427 | `	pAttr->sClass = *pClass;` |
|    15943 |  428 | `	pAttr->sTypeName = *pTypeName;` |
|    15943 |  429 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  430 | `		sxu32 i;` |
|       73 |  431 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|       51 |  432 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|       51 |  433 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|       28 |  434 | `		}` |
|       11 |  435 | `	}` |
|    15943 |  436 | `}` |
|   342176 |  437 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  438 | `{` |
|   342181 |  439 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  440 | `	SySet *pInstrContainer;` |
|        - |  441 | `	ph7_class_attr *pCons;` |
|        - |  442 | `	SyString *pName;` |
|        - |  443 | `	sxi32 rc;` |
|   342181 |  444 | `	sxu32 nType = 0;` |
|        - |  445 | `	SyString sTypeClass;` |
|        - |  446 | `	SyString sTypeText;` |
|        - |  447 | `	SySet aUnionAlts;` |
|   342181 |  448 | `	sxi32 iTypeFlags = 0;` |
|   342181 |  449 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   342181 |  450 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   342181 |  451 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  452 | `	/* Extract visibility level */` |
|   342181 |  453 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  454 | `	/* Mark as constant */` |
|   342181 |  455 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|   342181 |  456 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        - |  457 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|        - |  458 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|   342200 |  459 | `	if( GenStateClassConstHasType(pGen) ){` |
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
|   171088 |  481 | `loop:` |
|   342183 |  482 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - |  483 | `		/* Invalid constant name */` |
|      ! 0 |  484 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|      ! 0 |  485 | `		if( rc == SXERR_ABORT ){` |
|        - |  486 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  487 | `			return SXERR_ABORT;` |
|        - |  488 | `		}` |
|      ! 0 |  489 | `		goto Synchronize;` |
|        - |  490 | `	}` |
|        - |  491 | `	/* Peek constant name */` |
|   342183 |  492 | `	pName = &pGen->pIn->sData;` |
|        - |  493 | `	/* Make sure the constant name isn't reserved */` |
|   342183 |  494 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  495 | `		/* Reserved constant name */` |
|      ! 0 |  496 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|      ! 0 |  497 | `		if( rc == SXERR_ABORT ){` |
|        - |  498 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  499 | `			return SXERR_ABORT;` |
|        - |  500 | `		}` |
|      ! 0 |  501 | `		goto Synchronize;` |
|        - |  502 | `	}` |
|        - |  503 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|   342183 |  504 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
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
|   342181 |  515 | `	pGen->pIn++;` |
|   342181 |  516 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  517 | `		/* Invalid declaration */` |
|      ! 0 |  518 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|      ! 0 |  519 | `		if( rc == SXERR_ABORT ){` |
|        - |  520 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  521 | `			return SXERR_ABORT;` |
|        - |  522 | `		}` |
|      ! 0 |  523 | `		goto Synchronize;` |
|        - |  524 | `	}` |
|   342181 |  525 | `	pGen->pIn++; /* Jump the equal sign */` |
|        - |  526 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|        - |  527 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|        - |  528 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|        - |  529 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|   342176 |  530 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
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
|   342177 |  542 | `	if( PH7_GenStateInitHasCallExpr(pGen) ){` |
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
|   342177 |  553 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|        5 |  554 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  555 | `			"New expressions are not supported in this context");` |
|        5 |  556 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  557 | `			return SXERR_ABORT;` |
|        - |  558 | `		}` |
|        5 |  559 | `		goto Synchronize;` |
|        - |  560 | `	}` |
|        - |  561 | `	/* Allocate a new class attribute */` |
|   342173 |  562 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   342173 |  563 | `	if( pCons ){` |
|   342173 |  564 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|   342173 |  565 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  566 | `			return SXERR_ABORT;` |
|        - |  567 | `		}` |
|   171084 |  568 | `	}` |
|   342173 |  569 | `	if( pCons == 0 ){` |
|      ! 0 |  570 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  571 | `		return SXERR_ABORT;` |
|        - |  572 | `	}` |
|   342173 |  573 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       35 |  574 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       16 |  575 | `	}` |
|        - |  576 | `	/* Swap bytecode container */` |
|   342173 |  577 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   342173 |  578 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|        - |  579 | `	/* Compile constant value.` |
|        - |  580 | `	 */` |
|   342173 |  581 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   342173 |  582 | `	if( rc == SXERR_EMPTY ){` |
|        3 |  583 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|        3 |  584 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  585 | `			return SXERR_ABORT;` |
|        - |  586 | `		}` |
|        1 |  587 | `	}` |
|        - |  588 | `	/* Emit the done instruction */` |
|   342173 |  589 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   342173 |  590 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   342173 |  591 | `	if( rc == SXERR_ABORT ){` |
|        - |  592 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  593 | `		return SXERR_ABORT;` |
|        - |  594 | `	}` |
|        - |  595 | `	/* All done,install the constant */` |
|   342173 |  596 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|   342173 |  597 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  598 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  599 | `		return SXERR_ABORT;` |
|        - |  600 | `	}` |
|   342173 |  601 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  602 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|        3 |  603 | `		pGen->pIn++; /* Jump the comma */` |
|        3 |  604 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 |  605 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 |  606 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 |  607 | `				pTok--;` |
|      ! 0 |  608 | `			}` |
|      ! 0 |  609 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  610 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|      ! 0 |  611 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 |  612 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  613 | `				return SXERR_ABORT;` |
|        - |  614 | `			}` |
|      ! 0 |  615 | `		}else{` |
|        3 |  616 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|        3 |  617 | `				goto loop;` |
|        - |  618 | `			}` |
|        - |  619 | `		}` |
|      ! 0 |  620 | `	}` |
|   342171 |  621 | `	SySetRelease(&aUnionAlts);` |
|   342171 |  622 | `	return SXRET_OK;` |
|        5 |  623 | `Synchronize:` |
|       13 |  624 | `	SySetRelease(&aUnionAlts);` |
|        - |  625 | `	/* Synchronize with the first semi-colon */` |
|       45 |  626 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       35 |  627 | `		pGen->pIn++;` |
|        3 |  628 | `	}` |
|       13 |  629 | `	return SXERR_CORRUPT;` |
|   171093 |  630 | `}` |
|        - |  631 | `/*` |
|        - |  632 | ` * complie a class attribute or Properties in the PHP jargon.` |
|        - |  633 | ` * According to the PHP language reference manual` |
|        - |  634 | ` *  Properties` |
|        - |  635 | ` *  Class member variables are called "properties". You may also see them referred` |
|        - |  636 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|        - |  637 | ` *  of this reference we will use "properties". They are defined by using one` |
|        - |  638 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|        - |  639 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|        - |  640 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|        - |  641 | ` *  and must not depend on run-time information in order to be evaluated.` |
|        - |  642 | ` * Symisc eXtension.` |
|        - |  643 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|        - |  644 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  645 | ` *  Example:` |
|        - |  646 | ` *   class Test{` |
|        - |  647 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - |  648 | ` *   };` |
|        - |  649 | ` *   var_dump(TEST::myVar);` |
|        - |  650 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - |  651 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - |  652 | ` */` |
|        - |  653 | `/*` |
|        - |  654 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|        - |  655 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|        - |  656 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|        - |  657 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|        - |  658 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|        - |  659 | ` */` |
|  2625034 |  660 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|        5 |  661 | `{` |
|  2625039 |  662 | `	SyToken *p = pStart;` |
|  2625039 |  663 | `	int bFirst = 1;` |
|  2625039 |  664 | `	if( p >= pEnd ) return 0;` |
|        - |  665 | ``	/* Optional nullable `?` shorthand. */`` |
|  2625039 |  666 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|       41 |  667 | `		p++;` |
|       41 |  668 | `		if( p >= pEnd ) return 0;` |
|       19 |  669 | `	}` |
|        - |  670 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|        - |  671 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|        - |  672 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|        - |  673 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|  1312517 |  674 | `	for(;;){` |
|  2625059 |  675 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|        - |  676 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|        3 |  677 | `			p++;` |
|        9 |  678 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|        3 |  679 | `			if( p >= pEnd ) return 0;` |
|        3 |  680 | `			p++; /* skip ')' */` |
|        2 |  681 | `		}else{` |
|        - |  682 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|        - |  683 | ``			 * then any `&`-joined intersection members. */`` |
|  2625057 |  684 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  2625057 |  685 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  686 | `				return 0;` |
|        - |  687 | `			}` |
|        - |  688 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|        - |  689 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|        - |  690 | `			 * may still appear at the initial dispatch site). */` |
|  2625057 |  691 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  2625001 |  692 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  2624996 |  693 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|   124854 |  694 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  2609139 |  695 | `					return 0;` |
|        - |  696 | `				}` |
|     7931 |  697 | `			}` |
|    15923 |  698 | `			p++;` |
|    15925 |  699 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  700 | `				p += 2;` |
|        1 |  701 | `			}` |
|    23880 |  702 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|    15926 |  703 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  704 | `				p++; /* skip '&' */` |
|        3 |  705 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|        3 |  706 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|        3 |  707 | `				p++;` |
|        3 |  708 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      ! 0 |  709 | `					p += 2;` |
|      ! 0 |  710 | `				}` |
|        1 |  711 | `			}` |
|        - |  712 | `		}` |
|    15925 |  713 | `		bFirst = 0;` |
|    15920 |  714 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       25 |  715 | `			&& p->sData.zString[0] == '\|' ){` |
|       25 |  716 | ``			p++; /* next `\|`-separated part */`` |
|       25 |  717 | `			continue;` |
|        - |  718 | `		}` |
|    15905 |  719 | `		break;` |
|      ! 0 |  720 | `	}` |
|    15905 |  721 | `	if( p >= pEnd ) return 0;` |
|    15905 |  722 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|  1312522 |  723 | `}` |
|        - |  724 |  |
|        - |  725 | `/*` |
|        - |  726 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|        - |  727 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|        - |  728 | ` * if not). Recognized forms:` |
|        - |  729 | ` *   ?Type, array, bool, int, float, string, object,` |
|        - |  730 | ` *   self, parent, \Ns\ClassName, ClassName` |
|        - |  731 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|        - |  732 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|        - |  733 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|        - |  734 | ` * on unrecoverable error.` |
|        - |  735 | ` *` |
|        - |  736 | ` * When a type is parsed:` |
|        - |  737 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|        - |  738 | ` *   *pClass is set to the class name (for class types)` |
|        - |  739 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|        - |  740 | ` *   *pTypeText is set to the original text span of the type` |
|        - |  741 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|        - |  742 | ` */` |
|    15910 |  743 | `static sxi32 GenStateParsePropertyType(` |
|        - |  744 | `	ph7_gen_state *pGen,` |
|        - |  745 | `	sxu32 *pnType,` |
|        - |  746 | `	SyString *pClass,` |
|        - |  747 | `	sxi32 *piTypeFlags,` |
|        - |  748 | `	SyString *pTypeText,` |
|        - |  749 | `	SySet *pAlts` |
|        5 |  750 | `){` |
|    15915 |  751 | `	sxi32 iFlags = 0;` |
|        - |  752 | `	sxi32 rc;` |
|    15915 |  753 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  754 | `		return SXRET_OK;` |
|        - |  755 | `	}` |
|        - |  756 | `	/* If the first token is '$', there's no type */` |
|    15915 |  757 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      ! 0 |  758 | `		return SXRET_OK;` |
|        - |  759 | `	}` |
|    15915 |  760 | `	rc = GenStateParseUnionTypeDecl(` |
|     7955 |  761 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|        - |  762 | `		PH7_CLASS_ATTR_NULLABLE,` |
|        - |  763 | `		PH7_CLASS_ATTR_UNION,` |
|        - |  764 | `		/* bAllowVoid */ 0,` |
|    15910 |  765 | `		pGen->pIn->nLine);` |
|    15915 |  766 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  767 | `		return rc;` |
|        - |  768 | `	}` |
|        - |  769 | `	/* Verify next token is '$' (start of property name) */` |
|    15915 |  770 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  771 | `		return SXERR_SYNTAX;` |
|        - |  772 | `	}` |
|    15915 |  773 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|    15915 |  774 | `	return SXRET_OK;` |
|     7960 |  775 | `}` |
|        - |  776 |  |
|        - |  777 | `/*` |
|        - |  778 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|        - |  779 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|        - |  780 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|        - |  781 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|        - |  782 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|        - |  783 | ` * by the type parser itself before reaching here.` |
|        - |  784 | ` *` |
|        - |  785 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|        - |  786 | ` * use in the error message.` |
|        - |  787 | ` */` |
|    16088 |  788 | `static int GenStateIsDisallowedPropertyAtom(` |
|        - |  789 | `	sxu32 nType,` |
|        - |  790 | `	const SyString *pClass,` |
|        - |  791 | `	const char **pzName,` |
|        - |  792 | `	sxu32 *pnName)` |
|        5 |  793 | `{` |
|        - |  794 | `	const char *z;` |
|        - |  795 | `	sxu32 n;` |
|    16093 |  796 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|    16027 |  797 | `		return 0;` |
|        - |  798 | `	}` |
|       70 |  799 | `	z = pClass->zString;` |
|       70 |  800 | `	n = pClass->nByte;` |
|       70 |  801 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|        8 |  802 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|        - |  803 | `	}` |
|        - |  804 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|        - |  805 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|        - |  806 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|       63 |  807 | `	return 0;` |
|     8049 |  808 | `}` |
|        - |  809 |  |
|        - |  810 | `/*` |
|        - |  811 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|        - |  812 | ` * constant) — the main atom plus any union alternatives — against the` |
|        - |  813 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|        - |  814 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|        - |  815 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|        - |  816 | ` * type T" vs "Class constant C::X cannot have type T").` |
|        - |  817 | ` *` |
|        - |  818 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|        - |  819 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|        - |  820 | ` */` |
|    16026 |  821 | `PH7_PRIVATE sxi32 GenStateValidateMemberType(` |
|        - |  822 | `	ph7_gen_state *pGen,` |
|        - |  823 | `	ph7_class *pClass,` |
|        - |  824 | `	const SyString *pMemberName,` |
|        - |  825 | `	sxu32 nType,` |
|        - |  826 | `	const SyString *pTypeClass,` |
|        - |  827 | `	const SyString *pTypeText,` |
|        - |  828 | `	SySet *pUnionAlts,` |
|        - |  829 | `	const char *zErrFmt,` |
|        - |  830 | `	sxu32 nLine)` |
|        5 |  831 | `{` |
|    16031 |  832 | `	const char *zBad = 0;` |
|    16031 |  833 | `	sxu32 nBad = 0;` |
|        - |  834 | `	SyString sFallback;` |
|        - |  835 | `	const SyString *pBad;` |
|        - |  836 | `	sxi32 rc;` |
|    16031 |  837 | `	int bDisallowed = 0;` |
|    16031 |  838 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|        5 |  839 | `		bDisallowed = 1;` |
|    16029 |  840 | `	}else if( pUnionAlts ){` |
|        - |  841 | `		sxu32 i;` |
|       95 |  842 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|       67 |  843 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|       67 |  844 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|        3 |  845 | `				bDisallowed = 1;` |
|        3 |  846 | `				break;` |
|        - |  847 | `			}` |
|       35 |  848 | `		}` |
|       15 |  849 | `	}` |
|    16031 |  850 | `	if( !bDisallowed ){` |
|    16025 |  851 | `		return SXRET_OK;` |
|        - |  852 | `	}` |
|        - |  853 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|        - |  854 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|        - |  855 | `	 * canonical spelling if the type text is unavailable. */` |
|        8 |  856 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|        8 |  857 | `		pBad = pTypeText;` |
|        5 |  858 | `	}else{` |
|      ! 0 |  859 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|      ! 0 |  860 | `		pBad = &sFallback;` |
|        - |  861 | `	}` |
|       11 |  862 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        3 |  863 | `		zErrFmt,` |
|        3 |  864 | `		&pClass->sName,pMemberName,pBad);` |
|        8 |  865 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  866 | `		return SXERR_ABORT;` |
|        - |  867 | `	}` |
|        8 |  868 | `	return SXERR_SYNTAX;` |
|     8018 |  869 | `}` |
|        - |  870 | `/*` |
|        - |  871 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|        - |  872 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|        - |  873 | ` * matched as a plain identifier in the class-member modifier position rather` |
|        - |  874 | ` * than promoted to a lexer keyword.` |
|        - |  875 | ` */` |
| 23558898 |  876 | `PH7_PRIVATE int GenStateIsReadonly(SyToken *pTok)` |
|        5 |  877 | `{` |
| 23785130 |  878 | `	return (pTok->nType & PH7_TK_ID)` |
| 12005676 |  879 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 23785125 |  880 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|        5 |  881 | `}` |
|        - |  882 | `/*` |
|        - |  883 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|        - |  884 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|        - |  885 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|        - |  886 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|        - |  887 | ` */` |
|  8459774 |  888 | `PH7_PRIVATE sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|        5 |  889 | `{` |
|  8459779 |  890 | `	*pnTok = 0;` |
|  8459774 |  891 | `	if( &pTok[3] < pEnd` |
|  7909055 |  892 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|  6450172 |  893 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|  2771012 |  894 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       16 |  895 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|       16 |  896 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|       21 |  897 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|       17 |  898 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|       17 |  899 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|       17 |  900 | `			*pnTok = 4;` |
|       17 |  901 | `			return nKw;` |
|        - |  902 | `		}` |
|      ! 0 |  903 | `	}` |
|  8459763 |  904 | `	return 0;` |
|  4229892 |  905 | `}` |
|        - |  906 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|       16 |  907 | `PH7_PRIVATE sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|        1 |  908 | `{` |
|       17 |  909 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|       13 |  910 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|        - |  911 | `	}` |
|        5 |  912 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|        3 |  913 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|        - |  914 | `	}` |
|        3 |  915 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|        9 |  916 | `}` |
|   591786 |  917 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  918 | `{` |
|   591791 |  919 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  920 | `	ph7_class_attr *pAttr;` |
|        - |  921 | `	SyString *pName;` |
|        - |  922 | `	sxi32 rc;` |
|   591791 |  923 | `	sxu32 nType = 0;` |
|        - |  924 | `	SyString sTypeClass;` |
|        - |  925 | `	SyString sTypeText;` |
|        - |  926 | `	SySet aUnionAlts;` |
|   591791 |  927 | `	sxi32 iTypeFlags = 0;` |
|   591791 |  928 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   591791 |  929 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   591791 |  930 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  931 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|        - |  932 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|        - |  933 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   591791 |  934 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|       21 |  935 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        9 |  936 | `	}` |
|        - |  937 | `	/* Extract visibility level */` |
|   591791 |  938 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  939 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   599746 |  940 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|    15915 |  941 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|    15915 |  942 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  943 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  944 | `			goto Synchronize;` |
|    15915 |  945 | `		}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  946 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  947 | `				"Invalid property type or declaration near '%z'",` |
|      ! 0 |  948 | `				&pGen->pIn->sData);` |
|      ! 0 |  949 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  950 | `				return SXERR_ABORT;` |
|        - |  951 | `			}` |
|      ! 0 |  952 | `			goto Synchronize;` |
|    15915 |  953 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  954 | `			return SXERR_ABORT;` |
|        - |  955 | `		}` |
|     7955 |  956 | `	}` |
|      ! 0 |  957 | `loop:` |
|   591795 |  958 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  959 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|      ! 0 |  960 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  961 | `			return SXERR_ABORT;` |
|        - |  962 | `		}` |
|      ! 0 |  963 | `		goto Synchronize;` |
|        - |  964 | `	}` |
|   591795 |  965 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   591795 |  966 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        - |  967 | `		/* Invalid attribute name */` |
|      ! 0 |  968 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|      ! 0 |  969 | `		if( rc == SXERR_ABORT ){` |
|        - |  970 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  971 | `			return SXERR_ABORT;` |
|        - |  972 | `		}` |
|      ! 0 |  973 | `		goto Synchronize;` |
|        - |  974 | `	}` |
|        - |  975 | `	/* Peek attribute name */` |
|   591795 |  976 | `	pName = &pGen->pIn->sData;` |
|        - |  977 | `	/* Advance the stream cursor */` |
|   591795 |  978 | `	pGen->pIn++;` |
|   591795 |  979 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|        - |  980 | `		/* Invalid declaration */` |
|        3 |  981 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|        3 |  982 | `		if( rc == SXERR_ABORT ){` |
|        - |  983 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  984 | `			return SXERR_ABORT;` |
|        - |  985 | `		}` |
|        3 |  986 | `		goto Synchronize;` |
|        - |  987 | `	}` |
|        - |  988 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|        - |  989 | `	 * the read visibility must not be narrower than the set visibility. */` |
|   591793 |  990 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|       13 |  991 | `		const char *zAvErr = 0;` |
|       19 |  992 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|       10 |  993 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|        2 |  994 | `			: PH7_CLASS_PROT_PUBLIC;` |
|       13 |  995 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  996 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|       13 |  997 | `		}else if( iProtection > iSetLevel ){` |
|      ! 0 |  998 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|      ! 0 |  999 | `		}` |
|       13 | 1000 | `		if( zAvErr ){` |
|      ! 0 | 1001 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|      ! 0 | 1002 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1003 | `				return SXERR_ABORT;` |
|        - | 1004 | `			}` |
|      ! 0 | 1005 | `			goto Synchronize;` |
|        - | 1006 | `		}` |
|        6 | 1007 | `	}` |
|        - | 1008 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|        - | 1009 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   591793 | 1010 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       51 | 1011 | `		const char *zRoErr = 0;` |
|       51 | 1012 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        3 | 1013 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|       50 | 1014 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        6 | 1015 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|       47 | 1016 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|        6 | 1017 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|        2 | 1018 | `		}` |
|       51 | 1019 | `		if( zRoErr ){` |
|       13 | 1020 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|       13 | 1021 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1022 | `				return SXERR_ABORT;` |
|        - | 1023 | `			}` |
|       13 | 1024 | `			goto Synchronize;` |
|        - | 1025 | `		}` |
|       18 | 1026 | `	}` |
|        - | 1027 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|        - | 1028 | `	 * type atom or any union alternative. void/never are already rejected` |
|        - | 1029 | `	 * by the type parser. */` |
|   591783 | 1030 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|    23867 | 1031 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|        - | 1032 | `			&sTypeText,` |
|    15908 | 1033 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|     7954 | 1034 | `			"Property %z::$%z cannot have type %z",nLine);` |
|    15913 | 1035 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1036 | `			return SXERR_ABORT;` |
|    15913 | 1037 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 | 1038 | `			goto Synchronize;` |
|        - | 1039 | `		}` |
|     7954 | 1040 | `	}` |
|        - | 1041 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   591783 | 1042 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|        4 | 1043 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1044 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|        3 | 1045 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1046 | `			return SXERR_ABORT;` |
|        - | 1047 | `		}` |
|        3 | 1048 | `		goto Synchronize;` |
|        - | 1049 | `	}` |
|        - | 1050 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|        - | 1051 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|        - | 1052 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|        - | 1053 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|        - | 1054 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|        - | 1055 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|        - | 1056 | `	/* php: a property default may not CALL anything either. */` |
|   591781 | 1057 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && PH7_GenStateInitHasCallExpr(pGen) ){` |
|      ! 0 | 1058 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1059 | `			"Constant expression contains invalid operations");` |
|      ! 0 | 1060 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1061 | `			return SXERR_ABORT;` |
|        - | 1062 | `		}` |
|      ! 0 | 1063 | `		goto Synchronize;` |
|        - | 1064 | `	}` |
|   591781 | 1065 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|        6 | 1066 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1067 | `			"New expressions are not supported in this context");` |
|        6 | 1068 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1069 | `			return SXERR_ABORT;` |
|        - | 1070 | `		}` |
|        6 | 1071 | `		goto Synchronize;` |
|        - | 1072 | `	}` |
|        - | 1073 | `	/* Allocate a new class attribute */` |
|   591777 | 1074 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   591777 | 1075 | `	if( pAttr ){` |
|   591777 | 1076 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|   591777 | 1077 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 1078 | `			return SXERR_ABORT;` |
|        - | 1079 | `		}` |
|   295886 | 1080 | `	}` |
|   591777 | 1081 | `	if( pAttr == 0 ){` |
|      ! 0 | 1082 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1083 | `		return SXERR_ABORT;` |
|        - | 1084 | `	}` |
|   591777 | 1085 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|    15911 | 1086 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|     7953 | 1087 | `	}` |
|   591777 | 1088 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|        - | 1089 | `		SySet *pInstrContainer;` |
|   381609 | 1090 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|   381609 | 1091 | `		pGen->pIn++; /*Jump the equal sign */` |
|        - | 1092 | `		{` |
|        - | 1093 | `			/* Delimit the default expression: it ends at the declaration's` |
|        - | 1094 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|        - | 1095 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|        - | 1096 | `			 * compiler would otherwise run into the hook tokens. */` |
|   381609 | 1097 | `			SyToken *pScan = pGen->pIn;` |
|   381609 | 1098 | `			sxi32 iNest = 0;` |
|   841519 | 1099 | `			while( pScan < pGen->pEnd ){` |
|   841519 | 1100 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|    50611 | 1101 | `					iNest++;` |
|   816216 | 1102 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|    50611 | 1103 | `					iNest--;` |
|   765610 | 1104 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|   381609 | 1105 | `					break;` |
|        - | 1106 | `				}` |
|   459915 | 1107 | `				pScan++;` |
|        5 | 1108 | `			}` |
|   381609 | 1109 | `			pGen->pEnd = pScan;` |
|        - | 1110 | `		}` |
|        - | 1111 | `		/* Swap bytecode container */` |
|   381609 | 1112 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   381609 | 1113 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|        - | 1114 | `		/* Compile attribute value.` |
|        - | 1115 | `		 */` |
|   381609 | 1116 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   381609 | 1117 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 1118 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|      ! 0 | 1119 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1120 | `				return SXERR_ABORT;` |
|        - | 1121 | `			}` |
|      ! 0 | 1122 | `		}` |
|        - | 1123 | `		/* Emit the done instruction */` |
|   381609 | 1124 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   381609 | 1125 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   381609 | 1126 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|   381609 | 1127 | `		pGen->pEnd = pSavedDefEnd;` |
|   190802 | 1128 | `	}` |
|        - | 1129 | `	/* All done,install the attribute */` |
|   591777 | 1130 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   591777 | 1131 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1132 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1133 | `		return SXERR_ABORT;` |
|        - | 1134 | `	}` |
|   591777 | 1135 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|        - | 1136 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|        - | 1137 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|       95 | 1138 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|       95 | 1139 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1140 | `			return SXERR_ABORT;` |
|        - | 1141 | `		}` |
|       95 | 1142 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1143 | `			goto Synchronize;` |
|        - | 1144 | `		}` |
|       95 | 1145 | `		SySetRelease(&aUnionAlts);` |
|       95 | 1146 | `		return SXRET_OK;` |
|        - | 1147 | `	}` |
|   591683 | 1148 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - | 1149 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|        - | 1150 | `		 * wording differs per declaration site) */` |
|      ! 0 | 1151 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 1152 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|        - | 1153 | `				? "Interfaces may only include hooked properties"` |
|        - | 1154 | `				: "Only hooked properties may be declared abstract");` |
|      ! 0 | 1155 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1156 | `			return SXERR_ABORT;` |
|        - | 1157 | `		}` |
|      ! 0 | 1158 | `		goto Synchronize;` |
|        - | 1159 | `	}` |
|   591683 | 1160 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - | 1161 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|        5 | 1162 | `		pGen->pIn++; /* Jump the comma */` |
|        5 | 1163 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 | 1164 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 | 1165 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 | 1166 | `				pTok--;` |
|      ! 0 | 1167 | `			}` |
|      ! 0 | 1168 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 1169 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|      ! 0 | 1170 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 | 1171 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1172 | `				return SXERR_ABORT;` |
|        - | 1173 | `			}` |
|      ! 0 | 1174 | `		}else{` |
|        5 | 1175 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        5 | 1176 | `				goto loop;` |
|        - | 1177 | `			}` |
|        - | 1178 | `		}` |
|      ! 0 | 1179 | `	}` |
|   591679 | 1180 | `	SySetRelease(&aUnionAlts);` |
|   591679 | 1181 | `	return SXRET_OK;` |
|        9 | 1182 | `Synchronize:` |
|        - | 1183 | `	/* Synchronize with the first semi-colon */` |
|       56 | 1184 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       38 | 1185 | `		pGen->pIn++;` |
|        4 | 1186 | `	}` |
|       22 | 1187 | `	SySetRelease(&aUnionAlts);` |
|       22 | 1188 | `	return SXERR_CORRUPT;` |
|   295898 | 1189 | `}` |
|        - | 1190 | `/*` |
|        - | 1191 | ` * Compile a class method.` |
|        - | 1192 | ` *` |
|        - | 1193 | ` * Refer to the official documentation for more information` |
|        - | 1194 | ` * on the powerful extension introduced by the PH7 engine` |
|        - | 1195 | ` * to the OO subsystem such as full type hinting,method` |
|        - | 1196 | ` * overloading and many more.` |
|        - | 1197 | ` */` |
|  2900836 | 1198 | `static sxi32 GenStateCompileClassMethod(` |
|        - | 1199 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 1200 | `	sxi32 iProtection,   /* Visibility level */` |
|        - | 1201 | `	sxi32 iFlags,        /* Configuration flags */` |
|        - | 1202 | `	int doBody,          /* TRUE to process method body */` |
|        - | 1203 | `	ph7_class *pClass    /* Class this method belongs */` |
|        - | 1204 | `	)` |
|        5 | 1205 | `{` |
|  2900841 | 1206 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  2900841 | 1207 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|        - | 1208 | `	ph7_class_method *pMeth;` |
|        - | 1209 | `	sxi32 iFuncFlags;` |
|        - | 1210 | `	SyString *pName;` |
|        - | 1211 | `	SyToken *pEnd;` |
|        - | 1212 | `	sxi32 rc;` |
|        - | 1213 | `	/* Extract visibility level */` |
|  2900841 | 1214 | `	iProtection = GetProtectionLevel(iProtection);` |
|  2900841 | 1215 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  2900841 | 1216 | `	iFuncFlags = 0;` |
|  2900841 | 1217 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1218 | `		/* Invalid method name */` |
|      ! 0 | 1219 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|      ! 0 | 1220 | `		if( rc == SXERR_ABORT ){` |
|        - | 1221 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1222 | `			return SXERR_ABORT;` |
|        - | 1223 | `		}` |
|      ! 0 | 1224 | `		goto Synchronize;` |
|        - | 1225 | `	}` |
|  2900841 | 1226 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - | 1227 | `		/* Return by reference,remember that */` |
|      ! 0 | 1228 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|        - | 1229 | `		/* Jump the '&' token */` |
|      ! 0 | 1230 | `		pGen->pIn++;` |
|      ! 0 | 1231 | `	}` |
|  2900841 | 1232 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 1233 | `		/* Invalid method name */` |
|      ! 0 | 1234 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|      ! 0 | 1235 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1236 | `			return SXERR_ABORT;` |
|        - | 1237 | `		}` |
|      ! 0 | 1238 | `		goto Synchronize;` |
|        - | 1239 | `	}` |
|        - | 1240 | `	/* Peek method name */` |
|  2900841 | 1241 | `	pName = &pGen->pIn->sData;` |
|  2900841 | 1242 | `	nLine = pGen->pIn->nLine;` |
|        - | 1243 | `	/* Jump the method name */` |
|  2900841 | 1244 | `	pGen->pIn++;` |
|  2900841 | 1245 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - | 1246 | `		/* Abstract method */` |
|   139977 | 1247 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|      ! 0 | 1248 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1249 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|      ! 0 | 1250 | `				&pClass->sName,pName);` |
|      ! 0 | 1251 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1252 | `				return SXERR_ABORT;` |
|        - | 1253 | `			}` |
|      ! 0 | 1254 | `		}` |
|        - | 1255 | `		/* Assemble method signature only */` |
|   139977 | 1256 | `		doBody = FALSE;` |
|    69986 | 1257 | `	}` |
|  2900841 | 1258 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1259 | `		/* Syntax error */` |
|      ! 0 | 1260 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|      ! 0 | 1261 | `		if( rc == SXERR_ABORT ){` |
|        - | 1262 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1263 | `			return SXERR_ABORT;` |
|        - | 1264 | `		}` |
|      ! 0 | 1265 | `		goto Synchronize;` |
|        - | 1266 | `	}` |
|        - | 1267 | `	/* Allocate a new class_method instance */` |
|  2900841 | 1268 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  2900841 | 1269 | `	if( pMeth == 0 ){` |
|      ! 0 | 1270 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1271 | `		return SXERR_ABORT;` |
|        - | 1272 | `	}` |
|  2900841 | 1273 | `	pMeth->sFunc.nLine = nKwLine;` |
|  2900841 | 1274 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|  2900841 | 1275 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 1276 | `		return SXERR_ABORT;` |
|        - | 1277 | `	}` |
|        - | 1278 | `	/* Jump the left parenthesis '(' */` |
|  2900841 | 1279 | `	pGen->pIn++;` |
|  2900841 | 1280 | `	pEnd = 0; /* cc warning */` |
|        - | 1281 | `	/* Delimit the method signature */` |
|  2900841 | 1282 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  2900841 | 1283 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 1284 | `		/* Syntax error */` |
|        3 | 1285 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|        3 | 1286 | `		if( rc == SXERR_ABORT ){` |
|        - | 1287 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1288 | `			return SXERR_ABORT;` |
|        - | 1289 | `		}` |
|        3 | 1290 | `		goto Synchronize;` |
|        - | 1291 | `	}` |
|        - | 1292 | `	{` |
|  2900839 | 1293 | `		int bIsCtor = 0;` |
|  2900839 | 1294 | `		int bAbstractCtor = 0;` |
|  2900834 | 1295 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  1703178 | 1296 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  2797747 | 1297 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   206189 | 1298 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        3 | 1299 | `				bAbstractCtor = 1;` |
|        2 | 1300 | `			}else{` |
|   206187 | 1301 | `				bIsCtor = 1;` |
|        - | 1302 | `			}` |
|   103092 | 1303 | `		}` |
|  2900839 | 1304 | `		if( pGen->pIn < pEnd ){` |
|        - | 1305 | `			/* Collect method arguments */` |
|  1116027 | 1306 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|  1116027 | 1307 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1308 | `				return SXERR_ABORT;` |
|        - | 1309 | `			}` |
|   558011 | 1310 | `		}` |
|        - | 1311 | `	}` |
|        - | 1312 | `	/* Point past ')' and parse optional return type ': type' */` |
|  2900839 | 1313 | `	pGen->pIn = &pEnd[1];` |
|        - | 1314 | `	{` |
|  2900839 | 1315 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  2900839 | 1316 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 | 1317 | `			return SXERR_ABORT;` |
|  2900839 | 1318 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|      ! 0 | 1319 | `			goto Synchronize;` |
|        - | 1320 | `		}` |
|        - | 1321 | `	}` |
|        - | 1322 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|        - | 1323 | `	 * property init/typecheck is handled by the generic typed-property path` |
|        - | 1324 | `	 * since we mint real ph7_class_attr entries. */` |
|        - | 1325 | `	{` |
|  2900839 | 1326 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|        - | 1327 | `		sxu32 i;` |
|  4568867 | 1328 | `		for( i = 0; i < nArg; i++ ){` |
|  1668043 | 1329 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|        - | 1330 | `			ph7_class_attr *pAttr;` |
|  1668043 | 1331 | `			sxi32 iAttrFlags = 0;` |
|        - | 1332 | `			int bArgTyped;` |
|  1668043 | 1333 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  1667957 | 1334 | `				continue;` |
|        - | 1335 | `			}` |
|        - | 1336 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|        - | 1337 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|        - | 1338 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|       60 | 1339 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|       92 | 1340 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|       91 | 1341 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        3 | 1342 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1343 | `					"Cannot declare variadic promoted property");` |
|        3 | 1344 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1345 | `					return SXERR_ABORT;` |
|        - | 1346 | `				}` |
|        3 | 1347 | `				goto Synchronize;` |
|        - | 1348 | `			}` |
|        - | 1349 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|        - | 1350 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|        - | 1351 | `			 * appear as an alternative of a union type. */` |
|       89 | 1352 | `			if( bArgTyped ){` |
|      125 | 1353 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|       80 | 1354 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|       80 | 1355 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|       40 | 1356 | `					"Property %z::$%z cannot have type %z",nLine);` |
|       85 | 1357 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1358 | `					return SXERR_ABORT;` |
|       85 | 1359 | `				}else if( rc != SXRET_OK ){` |
|        6 | 1360 | `					goto Synchronize;` |
|        - | 1361 | `				}` |
|       38 | 1362 | `			}` |
|        - | 1363 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|       85 | 1364 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|        4 | 1365 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1366 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|        3 | 1367 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1368 | `					return SXERR_ABORT;` |
|        - | 1369 | `				}` |
|        3 | 1370 | `				goto Synchronize;` |
|        - | 1371 | `			}` |
|       83 | 1372 | `			if( bArgTyped ){` |
|       79 | 1373 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       37 | 1374 | `			}` |
|       83 | 1375 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|        3 | 1376 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|        1 | 1377 | `			}` |
|       83 | 1378 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|        8 | 1379 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|        3 | 1380 | `			}` |
|       83 | 1381 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|        - | 1382 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|        - | 1383 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|       26 | 1384 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        4 | 1385 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1386 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|        3 | 1387 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1388 | `						return SXERR_ABORT;` |
|        - | 1389 | `					}` |
|        3 | 1390 | `					goto Synchronize;` |
|        - | 1391 | `				}` |
|       24 | 1392 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       10 | 1393 | `			}` |
|       81 | 1394 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|        - | 1395 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|        5 | 1396 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 | 1397 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1398 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|      ! 0 | 1399 | `						&pClass->sName,&pArg->sName);` |
|      ! 0 | 1400 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1401 | `						return SXERR_ABORT;` |
|        - | 1402 | `					}` |
|      ! 0 | 1403 | `					goto Synchronize;` |
|        - | 1404 | `				}` |
|        5 | 1405 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|        2 | 1406 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|        2 | 1407 | `			}` |
|       81 | 1408 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|       81 | 1409 | `			if( pAttr == 0 ){` |
|      ! 0 | 1410 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1411 | `				return SXERR_ABORT;` |
|        - | 1412 | `			}` |
|       81 | 1413 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|       79 | 1414 | `				pAttr->nType = pArg->nType;` |
|       79 | 1415 | `				pAttr->sClass = pArg->sClass;` |
|       79 | 1416 | `				pAttr->sTypeName = pArg->sTypeName;` |
|       79 | 1417 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|        - | 1418 | `					sxu32 k;` |
|       20 | 1419 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|       14 | 1420 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|       14 | 1421 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|        8 | 1422 | `					}` |
|        3 | 1423 | `				}` |
|       37 | 1424 | `			}` |
|       81 | 1425 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|       81 | 1426 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1427 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1428 | `				return SXERR_ABORT;` |
|        - | 1429 | `			}` |
|       43 | 1430 | `		}` |
|        - | 1431 | `	}` |
|  2900829 | 1432 | `	if( doBody ){` |
|        - | 1433 | `		/* Compile method body */` |
|  2760857 | 1434 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  2760857 | 1435 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1436 | `			return SXERR_ABORT;` |
|        - | 1437 | `		}` |
|        - | 1438 | `		/* The cursor sits just past the body's closing brace */` |
|  2760857 | 1439 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|  1380431 | 1440 | `	}else{` |
|        - | 1441 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|   139977 | 1442 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|   139977 | 1443 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|    69986 | 1444 | `		}` |
|        - | 1445 | `		/* Only method signature is allowed */` |
|   139977 | 1446 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|      ! 0 | 1447 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 1448 | `				"Expected ';' after method signature '%z'",pName);` |
|      ! 0 | 1449 | `				if( rc == SXERR_ABORT ){` |
|        - | 1450 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 | 1451 | `					return SXERR_ABORT;` |
|        - | 1452 | `				}` |
|      ! 0 | 1453 | `				return SXERR_CORRUPT;` |
|        - | 1454 | `			}` |
|        - | 1455 | `	}` |
|        - | 1456 | `	/* All done,install the method */` |
|  2900829 | 1457 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  2900829 | 1458 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1459 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1460 | `		return SXERR_ABORT;` |
|        - | 1461 | `	}` |
|  2900829 | 1462 | `	return SXRET_OK;` |
|        6 | 1463 | `Synchronize:` |
|        - | 1464 | `	/* Synchronize with the first semi-colon */` |
|       40 | 1465 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       28 | 1466 | `		pGen->pIn++;` |
|        4 | 1467 | `	}` |
|       16 | 1468 | `	return SXERR_CORRUPT;` |
|  1450423 | 1469 | `}` |
|        - | 1470 | `/*` |
|        - | 1471 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|        - | 1472 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|        - | 1473 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|        - | 1474 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|        - | 1475 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|        - | 1476 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|        - | 1477 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|        - | 1478 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|        - | 1479 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|        - | 1480 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|        - | 1481 | `` * implicit `$value` formal.`` |
|        - | 1482 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|        - | 1483 | ` */` |
|        - | 1484 | `/*` |
|        - | 1485 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|        - | 1486 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|        - | 1487 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|        - | 1488 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|        - | 1489 | ` * allowed, excluded from the raw object surfaces.` |
|        - | 1490 | ` */` |
|       94 | 1491 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|        1 | 1492 | `{` |
|        - | 1493 | `	SyToken *p;` |
|      345 | 1494 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|      303 | 1495 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      223 | 1496 | `			continue;` |
|        - | 1497 | `		}` |
|        - | 1498 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|       80 | 1499 | `		if( p + 3 < pEnd` |
|       80 | 1500 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       80 | 1501 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|       73 | 1502 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|       66 | 1503 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|       66 | 1504 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       66 | 1505 | `		 && p[3].sData.nByte == pName->nByte` |
|       60 | 1506 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       51 | 1507 | `			return 1;` |
|        - | 1508 | `		}` |
|        - | 1509 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|        - | 1510 | `		 * hook operates on the shared per-instance backing store, so the` |
|        - | 1511 | `		 * property is backed (php compiles a default alongside it). */` |
|       30 | 1512 | `		if( p > pStart` |
|       26 | 1513 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|       12 | 1514 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        2 | 1515 | `		 && p[1].sData.nByte == pName->nByte` |
|        3 | 1516 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        3 | 1517 | `			return 1;` |
|        - | 1518 | `		}` |
|       15 | 1519 | `	}` |
|       43 | 1520 | `	return 0;` |
|       48 | 1521 | `}` |
|        - | 1522 | `/*` |
|        - | 1523 | ` * True when p opens php 8.4's parent-hook call form` |
|        - | 1524 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|        - | 1525 | ` */` |
|      990 | 1526 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|        1 | 1527 | `{` |
|     1167 | 1528 | `	return p + 6 < pEnd` |
|      671 | 1529 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|      250 | 1530 | `	 && p->sData.nByte == sizeof("parent")-1` |
|       81 | 1531 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|       11 | 1532 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|        8 | 1533 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|        8 | 1534 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        8 | 1535 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|        8 | 1536 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        8 | 1537 | `	 && p[5].sData.nByte == 3` |
|        8 | 1538 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|        6 | 1539 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|     1166 | 1540 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|        1 | 1541 | `}` |
|        - | 1542 | `/*` |
|        - | 1543 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|        - | 1544 | ` * hook body into calls of the parent class's synthesized hook method` |
|        - | 1545 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|        - | 1546 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|        - | 1547 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|        - | 1548 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|        - | 1549 | ` * or SXERR_MEM.` |
|        - | 1550 | ` */` |
|        4 | 1551 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|        - | 1552 | `	SyToken *pStart,SyToken *pEnd)` |
|        1 | 1553 | `{` |
|        5 | 1554 | `	SyToken *p = pStart;` |
|       35 | 1555 | `	while( p < pEnd ){` |
|       31 | 1556 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|        - | 1557 | `			SyToken sTok;` |
|        - | 1558 | `			char zName[384];` |
|        - | 1559 | `			sxu32 nName;` |
|        - | 1560 | `			char *zDup;` |
|        - | 1561 | ``			/* `parent` `::` */`` |
|        5 | 1562 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|        5 | 1563 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|        7 | 1564 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|        4 | 1565 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|        5 | 1566 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|        5 | 1567 | `			if( zDup == 0 ){` |
|      ! 0 | 1568 | `				return SXERR_MEM;` |
|        - | 1569 | `			}` |
|        5 | 1570 | `			sTok = p[3]; /* keep the line info of the property name */` |
|        5 | 1571 | `			sTok.nType = PH7_TK_ID;` |
|        5 | 1572 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|        5 | 1573 | `			sTok.pUserData = 0;` |
|        5 | 1574 | `			SySetPut(pCopy,(const void *)&sTok);` |
|        5 | 1575 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|        5 | 1576 | `			continue;` |
|        - | 1577 | `		}` |
|       27 | 1578 | `		SySetPut(pCopy,(const void *)p);` |
|       27 | 1579 | `		p++;` |
|        1 | 1580 | `	}` |
|        5 | 1581 | `	return SXRET_OK;` |
|        3 | 1582 | `}` |
|       94 | 1583 | `PH7_PRIVATE sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 | 1584 | `{` |
|       95 | 1585 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 1586 | `	sxi32 rc;` |
|       95 | 1587 | `	int bRefsSelf = 0;` |
|       95 | 1588 | `	pGen->pIn++; /* Jump '{' */` |
|      253 | 1589 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|        - | 1590 | `		char zHook[384];` |
|        - | 1591 | `		SyString sHookName;` |
|        - | 1592 | `		ph7_class_method *pMeth;` |
|        - | 1593 | `		int bGet;` |
|      159 | 1594 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|      159 | 1595 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       15 | 1596 | `			pGen->pIn++; /* stray ';' between hooks */` |
|       22 | 1597 | `			continue;` |
|        - | 1598 | `		}` |
|      145 | 1599 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - | 1600 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|      ! 0 | 1601 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 1602 | `				"By-reference property hooks are not supported for %z::$%z",` |
|      ! 0 | 1603 | `				&pClass->sName,&pAttr->sName);` |
|      ! 0 | 1604 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1605 | `				return SXERR_ABORT;` |
|        - | 1606 | `			}` |
|      ! 0 | 1607 | `			return SXERR_CORRUPT;` |
|        - | 1608 | `		}` |
|      145 | 1609 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 1610 | `			goto HookSyntax;` |
|        - | 1611 | `		}` |
|      144 | 1612 | `		if( pGen->pIn->sData.nByte == 3` |
|      145 | 1613 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|       79 | 1614 | `			bGet = 1;` |
|      106 | 1615 | `		}else if( pGen->pIn->sData.nByte == 3` |
|       67 | 1616 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|       67 | 1617 | `			bGet = 0;` |
|       34 | 1618 | `		}else{` |
|      ! 0 | 1619 | `			goto HookSyntax;` |
|        - | 1620 | `		}` |
|      145 | 1621 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|      145 | 1622 | `		sHookName.zString = zHook;` |
|      217 | 1623 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|       72 | 1624 | `			bGet ? "get" : "set",&pAttr->sName);` |
|      145 | 1625 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|        - | 1626 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|        - | 1627 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|        - | 1628 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|        - | 1629 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|        - | 1630 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|       14 | 1631 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|        8 | 1632 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 1633 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 1634 | `					"Non-abstract property hook must have a body");` |
|      ! 0 | 1635 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1636 | `					return SXERR_ABORT;` |
|        - | 1637 | `				}` |
|      ! 0 | 1638 | `				return SXERR_CORRUPT;` |
|        - | 1639 | `			}` |
|       15 | 1640 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|        - | 1641 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|       15 | 1642 | `			if( pMeth == 0 ){` |
|      ! 0 | 1643 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1644 | `				return SXERR_ABORT;` |
|        - | 1645 | `			}` |
|       15 | 1646 | `			pMeth->sFunc.nLine = nHLine;` |
|       15 | 1647 | `			if( !bGet ){` |
|        - | 1648 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|        - | 1649 | `				 * compatible with concrete set-hook implementations (which` |
|        - | 1650 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|        - | 1651 | `				 * type (php: the abstract set's parameter type IS the property` |
|        - | 1652 | `				 * type), so the override contravariance check accepts a typed` |
|        - | 1653 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|        - | 1654 | `				ph7_vm_func_arg sVArg;` |
|        7 | 1655 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        7 | 1656 | `				if( zVName == 0 ){` |
|      ! 0 | 1657 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1658 | `					return SXERR_ABORT;` |
|        - | 1659 | `				}` |
|        7 | 1660 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        7 | 1661 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        7 | 1662 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        7 | 1663 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        7 | 1664 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        7 | 1665 | `				sVArg.nType = pAttr->nType;` |
|        7 | 1666 | `				sVArg.sClass = pAttr->sClass;` |
|        7 | 1667 | `				sVArg.sTypeName = pAttr->sTypeName;` |
|        7 | 1668 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|      ! 0 | 1669 | `					sVArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|      ! 0 | 1670 | `				}` |
|        7 | 1671 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        3 | 1672 | `			}` |
|       15 | 1673 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       15 | 1674 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1675 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1676 | `				return SXERR_ABORT;` |
|        - | 1677 | `			}` |
|       15 | 1678 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|       15 | 1679 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|        - | 1680 | `		}` |
|      130 | 1681 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|      131 | 1682 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|        - | 1683 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|      ! 0 | 1684 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 1685 | `				"Abstract property hook cannot have body");` |
|      ! 0 | 1686 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1687 | `				return SXERR_ABORT;` |
|        - | 1688 | `			}` |
|      ! 0 | 1689 | `			return SXERR_CORRUPT;` |
|        - | 1690 | `		}` |
|      131 | 1691 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|        - | 1692 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|      131 | 1693 | `		if( pMeth == 0 ){` |
|      ! 0 | 1694 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1695 | `			return SXERR_ABORT;` |
|        - | 1696 | `		}` |
|      131 | 1697 | `		pMeth->sFunc.nLine = nHLine;` |
|      131 | 1698 | `		if( !bGet ){` |
|        - | 1699 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|       61 | 1700 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       17 | 1701 | `				SyToken *pRp = 0;` |
|       17 | 1702 | `				pGen->pIn++;` |
|       17 | 1703 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|       17 | 1704 | `				if( pRp >= pGen->pEnd ){` |
|      ! 0 | 1705 | `					goto HookSyntax;` |
|        - | 1706 | `				}` |
|       17 | 1707 | `				if( pGen->pIn < pRp ){` |
|       17 | 1708 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|       17 | 1709 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1710 | `						return SXERR_ABORT;` |
|        - | 1711 | `					}` |
|        8 | 1712 | `				}` |
|       17 | 1713 | `				pGen->pIn = &pRp[1];` |
|        8 | 1714 | `			}` |
|       61 | 1715 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|        - | 1716 | `				/* Implicit $value formal */` |
|        - | 1717 | `				ph7_vm_func_arg sVArg;` |
|       45 | 1718 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|       45 | 1719 | `				if( zVName == 0 ){` |
|      ! 0 | 1720 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1721 | `					return SXERR_ABORT;` |
|        - | 1722 | `				}` |
|       45 | 1723 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|       45 | 1724 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|       45 | 1725 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       45 | 1726 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       45 | 1727 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|       45 | 1728 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|       45 | 1729 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|       22 | 1730 | `			}` |
|       30 | 1731 | `		}` |
|      165 | 1732 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - | 1733 | `			/* Block body */` |
|       69 | 1734 | `			SyToken *pBodyStart = pGen->pIn;` |
|       69 | 1735 | `			SyToken *pCloser = 0;` |
|       69 | 1736 | `			int bParentCall = 0;` |
|       69 | 1737 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|       69 | 1738 | `			if( pCloser < pGen->pEnd ){` |
|        - | 1739 | `				SyToken *pScan;` |
|      753 | 1740 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|      687 | 1741 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|        3 | 1742 | `						bParentCall = 1;` |
|        3 | 1743 | `						break;` |
|        - | 1744 | `					}` |
|      343 | 1745 | `				}` |
|       34 | 1746 | `			}` |
|       69 | 1747 | `			if( bParentCall ){` |
|        - | 1748 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|        - | 1749 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|        - | 1750 | `				 * hook method), then continue past the original body. */` |
|        - | 1751 | `				SySet sBody;` |
|        3 | 1752 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|        3 | 1753 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        3 | 1754 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|        3 | 1755 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1756 | `					SySetRelease(&sBody);` |
|      ! 0 | 1757 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1758 | `					return SXERR_ABORT;` |
|        - | 1759 | `				}` |
|        3 | 1760 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|        3 | 1761 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|        3 | 1762 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        3 | 1763 | `				pGen->pIn = &pCloser[1];` |
|        3 | 1764 | `				pGen->pEnd = pSavedEnd;` |
|        3 | 1765 | `				SySetRelease(&sBody);` |
|        3 | 1766 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1767 | `					return SXERR_ABORT;` |
|        - | 1768 | `				}` |
|        3 | 1769 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|        2 | 1770 | `			}else{` |
|       67 | 1771 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|       67 | 1772 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1773 | `					return SXERR_ABORT;` |
|        - | 1774 | `				}` |
|       67 | 1775 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|        - | 1776 | `			}` |
|       69 | 1777 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|       17 | 1778 | `				bRefsSelf = 1;` |
|        9 | 1779 | `			}` |
|      128 | 1780 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|        - | 1781 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|        - | 1782 | `			GenBlock *pBlock;` |
|        - | 1783 | `			SySet *pInstrContainer;` |
|        - | 1784 | `			SyToken *pBodyStart;` |
|        - | 1785 | `			SyToken *pExprEnd;` |
|       63 | 1786 | `			SyToken *pSavedEnd = 0;` |
|        - | 1787 | `			SySet sBody;` |
|       63 | 1788 | `			int bParentCall = 0;` |
|       63 | 1789 | `			pGen->pIn++; /* Jump '=>' */` |
|       63 | 1790 | `			pBodyStart = pGen->pIn;` |
|        - | 1791 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|        - | 1792 | `			 * would end the enclosing hook list) and rewrite any` |
|        - | 1793 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|        - | 1794 | `			 * method on a token copy. */` |
|        - | 1795 | `			{` |
|       63 | 1796 | `				sxi32 iNest = 0;` |
|       63 | 1797 | `				pExprEnd = pBodyStart;` |
|      355 | 1798 | `				while( pExprEnd < pGen->pEnd ){` |
|      355 | 1799 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        9 | 1800 | `						iNest++;` |
|      351 | 1801 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        9 | 1802 | `						if( iNest <= 0 ){` |
|      ! 0 | 1803 | `							break;` |
|        - | 1804 | `						}` |
|        9 | 1805 | `						iNest--;` |
|      343 | 1806 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|       63 | 1807 | `						break;` |
|        - | 1808 | `					}` |
|      293 | 1809 | `					pExprEnd++;` |
|        1 | 1810 | `				}` |
|        - | 1811 | `			}` |
|        - | 1812 | `			{` |
|        - | 1813 | `				SyToken *pScan;` |
|      335 | 1814 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|      275 | 1815 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|        3 | 1816 | `						bParentCall = 1;` |
|        3 | 1817 | `						break;` |
|        - | 1818 | `					}` |
|      137 | 1819 | `				}` |
|        - | 1820 | `			}` |
|       63 | 1821 | `			if( bParentCall ){` |
|        3 | 1822 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        3 | 1823 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|        3 | 1824 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1825 | `					SySetRelease(&sBody);` |
|      ! 0 | 1826 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1827 | `					return SXERR_ABORT;` |
|        - | 1828 | `				}` |
|        3 | 1829 | `				pSavedEnd = pGen->pEnd;` |
|        3 | 1830 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|        3 | 1831 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|        1 | 1832 | `			}` |
|       94 | 1833 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       62 | 1834 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|       63 | 1835 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1836 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|      ! 0 | 1837 | `				return SXERR_ABORT;` |
|        - | 1838 | `			}` |
|       63 | 1839 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       63 | 1840 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|       63 | 1841 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|       63 | 1842 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       63 | 1843 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       63 | 1844 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       63 | 1845 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       63 | 1846 | `			GenStateLeaveBlock(&(*pGen),0);` |
|       63 | 1847 | `			if( bParentCall ){` |
|        3 | 1848 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|        3 | 1849 | `				pGen->pEnd = pSavedEnd;` |
|        3 | 1850 | `				SySetRelease(&sBody);` |
|        1 | 1851 | `			}` |
|       63 | 1852 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1853 | `				return SXERR_ABORT;` |
|        - | 1854 | `			}` |
|       63 | 1855 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|       63 | 1856 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|       37 | 1857 | `				bRefsSelf = 1;` |
|       18 | 1858 | `			}` |
|       63 | 1859 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       63 | 1860 | `				pGen->pIn++; /* Jump ';' */` |
|       31 | 1861 | `			}` |
|       63 | 1862 | `			if( !bGet ){` |
|        - | 1863 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|        - | 1864 | `				 * the dispatcher consumes the implicit return value — which` |
|        - | 1865 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|        - | 1866 | ``				 * for `$this->NAME = expr`). */`` |
|        3 | 1867 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|        3 | 1868 | `				bRefsSelf = 1;` |
|        1 | 1869 | `			}` |
|       32 | 1870 | `		}else{` |
|      ! 0 | 1871 | `			goto HookSyntax;` |
|        - | 1872 | `		}` |
|      131 | 1873 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|      131 | 1874 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1875 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1876 | `			return SXERR_ABORT;` |
|        - | 1877 | `		}` |
|      131 | 1878 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        1 | 1879 | `	}` |
|       95 | 1880 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|      ! 0 | 1881 | `		goto HookSyntax;` |
|        - | 1882 | `	}` |
|       95 | 1883 | `	pGen->pIn++; /* Jump '}' */` |
|       95 | 1884 | `	if( !bRefsSelf ){` |
|        - | 1885 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|        - | 1886 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|        - | 1887 | `		 * a default value (compile fatal, php's exact wording). */` |
|       41 | 1888 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|       41 | 1889 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      ! 0 | 1890 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1891 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|      ! 0 | 1892 | `				&pClass->sName,&pAttr->sName);` |
|      ! 0 | 1893 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1894 | `				return SXERR_ABORT;` |
|        - | 1895 | `			}` |
|      ! 0 | 1896 | `			return SXERR_CORRUPT;` |
|        - | 1897 | `		}` |
|       20 | 1898 | `	}` |
|       95 | 1899 | `	return SXRET_OK;` |
|      ! 0 | 1900 | `HookSyntax:` |
|      ! 0 | 1901 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1902 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|      ! 0 | 1903 | `		&pClass->sName,&pAttr->sName);` |
|      ! 0 | 1904 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1905 | `		return SXERR_ABORT;` |
|        - | 1906 | `	}` |
|      ! 0 | 1907 | `	return SXERR_CORRUPT;` |
|       48 | 1908 | `}` |
|        - | 1909 | `/*` |
|        - | 1910 | ` * Compile an object interface.` |
|        - | 1911 | ` *  According to the PHP language reference manual` |
|        - | 1912 | ` *   Object Interfaces:` |
|        - | 1913 | ` *   Object interfaces allow you to create code which specifies which methods` |
|        - | 1914 | ` *   a class must implement, without having to define how these methods are handled.` |
|        - | 1915 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - | 1916 | ` *   class, but without any of the methods having their contents defined.` |
|        - | 1917 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|        - | 1918 | ` */` |
|    70072 | 1919 | `PH7_PRIVATE sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|        5 | 1920 | `{` |
|    70077 | 1921 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 1922 | `	ph7_class *pClass,*pBase;` |
|        - | 1923 | `	SyToken *pEnd,*pTmp;` |
|        - | 1924 | `	SyString *pName;` |
|        - | 1925 | `	sxi32 nKwrd;` |
|        - | 1926 | `	sxi32 rc;` |
|        - | 1927 | `	/* Jump the 'interface' keyword */` |
|    70077 | 1928 | `	pGen->pIn++;` |
|        - | 1929 | `	/* Extract interface name */` |
|    70077 | 1930 | `	pName = &pGen->pIn->sData;` |
|        - | 1931 | `	/* Advance the stream cursor */` |
|    70077 | 1932 | `	pGen->pIn++;` |
|        - | 1933 | `	/* Build FQN and obtain a raw class */ {` |
|        - | 1934 | `		SyBlob sFQN;` |
|        - | 1935 | `		SyString sFQNStr;` |
|    70077 | 1936 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    70077 | 1937 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    70077 | 1938 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    70077 | 1939 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    70077 | 1940 | `		SyBlobRelease(&sFQN);` |
|        - | 1941 | `	}` |
|    70077 | 1942 | `	if( pClass == 0 ){` |
|      ! 0 | 1943 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1944 | `		return SXERR_ABORT;` |
|        - | 1945 | `	}` |
|    70077 | 1946 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    70077 | 1947 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 1948 | `		return SXERR_ABORT;` |
|        - | 1949 | `	}` |
|        - | 1950 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|    70077 | 1951 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|        - | 1952 | `	/* Assume no base class is given */` |
|    70077 | 1953 | `	pBase = 0;` |
|    70077 | 1954 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    27219 | 1955 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    27219 | 1956 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a, c, … */ ){` |
|        - | 1957 | `			/* php lets an interface extend SEVERAL parent interfaces. The first` |
|        - | 1958 | `			 * becomes pBase (single-inheritance chain, hDerived); every extra one` |
|        - | 1959 | `			 * is recorded via PH7_ClassImplement so it lands in aInterface — the` |
|        - | 1960 | `			 * runtime subtype walk (VmInterfaceReaches) follows both. */` |
|    27219 | 1961 | `			pGen->pIn++;` |
|    13608 | 1962 | `			for(;;){` |
|        - | 1963 | `				SyBlob sResolved;` |
|        - | 1964 | `				SyString sBaseName;` |
|        - | 1965 | `				sxu32 nRefLine;` |
|        - | 1966 | `				ph7_class *pParent;` |
|    27221 | 1967 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    27221 | 1968 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    27221 | 1969 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 1970 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 1971 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1972 | `						"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|      ! 0 | 1973 | `						pName);` |
|      ! 0 | 1974 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 1975 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1976 | `						return SXERR_ABORT;` |
|        - | 1977 | `					}` |
|      ! 0 | 1978 | `					return SXRET_OK;` |
|        - | 1979 | `				}` |
|    40829 | 1980 | `				pParent = PH7_VmExtractClass(pGen->pVm,` |
|    27216 | 1981 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    27221 | 1982 | `				SyStringInitFromBuf(&sBaseName,` |
|        - | 1983 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 1984 | `				/* Only interfaces is allowed */` |
|    27221 | 1985 | `				while( pParent && (pParent->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 1986 | `					pParent = pParent->pNextName;` |
|      ! 0 | 1987 | `				}` |
|    27221 | 1988 | `				if( pParent == 0 ){` |
|      ! 0 | 1989 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 1990 | `						"Nonexistent base interface '%z'",&sBaseName);` |
|      ! 0 | 1991 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1992 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 1993 | `						return SXERR_ABORT;` |
|      ! 0 | 1994 | `					}` |
|    27221 | 1995 | `				}else if( pBase == 0 ){` |
|        - | 1996 | `					/* First parent → single-inheritance base */` |
|    27219 | 1997 | `					pBase = pParent;` |
|    13612 | 1998 | `				}else{` |
|        - | 1999 | `					/* Additional parent → record it in aInterface (+ copy its` |
|        - | 2000 | `					 * constants/method stubs) so instanceof reaches it too. */` |
|        3 | 2001 | `					PH7_ClassImplement(pClass,pParent);` |
|        - | 2002 | `				}` |
|    27221 | 2003 | `				SyBlobRelease(&sResolved);` |
|        - | 2004 | `				/* Continue on a comma-separated list */` |
|    27221 | 2005 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 | 2006 | `					pGen->pIn++;` |
|        3 | 2007 | `					continue;` |
|        - | 2008 | `				}` |
|    27219 | 2009 | `				break;` |
|      ! 0 | 2010 | `			}` |
|    13607 | 2011 | `		}` |
|    13607 | 2012 | `	}` |
|    70077 | 2013 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - | 2014 | `		/* Syntax error */` |
|      ! 0 | 2015 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|      ! 0 | 2016 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 2017 | `		if( rc == SXERR_ABORT ){` |
|        - | 2018 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 2019 | `			return SXERR_ABORT;` |
|        - | 2020 | `		}` |
|      ! 0 | 2021 | `		return SXRET_OK;` |
|        - | 2022 | `	}` |
|    70077 | 2023 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    70077 | 2024 | `	pEnd = 0; /* cc warning */` |
|        - | 2025 | `	/* Delimit the interface body */` |
|    70077 | 2026 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    70077 | 2027 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 2028 | `		/* Syntax error */` |
|      ! 0 | 2029 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|      ! 0 | 2030 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 2031 | `		if( rc == SXERR_ABORT ){` |
|        - | 2032 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 2033 | `			return SXERR_ABORT;` |
|        - | 2034 | `		}` |
|      ! 0 | 2035 | `		return SXRET_OK;` |
|        - | 2036 | `	}` |
|        - | 2037 | `	/* The delimiter token is the interface body's closing brace */` |
|    70077 | 2038 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 2039 | `	/* Swap token stream */` |
|    70077 | 2040 | `	pTmp = pGen->pEnd;` |
|    70077 | 2041 | `	pGen->pEnd = pEnd;` |
|        - | 2042 | `	/* Start the parse process` |
|        - | 2043 | `	 * Note (According to the PHP reference manual):` |
|        - | 2044 | `	 *  Only constants and function signatures(without body) are allowed.` |
|        - | 2045 | `	 *  Only 'public' visibility is allowed.` |
|        - | 2046 | `	 */` |
|   128333 | 2047 | `	for(;;){` |
|        - | 2048 | `		/* Jump leading/trailing semi-colons */` |
|   443269 | 2049 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   186599 | 2050 | `			pGen->pIn++;` |
|        5 | 2051 | `		}` |
|   256675 | 2052 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 2053 | `			/* End of interface body */` |
|    70073 | 2054 | `			break;` |
|        - | 2055 | `		}` |
|        - | 2056 | `		/* Bind a directly-preceding docblock to this member */` |
|   186607 | 2057 | `		GenStateSetPendingDoc(&(*pGen));` |
|   186607 | 2058 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 2059 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 2060 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|      ! 0 | 2061 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 2062 | `			if( rc == SXERR_ABORT ){` |
|        - | 2063 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 2064 | `				return SXERR_ABORT;` |
|        - | 2065 | `			}` |
|      ! 0 | 2066 | `			goto done;` |
|        - | 2067 | `		}` |
|        - | 2068 | `		/* Extract the current keyword */` |
|   186607 | 2069 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   186607 | 2070 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        - | 2071 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|        - | 2072 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|        3 | 2073 | `			const char *zKind = "member";` |
|        3 | 2074 | `			SyString *pMemberName = 0;` |
|        3 | 2075 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|        3 | 2076 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|        3 | 2077 | `				if( nNext == PH7_TKWRD_CONST ){` |
|        3 | 2078 | `					zKind = "constant";` |
|        3 | 2079 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|        3 | 2080 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|        2 | 2081 | `					}` |
|        1 | 2082 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 2083 | `					zKind = "method";` |
|      ! 0 | 2084 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|      ! 0 | 2085 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|      ! 0 | 2086 | `					}` |
|      ! 0 | 2087 | `				}` |
|        1 | 2088 | `			}` |
|        3 | 2089 | `			if( pMemberName ){` |
|        4 | 2090 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        1 | 2091 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|        2 | 2092 | `			}else{` |
|      ! 0 | 2093 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2094 | `					"Access type for interface %s must be public",zKind);` |
|        - | 2095 | `			}` |
|        3 | 2096 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2097 | `				return SXERR_ABORT;` |
|        - | 2098 | `			}` |
|        3 | 2099 | `			goto done;` |
|        - | 2100 | `		}` |
|   186605 | 2101 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|      ! 0 | 2102 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2103 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 | 2104 | `			if( rc == SXERR_ABORT ){` |
|        - | 2105 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 2106 | `				return SXERR_ABORT;` |
|        - | 2107 | `			}` |
|      ! 0 | 2108 | `			goto done;` |
|        - | 2109 | `		}` |
|   186605 | 2110 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|        - | 2111 | `			/* Advance the stream cursor */` |
|   132183 | 2112 | `			pGen->pIn++;` |
|   132178 | 2113 | `			if( pGen->pIn < pGen->pEnd` |
|   132183 | 2114 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|   132178 | 2115 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|        - | 2116 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|        - | 2117 | `				 * requirement. The attribute compiler + hook parser handle it` |
|        - | 2118 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|        - | 2119 | `				 * property without hooks is ITS "Interfaces may only include` |
|        - | 2120 | `				 * hooked properties" error). */` |
|      ! 0 | 2121 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|      ! 0 | 2122 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|      ! 0 | 2123 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 2124 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2125 | `						return SXERR_ABORT;` |
|        - | 2126 | `					}` |
|      ! 0 | 2127 | `					goto done;` |
|        - | 2128 | `				}` |
|      ! 0 | 2129 | `				continue;` |
|        - | 2130 | `			}` |
|   132183 | 2131 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|        - | 2132 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|        - | 2133 | `				 * '$' also opens a hooked-property requirement. */` |
|      ! 0 | 2134 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|      ! 0 | 2135 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|      ! 0 | 2136 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|      ! 0 | 2137 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|      ! 0 | 2138 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|      ! 0 | 2139 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 2140 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 2141 | `							return SXERR_ABORT;` |
|        - | 2142 | `						}` |
|      ! 0 | 2143 | `						goto done;` |
|        - | 2144 | `					}` |
|      ! 0 | 2145 | `					continue;` |
|        - | 2146 | `				}` |
|      ! 0 | 2147 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2148 | `					"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 | 2149 | `				if( rc == SXERR_ABORT ){` |
|        - | 2150 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 | 2151 | `					return SXERR_ABORT;` |
|        - | 2152 | `				}` |
|      ! 0 | 2153 | `				goto done;` |
|        - | 2154 | `			}` |
|   132183 | 2155 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   132183 | 2156 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|        - | 2157 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|        - | 2158 | `				 * hooked-property requirement (PHP 8.4). */` |
|        4 | 2159 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|        5 | 2160 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|        7 | 2161 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|        2 | 2162 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|        5 | 2163 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 2164 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 2165 | `							return SXERR_ABORT;` |
|        - | 2166 | `						}` |
|      ! 0 | 2167 | `						goto done;` |
|        - | 2168 | `					}` |
|        5 | 2169 | `					continue;` |
|        - | 2170 | `				}` |
|      ! 0 | 2171 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2172 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 | 2173 | `				if( rc == SXERR_ABORT ){` |
|        - | 2174 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 | 2175 | `					return SXERR_ABORT;` |
|        - | 2176 | `				}` |
|      ! 0 | 2177 | `				goto done;` |
|        - | 2178 | `			}` |
|    66087 | 2179 | `		}` |
|   186601 | 2180 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|        - | 2181 | `			/* Parse constant */` |
|    54423 | 2182 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|    54423 | 2183 | `			if( rc != SXRET_OK ){` |
|        3 | 2184 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2185 | `					return SXERR_ABORT;` |
|        - | 2186 | `				}` |
|        3 | 2187 | `				goto done;` |
|        - | 2188 | `			}` |
|    27213 | 2189 | `		}else{` |
|   132183 | 2190 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   132183 | 2191 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - | 2192 | `				/* Static method,record that */` |
|    11663 | 2193 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|        - | 2194 | `				/* Advance the stream cursor */` |
|    11663 | 2195 | `				pGen->pIn++;` |
|    11658 | 2196 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    11663 | 2197 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 2198 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2199 | `							"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 | 2200 | `						if( rc == SXERR_ABORT ){` |
|        - | 2201 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 2202 | `							return SXERR_ABORT;` |
|        - | 2203 | `						}` |
|      ! 0 | 2204 | `						goto done;` |
|        - | 2205 | `				}` |
|     5829 | 2206 | `			}` |
|        - | 2207 | `			/* Process method signature (no body for interface methods) */` |
|   132183 | 2208 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   132183 | 2209 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2210 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2211 | `					return SXERR_ABORT;` |
|        - | 2212 | `				}` |
|      ! 0 | 2213 | `				goto done;` |
|        - | 2214 | `			}` |
|        - | 2215 | `		}` |
|        5 | 2216 | `	}` |
|        - | 2217 | `	/* Reject a php-fatal redeclaration before hoisting the interface */` |
|    70073 | 2218 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|        3 | 2219 | `		return SXERR_ABORT;` |
|        - | 2220 | `	}` |
|        - | 2221 | `	/* Install the interface */` |
|    70071 | 2222 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    70071 | 2223 | `	if( rc == SXRET_OK && pBase ){` |
|        - | 2224 | `		/* Inherit from the base interface */` |
|    27219 | 2225 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    13607 | 2226 | `	}` |
|    70071 | 2227 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2228 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2229 | `		return SXERR_ABORT;` |
|        - | 2230 | `	}` |
|    35033 | 2231 | `done:` |
|        - | 2232 | `	/* Point beyond the interface body */` |
|    70075 | 2233 | `	pGen->pIn  = &pEnd[1];` |
|    70075 | 2234 | `	pGen->pEnd = pTmp;` |
|    70075 | 2235 | `	return PH7_OK;` |
|    35041 | 2236 | `}` |
|        - | 2237 | `/*` |
|        - | 2238 | ` * Compile a user-defined class.` |
|        - | 2239 | ` * According to the PHP language reference manual` |
|        - | 2240 | ` *  class` |
|        - | 2241 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|        - | 2242 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|        - | 2243 | ` *  of the properties and methods belonging to the class.` |
|        - | 2244 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|        - | 2245 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|        - | 2246 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|        - | 2247 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - | 2248 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|        - | 2249 | ` *  (called "methods").` |
|        - | 2250 | ` */` |
|        - | 2251 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|        - | 2252 | `typedef struct TraitUseEntry TraitUseEntry;` |
|        - | 2253 | `struct TraitUseEntry {` |
|        - | 2254 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|        - | 2255 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|        - | 2256 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|        - | 2257 | `};` |
|        - | 2258 | `/*` |
|        - | 2259 | ` * Validate that methods implementing interface contracts have compatible` |
|        - | 2260 | ` * signatures: public visibility and at least as many parameters as declared.` |
|        - | 2261 | ` */` |
|   452716 | 2262 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 2263 | `{` |
|        - | 2264 | `	ph7_class **apIface;` |
|        - | 2265 | `	sxu32 nIface,i;` |
|        - | 2266 | `	sxi32 rc;` |
|   452721 | 2267 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      ! 0 | 2268 | `		return SXRET_OK;` |
|        - | 2269 | `	}` |
|   452721 | 2270 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   452721 | 2271 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   876649 | 2272 | `	for(i = 0; i < nIface; i++){` |
|   423933 | 2273 | `		ph7_class *pIface = apIface[i];` |
|        - | 2274 | `		SyHashEntry *pEntry;` |
|   423933 | 2275 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  1248371 | 2276 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   824443 | 2277 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - | 2278 | `			ph7_class_method *pImplMeth;` |
|   824443 | 2279 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|        - | 2280 | `			/* Find the implementing method in the class */` |
|   824443 | 2281 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   824443 | 2282 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       23 | 2283 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|        - | 2284 | `			}` |
|        - | 2285 | `			/* Check visibility: interface methods must be implemented as public */` |
|   824425 | 2286 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        4 | 2287 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - | 2288 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|        1 | 2289 | `					&pClass->sName,pMName,&pIface->sName);` |
|        3 | 2290 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2291 | `					return SXERR_ABORT;` |
|        - | 2292 | `				}` |
|        1 | 2293 | `			}` |
|        - | 2294 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|        - | 2295 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|        - | 2296 | `			 */` |
|        - | 2297 | `			{` |
|   824425 | 2298 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   824425 | 2299 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   824425 | 2300 | `				int sigError = 0;` |
|   824425 | 2301 | `				if( nImplArgs < nIfaceArgs ){` |
|        3 | 2302 | `					sigError = 1;` |
|   824424 | 2303 | `				}else if( nImplArgs > nIfaceArgs ){` |
|        - | 2304 | `					/* Extra parameters must all have default values */` |
|     3895 | 2305 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        - | 2306 | `					sxu32 k;` |
|     7783 | 2307 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|     3895 | 2308 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|        3 | 2309 | `							sigError = 1;` |
|        3 | 2310 | `							break;` |
|        - | 2311 | `						}` |
|     1949 | 2312 | `					}` |
|     1945 | 2313 | `				}` |
|   824425 | 2314 | `				if( sigError ){` |
|        - | 2315 | `					SyBlob sImplSig, sIfaceSig;` |
|        - | 2316 | `					ph7_vm_func_arg *aArgs;` |
|        - | 2317 | `					sxu32 j;` |
|        6 | 2318 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|        6 | 2319 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|        - | 2320 | `					/* Build implementing method signature */` |
|        6 | 2321 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       12 | 2322 | `					for(j = 0; j < nImplArgs; j++){` |
|        8 | 2323 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|        8 | 2324 | `						SyBlobAppend(&sImplSig,"$",1);` |
|        8 | 2325 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 | 2326 | `					}` |
|        - | 2327 | `					/* Build interface method signature */` |
|        6 | 2328 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|       12 | 2329 | `					for(j = 0; j < nIfaceArgs; j++){` |
|        8 | 2330 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|        8 | 2331 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|        8 | 2332 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 | 2333 | `					}` |
|        8 | 2334 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - | 2335 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|        2 | 2336 | `						&pClass->sName,pMName,` |
|        4 | 2337 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|        2 | 2338 | `						&pIface->sName,pMName,` |
|        4 | 2339 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|        6 | 2340 | `					SyBlobRelease(&sImplSig);` |
|        6 | 2341 | `					SyBlobRelease(&sIfaceSig);` |
|        6 | 2342 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2343 | `						return SXERR_ABORT;` |
|        - | 2344 | `					}` |
|        2 | 2345 | `				}` |
|        - | 2346 | `			}` |
|        5 | 2347 | `		}` |
|   211969 | 2348 | `	}` |
|   452721 | 2349 | `	return SXRET_OK;` |
|   226363 | 2350 | `}` |
|        - | 2351 | `/*` |
|        - | 2352 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|        - | 2353 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|        - | 2354 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|        - | 2355 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|        - | 2356 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|        - | 2357 | ` * means that specific hook is still missing.` |
|        - | 2358 | ` */` |
|       38 | 2359 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|        5 | 2360 | `{` |
|        - | 2361 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        - | 2362 | `	ph7_class_attr *pProp;` |
|       38 | 2363 | `	if( pMName->nByte <= nPfx` |
|       27 | 2364 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|        4 | 2365 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|       36 | 2366 | `		return 0; /* not a hook stub */` |
|        - | 2367 | `	}` |
|        7 | 2368 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|        7 | 2369 | `	return pProp != 0` |
|        6 | 2370 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|        3 | 2371 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|       24 | 2372 | `}` |
|        - | 2373 | `/*` |
|        - | 2374 | ` * Append an abstract member's display name to the message blob, translating a` |
|        - | 2375 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|        - | 2376 | ` */` |
|       16 | 2377 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|        4 | 2378 | `{` |
|        - | 2379 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|       16 | 2380 | `	if( pMName->nByte > nPfx` |
|       12 | 2381 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|      ! 0 | 2382 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|      ! 0 | 2383 | `		SyBlobAppend(pMsg,"$",1);` |
|      ! 0 | 2384 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|      ! 0 | 2385 | `		SyBlobAppend(pMsg,"::",2);` |
|      ! 0 | 2386 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|      ! 0 | 2387 | `		return;` |
|        - | 2388 | `	}` |
|       20 | 2389 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|       12 | 2390 | `}` |
|        - | 2391 | `/*` |
|        - | 2392 | ` * Check that a concrete class has no remaining abstract methods.` |
|        - | 2393 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|        - | 2394 | ` */` |
|   452716 | 2395 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 2396 | `{` |
|        - | 2397 | `	ph7_class_method *pMeth;` |
|        - | 2398 | `	SyHashEntry *pEntry;` |
|        - | 2399 | `	sxu32 nAbstract;` |
|        - | 2400 | `	SyBlob sMsg;` |
|        - | 2401 | `	sxi32 rc;` |
|        - | 2402 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   452721 | 2403 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|    19489 | 2404 | `		return SXRET_OK;` |
|        - | 2405 | `	}` |
|        - | 2406 | `	/* Count abstract methods */` |
|   433237 | 2407 | `	nAbstract = 0;` |
|   433237 | 2408 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  6473909 | 2409 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  5824061 | 2410 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  5824061 | 2411 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       27 | 2412 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|        7 | 2413 | `				continue; /* hook requirement met by a plain property (php) */` |
|        - | 2414 | `			}` |
|       20 | 2415 | `			nAbstract++;` |
|        8 | 2416 | `		}` |
|        5 | 2417 | `	}` |
|   433237 | 2418 | `	if( nAbstract == 0 ){` |
|   433223 | 2419 | `		return SXRET_OK;` |
|        - | 2420 | `	}` |
|        - | 2421 | `	/* Build the error message listing all abstract methods with origins */` |
|       18 | 2422 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       18 | 2423 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|        - | 2424 | `		"be declared abstract or implement the remaining method%s (",` |
|        7 | 2425 | `		&pClass->sName,nAbstract,` |
|        7 | 2426 | `		(nAbstract > 1 ? "s" : ""),` |
|        7 | 2427 | `		(nAbstract > 1 ? "s" : ""));` |
|        - | 2428 | `	/* Second pass: list methods with origins */` |
|        - | 2429 | `	{` |
|       18 | 2430 | `		sxu32 nListed = 0;` |
|       18 | 2431 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|       36 | 2432 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|       22 | 2433 | `			ph7_class *pOrigin = 0;` |
|        - | 2434 | `			SyString *pMName;` |
|       22 | 2435 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|       22 | 2436 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        3 | 2437 | `				continue;` |
|        - | 2438 | `			}` |
|       20 | 2439 | `			pMName = &pMeth->sFunc.sName;` |
|       20 | 2440 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|      ! 0 | 2441 | `				continue; /* hook requirement met by a plain property (php) */` |
|        - | 2442 | `			}` |
|       20 | 2443 | `			if( nListed > 0 ){` |
|        3 | 2444 | `				SyBlobAppend(&sMsg,", ",2);` |
|        1 | 2445 | `			}` |
|        - | 2446 | `			/* Find the origin of this abstract method.` |
|        - | 2447 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|        - | 2448 | `			 * inheritance chains) take precedence for interface-declared` |
|        - | 2449 | `			 * methods. Abstract class methods only win when the class` |
|        - | 2450 | `			 * itself declared the abstract method (not inherited from` |
|        - | 2451 | `			 * an interface). Trait methods are adopted into the using` |
|        - | 2452 | `			 * class's namespace.` |
|        - | 2453 | `			 */` |
|        - | 2454 | `			{` |
|        - | 2455 | `				ph7_class **apIface;` |
|        - | 2456 | `				ph7_class **apTrait;` |
|        - | 2457 | `				ph7_class *pWalk;` |
|        - | 2458 | `				sxu32 i;` |
|        - | 2459 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|        - | 2460 | `				 * (one that was written in the class body, not inherited from an` |
|        - | 2461 | `				 * interface). PHP attributes origin to the declaring class.` |
|        - | 2462 | `				 */` |
|       20 | 2463 | `				if( pClass->pBase ){` |
|       11 | 2464 | `					pWalk = pClass->pBase;` |
|       19 | 2465 | `					while( pWalk ){` |
|        - | 2466 | `						ph7_class_method *pParentMeth;` |
|       13 | 2467 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|       13 | 2468 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        - | 2469 | `							/* Exclude methods that came from an interface anywhere` |
|        - | 2470 | `							 * in this class's ancestor chain.` |
|        - | 2471 | `							 */` |
|       13 | 2472 | `							int fromIface = 0;` |
|       13 | 2473 | `							ph7_class *pAnc = pWalk;` |
|       17 | 2474 | `							while( pAnc ){` |
|        - | 2475 | `								ph7_class **apPI;` |
|        - | 2476 | `								sxu32 j;` |
|       15 | 2477 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|       15 | 2478 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       10 | 2479 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       10 | 2480 | `										fromIface = 1;` |
|       10 | 2481 | `										break;` |
|        - | 2482 | `									}` |
|      ! 0 | 2483 | `								}` |
|       15 | 2484 | `								if( fromIface ) break;` |
|        6 | 2485 | `								pAnc = pAnc->pBase;` |
|        2 | 2486 | `							}` |
|       13 | 2487 | `							if( !fromIface ){` |
|        3 | 2488 | `								pOrigin = pWalk;` |
|        3 | 2489 | `								break;` |
|        - | 2490 | `							}` |
|        4 | 2491 | `						}` |
|       10 | 2492 | `						pWalk = pWalk->pBase;` |
|        2 | 2493 | `					}` |
|        4 | 2494 | `				}` |
|        - | 2495 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|        - | 2496 | `				 * each interface's own parent chain for the deepest origin.` |
|        - | 2497 | `				 */` |
|       20 | 2498 | `				if( !pOrigin ){` |
|       18 | 2499 | `					pWalk = pClass;` |
|       40 | 2500 | `					while( pWalk && !pOrigin ){` |
|       26 | 2501 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|       26 | 2502 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|       16 | 2503 | `							ph7_class *pIface = apIface[i];` |
|       16 | 2504 | `							ph7_class *pDeepest = 0;` |
|       28 | 2505 | `							while( pIface ){` |
|       16 | 2506 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|       16 | 2507 | `									pDeepest = pIface;` |
|        6 | 2508 | `								}` |
|       16 | 2509 | `								pIface = pIface->pBase;` |
|        4 | 2510 | `							}` |
|       16 | 2511 | `							if( pDeepest ){` |
|       16 | 2512 | `								pOrigin = pDeepest;` |
|       16 | 2513 | `								break;` |
|        - | 2514 | `							}` |
|      ! 0 | 2515 | `						}` |
|       26 | 2516 | `						pWalk = pWalk->pBase;` |
|        4 | 2517 | `					}` |
|        7 | 2518 | `				}` |
|        - | 2519 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|       20 | 2520 | `				if( !pOrigin ){` |
|        3 | 2521 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        3 | 2522 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|        3 | 2523 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|        3 | 2524 | `							pOrigin = pClass;` |
|        3 | 2525 | `							break;` |
|        - | 2526 | `						}` |
|      ! 0 | 2527 | `					}` |
|        1 | 2528 | `				}` |
|        - | 2529 | `			}` |
|       20 | 2530 | `			if( pOrigin ){` |
|       20 | 2531 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|       12 | 2532 | `			}else{` |
|        - | 2533 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|      ! 0 | 2534 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|        - | 2535 | `			}` |
|       20 | 2536 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|       20 | 2537 | `			nListed++;` |
|        4 | 2538 | `		}` |
|        - | 2539 | `	}` |
|       18 | 2540 | `	SyBlobAppend(&sMsg,")",1);` |
|       25 | 2541 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|       14 | 2542 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|       18 | 2543 | `	SyBlobRelease(&sMsg);` |
|       18 | 2544 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2545 | `		return SXERR_ABORT;` |
|        - | 2546 | `	}` |
|       18 | 2547 | `	return SXRET_OK;` |
|   226363 | 2548 | `}` |
|        - | 2549 | `/*` |
|        - | 2550 | ` * Parse a class/interface name reference from the current token stream.` |
|        - | 2551 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|        - | 2552 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|        - | 2553 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|        - | 2554 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|        - | 2555 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|        - | 2556 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|        - | 2557 | ` */` |
|   515308 | 2558 | `PH7_PRIVATE sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|        5 | 2559 | `{` |
|   515313 | 2560 | `	int isAbsolute = 0;` |
|   515313 | 2561 | `	SyToken *pStart = pGen->pIn;` |
|        - | 2562 | `	SyBlob sName;` |
|   515313 | 2563 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|     4541 | 2564 | `		isAbsolute = 1;` |
|     4541 | 2565 | `		pGen->pIn++;` |
|     2268 | 2566 | `	}` |
|   515313 | 2567 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        8 | 2568 | `		pGen->pIn = pStart;` |
|        8 | 2569 | `		return SXERR_INVALID;` |
|        - | 2570 | `	}` |
|   515307 | 2571 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   515307 | 2572 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   515307 | 2573 | `	pGen->pIn++;` |
|   772992 | 2574 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   257695 | 2575 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       28 | 2576 | `		SyBlobAppend(&sName,"\\",1);` |
|       28 | 2577 | `		pGen->pIn++;` |
|       28 | 2578 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       28 | 2579 | `		pGen->pIn++;` |
|        2 | 2580 | `	}` |
|   515307 | 2581 | `	if( isAbsolute ){` |
|     4539 | 2582 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     2272 | 2583 | `	}else{` |
|        - | 2584 | `		SyString sRaw;` |
|   510773 | 2585 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   510773 | 2586 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|        - | 2587 | `	}` |
|   515307 | 2588 | `	SyBlobRelease(&sName);` |
|   515307 | 2589 | `	return SXRET_OK;` |
|   257659 | 2590 | `}` |
|        - | 2591 | `/*` |
|        - | 2592 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|        - | 2593 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|        - | 2594 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|        - | 2595 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|        - | 2596 | ` * either direction cannot run unbounded.` |
|        - | 2597 | ` */` |
|        - | 2598 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|   198398 | 2599 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|        5 | 2600 | `{` |
|        - | 2601 | `	ph7_class **apParent;` |
|        - | 2602 | `	sxu32 n;` |
|   525079 | 2603 | `	while( pInterface ){` |
|   334463 | 2604 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|      ! 0 | 2605 | `			return FALSE;` |
|        - | 2606 | `		}` |
|   373343 | 2607 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    77760 | 2608 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|     7787 | 2609 | `			return TRUE;` |
|        - | 2610 | `		}` |
|   326681 | 2611 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|   326683 | 2612 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|        3 | 2613 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|      ! 0 | 2614 | `				return TRUE;` |
|        - | 2615 | `			}` |
|        2 | 2616 | `		}` |
|   326681 | 2617 | `		pInterface = pInterface->pBase;` |
|   326681 | 2618 | `		iDepth++;` |
|        5 | 2619 | `	}` |
|   190621 | 2620 | `	return FALSE;` |
|    99204 | 2621 | `}` |
|   198396 | 2622 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|        5 | 2623 | `{` |
|   198401 | 2624 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|        5 | 2625 | `}` |
|        - | 2626 | `/*` |
|        - | 2627 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|        - | 2628 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|        - | 2629 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|        - | 2630 | ` */` |
|     7782 | 2631 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|        5 | 2632 | `{` |
|     7791 | 2633 | `	while( pBase ){` |
|       10 | 2634 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|        2 | 2635 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|        3 | 2636 | `			return TRUE;` |
|        - | 2637 | `		}` |
|       10 | 2638 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|        6 | 2639 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|        3 | 2640 | `			return TRUE;` |
|        - | 2641 | `		}` |
|        5 | 2642 | `		pBase = pBase->pBase;` |
|        1 | 2643 | `	}` |
|     7783 | 2644 | `	return FALSE;` |
|     3896 | 2645 | `}` |
|        - | 2646 | `/*` |
|        - | 2647 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|        - | 2648 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|        - | 2649 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|        - | 2650 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|        - | 2651 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|        - | 2652 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|        - | 2653 | ` * pClass->aEnumCases for cases().` |
|        - | 2654 | ` */` |
|     7826 | 2655 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 2656 | `{` |
|     7831 | 2657 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2658 | `	SySet *pInstrContainer;` |
|        - | 2659 | `	ph7_class_attr *pCase;` |
|        - | 2660 | `	SyString *pName;` |
|        - | 2661 | `	sxi32 rc;` |
|     7831 | 2662 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|     7831 | 2663 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2664 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 2665 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|      ! 0 | 2666 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2667 | `			return SXERR_ABORT;` |
|        - | 2668 | `		}` |
|      ! 0 | 2669 | `		goto Synchronize;` |
|        - | 2670 | `	}` |
|     7831 | 2671 | `	pName = &pGen->pIn->sData;` |
|        - | 2672 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|     7831 | 2673 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      ! 0 | 2674 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2675 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|      ! 0 | 2676 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2677 | `			return SXERR_ABORT;` |
|        - | 2678 | `		}` |
|      ! 0 | 2679 | `		goto Synchronize;` |
|        - | 2680 | `	}` |
|     7831 | 2681 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 2682 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|     7831 | 2683 | `	if( pCase == 0 ){` |
|      ! 0 | 2684 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2685 | `		return SXERR_ABORT;` |
|        - | 2686 | `	}` |
|     7831 | 2687 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|     7831 | 2688 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 2689 | `		return SXERR_ABORT;` |
|        - | 2690 | `	}` |
|     7831 | 2691 | `	pGen->pIn++; /* Jump the case name */` |
|     7831 | 2692 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|     7817 | 2693 | `		if( pClass->nEnumBacking == 0 ){` |
|        8 | 2694 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        2 | 2695 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|        6 | 2696 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2697 | `				return SXERR_ABORT;` |
|        - | 2698 | `			}` |
|        6 | 2699 | `			goto Synchronize;` |
|        - | 2700 | `		}` |
|     7813 | 2701 | `		pGen->pIn++; /* Jump the equal sign */` |
|        - | 2702 | `		/* Compile the backing value expression into the case's own container` |
|        - | 2703 | `		 * (same technique as class constants). */` |
|     7813 | 2704 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     7813 | 2705 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|     7813 | 2706 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     7813 | 2707 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2708 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 2709 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|      ! 0 | 2710 | `		}` |
|     7813 | 2711 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|     7813 | 2712 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     7813 | 2713 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2714 | `			return SXERR_ABORT;` |
|        - | 2715 | `		}` |
|     3909 | 2716 | `	}else{` |
|       17 | 2717 | `		if( pClass->nEnumBacking != 0 ){` |
|      ! 0 | 2718 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 2719 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|      ! 0 | 2720 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2721 | `				return SXERR_ABORT;` |
|        - | 2722 | `			}` |
|      ! 0 | 2723 | `			goto Synchronize;` |
|        - | 2724 | `		}` |
|        - | 2725 | `	}` |
|     7827 | 2726 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|     7827 | 2727 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2728 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2729 | `		return SXERR_ABORT;` |
|        - | 2730 | `	}` |
|     7827 | 2731 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|     7827 | 2732 | `	return SXRET_OK;` |
|        2 | 2733 | `Synchronize:` |
|        - | 2734 | `	/* Synchronize with the first semi-colon */` |
|       14 | 2735 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       10 | 2736 | `		pGen->pIn++;` |
|        2 | 2737 | `	}` |
|        6 | 2738 | `	return SXERR_CORRUPT;` |
|     3918 | 2739 | `}` |
|        - | 2740 | `/*` |
|        - | 2741 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|        - | 2742 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|        - | 2743 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|        - | 2744 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|        - | 2745 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|        - | 2746 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|        - | 2747 | ` * pointers into it (see the constructor-promotion precedent above).` |
|        - | 2748 | ` */` |
|     3918 | 2749 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 2750 | `{` |
|        - | 2751 | `	SyToken *pSaveIn,*pSaveEnd;` |
|        - | 2752 | `	const char *zBack;` |
|        - | 2753 | `	SySet sToken;` |
|        - | 2754 | `	char *zSrc;` |
|        - | 2755 | `	sxu32 nSrc,nMax;` |
|     3923 | 2756 | `	sxi32 rc = SXRET_OK;` |
|     3923 | 2757 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|     3918 | 2758 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|     3923 | 2759 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|     3923 | 2760 | `	if( zSrc == 0 ){` |
|      ! 0 | 2761 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2762 | `		return SXERR_ABORT;` |
|        - | 2763 | `	}` |
|     3923 | 2764 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|     3923 | 2765 | `	if( pClass->nEnumBacking != 0 ){` |
|     5855 | 2766 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        - | 2767 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|        - | 2768 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|        - | 2769 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|     1950 | 2770 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|     1955 | 2771 | `	}else{` |
|       30 | 2772 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        9 | 2773 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|        - | 2774 | `	}` |
|     3923 | 2775 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|     3923 | 2776 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|     3923 | 2777 | `	pSaveIn = pGen->pIn;` |
|     3923 | 2778 | `	pSaveEnd = pGen->pEnd;` |
|     3923 | 2779 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|     3923 | 2780 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|    15641 | 2781 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|    11723 | 2782 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|        5 | 2783 | `	}` |
|     3923 | 2784 | `	pGen->pIn = pSaveIn;` |
|     3923 | 2785 | `	pGen->pEnd = pSaveEnd;` |
|     3923 | 2786 | `	SySetRelease(&sToken);` |
|     3923 | 2787 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|     1964 | 2788 | `}` |
|        - | 2789 | `/*` |
|        - | 2790 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|        - | 2791 | ` * __call/__callStatic/__invoke stay allowed).` |
|        - | 2792 | ` */` |
|        - | 2793 | `static const char *azEnumBannedMagic[] = {` |
|        - | 2794 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|        - | 2795 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|        - | 2796 | `};` |
|        - | 2797 | `/*` |
|        - | 2798 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|        - | 2799 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|        - | 2800 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|        - | 2801 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|        - | 2802 | ` * and before the class is installed.` |
|        - | 2803 | ` */` |
|     3918 | 2804 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|        5 | 2805 | `{` |
|        - | 2806 | `	SyHashEntry *pEntry;` |
|        - | 2807 | `	sxi32 rc;` |
|        - | 2808 | `	sxu32 n;` |
|        - | 2809 | `	/* php: "Enum %s cannot include properties" */` |
|     3923 | 2810 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    11751 | 2811 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     7835 | 2812 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     7835 | 2813 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        3 | 2814 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|        1 | 2815 | `				"Enum %z cannot include properties",&pClass->sName);` |
|        3 | 2816 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2817 | `				return SXERR_ABORT;` |
|        - | 2818 | `			}` |
|        3 | 2819 | `			break;` |
|        - | 2820 | `		}` |
|        5 | 2821 | `	}` |
|        - | 2822 | `	/* php: "Enum %s cannot include magic method %s" */` |
|    54857 | 2823 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|    76401 | 2824 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|    50939 | 2825 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|      ! 0 | 2826 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 2827 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|      ! 0 | 2828 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2829 | `				return SXERR_ABORT;` |
|        - | 2830 | `			}` |
|      ! 0 | 2831 | `		}` |
|    25472 | 2832 | `	}` |
|        - | 2833 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|        - | 2834 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|        - | 2835 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|        - | 2836 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|        - | 2837 | `	{` |
|        - | 2838 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|        - | 2839 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|        - | 2840 | `		ph7_class_attr *pAttr;` |
|     3923 | 2841 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 2842 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|     3923 | 2843 | `		if( pAttr == 0 ){` |
|      ! 0 | 2844 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2845 | `			return SXERR_ABORT;` |
|        - | 2846 | `		}` |
|     3923 | 2847 | `		pAttr->nType = MEMOBJ_STRING;` |
|     3923 | 2848 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|     3923 | 2849 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|     3923 | 2850 | `		if( pClass->nEnumBacking != 0 ){` |
|     3905 | 2851 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 2852 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|     3905 | 2853 | `			if( pAttr == 0 ){` |
|      ! 0 | 2854 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2855 | `				return SXERR_ABORT;` |
|        - | 2856 | `			}` |
|     3905 | 2857 | `			pAttr->nType = pClass->nEnumBacking;` |
|     3905 | 2858 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        7 | 2859 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|        4 | 2860 | `			}else{` |
|     3899 | 2861 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|        - | 2862 | `			}` |
|     3905 | 2863 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|     1950 | 2864 | `		}` |
|        - | 2865 | `	}` |
|     3923 | 2866 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|     1964 | 2867 | `}` |
|        - | 2868 | `/*` |
|        - | 2869 | ` * Compile a class declaration, named or anonymous.` |
|        - | 2870 | ` *` |
|        - | 2871 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|        - | 2872 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|        - | 2873 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|        - | 2874 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|        - | 2875 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|        - | 2876 | ` * implements, body, install) is shared by both paths.` |
|        - | 2877 | ` */` |
|   452766 | 2878 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|        - | 2879 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|        5 | 2880 | `{` |
|   452771 | 2881 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2882 | `	ph7_class *pClass,*pBase;` |
|        - | 2883 | `	SyToken *pEnd,*pTmp;` |
|        - | 2884 | `	sxi32 iProtection;` |
|        - | 2885 | `	SySet aInterfaces;` |
|        - | 2886 | `	SySet aUseEntries;` |
|        - | 2887 | `	sxi32 iAttrflags;` |
|        - | 2888 | `	SyString *pName;` |
|        - | 2889 | `	sxi32 nKwrd;` |
|        - | 2890 | `	sxi32 rc;` |
|        - | 2891 | `	/* Jump the 'class' keyword */` |
|   452771 | 2892 | `	pGen->pIn++;` |
|   452771 | 2893 | `	if( pAnonName ){` |
|        - | 2894 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|        - | 2895 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|        - | 2896 | `		 * then use the synthesized name. */` |
|       34 | 2897 | `		*ppArgStart = *ppArgEnd = 0;` |
|       34 | 2898 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        7 | 2899 | `			pGen->pIn++; /* Jump '(' */` |
|        7 | 2900 | `			*ppArgStart = pGen->pIn;` |
|       10 | 2901 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|        3 | 2902 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|        7 | 2903 | `			pGen->pIn = *ppArgEnd;` |
|        7 | 2904 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|        3 | 2905 | `		}` |
|       34 | 2906 | `		pName = pAnonName;` |
|       34 | 2907 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|       19 | 2908 | `	}else{` |
|   452741 | 2909 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - | 2910 | `			/* Syntax error */` |
|      ! 0 | 2911 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|      ! 0 | 2912 | `			if( rc == SXERR_ABORT ){` |
|        - | 2913 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 2914 | `				return SXERR_ABORT;` |
|        - | 2915 | `			}` |
|        - | 2916 | `			/* Synchronize with the first semi-colon or curly braces */` |
|      ! 0 | 2917 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|      ! 0 | 2918 | `				pGen->pIn++;` |
|      ! 0 | 2919 | `			}` |
|      ! 0 | 2920 | `			return SXRET_OK;` |
|        - | 2921 | `		}` |
|        - | 2922 | `		/* Extract class name */` |
|   452741 | 2923 | `		pName = &pGen->pIn->sData;` |
|        - | 2924 | `		/* Advance the stream cursor */` |
|   452741 | 2925 | `		pGen->pIn++;` |
|        - | 2926 | `		/* Build FQN and obtain a raw class */ {` |
|        - | 2927 | `			SyBlob sFQN;` |
|        - | 2928 | `			SyString sFQNStr;` |
|   452741 | 2929 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   452741 | 2930 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|   452741 | 2931 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   452741 | 2932 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   452741 | 2933 | `			SyBlobRelease(&sFQN);` |
|        - | 2934 | `		}` |
|        - | 2935 | `	}` |
|   452771 | 2936 | `	if( pClass == 0 ){` |
|      ! 0 | 2937 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2938 | `		return SXERR_ABORT;` |
|        - | 2939 | `	}` |
|   452766 | 2940 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|     3927 | 2941 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|        - | 2942 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|     3907 | 2943 | `		pGen->pIn++; /* Jump ':' */` |
|     3902 | 2944 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     3907 | 2945 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|        7 | 2946 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|        7 | 2947 | `			pGen->pIn++;` |
|     3900 | 2948 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     3901 | 2949 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|     3899 | 2950 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|     3899 | 2951 | `			pGen->pIn++;` |
|     1952 | 2952 | `		}else{` |
|        3 | 2953 | `			SyToken *pTok = pGen->pIn;` |
|        3 | 2954 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|        4 | 2955 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|        1 | 2956 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|        3 | 2957 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2958 | `				return SXERR_ABORT;` |
|        - | 2959 | `			}` |
|        3 | 2960 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|        3 | 2961 | `				pGen->pIn++; /* Skip the bogus type token */` |
|        1 | 2962 | `			}` |
|        - | 2963 | `		}` |
|     1951 | 2964 | `	}` |
|   452771 | 2965 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|   452771 | 2966 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 2967 | `		return SXERR_ABORT;` |
|        - | 2968 | `	}` |
|        - | 2969 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   452771 | 2970 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   452771 | 2971 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|        - | 2972 | `	/* Assume a standalone class */` |
|   452771 | 2973 | `	pBase = 0;` |
|   452771 | 2974 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   369665 | 2975 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   369665 | 2976 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|        - | 2977 | `			SyBlob sResolved;` |
|        - | 2978 | `			SyString sBaseName;` |
|        - | 2979 | `			sxu32 nRefLine;` |
|   249019 | 2980 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|        - | 2981 | `				/* php parse-fatals here (enums have no inheritance) */` |
|      ! 0 | 2982 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2983 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|      ! 0 | 2984 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2985 | `					return SXERR_ABORT;` |
|        - | 2986 | `				}` |
|      ! 0 | 2987 | `			}` |
|   249019 | 2988 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   249019 | 2989 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   249019 | 2990 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   249019 | 2991 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        3 | 2992 | `				SyBlobRelease(&sResolved);` |
|        4 | 2993 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2994 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|        1 | 2995 | `					pName);` |
|        3 | 2996 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|        3 | 2997 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2998 | `					return SXERR_ABORT;` |
|        - | 2999 | `				}` |
|        3 | 3000 | `				return SXRET_OK;` |
|        - | 3001 | `			}` |
|   373523 | 3002 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   249012 | 3003 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   249017 | 3004 | `			SyStringInitFromBuf(&sBaseName,` |
|        - | 3005 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 3006 | `			/* Interfaces are not allowed */` |
|   249017 | 3007 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|      ! 0 | 3008 | `				pBase = pBase->pNextName;` |
|      ! 0 | 3009 | `			}` |
|   249017 | 3010 | `			if( pBase == 0 ){` |
|      ! 0 | 3011 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 3012 | `					"Nonexistent base class '%z'",&sBaseName);` |
|      ! 0 | 3013 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3014 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 3015 | `					return SXERR_ABORT;` |
|        - | 3016 | `				}` |
|      ! 0 | 3017 | `			}else{` |
|   249017 | 3018 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|        4 | 3019 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 3020 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|        3 | 3021 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3022 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 3023 | `						return SXERR_ABORT;` |
|        - | 3024 | `					}` |
|        3 | 3025 | `					pBase = 0; /* Never inherit from an enum */` |
|   249016 | 3026 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|      ! 0 | 3027 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 3028 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|      ! 0 | 3029 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3030 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 3031 | `						return SXERR_ABORT;` |
|        - | 3032 | `					}` |
|      ! 0 | 3033 | `				}` |
|        - | 3034 | `			}` |
|   249017 | 3035 | `			SyBlobRelease(&sResolved);` |
|   249017 | 3036 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 | 3037 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|      ! 0 | 3038 | `			}` |
|   124506 | 3039 | `		}` |
|   369663 | 3040 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|        - | 3041 | `			ph7_class *pInterface;` |
|        - | 3042 | `			/* Interface implementation */` |
|   136211 | 3043 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   130293 | 3044 | `			for(;;){` |
|        - | 3045 | `				SyBlob sResolved;` |
|        - | 3046 | `				SyString sIntName;` |
|        - | 3047 | `				sxu32 nRefLine;` |
|   198401 | 3048 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   198401 | 3049 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   198401 | 3050 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 3051 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 3052 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 3053 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|      ! 0 | 3054 | `						pName);` |
|      ! 0 | 3055 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3056 | `						return SXERR_ABORT;` |
|        - | 3057 | `					}` |
|      ! 0 | 3058 | `					break;` |
|        - | 3059 | `				}` |
|   396797 | 3060 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|   198396 | 3061 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   198401 | 3062 | `				SyStringInitFromBuf(&sIntName,` |
|        - | 3063 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 3064 | `				/* Only interfaces are allowed */` |
|   198401 | 3065 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 3066 | `					pInterface = pInterface->pNextName;` |
|      ! 0 | 3067 | `				}` |
|   198401 | 3068 | `				if( pInterface == 0 ){` |
|      ! 0 | 3069 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 3070 | `						"Nonexistent base interface '%z'",&sIntName);` |
|      ! 0 | 3071 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3072 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 3073 | `						return SXERR_ABORT;` |
|        - | 3074 | `					}` |
|      ! 0 | 3075 | `				}else{` |
|        - | 3076 | `					/* Reject user classes that try to implement Throwable` |
|        - | 3077 | `					 * directly (or via an interface that extends Throwable)` |
|        - | 3078 | `					 * unless they already extend Exception or Error.` |
|        - | 3079 | `					 * Exception and Error themselves are compiled from the` |
|        - | 3080 | `					 * built-in library and are exempt by FQN — a namespaced` |
|        - | 3081 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|   198401 | 3082 | `					SyString *pFqn = &pClass->sName;` |
|   198401 | 3083 | `					int bIsExceptionOrError =` |
|   103088 | 3084 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   299541 | 3085 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|   196460 | 3086 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|     3900 | 3087 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   202287 | 3088 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    11676 | 3089 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|     3889 | 3090 | `						!bIsExceptionOrError ){` |
|       12 | 3091 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3092 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|        3 | 3093 | `							&pClass->sName);` |
|        9 | 3094 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3095 | `							SyBlobRelease(&sResolved);` |
|      ! 0 | 3096 | `							return SXERR_ABORT;` |
|        - | 3097 | `						}` |
|        - | 3098 | `						/* Skip registration so the follow-up abstract-method` |
|        - | 3099 | `						 * check does not produce a duplicate fatal. */` |
|        6 | 3100 | `					}else{` |
|   198395 | 3101 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|        - | 3102 | `					}` |
|        - | 3103 | `				}` |
|   198401 | 3104 | `				SyBlobRelease(&sResolved);` |
|   198401 | 3105 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    68108 | 3106 | `					break;` |
|        - | 3107 | `				}` |
|    62195 | 3108 | `				pGen->pIn++;/* Jump the comma */` |
|        5 | 3109 | `			}` |
|    68103 | 3110 | `		}` |
|   184829 | 3111 | `	}` |
|   452769 | 3112 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - | 3113 | `		/* Syntax error */` |
|      ! 0 | 3114 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|      ! 0 | 3115 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 3116 | `		if( rc == SXERR_ABORT ){` |
|        - | 3117 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3118 | `			return SXERR_ABORT;` |
|        - | 3119 | `		}` |
|      ! 0 | 3120 | `		return SXRET_OK;` |
|        - | 3121 | `	}` |
|   452769 | 3122 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   452769 | 3123 | `	pEnd = 0; /* cc warning */` |
|        - | 3124 | `	/* Delimit the class body */` |
|   452769 | 3125 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   452769 | 3126 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 3127 | `		/* Syntax error */` |
|      ! 0 | 3128 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|      ! 0 | 3129 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 3130 | `		if( rc == SXERR_ABORT ){` |
|        - | 3131 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3132 | `			return SXERR_ABORT;` |
|        - | 3133 | `		}` |
|      ! 0 | 3134 | `		return SXRET_OK;` |
|        - | 3135 | `	}` |
|        - | 3136 | `	/* The delimiter token is the class body's closing brace */` |
|   452769 | 3137 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 3138 | `	/* Swap token stream */` |
|   452769 | 3139 | `	pTmp = pGen->pEnd;` |
|   452769 | 3140 | `	pGen->pEnd = pEnd;` |
|        - | 3141 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|   452769 | 3142 | `	pClass->iFlags \|= iFlags;` |
|        - | 3143 | `	/* Start the parse process */` |
|  1696206 | 3144 | `	for(;;){` |
|        - | 3145 | `		/* Jump leading/trailing semi-colons */` |
|  4871621 | 3146 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   887323 | 3147 | `			pGen->pIn++;` |
|        5 | 3148 | `		}` |
|  3984303 | 3149 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3150 | `			/* End of class body */` |
|   452727 | 3151 | `			break;` |
|        - | 3152 | `		}` |
|        - | 3153 | `		/* Bind a directly-preceding docblock to this member */` |
|  3531581 | 3154 | `		GenStateSetPendingDoc(&(*pGen));` |
|  3531576 | 3155 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  1765793 | 3156 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|      ! 0 | 3157 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3158 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 | 3159 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 3160 | `			if( rc == SXERR_ABORT ){` |
|        - | 3161 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 3162 | `				return SXERR_ABORT;` |
|        - | 3163 | `			}` |
|      ! 0 | 3164 | `			goto done;` |
|        - | 3165 | `		}` |
|        - | 3166 | `		/* Assume public visibility */` |
|  3531581 | 3167 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  3531581 | 3168 | `		iAttrflags = 0;` |
|        - | 3169 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|        - | 3170 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|        - | 3171 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|        - | 3172 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  3531581 | 3173 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 3174 | `			int bMod = 0;` |
|      ! 0 | 3175 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 | 3176 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        - | 3177 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|        - | 3178 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|        - | 3179 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|        - | 3180 | `			 * that the generic keyword dispatch would misread as a method. */` |
|      ! 0 | 3181 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      ! 0 | 3182 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 | 3183 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|      ! 0 | 3184 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|      ! 0 | 3185 | `			}` |
|      ! 0 | 3186 | `			if( !bMod ){` |
|      ! 0 | 3187 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 3188 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 3189 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3190 | `						return SXERR_ABORT;` |
|        - | 3191 | `					}` |
|      ! 0 | 3192 | `					goto done;` |
|        - | 3193 | `				}` |
|      ! 0 | 3194 | `				continue;` |
|        - | 3195 | `			}` |
|      ! 0 | 3196 | `		}` |
|  3531581 | 3197 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 3198 | `			/* Extract the current keyword */` |
|  3531581 | 3199 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  3531581 | 3200 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|        - | 3201 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|     7831 | 3202 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|     7831 | 3203 | `				if( rc != SXRET_OK ){` |
|        6 | 3204 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3205 | `						return SXERR_ABORT;` |
|        - | 3206 | `					}` |
|        6 | 3207 | `					goto done;` |
|        - | 3208 | `				}` |
|     7827 | 3209 | `				continue;` |
|        - | 3210 | `			}` |
|  3523755 | 3211 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 3212 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|        - | 3213 | `				TraitUseEntry sUse;` |
|    15639 | 3214 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    15639 | 3215 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|    15639 | 3216 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|     7825 | 3217 | `				for(;;){` |
|        - | 3218 | `					ph7_class *pTrait;` |
|        - | 3219 | `					SyBlob sResolved;` |
|        - | 3220 | `					SyString sTraitName;` |
|    15647 | 3221 | `					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|        - | 3222 | `					/* A trait name is a full class reference: it may be qualified or` |
|        - | 3223 | ``					 * fully-qualified (`use Foo\Bar\T;`, `use \Foo\Bar\T;`) — the generated`` |
|        - | 3224 | `					 * PHPUnit mocks name their traits absolutely. Parse it with the shared` |
|        - | 3225 | `					 * class-reference reader (handles the leading '\', every '\'-segment and` |
|        - | 3226 | `					 * namespace/import resolution) instead of a single-identifier read, which` |
|        - | 3227 | `					 * choked on the first '\'. */` |
|    15647 | 3228 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    15647 | 3229 | `					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 3230 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 3231 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|      ! 0 | 3232 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|      ! 0 | 3233 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3234 | `							return SXERR_ABORT;` |
|        - | 3235 | `						}` |
|      ! 0 | 3236 | `						break;` |
|        - | 3237 | `					}` |
|    31289 | 3238 | `					pTrait = PH7_VmExtractClass(pGen->pVm,` |
|    15642 | 3239 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    15647 | 3240 | `					SyStringInitFromBuf(&sTraitName,` |
|        - | 3241 | `						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 3242 | `					/* Only traits are allowed */` |
|    15647 | 3243 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 3244 | `						pTrait = pTrait->pNextName;` |
|      ! 0 | 3245 | `					}` |
|    15647 | 3246 | `					if( pTrait == 0 ){` |
|      ! 0 | 3247 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|        - | 3248 | `							"'%z' is not a trait",&sTraitName);` |
|      ! 0 | 3249 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3250 | `							SyBlobRelease(&sResolved);` |
|      ! 0 | 3251 | `							return SXERR_ABORT;` |
|        - | 3252 | `						}` |
|      ! 0 | 3253 | `					}else{` |
|    15647 | 3254 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|        - | 3255 | `					}` |
|    15647 | 3256 | `					SyBlobRelease(&sResolved);` |
|        - | 3257 | `					/* GenStateParseClassReference already advanced past the whole name —` |
|        - | 3258 | `					 * continue only across a comma-separated trait list. */` |
|    15647 | 3259 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     7822 | 3260 | `						break;` |
|        - | 3261 | `					}` |
|       10 | 3262 | `					pGen->pIn++; /* Jump the comma */` |
|        2 | 3263 | `				}` |
|        - | 3264 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|    15639 | 3265 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - | 3266 | `					SyToken *pBlock;` |
|       13 | 3267 | `					pGen->pIn++; /* Jump '{' */` |
|       13 | 3268 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       13 | 3269 | `					sUse.pResolvStart = pGen->pIn;` |
|       13 | 3270 | `					sUse.pResolvEnd = pBlock;` |
|       13 | 3271 | `					if( pBlock < pGen->pEnd ){` |
|       13 | 3272 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|        8 | 3273 | `					}else{` |
|      ! 0 | 3274 | `						pGen->pIn = pGen->pEnd;` |
|        - | 3275 | `					}` |
|        5 | 3276 | `				}` |
|    15639 | 3277 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|        - | 3278 | `				/* The semicolon will be consumed by the outer loop */` |
|    15639 | 3279 | `				continue;` |
|        - | 3280 | `			}` |
|  3508121 | 3281 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        - | 3282 | `				int nSetTok;` |
|  2963551 | 3283 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|  2963551 | 3284 | `				if( nSetVis ){` |
|        - | 3285 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|        - | 3286 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|        3 | 3287 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        3 | 3288 | `					pGen->pIn += nSetTok;` |
|        2 | 3289 | `				}else{` |
|  2963549 | 3290 | `					iProtection = nKwrd;` |
|  2963549 | 3291 | `					pGen->pIn++; /* Jump the visibility token */` |
|        - | 3292 | `					/* Optional asymmetric set-visibility after the read` |
|        - | 3293 | ``					 * visibility: `public private(set) int $x`. */`` |
|  2963549 | 3294 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|  2963549 | 3295 | `					if( nSetVis ){` |
|        9 | 3296 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        9 | 3297 | `						pGen->pIn += nSetTok;` |
|        4 | 3298 | `					}` |
|        - | 3299 | `				}` |
|        - | 3300 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|        - | 3301 | ``				 * `public private(set) readonly int $x`. */`` |
|  2963551 | 3302 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       28 | 3303 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       28 | 3304 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       12 | 3305 | `				}` |
|  2963546 | 3306 | `				if( pGen->pIn >= pGen->pEnd` |
|  2963551 | 3307 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 3308 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3309 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 | 3310 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 3311 | `					if( rc == SXERR_ABORT ){` |
|        - | 3312 | `						/* Error count limit reached,abort immediately */` |
|      ! 0 | 3313 | `						return SXERR_ABORT;` |
|        - | 3314 | `					}` |
|      ! 0 | 3315 | `					goto done;` |
|        - | 3316 | `				}` |
|  2963551 | 3317 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 3318 | `					/* Attribute declaration (untyped) */` |
|   525303 | 3319 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   525303 | 3320 | `					if( rc != SXRET_OK ){` |
|       11 | 3321 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3322 | `							return SXERR_ABORT;` |
|        - | 3323 | `						}` |
|       11 | 3324 | `						goto done;` |
|        - | 3325 | `					}` |
|   533230 | 3326 | `					continue;` |
|        - | 3327 | `				}` |
|  2438253 | 3328 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 3329 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|    15881 | 3330 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    15881 | 3331 | `					if( rc != SXRET_OK ){` |
|        9 | 3332 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3333 | `							return SXERR_ABORT;` |
|        - | 3334 | `						}` |
|        9 | 3335 | `						goto done;` |
|        - | 3336 | `					}` |
|    15875 | 3337 | `					continue;` |
|        - | 3338 | `				}` |
|        - | 3339 | `				/* Extract the keyword */` |
|  2422377 | 3340 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1211186 | 3341 | `			}` |
|  2966947 | 3342 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|        - | 3343 | `				/* Process constant declaration */` |
|   287751 | 3344 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|   287751 | 3345 | `				if( rc != SXRET_OK ){` |
|       11 | 3346 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3347 | `						return SXERR_ABORT;` |
|        - | 3348 | `					}` |
|       11 | 3349 | `					goto done;` |
|        - | 3350 | `				}` |
|   143874 | 3351 | `			}else{` |
|  2679201 | 3352 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - | 3353 | `					/* Static method or attribute,record that */` |
|   101235 | 3354 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|   101235 | 3355 | `					pGen->pIn++; /* Jump the static keyword */` |
|   101235 | 3356 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 3357 | `						int nSetTok;` |
|    73991 | 3358 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|    73991 | 3359 | `						if( nSetVis ){` |
|        - | 3360 | ``							/* `static private(set) int $x` — read side stays public */`` |
|        3 | 3361 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        3 | 3362 | `							pGen->pIn += nSetTok;` |
|        2 | 3363 | `						}else{` |
|        - | 3364 | `							/* Extract the keyword */` |
|    73989 | 3365 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    73989 | 3366 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 3367 | `								iProtection = nKwrd;` |
|      ! 0 | 3368 | `								pGen->pIn++; /* Jump the visibility token */` |
|      ! 0 | 3369 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|      ! 0 | 3370 | `								if( nSetVis ){` |
|      ! 0 | 3371 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|      ! 0 | 3372 | `									pGen->pIn += nSetTok;` |
|      ! 0 | 3373 | `								}` |
|      ! 0 | 3374 | `							}` |
|        - | 3375 | `						}` |
|    36993 | 3376 | `					}` |
|        - | 3377 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|        - | 3378 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|        - | 3379 | `					 * than a generic "expecting method" parse error. */` |
|   101235 | 3380 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 3381 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 | 3382 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|      ! 0 | 3383 | `					}` |
|   101230 | 3384 | `					if( pGen->pIn >= pGen->pEnd` |
|   101235 | 3385 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 3386 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3387 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|      ! 0 | 3388 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 3389 | `						if( rc == SXERR_ABORT ){` |
|        - | 3390 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 3391 | `							return SXERR_ABORT;` |
|        - | 3392 | `						}` |
|      ! 0 | 3393 | `						goto done;` |
|        - | 3394 | `					}` |
|   101235 | 3395 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 3396 | `						/* Attribute declaration */` |
|    27245 | 3397 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    27245 | 3398 | `						if( rc != SXRET_OK ){` |
|        3 | 3399 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 3400 | `								return SXERR_ABORT;` |
|        - | 3401 | `							}` |
|        3 | 3402 | `							goto done;` |
|        - | 3403 | `						}` |
|    27243 | 3404 | `						continue;` |
|        - | 3405 | `					}` |
|    73995 | 3406 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 3407 | `						/* Typed static attribute declaration */` |
|       19 | 3408 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       19 | 3409 | `						if( rc != SXRET_OK ){` |
|        3 | 3410 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 3411 | `								return SXERR_ABORT;` |
|        - | 3412 | `							}` |
|        3 | 3413 | `							goto done;` |
|        - | 3414 | `						}` |
|       17 | 3415 | `						continue;` |
|        - | 3416 | `					}` |
|        - | 3417 | `					/* Extract the keyword */` |
|    73979 | 3418 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  2614958 | 3419 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        - | 3420 | `					/* Abstract method,record that */` |
|     7799 | 3421 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        - | 3422 | `					/* Mark the whole class as abstract */` |
|     7799 | 3423 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|        - | 3424 | `					/* Advance the stream cursor */` |
|     7799 | 3425 | `					pGen->pIn++;` |
|     7799 | 3426 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     7799 | 3427 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     7799 | 3428 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     7797 | 3429 | `							iProtection = nKwrd;` |
|     7797 | 3430 | `							pGen->pIn++; /* Jump the visibility token */` |
|     3896 | 3431 | `						}` |
|     3897 | 3432 | `					}` |
|     7799 | 3433 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     7794 | 3434 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 3435 | `							/* Static method */` |
|      ! 0 | 3436 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 3437 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 3438 | `					}` |
|     7799 | 3439 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|     7794 | 3440 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|        - | 3441 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|        - | 3442 | `							 * HOOKED property declaration. Route anything that is not a` |
|        - | 3443 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|        - | 3444 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|        - | 3445 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|        6 | 3446 | `							if( pGen->pIn < pGen->pEnd` |
|        7 | 3447 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|        3 | 3448 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|        7 | 3449 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        7 | 3450 | `								if( rc != SXRET_OK ){` |
|      ! 0 | 3451 | `									if( rc == SXERR_ABORT ){` |
|      ! 0 | 3452 | `										return SXERR_ABORT;` |
|        - | 3453 | `									}` |
|      ! 0 | 3454 | `									goto done;` |
|        - | 3455 | `								}` |
|        7 | 3456 | `								continue;` |
|        - | 3457 | `							}` |
|      ! 0 | 3458 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3459 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|      ! 0 | 3460 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 3461 | `							if( rc == SXERR_ABORT ){` |
|        - | 3462 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 3463 | `								return SXERR_ABORT;` |
|        - | 3464 | `							}` |
|      ! 0 | 3465 | `							goto done;` |
|        - | 3466 | `					}` |
|     7793 | 3467 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  2574071 | 3468 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|        - | 3469 | `					/* final method ,record that */` |
|       21 | 3470 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       21 | 3471 | `					pGen->pIn++; /* Jump the final keyword */` |
|       21 | 3472 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 3473 | `						/* Extract the keyword */` |
|       21 | 3474 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       21 | 3475 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       11 | 3476 | `							iProtection = nKwrd;` |
|       11 | 3477 | `							pGen->pIn++; /* Jump the visibility token */` |
|        4 | 3478 | `						}` |
|        9 | 3479 | `					}` |
|       21 | 3480 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       18 | 3481 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|        - | 3482 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|        - | 3483 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|        - | 3484 | `							 * child class is compiled (PH7_ClassInherit). */` |
|       14 | 3485 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|       14 | 3486 | `							if( rc != SXRET_OK ){` |
|      ! 0 | 3487 | `								if( rc == SXERR_ABORT ){` |
|      ! 0 | 3488 | `									return SXERR_ABORT;` |
|        - | 3489 | `								}` |
|      ! 0 | 3490 | `								goto done;` |
|        - | 3491 | `							}` |
|       14 | 3492 | `							continue;` |
|        - | 3493 | `					}` |
|        9 | 3494 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        6 | 3495 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 3496 | `							/* Static method */` |
|      ! 0 | 3497 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 3498 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 3499 | `					}` |
|        9 | 3500 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        6 | 3501 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 3502 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3503 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|      ! 0 | 3504 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 3505 | `							if( rc == SXERR_ABORT ){` |
|        - | 3506 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 3507 | `								return SXERR_ABORT;` |
|        - | 3508 | `							}` |
|      ! 0 | 3509 | `							goto done;` |
|        - | 3510 | `					}` |
|        9 | 3511 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        3 | 3512 | `				}` |
|  2651927 | 3513 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 3514 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3515 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|      ! 0 | 3516 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 3517 | `						if( rc == SXERR_ABORT ){` |
|        - | 3518 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 3519 | `							return SXERR_ABORT;` |
|        - | 3520 | `						}` |
|      ! 0 | 3521 | `						goto done;` |
|        - | 3522 | `				}` |
|  2651927 | 3523 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|        7 | 3524 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|        7 | 3525 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|      ! 0 | 3526 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3527 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 3528 | `						if( rc == SXERR_ABORT ){` |
|        - | 3529 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 3530 | `							return SXERR_ABORT;` |
|        - | 3531 | `						}` |
|      ! 0 | 3532 | `						goto done;` |
|        - | 3533 | `					}` |
|        - | 3534 | `					/* Attribute declaration */` |
|        7 | 3535 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        4 | 3536 | `				}else{` |
|        - | 3537 | `					/* Process method declaration */` |
|  2651921 | 3538 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 3539 | `				}` |
|  2651927 | 3540 | `				if( rc != SXRET_OK ){` |
|       16 | 3541 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3542 | `						return SXERR_ABORT;` |
|        - | 3543 | `					}` |
|       16 | 3544 | `					goto done;` |
|        - | 3545 | `				}` |
|        - | 3546 | `			}` |
|  1469829 | 3547 | `		}else{` |
|        - | 3548 | `			/* Attribute declaration */` |
|      ! 0 | 3549 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 3550 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 3551 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3552 | `					return SXERR_ABORT;` |
|        - | 3553 | `				}` |
|      ! 0 | 3554 | `				goto done;` |
|        - | 3555 | `			}` |
|        - | 3556 | `		}` |
|        5 | 3557 | `	}` |
|        - | 3558 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|        - | 3559 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|        - | 3560 | `	 */` |
|        - | 3561 | `	{` |
|        - | 3562 | `		TraitUseEntry *apUse;` |
|        - | 3563 | `		sxu32 nU;` |
|   452727 | 3564 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   468361 | 3565 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|    15639 | 3566 | `			TraitUseEntry *pUse = &apUse[nU];` |
|    15639 | 3567 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|    15639 | 3568 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|    15639 | 3569 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|        - | 3570 | `			sxu32 nT;` |
|    15639 | 3571 | `			if( !hasResolution ){` |
|        - | 3572 | `				/* No conflict resolution block: use standard trait application */` |
|    31259 | 3573 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|    15635 | 3574 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|    15635 | 3575 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 3576 | `						break;` |
|        - | 3577 | `					}` |
|     7820 | 3578 | `				}` |
|     7817 | 3579 | `			}else{` |
|        - | 3580 | `				/* With resolution block: copy attributes, record traits,` |
|        - | 3581 | `				 * then use the block to resolve method conflicts.` |
|        - | 3582 | `				 */` |
|        - | 3583 | `				SyToken *pR;` |
|       25 | 3584 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       15 | 3585 | `					ph7_class *pTR = apTrait[nT];` |
|        - | 3586 | `					ph7_class_attr *pAR;` |
|        - | 3587 | `					SyHashEntry *pER;` |
|        - | 3588 | `					SyString *pNR;` |
|       15 | 3589 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|       21 | 3590 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|      ! 0 | 3591 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|      ! 0 | 3592 | `						pNR = &pAR->sName;` |
|      ! 0 | 3593 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      ! 0 | 3594 | `							SyHashInsertTail(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|      ! 0 | 3595 | `						}` |
|      ! 0 | 3596 | `					}` |
|       15 | 3597 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|        9 | 3598 | `				}` |
|        - | 3599 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       13 | 3600 | `				pR = pUse->pResolvStart;` |
|       27 | 3601 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 3602 | `					SyString sTrait,sMethod;` |
|        - | 3603 | `					ph7_class *pSrcTrait;` |
|        - | 3604 | `					ph7_class_method *pMeth;` |
|        - | 3605 | `					sxi32 nRKwrd;` |
|       41 | 3606 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       27 | 3607 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       17 | 3608 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       17 | 3609 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       17 | 3610 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       17 | 3611 | `					sMethod = pR->sData;` |
|       17 | 3612 | `					pR++;` |
|       17 | 3613 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 3614 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 3615 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 3616 | `							sTrait = sMethod;` |
|        7 | 3617 | `							pR++;` |
|        7 | 3618 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 3619 | `							sMethod = pR->sData;` |
|        7 | 3620 | `							pR++;` |
|        3 | 3621 | `						}` |
|        3 | 3622 | `					}` |
|       17 | 3623 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 3624 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 3625 | `						continue;` |
|        - | 3626 | `					}` |
|       17 | 3627 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       17 | 3628 | `					pR++;` |
|       17 | 3629 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|        5 | 3630 | `						pSrcTrait = 0;` |
|        7 | 3631 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        7 | 3632 | `							SyString *pTN = &apTrait[nT]->sName;` |
|       10 | 3633 | `							if( pTN->nByte >= sTrait.nByte &&` |
|        6 | 3634 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        5 | 3635 | `								pSrcTrait = apTrait[nT];` |
|        5 | 3636 | `								break;` |
|        - | 3637 | `							}` |
|        2 | 3638 | `						}` |
|        5 | 3639 | `						if( pSrcTrait ){` |
|        5 | 3640 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        5 | 3641 | `							if( pMeth ){` |
|        5 | 3642 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|        5 | 3643 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|        5 | 3644 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|        2 | 3645 | `								}` |
|        2 | 3646 | `							}` |
|        2 | 3647 | `						}` |
|        2 | 3648 | `					}` |
|       35 | 3649 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        3 | 3650 | `				}` |
|        - | 3651 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|       25 | 3652 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        - | 3653 | `					ph7_class_method *pMR;` |
|        - | 3654 | `					SyHashEntry *pER;` |
|        - | 3655 | `					SyString *pNR;` |
|       15 | 3656 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|       41 | 3657 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|       23 | 3658 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|       23 | 3659 | `						pNR = &pMR->sFunc.sName;` |
|       23 | 3660 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       14 | 3661 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|        6 | 3662 | `						}` |
|        3 | 3663 | `					}` |
|        9 | 3664 | `				}` |
|        - | 3665 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       13 | 3666 | `				pR = pUse->pResolvStart;` |
|       27 | 3667 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 3668 | `					SyString sTrait,sMethod,sAlias;` |
|        - | 3669 | `					ph7_class *pSrcTrait;` |
|        - | 3670 | `					ph7_class_method *pMeth;` |
|       27 | 3671 | `					int hasQual = 0;` |
|        - | 3672 | `					sxi32 nRKwrd;` |
|       41 | 3673 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       27 | 3674 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       17 | 3675 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       17 | 3676 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       17 | 3677 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|       17 | 3678 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       17 | 3679 | `					sMethod = pR->sData;` |
|       17 | 3680 | `					pR++;` |
|       17 | 3681 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 3682 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 3683 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 3684 | `							sTrait = sMethod;` |
|        7 | 3685 | `							hasQual = 1;` |
|        7 | 3686 | `							pR++;` |
|        7 | 3687 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 3688 | `							sMethod = pR->sData;` |
|        7 | 3689 | `							pR++;` |
|        3 | 3690 | `						}` |
|        3 | 3691 | `					}` |
|       17 | 3692 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 3693 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 3694 | `						continue;` |
|        - | 3695 | `					}` |
|       17 | 3696 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       17 | 3697 | `					pR++;` |
|       17 | 3698 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       13 | 3699 | `						sxi32 iNewVis = -1;` |
|       13 | 3700 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|        7 | 3701 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|        7 | 3702 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|        7 | 3703 | `								iNewVis = nAK;` |
|        7 | 3704 | `								pR++;` |
|        3 | 3705 | `							}` |
|        3 | 3706 | `						}` |
|       13 | 3707 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       11 | 3708 | `							sAlias = pR->sData;` |
|       11 | 3709 | `							pR++;` |
|        4 | 3710 | `						}` |
|       13 | 3711 | `						pMeth = 0;` |
|       13 | 3712 | `						if( hasQual ){` |
|        3 | 3713 | `							pSrcTrait = 0;` |
|        5 | 3714 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        5 | 3715 | `								SyString *pTN = &apTrait[nT]->sName;` |
|        7 | 3716 | `								if( pTN->nByte >= sTrait.nByte &&` |
|        4 | 3717 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        3 | 3718 | `									pSrcTrait = apTrait[nT];` |
|        3 | 3719 | `									break;` |
|        - | 3720 | `								}` |
|        2 | 3721 | `							}` |
|        3 | 3722 | `							if( pSrcTrait ){` |
|        3 | 3723 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        1 | 3724 | `							}` |
|        2 | 3725 | `						}else{` |
|       10 | 3726 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|        - | 3727 | `						}` |
|       13 | 3728 | `						if( pMeth ){` |
|       13 | 3729 | `							if( sAlias.nByte > 0 ){` |
|        - | 3730 | `								/* Create a shallow copy of the method struct for the alias` |
|        - | 3731 | `								 * so it can carry its own visibility without affecting the original.` |
|        - | 3732 | `								 */` |
|        - | 3733 | `								ph7_class_method *pAlias;` |
|        - | 3734 | `								char *zAliasDup;` |
|       11 | 3735 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       11 | 3736 | `								if( pAlias ){` |
|       11 | 3737 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       11 | 3738 | `									if( iNewVis >= 0 ){` |
|        5 | 3739 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 3740 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 3741 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        2 | 3742 | `									}` |
|       11 | 3743 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       11 | 3744 | `									if( zAliasDup ){` |
|       11 | 3745 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|        4 | 3746 | `									}` |
|        7 | 3747 | `								}` |
|        7 | 3748 | `							}else if( iNewVis >= 0 ){` |
|        - | 3749 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|        - | 3750 | `								ph7_class_method *pCopy;` |
|        3 | 3751 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        3 | 3752 | `								if( pCopy ){` |
|        3 | 3753 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|        3 | 3754 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|        3 | 3755 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 3756 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 3757 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        - | 3758 | `									/* Replace the method in the class hash */` |
|        3 | 3759 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|        3 | 3760 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|        1 | 3761 | `								}` |
|        1 | 3762 | `							}` |
|        5 | 3763 | `						}` |
|        5 | 3764 | `						SXUNUSED(hasQual);` |
|        5 | 3765 | `					}` |
|       21 | 3766 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        3 | 3767 | `				}` |
|        - | 3768 | `			}` |
|    15639 | 3769 | `			SySetRelease(&pUse->aTraits);` |
|     7822 | 3770 | `		}` |
|        - | 3771 | `	}` |
|   452727 | 3772 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - | 3773 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|        - | 3774 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|     3923 | 3775 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|     3923 | 3776 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3777 | `			SySetRelease(&aUseEntries);` |
|      ! 0 | 3778 | `			SySetRelease(&aInterfaces);` |
|      ! 0 | 3779 | `			return SXERR_ABORT;` |
|        - | 3780 | `		}` |
|     1959 | 3781 | `	}` |
|        - | 3782 | `	/* Reject a php-fatal redeclaration before hoisting the class */` |
|   452727 | 3783 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|        9 | 3784 | `		return SXERR_ABORT;` |
|        - | 3785 | `	}` |
|        - | 3786 | `	/* Install the class */` |
|   452721 | 3787 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   452721 | 3788 | `	if( rc == SXRET_OK ){` |
|        - | 3789 | `		ph7_class **apInterface;` |
|        - | 3790 | `		sxu32 n;` |
|   452721 | 3791 | `		if( pBase ){` |
|        - | 3792 | `			/* Inherit from base class and mark as a subclass */` |
|   249015 | 3793 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   124505 | 3794 | `		}` |
|   452721 | 3795 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   651111 | 3796 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|        - | 3797 | `			/* Implements one or more interface */` |
|   198395 | 3798 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|   198395 | 3799 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 3800 | `				break;` |
|        - | 3801 | `			}` |
|    99200 | 3802 | `		}` |
|        - | 3803 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|        - | 3804 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|   452721 | 3805 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|     3921 | 3806 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|     3921 | 3807 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 3808 | `				pIntf = pIntf->pNextName;` |
|      ! 0 | 3809 | `			}` |
|     3921 | 3810 | `			if( pIntf ){` |
|     3921 | 3811 | `				PH7_ClassImplement(pClass,pIntf);` |
|     1958 | 3812 | `			}` |
|     3921 | 3813 | `			if( pClass->nEnumBacking != 0 ){` |
|     3905 | 3814 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|     3905 | 3815 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 3816 | `					pIntf = pIntf->pNextName;` |
|      ! 0 | 3817 | `				}` |
|     3905 | 3818 | `				if( pIntf ){` |
|     3905 | 3819 | `					PH7_ClassImplement(pClass,pIntf);` |
|     1950 | 3820 | `				}` |
|     1950 | 3821 | `			}` |
|     1958 | 3822 | `		}` |
|        - | 3823 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|        - | 3824 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|   452716 | 3825 | `		if( rc == SXRET_OK` |
|   452716 | 3826 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|   452721 | 3827 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   229387 | 3828 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|        - | 3829 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   229387 | 3830 | `			if( pStringable ){` |
|   229387 | 3831 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   229387 | 3832 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|        - | 3833 | `				sxu32 i;` |
|   229387 | 3834 | `				int bAlready = 0;` |
|   276023 | 3835 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    58301 | 3836 | `					if( apImpl[i] == pStringable ){` |
|    11665 | 3837 | `						bAlready = 1;` |
|    11665 | 3838 | `						break;` |
|        - | 3839 | `					}` |
|    23323 | 3840 | `				}` |
|   229387 | 3841 | `				if( !bAlready ){` |
|   217727 | 3842 | `					PH7_ClassImplement(pClass,pStringable);` |
|   108861 | 3843 | `				}` |
|   114691 | 3844 | `			}` |
|   114691 | 3845 | `		}` |
|        - | 3846 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   452721 | 3847 | `		if( rc == SXRET_OK ){` |
|   452721 | 3848 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   452721 | 3849 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 3850 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 3851 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 3852 | `				return SXERR_ABORT;` |
|        - | 3853 | `			}` |
|   226358 | 3854 | `		}` |
|        - | 3855 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   452721 | 3856 | `		if( rc == SXRET_OK ){` |
|   452721 | 3857 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   452721 | 3858 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 3859 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 3860 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 3861 | `				return SXERR_ABORT;` |
|        - | 3862 | `			}` |
|   226358 | 3863 | `		}` |
|   226358 | 3864 | `	}` |
|   452721 | 3865 | `	SySetRelease(&aUseEntries);` |
|   452721 | 3866 | `	SySetRelease(&aInterfaces);` |
|   452721 | 3867 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3868 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3869 | `		return SXERR_ABORT;` |
|        - | 3870 | `	}` |
|   226358 | 3871 | `done:` |
|        - | 3872 | `	/* Point beyond the class body */` |
|   452763 | 3873 | `	pGen->pIn = &pEnd[1];` |
|   452763 | 3874 | `	pGen->pEnd = pTmp;` |
|   452763 | 3875 | `	return PH7_OK;` |
|   226388 | 3876 | `}` |
|        - | 3877 | `/* Compile a named class declaration (the common case). */` |
|   452736 | 3878 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|        5 | 3879 | `{` |
|   452741 | 3880 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|        5 | 3881 | `}` |
|        - | 3882 | `/*` |
|        - | 3883 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|        - | 3884 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|        - | 3885 | ` * compile + install the class body once (at compile time, like every other` |
|        - | 3886 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|        - | 3887 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|        - | 3888 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|        - | 3889 | ` */` |
|       30 | 3890 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 | 3891 | `{` |
|        - | 3892 | `	char zName[128];         /* Synthesized class name */` |
|        - | 3893 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|        - | 3894 | `	SyString sName;` |
|        - | 3895 | `	SyToken *pArgStart,*pArgEnd;` |
|       34 | 3896 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|        - | 3897 | `	                              * is keyed to this 'class' token */` |
|        - | 3898 | `	ph7_value *pObj;` |
|       34 | 3899 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3900 | `	sxu32 nIdx,nLen;` |
|        - | 3901 | `	sxi32 nArg,rc;` |
|       15 | 3902 | `	SXUNUSED(iCompileFlag);` |
|        - | 3903 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|       34 | 3904 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       34 | 3905 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 | 3906 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      ! 0 | 3907 | `	}` |
|       34 | 3908 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - | 3909 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|        - | 3910 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|        - | 3911 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|       34 | 3912 | `	pArgStart = pArgEnd = 0;` |
|       34 | 3913 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|       34 | 3914 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3915 | `		return rc;` |
|        - | 3916 | `	}` |
|        - | 3917 | `	{` |
|        - | 3918 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|       34 | 3919 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|       30 | 3920 | `		if( pAnonClass` |
|       34 | 3921 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 3922 | `			return SXERR_ABORT;` |
|        - | 3923 | `		}` |
|        - | 3924 | `	}` |
|        - | 3925 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|        - | 3926 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|       34 | 3927 | `	nArg = 0;` |
|       34 | 3928 | `	if( pArgStart < pArgEnd ){` |
|        7 | 3929 | `		SyToken *pSavedIn = pGen->pIn;` |
|        7 | 3930 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 3931 | `		SyToken *pArgNext;` |
|        7 | 3932 | `		pGen->pIn = pArgStart;` |
|        7 | 3933 | `		pGen->pEnd = pArgEnd;` |
|       13 | 3934 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|        7 | 3935 | `			if( pGen->pIn < pArgNext ){` |
|        7 | 3936 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|        7 | 3937 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3938 | `					pGen->pIn = pSavedIn;` |
|      ! 0 | 3939 | `					pGen->pEnd = pSavedEnd;` |
|      ! 0 | 3940 | `					return SXERR_ABORT;` |
|        - | 3941 | `				}` |
|        7 | 3942 | `				nArg++;` |
|        3 | 3943 | `			}` |
|        7 | 3944 | `			pGen->pIn = &pArgNext[1];` |
|        1 | 3945 | `		}` |
|        7 | 3946 | `		pGen->pIn = pSavedIn;` |
|        7 | 3947 | `		pGen->pEnd = pSavedEnd;` |
|        3 | 3948 | `	}` |
|        - | 3949 | `	/* Load the synthesized class name */` |
|       34 | 3950 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       34 | 3951 | `	if( pObj == 0 ){` |
|      ! 0 | 3952 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 3953 | `		return SXERR_ABORT;` |
|        - | 3954 | `	}` |
|       34 | 3955 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       34 | 3956 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - | 3957 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|       34 | 3958 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       34 | 3959 | `	return SXRET_OK;` |
|       19 | 3960 | `}` |
|        - | 3961 | `/*` |
|        - | 3962 | ` * Compile a user-defined abstract class.` |
|        - | 3963 | ` *  According to the PHP language reference manual` |
|        - | 3964 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|        - | 3965 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|        - | 3966 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|        - | 3967 | ` *   the method's signature - they cannot define the implementation.` |
|        - | 3968 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|        - | 3969 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|        - | 3970 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|        - | 3971 | ` *   method is defined as protected, the function implementation must be defined as either` |
|        - | 3972 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|        - | 3973 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|        - | 3974 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|        - | 3975 | ` *   could differ.` |
|        - | 3976 | ` */` |
|        - | 3977 | `/*` |
|        - | 3978 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|        - | 3979 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|        - | 3980 | ` * receives the corresponding PH7_CLASS_* bit.` |
|        - | 3981 | ` */` |
| 14468916 | 3982 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|        5 | 3983 | `{` |
| 14468921 | 3984 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  8659979 | 3985 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  8659979 | 3986 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  8605529 | 3987 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  4283279 | 3988 | `	}` |
| 14375505 | 3989 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
| 14375445 | 3990 | `	return FALSE;` |
|  7234463 | 3991 | `}` |
|        - | 3992 | `/*` |
|        - | 3993 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|        - | 3994 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|        - | 3995 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|        - | 3996 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|        - | 3997 | ` */` |
| 14375440 | 3998 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|        5 | 3999 | `{` |
| 14375445 | 4000 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
| 14375445 | 4001 | `	sxi32 iFlags = 0,iFlag;` |
| 14468921 | 4002 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    93481 | 4003 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|        5 | 4004 | `			pDup = pIn;` |
|        2 | 4005 | `		}` |
|    93481 | 4006 | `		iFlags \|= iFlag;` |
|    93481 | 4007 | `		pIn++;` |
|        5 | 4008 | `	}` |
| 14375445 | 4009 | `	*ppIn = pIn;` |
| 14375445 | 4010 | `	if( ppDup ){ *ppDup = pDup; }` |
| 14375445 | 4011 | `	return iFlags;` |
|        5 | 4012 | `}` |
|        - | 4013 | `/*` |
|        - | 4014 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|        - | 4015 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|        - | 4016 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|        - | 4017 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|        - | 4018 | `` * `readonly`) to their existing handlers.`` |
|        - | 4019 | ` */` |
| 14332598 | 4020 | `PH7_PRIVATE int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|        5 | 4021 | `{` |
| 14332603 | 4022 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  7216920 | 4023 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
| 14357907 | 4024 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|        5 | 4025 | `}` |
|        - | 4026 | `/*` |
|        - | 4027 | ` * Compile a class declaration carrying one or more leading modifiers` |
|        - | 4028 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|        - | 4029 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|        - | 4030 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|        - | 4031 | `` * `abstract`+`final` pair, like PHP.`` |
|        - | 4032 | ` */` |
|    42842 | 4033 | `PH7_PRIVATE sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|        5 | 4034 | `{` |
|        - | 4035 | `	SyToken *pDup;` |
|    42847 | 4036 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|        - | 4037 | `	sxi32 rc;` |
|    42847 | 4038 | `	if( pDup ){` |
|        4 | 4039 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|        2 | 4040 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|        3 | 4041 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4042 | `			return SXERR_ABORT;` |
|        - | 4043 | `		}` |
|        1 | 4044 | `	}` |
|    42842 | 4045 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    21426 | 4046 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|        3 | 4047 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4048 | `			"Cannot use the final modifier on an abstract class");` |
|        3 | 4049 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4050 | `			return SXERR_ABORT;` |
|        - | 4051 | `		}` |
|        1 | 4052 | `	}` |
|    42847 | 4053 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    21426 | 4054 | `}` |
|        - | 4055 | `/*` |
|        - | 4056 | ` * Compile a user-defined trait.` |
|        - | 4057 | ` *  Traits are similar to classes, but only intended to group functionality` |
|        - | 4058 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|        - | 4059 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|        - | 4060 | ` */` |
|     7872 | 4061 | `PH7_PRIVATE sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|        5 | 4062 | `{` |
|     7877 | 4063 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 4064 | `	ph7_class *pClass;` |
|        - | 4065 | `	SyToken *pEnd,*pTmp;` |
|        - | 4066 | `	sxi32 iProtection;` |
|        - | 4067 | `	sxi32 iAttrflags;` |
|        - | 4068 | `	SyString *pName;` |
|        - | 4069 | `	sxi32 nKwrd;` |
|        - | 4070 | `	sxi32 rc;` |
|        - | 4071 | `	/* Jump the 'trait' keyword */` |
|     7877 | 4072 | `	pGen->pIn++;` |
|     7877 | 4073 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 4074 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|      ! 0 | 4075 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4076 | `			return SXERR_ABORT;` |
|        - | 4077 | `		}` |
|      ! 0 | 4078 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|      ! 0 | 4079 | `			pGen->pIn++;` |
|      ! 0 | 4080 | `		}` |
|      ! 0 | 4081 | `		return SXRET_OK;` |
|        - | 4082 | `	}` |
|        - | 4083 | `	/* Extract trait name */` |
|     7877 | 4084 | `	pName = &pGen->pIn->sData;` |
|     7877 | 4085 | `	pGen->pIn++;` |
|        - | 4086 | `	/* Build FQN and obtain a raw class */ {` |
|        - | 4087 | `		SyBlob sFQN;` |
|        - | 4088 | `		SyString sFQNStr;` |
|     7877 | 4089 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     7877 | 4090 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     7877 | 4091 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|     7877 | 4092 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     7877 | 4093 | `		SyBlobRelease(&sFQN);` |
|        - | 4094 | `	}` |
|     7877 | 4095 | `	if( pClass == 0 ){` |
|      ! 0 | 4096 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4097 | `		return SXERR_ABORT;` |
|        - | 4098 | `	}` |
|     7877 | 4099 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     7877 | 4100 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 4101 | `		return SXERR_ABORT;` |
|        - | 4102 | `	}` |
|        - | 4103 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|     7877 | 4104 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 | 4105 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|      ! 0 | 4106 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 4107 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4108 | `			return SXERR_ABORT;` |
|        - | 4109 | `		}` |
|      ! 0 | 4110 | `		return SXRET_OK;` |
|        - | 4111 | `	}` |
|     7877 | 4112 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     7877 | 4113 | `	pEnd = 0;` |
|     7877 | 4114 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|     7877 | 4115 | `	if( pEnd >= pGen->pEnd ){` |
|      ! 0 | 4116 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|      ! 0 | 4117 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 4118 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4119 | `			return SXERR_ABORT;` |
|        - | 4120 | `		}` |
|      ! 0 | 4121 | `		return SXRET_OK;` |
|        - | 4122 | `	}` |
|        - | 4123 | `	/* The delimiter token is the trait body's closing brace */` |
|     7877 | 4124 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 4125 | `	/* Swap token stream */` |
|     7877 | 4126 | `	pTmp = pGen->pEnd;` |
|     7877 | 4127 | `	pGen->pEnd = pEnd;` |
|        - | 4128 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|     7877 | 4129 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|        - | 4130 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|    56448 | 4131 | `	for(;;){` |
|   159595 | 4132 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|    23355 | 4133 | `			pGen->pIn++;` |
|        5 | 4134 | `		}` |
|   136245 | 4135 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     7877 | 4136 | `			break;` |
|        - | 4137 | `		}` |
|        - | 4138 | `		/* Bind a directly-preceding docblock to this member */` |
|   128373 | 4139 | `		GenStateSetPendingDoc(&(*pGen));` |
|   128373 | 4140 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|      ! 0 | 4141 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4142 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 4143 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 4144 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4145 | `				return SXERR_ABORT;` |
|        - | 4146 | `			}` |
|      ! 0 | 4147 | `			goto done;` |
|        - | 4148 | `		}` |
|   128373 | 4149 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   128373 | 4150 | `		iAttrflags = 0;` |
|   128373 | 4151 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   128373 | 4152 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   128373 | 4153 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 4154 | `				/* Trait uses another trait: use OtherTrait; */` |
|        5 | 4155 | `				pGen->pIn++; /* Jump 'use' */` |
|        2 | 4156 | `				for(;;){` |
|        - | 4157 | `					ph7_class *pUsedTrait;` |
|        - | 4158 | `					SyString *pUsedName;` |
|        5 | 4159 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 4160 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 4161 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|      ! 0 | 4162 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4163 | `							return SXERR_ABORT;` |
|        - | 4164 | `						}` |
|      ! 0 | 4165 | `						break;` |
|        - | 4166 | `					}` |
|        5 | 4167 | `					pUsedName = &pGen->pIn->sData;` |
|        - | 4168 | `					{` |
|        - | 4169 | `						SyBlob sResolved;` |
|        5 | 4170 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        5 | 4171 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|        7 | 4172 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|        4 | 4173 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|        5 | 4174 | `						SyBlobRelease(&sResolved);` |
|        - | 4175 | `					}` |
|        5 | 4176 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 4177 | `						pUsedTrait = pUsedTrait->pNextName;` |
|      ! 0 | 4178 | `					}` |
|        5 | 4179 | `					if( pUsedTrait == 0 ){` |
|        4 | 4180 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        1 | 4181 | `							"'%z' is not a trait",pUsedName);` |
|        3 | 4182 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4183 | `							return SXERR_ABORT;` |
|        - | 4184 | `						}` |
|        2 | 4185 | `					}else{` |
|        3 | 4186 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|        - | 4187 | `					}` |
|        5 | 4188 | `					pGen->pIn++;` |
|        5 | 4189 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|        3 | 4190 | `						break;` |
|        - | 4191 | `					}` |
|      ! 0 | 4192 | `					pGen->pIn++;` |
|      ! 0 | 4193 | `				}` |
|        5 | 4194 | `				continue;` |
|        - | 4195 | `			}` |
|   128369 | 4196 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|   128351 | 4197 | `				iProtection = nKwrd;` |
|   128351 | 4198 | `				pGen->pIn++;` |
|        - | 4199 | ``				/* Optional `readonly` after the visibility (PHP 8.1): `private readonly T`` |
|        - | 4200 | ``				 * $x` — the generated PHPUnit runtime traits (StubApi, …) declare their`` |
|        - | 4201 | `				 * readonly state this way. Mirrors the class-body attribute parser. */` |
|   128351 | 4202 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|        5 | 4203 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|        5 | 4204 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        2 | 4205 | `				}` |
|   128346 | 4206 | `				if( pGen->pIn >= pGen->pEnd` |
|   128351 | 4207 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 4208 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4209 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 4210 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 4211 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 4212 | `						return SXERR_ABORT;` |
|        - | 4213 | `					}` |
|      ! 0 | 4214 | `					goto done;` |
|        - | 4215 | `				}` |
|   128351 | 4216 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|    23335 | 4217 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    23335 | 4218 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 4219 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4220 | `							return SXERR_ABORT;` |
|        - | 4221 | `						}` |
|      ! 0 | 4222 | `						goto done;` |
|        - | 4223 | `					}` |
|    23335 | 4224 | `					continue;` |
|        - | 4225 | `				}` |
|   105021 | 4226 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        9 | 4227 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        9 | 4228 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 4229 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4230 | `							return SXERR_ABORT;` |
|        - | 4231 | `						}` |
|      ! 0 | 4232 | `						goto done;` |
|        - | 4233 | `					}` |
|        9 | 4234 | `					continue;` |
|        - | 4235 | `				}` |
|   105013 | 4236 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    52504 | 4237 | `			}` |
|   105031 | 4238 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|      ! 0 | 4239 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4240 | `					"Traits cannot have constants");` |
|      ! 0 | 4241 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 4242 | `					return SXERR_ABORT;` |
|        - | 4243 | `				}` |
|      ! 0 | 4244 | `				goto done;` |
|      ! 0 | 4245 | `			}else{` |
|   105031 | 4246 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|     7787 | 4247 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     7787 | 4248 | `					pGen->pIn++;` |
|     7787 | 4249 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     7785 | 4250 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     7785 | 4251 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 4252 | `							iProtection = nKwrd;` |
|      ! 0 | 4253 | `							pGen->pIn++;` |
|      ! 0 | 4254 | `						}` |
|     3890 | 4255 | `					}` |
|     7782 | 4256 | `					if( pGen->pIn >= pGen->pEnd` |
|     7787 | 4257 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 4258 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4259 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|      ! 0 | 4260 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 4261 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4262 | `							return SXERR_ABORT;` |
|        - | 4263 | `						}` |
|      ! 0 | 4264 | `						goto done;` |
|        - | 4265 | `					}` |
|     7787 | 4266 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        3 | 4267 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        3 | 4268 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 4269 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 4270 | `								return SXERR_ABORT;` |
|        - | 4271 | `							}` |
|      ! 0 | 4272 | `							goto done;` |
|        - | 4273 | `						}` |
|        3 | 4274 | `						continue;` |
|        - | 4275 | `					}` |
|     7785 | 4276 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|      ! 0 | 4277 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 4278 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 4279 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 4280 | `								return SXERR_ABORT;` |
|        - | 4281 | `							}` |
|      ! 0 | 4282 | `							goto done;` |
|        - | 4283 | `						}` |
|      ! 0 | 4284 | `						continue;` |
|        - | 4285 | `					}` |
|     7785 | 4286 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   101139 | 4287 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        9 | 4288 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        9 | 4289 | `					pGen->pIn++;` |
|        9 | 4290 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        9 | 4291 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        9 | 4292 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        9 | 4293 | `							iProtection = nKwrd;` |
|        9 | 4294 | `							pGen->pIn++;` |
|        3 | 4295 | `						}` |
|        3 | 4296 | `					}` |
|        9 | 4297 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        6 | 4298 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 4299 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4300 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|      ! 0 | 4301 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 4302 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4303 | `							return SXERR_ABORT;` |
|        - | 4304 | `						}` |
|      ! 0 | 4305 | `						goto done;` |
|        - | 4306 | `					}` |
|        9 | 4307 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        3 | 4308 | `				}` |
|   105029 | 4309 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 4310 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4311 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|      ! 0 | 4312 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 4313 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 4314 | `						return SXERR_ABORT;` |
|        - | 4315 | `					}` |
|      ! 0 | 4316 | `					goto done;` |
|        - | 4317 | `				}` |
|   105029 | 4318 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|      ! 0 | 4319 | `					pGen->pIn++;` |
|      ! 0 | 4320 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 4321 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4322 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 4323 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4324 | `							return SXERR_ABORT;` |
|        - | 4325 | `						}` |
|      ! 0 | 4326 | `						goto done;` |
|        - | 4327 | `					}` |
|      ! 0 | 4328 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 4329 | `				}else{` |
|   105029 | 4330 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 4331 | `				}` |
|   105029 | 4332 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 4333 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 4334 | `						return SXERR_ABORT;` |
|        - | 4335 | `					}` |
|      ! 0 | 4336 | `					goto done;` |
|        - | 4337 | `				}` |
|        - | 4338 | `			}` |
|    52517 | 4339 | `		}else{` |
|      ! 0 | 4340 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 4341 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 4342 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 4343 | `					return SXERR_ABORT;` |
|        - | 4344 | `				}` |
|      ! 0 | 4345 | `				goto done;` |
|        - | 4346 | `			}` |
|        - | 4347 | `		}` |
|        5 | 4348 | `	}` |
|        - | 4349 | `	/* Reject a php-fatal redeclaration before hoisting the trait */` |
|     7877 | 4350 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|        3 | 4351 | `		return SXERR_ABORT;` |
|        - | 4352 | `	}` |
|        - | 4353 | `	/* Install the trait */` |
|     7875 | 4354 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     7875 | 4355 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 4356 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4357 | `		return SXERR_ABORT;` |
|        - | 4358 | `	}` |
|     3935 | 4359 | `done:` |
|        - | 4360 | `	/* Point beyond the trait body */` |
|     7875 | 4361 | `	pGen->pIn = &pEnd[1];` |
|     7875 | 4362 | `	pGen->pEnd = pTmp;` |
|     7875 | 4363 | `	return PH7_OK;` |
|     3941 | 4364 | `}` |
|        - | 4365 | `/*` |
|        - | 4366 | ` * Compile a user-defined class.` |
|        - | 4367 | ` *  According to the PHP language reference manual` |
|        - | 4368 | ` *   Basic class definitions begin with the keyword class, followed` |
|        - | 4369 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|        - | 4370 | ` *   the definitions of the properties and methods belonging to the class.` |
|        - | 4371 | ` *   A class may contain its own constants, variables (called "properties")` |
|        - | 4372 | ` *   and functions (called "methods").` |
|        - | 4373 | ` */` |
|   405972 | 4374 | `PH7_PRIVATE sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|        5 | 4375 | `{` |
|        - | 4376 | `	sxi32 rc;` |
|   405977 | 4377 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   405977 | 4378 | `	return rc;` |
|        5 | 4379 | `}` |
|        - | 4380 | `/*` |
|        - | 4381 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|        - | 4382 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|        - | 4383 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|        - | 4384 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|        - | 4385 | `` * meaning; `enum Name` can never start a valid expression.`` |
|        - | 4386 | ` */` |
| 14281984 | 4387 | `PH7_PRIVATE int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|        5 | 4388 | `{` |
| 14498380 | 4389 | `	return (pIn->nType & PH7_TK_ID)` |
|  7357383 | 4390 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|   226253 | 4391 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
| 14498375 | 4392 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|        5 | 4393 | `}` |
|        - | 4394 | `/*` |
|        - | 4395 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|        - | 4396 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|        - | 4397 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|        - | 4398 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|        - | 4399 | ` */` |
|     3922 | 4400 | `PH7_PRIVATE sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|        5 | 4401 | `{` |
|     3927 | 4402 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|        5 | 4403 | `}` |
|        - | 4404 |  |
