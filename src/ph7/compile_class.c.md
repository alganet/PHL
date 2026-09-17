# src/ph7/compile_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2163/2848 lines (75.95%)

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
|   530940 |   27 | `static sxi32 GenStateGuardClassRedeclaration(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |   28 | `{` |
|        - |   29 | `	SyHashEntry *pEntry;` |
|   530945 |   30 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|       26 |   31 | `		return SXRET_OK; /* conditional/nested: keep hoisting */` |
|        - |   32 | `	}` |
|   530923 |   33 | `	pClass->iFlags \|= PH7_CLASS_BOUND;` |
|   530923 |   34 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|   528773 |   35 | `		return SXRET_OK; /* the prelude installs each builtin exactly once */` |
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
|   265475 |   59 | `}` |
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
|  3836780 |   71 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|        5 |   72 | `{` |
|  3836785 |   73 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|   280167 |   74 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  3556623 |   75 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   221723 |   76 | `		return PH7_CLASS_PROT_PROTECTED;` |
|        - |   77 | `	}` |
|        - |   78 | `	/* Assume public by default */` |
|  3334905 |   79 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  1918395 |   80 | `}` |
|        - |   81 | `/*` |
|        - |   82 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|        - |   83 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|        - |   84 | ` * token immediately followed by '='. Anything else with a leading type token` |
|        - |   85 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|        - |   86 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|        - |   87 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|        - |   88 | ` */` |
|   342352 |   89 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|        5 |   90 | `{` |
|        - |   91 | `	SyToken *p0, *p1;` |
|   342357 |   92 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |   93 | `		return 0;` |
|        - |   94 | `	}` |
|   342357 |   95 | `	p0 = pGen->pIn;` |
|        - |   96 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|   342357 |   97 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|      ! 0 |   98 | `		return 1;` |
|        - |   99 | `	}` |
|   342357 |  100 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|        5 |  101 | `		return 1;` |
|        - |  102 | `	}` |
|        - |  103 | `	/* A name-like first token begins a type only when followed by another` |
|        - |  104 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|        - |  105 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|   342353 |  106 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|   342353 |  107 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|   342353 |  108 | `		if( p1 ){` |
|   342353 |  109 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|       34 |  110 | `				return 1;` |
|        - |  111 | `			}` |
|   342323 |  112 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|        5 |  113 | `				return 1;` |
|        - |  114 | `			}` |
|   171157 |  115 | `		}` |
|   171157 |  116 | `	}` |
|   342319 |  117 | `	return 0;` |
|   171181 |  118 | `}` |
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
|   724152 |  173 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|        5 |  174 | `{` |
|   724157 |  175 | `	SyToken *p = pGen->pIn;` |
|   724157 |  176 | `	int iDepth = 0;` |
|  1912569 |  177 | `	while( p < pGen->pEnd ){` |
|  1912569 |  178 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   724105 |  179 | `			break; /* end of this initializer */` |
|        - |  180 | `		}` |
|  1188464 |  181 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   602033 |  182 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|    15591 |  183 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
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
|  1188465 |  243 | `		if( p->nType & PH7_TK_OCB ){` |
|       45 |  244 | `			if( iDepth == 0 ){` |
|        - |  245 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|        - |  246 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|        - |  247 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|        - |  248 | `				 * is legal — don't scan into it. */` |
|       45 |  249 | `				break;` |
|        - |  250 | `			}` |
|      ! 0 |  251 | `			iDepth++;` |
|  1188421 |  252 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|    50649 |  253 | `			iDepth++;` |
|  1163099 |  254 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|    50647 |  255 | `			if( iDepth > 0 ){` |
|    50647 |  256 | `				iDepth--;` |
|    25321 |  257 | `			}` |
|  1112456 |  258 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|   391809 |  259 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|        - |  260 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|        - |  261 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|        - |  262 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|       11 |  263 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|       11 |  264 | `				return 1;` |
|        - |  265 | `			}` |
|      ! 0 |  266 | `		}` |
|  1188413 |  267 | `		p++;` |
|        5 |  268 | `	}` |
|   724149 |  269 | `	return 0;` |
|   362081 |  270 | `}` |
|        - |  271 | `/*` |
|        - |  272 | ` * php keeps class CONSTANTS and PROPERTIES in separate namespaces: a class may` |
|        - |  273 | `` * declare `const C` and `public $C` together, and `$obj->C` never resolves to the`` |
|        - |  274 | ` * constant. PHL stores both in the single hAttr table (a constant is just an attr` |
|        - |  275 | ` * carrying PH7_CLASS_ATTR_CONSTANT), so a bare PH7_ClassExtractAttribute() collides` |
|        - |  276 | ` * across the two. These two lookups keep the namespaces apart.` |
|        - |  277 | ` */` |
|  1276768 |  278 | `static ph7_class_attr * GenStateExtractConstant(ph7_class *pClass,SyString *pName)` |
|        5 |  279 | `{` |
|  1276773 |  280 | `	ph7_class_attr *pAttr = PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte);` |
|  1276773 |  281 | `	return ( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) ) ? pAttr : 0;` |
|        5 |  282 | `}` |
|  1526588 |  283 | `static ph7_class_attr * GenStateExtractProperty(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  284 | `{` |
|  1526593 |  285 | `	ph7_class_attr *pAttr = PH7_ClassExtractAttribute(pClass,zName,nByte);` |
|  1526593 |  286 | `	return ( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ) ? pAttr : 0;` |
|        5 |  287 | `}` |
|        - |  288 | `/*` |
|        - |  289 | ` * Return TRUE if the constant expression starting at the current token performs a` |
|        - |  290 | ` * FUNCTION CALL, which php rejects with "Constant expression contains invalid` |
|        - |  291 | `` * operations" in every constant-expression context (global `const`, class/interface`` |
|        - |  292 | ` * constants, property defaults, parameter defaults, attribute arguments).` |
|        - |  293 | ` *` |
|        - |  294 | ` * Shares GenStateInitHasNewExpr's walk: depth-aware so a nested call is caught` |
|        - |  295 | `` * (`[1, f()]`) and an inner comma does not end the scan, and skipping any`` |
|        - |  296 | `` * `function`/`fn` construct outright — a call inside a closure body runs when the`` |
|        - |  297 | ` * closure is invoked, so php allows it.` |
|        - |  298 | ` *` |
|        - |  299 | ` * Deliberately NOT rejected, because php accepts them:` |
|        - |  300 | `` *   - first-class callables, `strlen(...)` — the parens hold only the ellipsis;`` |
|        - |  301 | ``  *   - `new X(...)` — constructor calls are legal in the contexts that allow `new` `` |
|        - |  302 | ` *     at all, and GenStateInitHasNewExpr owns the contexts that do not;` |
|        - |  303 | ` *   - anything inside a ternary. php FOLDS a constant condition and only rejects a` |
|        - |  304 | `` *     call that survives, so `true ? 1 : f()` is legal while `false ? 1 : f()` is`` |
|        - |  305 | ` *     not. PHL does not constant-fold here, so rather than risk rejecting valid` |
|        - |  306 | `` *     code this scan skips an initializer containing a depth-0 `?` entirely. The`` |
|        - |  307 | ` *     residual is a call hiding in a TAKEN ternary branch, which stays accepted.` |
|        - |  308 | ` */` |
|   724202 |  309 | `PH7_PRIVATE int PH7_GenStateInitHasCallExpr(ph7_gen_state *pGen)` |
|        5 |  310 | `{` |
|   724207 |  311 | `	SyToken *p = pGen->pIn;` |
|   724207 |  312 | `	int iDepth = 0;` |
|        - |  313 | `	/* Conservative ternary bail-out (see the note above). */` |
|        - |  314 | `	{` |
|   724207 |  315 | `		SyToken *q = pGen->pIn;` |
|   724207 |  316 | `		int iQd = 0;` |
|  1913917 |  317 | `		while( q < pGen->pEnd ){` |
|  1913879 |  318 | `			if( iQd == 0 && (q->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   724163 |  319 | `				break;` |
|        - |  320 | `			}` |
|  1189721 |  321 | `			if( q->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    50785 |  322 | `				iQd++;` |
|  1164331 |  323 | `			}else if( q->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|    50785 |  324 | `				if( iQd > 0 ){ iQd--; }` |
|  1113551 |  325 | `			}else if( (q->nType & PH7_TK_OP) && q->pUserData` |
|   392035 |  326 | `				&& ((const ph7_expr_op *)q->pUserData)->iOp == EXPR_OP_QUESTY ){` |
|        8 |  327 | `				return 0;` |
|        - |  328 | `			}` |
|  1189715 |  329 | `			q++;` |
|        5 |  330 | `		}` |
|        - |  331 | `	}` |
|  1912763 |  332 | `	while( p < pGen->pEnd ){` |
|  1912763 |  333 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   724157 |  334 | `			break; /* end of this initializer */` |
|        - |  335 | `		}` |
|  1188606 |  336 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   602104 |  337 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|    15591 |  338 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  339 | `			/* A call inside a closure/arrow-fn is deferred to call time: skip the` |
|        - |  340 | `			 * whole construct. Delegating to the sibling scanner is not possible` |
|        - |  341 | ``			 * (it reports `new`), so mirror its bracket walk. */`` |
|        6 |  342 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|        6 |  343 | `			int iBase = iDepth;` |
|        6 |  344 | `			p++;` |
|        6 |  345 | `			if( bArrow ){` |
|       17 |  346 | `				while( p < pGen->pEnd ){` |
|       17 |  347 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        5 |  348 | `						iDepth++;` |
|       15 |  349 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        5 |  350 | `						if( iDepth <= iBase ){` |
|      ! 0 |  351 | `							break;` |
|        - |  352 | `						}` |
|        5 |  353 | `						iDepth--;` |
|       11 |  354 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|        3 |  355 | `						break;` |
|        - |  356 | `					}` |
|       15 |  357 | `					p++;` |
|        1 |  358 | `				}` |
|        2 |  359 | `			}else{` |
|        3 |  360 | `				int iLocal = 0;` |
|        7 |  361 | `				while( p < pGen->pEnd ){` |
|        7 |  362 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|        3 |  363 | `						break;` |
|        - |  364 | `					}` |
|        5 |  365 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        3 |  366 | `						iLocal++;` |
|        4 |  367 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        3 |  368 | `						if( iLocal > 0 ){ iLocal--; }` |
|        1 |  369 | `					}` |
|        5 |  370 | `					p++;` |
|        1 |  371 | `				}` |
|        3 |  372 | `				if( p < pGen->pEnd ){` |
|        3 |  373 | `					int iBrace = 0;` |
|       17 |  374 | `					while( p < pGen->pEnd ){` |
|       17 |  375 | `						if( p->nType & PH7_TK_OCB ){` |
|        3 |  376 | `							iBrace++;` |
|       16 |  377 | `						}else if( p->nType & PH7_TK_CCB ){` |
|        3 |  378 | `							iBrace--;` |
|        3 |  379 | `							if( iBrace == 0 ){` |
|        3 |  380 | `								p++;` |
|        3 |  381 | `								break;` |
|        - |  382 | `							}` |
|      ! 0 |  383 | `						}` |
|       15 |  384 | `						p++;` |
|        1 |  385 | `					}` |
|        1 |  386 | `				}` |
|        - |  387 | `			}` |
|        6 |  388 | `			continue;` |
|        - |  389 | `		}` |
|  1188607 |  390 | `		if( p->nType & PH7_TK_OCB ){` |
|       43 |  391 | `			if( iDepth == 0 ){` |
|       43 |  392 | `				break; /* property-hook list: the default expression ends here */` |
|        - |  393 | `			}` |
|      ! 0 |  394 | `			iDepth++;` |
|  1188565 |  395 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|        - |  396 | `			/* A '(' directly after a NAME is a call. A name here is an identifier` |
|        - |  397 | ``			 * token; `new X(` is excluded by looking for the `new` operator, and`` |
|        - |  398 | ``			 * `f(...)` (first-class callable) by peeking for a lone ellipsis. */`` |
|    50664 |  399 | `			if( (p->nType & PH7_TK_LPAREN) && p > pGen->pIn` |
|    15582 |  400 | `				&& (p[-1].nType & PH7_TK_ID)` |
|     7803 |  401 | `				&& !((p[-1].nType & PH7_TK_OP) && p[-1].pUserData` |
|      ! 0 |  402 | `					&& ((const ph7_expr_op *)p[-1].pUserData)->iOp == EXPR_OP_NEW) ){` |
|       18 |  403 | `				int bNewCtor = 0;` |
|       18 |  404 | `				SyToken *q = &p[-1];` |
|        - |  405 | ``				/* Walk back over a qualified name (A\B, A::b, $o->m) to a `new`. */`` |
|       18 |  406 | `				while( q > pGen->pIn && (q[-1].nType & (PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) ){` |
|       10 |  407 | `					if( (q[-1].nType & PH7_TK_OP) && q[-1].pUserData` |
|       14 |  408 | `						&& ((const ph7_expr_op *)q[-1].pUserData)->iOp == EXPR_OP_NEW ){` |
|       14 |  409 | `						bNewCtor = 1;` |
|       14 |  410 | `						break;` |
|        - |  411 | `					}` |
|      ! 0 |  412 | `					if( !GenStateTokenIsMemberOp(&q[-1]) && (q[-1].nType & PH7_TK_NSSEP) == 0 ){` |
|      ! 0 |  413 | `						break;` |
|        - |  414 | `					}` |
|      ! 0 |  415 | `					q--;` |
|      ! 0 |  416 | `				}` |
|       14 |  417 | `				if( !bNewCtor` |
|       13 |  418 | `					&& !(&p[1] < pGen->pEnd && (p[1].nType & PH7_TK_ELLIPSIS)) ){` |
|        3 |  419 | `					return 1;` |
|        - |  420 | `				}` |
|        6 |  421 | `			}` |
|    50667 |  422 | `			iDepth++;` |
|  1163232 |  423 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|    50667 |  424 | `			if( iDepth > 0 ){` |
|    50667 |  425 | `				iDepth--;` |
|    25331 |  426 | `			}` |
|    25331 |  427 | `		}` |
|  1188563 |  428 | `		p++;` |
|        5 |  429 | `	}` |
|   724199 |  430 | `	return 0;` |
|   362106 |  431 | `}` |
|        - |  432 | `/*` |
|        - |  433 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|        - |  434 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|        - |  435 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|        - |  436 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|        - |  437 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|        - |  438 | ` * share the same backing.` |
|        - |  439 | ` */` |
|    15946 |  440 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|        - |  441 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|        5 |  442 | `{` |
|    15951 |  443 | `	pAttr->nType = nType;` |
|    15951 |  444 | `	pAttr->sClass = *pClass;` |
|    15951 |  445 | `	pAttr->sTypeName = *pTypeName;` |
|    15951 |  446 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  447 | `		sxu32 i;` |
|       72 |  448 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|       50 |  449 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|       50 |  450 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|       27 |  451 | `		}` |
|       11 |  452 | `	}` |
|    15951 |  453 | `}` |
|   342352 |  454 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  455 | `{` |
|   342357 |  456 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  457 | `	SySet *pInstrContainer;` |
|        - |  458 | `	ph7_class_attr *pCons;` |
|        - |  459 | `	SyString *pName;` |
|        - |  460 | `	sxi32 rc;` |
|   342357 |  461 | `	sxu32 nType = 0;` |
|        - |  462 | `	SyString sTypeClass;` |
|        - |  463 | `	SyString sTypeText;` |
|        - |  464 | `	SySet aUnionAlts;` |
|   342357 |  465 | `	sxi32 iTypeFlags = 0;` |
|   342357 |  466 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   342357 |  467 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   342357 |  468 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  469 | `	/* Extract visibility level */` |
|   342357 |  470 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  471 | `	/* Mark as constant */` |
|   342357 |  472 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|   342357 |  473 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        - |  474 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|        - |  475 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|   342376 |  476 | `	if( GenStateClassConstHasType(pGen) ){` |
|       61 |  477 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|       38 |  478 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|        - |  479 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|        - |  480 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|        - |  481 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|        - |  482 | `		 * and success paths release. */` |
|       42 |  483 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  484 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  485 | `			goto Synchronize;` |
|       42 |  486 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  487 | `			return SXERR_ABORT;` |
|       42 |  488 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 |  489 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  490 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|      ! 0 |  491 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  492 | `				return SXERR_ABORT;` |
|        - |  493 | `			}` |
|      ! 0 |  494 | `			goto Synchronize;` |
|        - |  495 | `		}` |
|       42 |  496 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       19 |  497 | `	}` |
|   171176 |  498 | `loop:` |
|   342359 |  499 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - |  500 | `		/* Invalid constant name */` |
|      ! 0 |  501 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|      ! 0 |  502 | `		if( rc == SXERR_ABORT ){` |
|        - |  503 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  504 | `			return SXERR_ABORT;` |
|        - |  505 | `		}` |
|      ! 0 |  506 | `		goto Synchronize;` |
|        - |  507 | `	}` |
|        - |  508 | `	/* Peek constant name */` |
|   342359 |  509 | `	pName = &pGen->pIn->sData;` |
|        - |  510 | `	/* Make sure the constant name isn't reserved */` |
|   342359 |  511 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  512 | `		/* Reserved constant name */` |
|      ! 0 |  513 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|      ! 0 |  514 | `		if( rc == SXERR_ABORT ){` |
|        - |  515 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  516 | `			return SXERR_ABORT;` |
|        - |  517 | `		}` |
|      ! 0 |  518 | `		goto Synchronize;` |
|        - |  519 | `	}` |
|        - |  520 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|   342359 |  521 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       61 |  522 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|       38 |  523 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       19 |  524 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|       42 |  525 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  526 | `			return SXERR_ABORT;` |
|       42 |  527 | `		}else if( rc != SXRET_OK ){` |
|        3 |  528 | `			goto Synchronize;` |
|        - |  529 | `		}` |
|       18 |  530 | `	}` |
|        - |  531 | `	/* Advance the stream cursor */` |
|   342357 |  532 | `	pGen->pIn++;` |
|   342357 |  533 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  534 | `		/* Invalid declaration */` |
|      ! 0 |  535 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|      ! 0 |  536 | `		if( rc == SXERR_ABORT ){` |
|        - |  537 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  538 | `			return SXERR_ABORT;` |
|        - |  539 | `		}` |
|      ! 0 |  540 | `		goto Synchronize;` |
|        - |  541 | `	}` |
|   342357 |  542 | `	pGen->pIn++; /* Jump the equal sign */` |
|        - |  543 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|        - |  544 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|        - |  545 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|        - |  546 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|   342352 |  547 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|       39 |  548 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|        8 |  549 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  550 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|        2 |  551 | `			&pClass->sName,pName,&sTypeText);` |
|        6 |  552 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  553 | `			return SXERR_ABORT;` |
|        - |  554 | `		}` |
|        6 |  555 | `		goto Synchronize;` |
|        - |  556 | `	}` |
|        - |  557 | `	/* php: a constant expression may not CALL anything. Same rule as the global` |
|        - |  558 | ``	 * `const` path in compile_stmt.c. */`` |
|   342353 |  559 | `	if( PH7_GenStateInitHasCallExpr(pGen) ){` |
|      ! 0 |  560 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  561 | `			"Constant expression contains invalid operations");` |
|      ! 0 |  562 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  563 | `			return SXERR_ABORT;` |
|        - |  564 | `		}` |
|      ! 0 |  565 | `		goto Synchronize;` |
|        - |  566 | `	}` |
|        - |  567 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|        - |  568 | `	 * constant initializer ("New expressions are not supported in this context").` |
|        - |  569 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|   342353 |  570 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|        6 |  571 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  572 | `			"New expressions are not supported in this context");` |
|        6 |  573 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  574 | `			return SXERR_ABORT;` |
|        - |  575 | `		}` |
|        6 |  576 | `		goto Synchronize;` |
|        - |  577 | `	}` |
|        - |  578 | `	/* php: a class constant may not be redefined in the same class body. The` |
|        - |  579 | `	 * property path already guarded this; the constant path did not, so` |
|        - |  580 | ``	 * `class C{const X=1; const X=2;}` silently kept one of them. */`` |
|        - |  581 | ``	/* php allows `const C` and `public $C` side by side -- separate namespaces --`` |
|        - |  582 | `	 * but PHL resolves both through the single hAttr table, so the member lookup` |
|        - |  583 | `	 * would return whichever was declared last. Rejecting it LOUDLY is the honest` |
|        - |  584 | `	 * state: refusing valid php is a discoverable limitation, silently returning` |
|        - |  585 | `	 * the wrong member is not. A recorded divergence. */` |
|   342344 |  586 | `	if( GenStateExtractConstant(pClass,pName) == 0` |
|   342349 |  587 | `	 && GenStateExtractProperty(pClass,pName->zString,pName->nByte) != 0 ){` |
|      ! 0 |  588 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  589 | `			"Class constant %z::%z collides with a property of the same name"` |
|      ! 0 |  590 | `			" (unsupported: PHL resolves both through one table)",&pClass->sName,pName);` |
|      ! 0 |  591 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  592 | `			return SXERR_ABORT;` |
|        - |  593 | `		}` |
|      ! 0 |  594 | `		goto Synchronize;` |
|        - |  595 | `	}` |
|   342349 |  596 | `	if( GenStateExtractConstant(pClass,pName) != 0 ){` |
|      ! 0 |  597 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  598 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|      ! 0 |  599 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  600 | `			return SXERR_ABORT;` |
|        - |  601 | `		}` |
|      ! 0 |  602 | `		goto Synchronize;` |
|        - |  603 | `	}` |
|        - |  604 | `	/* Allocate a new class attribute */` |
|   342349 |  605 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   342349 |  606 | `	if( pCons ){` |
|   342349 |  607 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|   342349 |  608 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  609 | `			return SXERR_ABORT;` |
|        - |  610 | `		}` |
|   171172 |  611 | `	}` |
|   342349 |  612 | `	if( pCons == 0 ){` |
|      ! 0 |  613 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  614 | `		return SXERR_ABORT;` |
|        - |  615 | `	}` |
|   342349 |  616 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       35 |  617 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       16 |  618 | `	}` |
|        - |  619 | `	/* Swap bytecode container */` |
|   342349 |  620 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   342349 |  621 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|        - |  622 | `	/* Compile constant value.` |
|        - |  623 | `	 */` |
|   342349 |  624 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   342349 |  625 | `	if( rc == SXERR_EMPTY ){` |
|        3 |  626 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|        3 |  627 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  628 | `			return SXERR_ABORT;` |
|        - |  629 | `		}` |
|        1 |  630 | `	}` |
|        - |  631 | `	/* Emit the done instruction */` |
|   342349 |  632 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   342349 |  633 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   342349 |  634 | `	if( rc == SXERR_ABORT ){` |
|        - |  635 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  636 | `		return SXERR_ABORT;` |
|        - |  637 | `	}` |
|        - |  638 | `	/* All done,install the constant */` |
|   342349 |  639 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|   342349 |  640 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  641 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  642 | `		return SXERR_ABORT;` |
|        - |  643 | `	}` |
|   342349 |  644 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  645 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|        3 |  646 | `		pGen->pIn++; /* Jump the comma */` |
|        3 |  647 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 |  648 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 |  649 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 |  650 | `				pTok--;` |
|      ! 0 |  651 | `			}` |
|      ! 0 |  652 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  653 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|      ! 0 |  654 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 |  655 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  656 | `				return SXERR_ABORT;` |
|        - |  657 | `			}` |
|      ! 0 |  658 | `		}else{` |
|        3 |  659 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|        3 |  660 | `				goto loop;` |
|        - |  661 | `			}` |
|        - |  662 | `		}` |
|      ! 0 |  663 | `	}` |
|   342347 |  664 | `	SySetRelease(&aUnionAlts);` |
|   342347 |  665 | `	return SXRET_OK;` |
|        5 |  666 | `Synchronize:` |
|       13 |  667 | `	SySetRelease(&aUnionAlts);` |
|        - |  668 | `	/* Synchronize with the first semi-colon */` |
|       45 |  669 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       35 |  670 | `		pGen->pIn++;` |
|        3 |  671 | `	}` |
|       13 |  672 | `	return SXERR_CORRUPT;` |
|   171181 |  673 | `}` |
|        - |  674 | `/*` |
|        - |  675 | ` * complie a class attribute or Properties in the PHP jargon.` |
|        - |  676 | ` * According to the PHP language reference manual` |
|        - |  677 | ` *  Properties` |
|        - |  678 | ` *  Class member variables are called "properties". You may also see them referred` |
|        - |  679 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|        - |  680 | ` *  of this reference we will use "properties". They are defined by using one` |
|        - |  681 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|        - |  682 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|        - |  683 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|        - |  684 | ` *  and must not depend on run-time information in order to be evaluated.` |
|        - |  685 | ` * Symisc eXtension.` |
|        - |  686 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|        - |  687 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  688 | ` *  Example:` |
|        - |  689 | ` *   class Test{` |
|        - |  690 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - |  691 | ` *   };` |
|        - |  692 | ` *   var_dump(TEST::myVar);` |
|        - |  693 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - |  694 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - |  695 | ` */` |
|        - |  696 | `/*` |
|        - |  697 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|        - |  698 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|        - |  699 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|        - |  700 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|        - |  701 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|        - |  702 | ` */` |
|  2626386 |  703 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|        5 |  704 | `{` |
|  2626391 |  705 | `	SyToken *p = pStart;` |
|  2626391 |  706 | `	int bFirst = 1;` |
|  2626391 |  707 | `	if( p >= pEnd ) return 0;` |
|        - |  708 | ``	/* Optional nullable `?` shorthand. */`` |
|  2626391 |  709 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|       41 |  710 | `		p++;` |
|       41 |  711 | `		if( p >= pEnd ) return 0;` |
|       19 |  712 | `	}` |
|        - |  713 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|        - |  714 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|        - |  715 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|        - |  716 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|  1313193 |  717 | `	for(;;){` |
|  2626411 |  718 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|        - |  719 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|        3 |  720 | `			p++;` |
|        9 |  721 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|        3 |  722 | `			if( p >= pEnd ) return 0;` |
|        3 |  723 | `			p++; /* skip ')' */` |
|        2 |  724 | `		}else{` |
|        - |  725 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|        - |  726 | ``			 * then any `&`-joined intersection members. */`` |
|  2626409 |  727 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  2626409 |  728 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  729 | `				return 0;` |
|        - |  730 | `			}` |
|        - |  731 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|        - |  732 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|        - |  733 | `			 * may still appear at the initial dispatch site). */` |
|  2626409 |  734 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  2626353 |  735 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  2626348 |  736 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|   124918 |  737 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  2610483 |  738 | `					return 0;` |
|        - |  739 | `				}` |
|     7935 |  740 | `			}` |
|    15931 |  741 | `			p++;` |
|    15933 |  742 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  743 | `				p += 2;` |
|        1 |  744 | `			}` |
|    23892 |  745 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|    15934 |  746 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  747 | `				p++; /* skip '&' */` |
|        3 |  748 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|        3 |  749 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|        3 |  750 | `				p++;` |
|        3 |  751 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      ! 0 |  752 | `					p += 2;` |
|      ! 0 |  753 | `				}` |
|        1 |  754 | `			}` |
|        - |  755 | `		}` |
|    15933 |  756 | `		bFirst = 0;` |
|    15928 |  757 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       25 |  758 | `			&& p->sData.zString[0] == '\|' ){` |
|       24 |  759 | ``			p++; /* next `\|`-separated part */`` |
|       24 |  760 | `			continue;` |
|        - |  761 | `		}` |
|    15913 |  762 | `		break;` |
|      ! 0 |  763 | `	}` |
|    15913 |  764 | `	if( p >= pEnd ) return 0;` |
|    15913 |  765 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|  1313198 |  766 | `}` |
|        - |  767 |  |
|        - |  768 | `/*` |
|        - |  769 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|        - |  770 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|        - |  771 | ` * if not). Recognized forms:` |
|        - |  772 | ` *   ?Type, array, bool, int, float, string, object,` |
|        - |  773 | ` *   self, parent, \Ns\ClassName, ClassName` |
|        - |  774 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|        - |  775 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|        - |  776 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|        - |  777 | ` * on unrecoverable error.` |
|        - |  778 | ` *` |
|        - |  779 | ` * When a type is parsed:` |
|        - |  780 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|        - |  781 | ` *   *pClass is set to the class name (for class types)` |
|        - |  782 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|        - |  783 | ` *   *pTypeText is set to the original text span of the type` |
|        - |  784 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|        - |  785 | ` */` |
|    15918 |  786 | `static sxi32 GenStateParsePropertyType(` |
|        - |  787 | `	ph7_gen_state *pGen,` |
|        - |  788 | `	sxu32 *pnType,` |
|        - |  789 | `	SyString *pClass,` |
|        - |  790 | `	sxi32 *piTypeFlags,` |
|        - |  791 | `	SyString *pTypeText,` |
|        - |  792 | `	SySet *pAlts` |
|        5 |  793 | `){` |
|    15923 |  794 | `	sxi32 iFlags = 0;` |
|        - |  795 | `	sxi32 rc;` |
|    15923 |  796 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  797 | `		return SXRET_OK;` |
|        - |  798 | `	}` |
|        - |  799 | `	/* If the first token is '$', there's no type */` |
|    15923 |  800 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      ! 0 |  801 | `		return SXRET_OK;` |
|        - |  802 | `	}` |
|    15923 |  803 | `	rc = GenStateParseUnionTypeDecl(` |
|     7959 |  804 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|        - |  805 | `		PH7_CLASS_ATTR_NULLABLE,` |
|        - |  806 | `		PH7_CLASS_ATTR_UNION,` |
|        - |  807 | `		/* bAllowVoid */ 0,` |
|    15918 |  808 | `		pGen->pIn->nLine);` |
|    15923 |  809 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  810 | `		return rc;` |
|        - |  811 | `	}` |
|        - |  812 | `	/* Verify next token is '$' (start of property name) */` |
|    15923 |  813 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  814 | `		return SXERR_SYNTAX;` |
|        - |  815 | `	}` |
|    15923 |  816 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|    15923 |  817 | `	return SXRET_OK;` |
|     7964 |  818 | `}` |
|        - |  819 |  |
|        - |  820 | `/*` |
|        - |  821 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|        - |  822 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|        - |  823 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|        - |  824 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|        - |  825 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|        - |  826 | ` * by the type parser itself before reaching here.` |
|        - |  827 | ` *` |
|        - |  828 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|        - |  829 | ` * use in the error message.` |
|        - |  830 | ` */` |
|    16096 |  831 | `static int GenStateIsDisallowedPropertyAtom(` |
|        - |  832 | `	sxu32 nType,` |
|        - |  833 | `	const SyString *pClass,` |
|        - |  834 | `	const char **pzName,` |
|        - |  835 | `	sxu32 *pnName)` |
|        5 |  836 | `{` |
|        - |  837 | `	const char *z;` |
|        - |  838 | `	sxu32 n;` |
|    16101 |  839 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|    16035 |  840 | `		return 0;` |
|        - |  841 | `	}` |
|       70 |  842 | `	z = pClass->zString;` |
|       70 |  843 | `	n = pClass->nByte;` |
|       70 |  844 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|        8 |  845 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|        - |  846 | `	}` |
|        - |  847 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|        - |  848 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|        - |  849 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|       64 |  850 | `	return 0;` |
|     8053 |  851 | `}` |
|        - |  852 |  |
|        - |  853 | `/*` |
|        - |  854 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|        - |  855 | ` * constant) — the main atom plus any union alternatives — against the` |
|        - |  856 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|        - |  857 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|        - |  858 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|        - |  859 | ` * type T" vs "Class constant C::X cannot have type T").` |
|        - |  860 | ` *` |
|        - |  861 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|        - |  862 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|        - |  863 | ` */` |
|    16034 |  864 | `PH7_PRIVATE sxi32 GenStateValidateMemberType(` |
|        - |  865 | `	ph7_gen_state *pGen,` |
|        - |  866 | `	ph7_class *pClass,` |
|        - |  867 | `	const SyString *pMemberName,` |
|        - |  868 | `	sxu32 nType,` |
|        - |  869 | `	const SyString *pTypeClass,` |
|        - |  870 | `	const SyString *pTypeText,` |
|        - |  871 | `	SySet *pUnionAlts,` |
|        - |  872 | `	const char *zErrFmt,` |
|        - |  873 | `	sxu32 nLine)` |
|        5 |  874 | `{` |
|    16039 |  875 | `	const char *zBad = 0;` |
|    16039 |  876 | `	sxu32 nBad = 0;` |
|        - |  877 | `	SyString sFallback;` |
|        - |  878 | `	const SyString *pBad;` |
|        - |  879 | `	sxi32 rc;` |
|    16039 |  880 | `	int bDisallowed = 0;` |
|    16039 |  881 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|        5 |  882 | `		bDisallowed = 1;` |
|    16037 |  883 | `	}else if( pUnionAlts ){` |
|        - |  884 | `		sxu32 i;` |
|       94 |  885 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|       66 |  886 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|       66 |  887 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|        3 |  888 | `				bDisallowed = 1;` |
|        3 |  889 | `				break;` |
|        - |  890 | `			}` |
|       34 |  891 | `		}` |
|       15 |  892 | `	}` |
|    16039 |  893 | `	if( !bDisallowed ){` |
|    16033 |  894 | `		return SXRET_OK;` |
|        - |  895 | `	}` |
|        - |  896 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|        - |  897 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|        - |  898 | `	 * canonical spelling if the type text is unavailable. */` |
|        8 |  899 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|        8 |  900 | `		pBad = pTypeText;` |
|        5 |  901 | `	}else{` |
|      ! 0 |  902 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|      ! 0 |  903 | `		pBad = &sFallback;` |
|        - |  904 | `	}` |
|       11 |  905 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        3 |  906 | `		zErrFmt,` |
|        3 |  907 | `		&pClass->sName,pMemberName,pBad);` |
|        8 |  908 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  909 | `		return SXERR_ABORT;` |
|        - |  910 | `	}` |
|        8 |  911 | `	return SXERR_SYNTAX;` |
|     8022 |  912 | `}` |
|        - |  913 | `/*` |
|        - |  914 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|        - |  915 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|        - |  916 | ` * matched as a plain identifier in the class-member modifier position rather` |
|        - |  917 | ` * than promoted to a lexer keyword.` |
|        - |  918 | ` */` |
| 23571110 |  919 | `PH7_PRIVATE int GenStateIsReadonly(SyToken *pTok)` |
|        5 |  920 | `{` |
| 23797488 |  921 | `	return (pTok->nType & PH7_TK_ID)` |
| 12011928 |  922 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 23797483 |  923 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|        5 |  924 | `}` |
|        - |  925 | `/*` |
|        - |  926 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|        - |  927 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|        - |  928 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|        - |  929 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|        - |  930 | ` */` |
|  8464138 |  931 | `PH7_PRIVATE sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|        5 |  932 | `{` |
|  8464143 |  933 | `	*pnTok = 0;` |
|  8464138 |  934 | `	if( &pTok[3] < pEnd` |
|  7913133 |  935 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|  6453494 |  936 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|  2772438 |  937 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       16 |  938 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|       16 |  939 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|       21 |  940 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|       17 |  941 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|       17 |  942 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|       17 |  943 | `			*pnTok = 4;` |
|       17 |  944 | `			return nKw;` |
|        - |  945 | `		}` |
|      ! 0 |  946 | `	}` |
|  8464127 |  947 | `	return 0;` |
|  4232074 |  948 | `}` |
|        - |  949 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|       16 |  950 | `PH7_PRIVATE sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|        1 |  951 | `{` |
|       17 |  952 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|       13 |  953 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|        - |  954 | `	}` |
|        5 |  955 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|        3 |  956 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|        - |  957 | `	}` |
|        3 |  958 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|        9 |  959 | `}` |
|   592090 |  960 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  961 | `{` |
|   592095 |  962 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  963 | `	ph7_class_attr *pAttr;` |
|        - |  964 | `	SyString *pName;` |
|        - |  965 | `	sxi32 rc;` |
|   592095 |  966 | `	sxu32 nType = 0;` |
|        - |  967 | `	SyString sTypeClass;` |
|        - |  968 | `	SyString sTypeText;` |
|        - |  969 | `	SySet aUnionAlts;` |
|   592095 |  970 | `	sxi32 iTypeFlags = 0;` |
|   592095 |  971 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   592095 |  972 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   592095 |  973 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  974 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|        - |  975 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|        - |  976 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   592095 |  977 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|       21 |  978 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        9 |  979 | `	}` |
|        - |  980 | `	/* Extract visibility level */` |
|   592095 |  981 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  982 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   600054 |  983 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|    15923 |  984 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|    15923 |  985 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  986 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  987 | `			goto Synchronize;` |
|    15923 |  988 | `		}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  989 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  990 | `				"Invalid property type or declaration near '%z'",` |
|      ! 0 |  991 | `				&pGen->pIn->sData);` |
|      ! 0 |  992 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  993 | `				return SXERR_ABORT;` |
|        - |  994 | `			}` |
|      ! 0 |  995 | `			goto Synchronize;` |
|    15923 |  996 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  997 | `			return SXERR_ABORT;` |
|        - |  998 | `		}` |
|     7959 |  999 | `	}` |
|      ! 0 | 1000 | `loop:` |
|   592099 | 1001 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 1002 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|      ! 0 | 1003 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1004 | `			return SXERR_ABORT;` |
|        - | 1005 | `		}` |
|      ! 0 | 1006 | `		goto Synchronize;` |
|        - | 1007 | `	}` |
|   592099 | 1008 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   592099 | 1009 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        - | 1010 | `		/* Invalid attribute name */` |
|      ! 0 | 1011 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|      ! 0 | 1012 | `		if( rc == SXERR_ABORT ){` |
|        - | 1013 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1014 | `			return SXERR_ABORT;` |
|        - | 1015 | `		}` |
|      ! 0 | 1016 | `		goto Synchronize;` |
|        - | 1017 | `	}` |
|        - | 1018 | `	/* Peek attribute name */` |
|   592099 | 1019 | `	pName = &pGen->pIn->sData;` |
|        - | 1020 | `	/* Advance the stream cursor */` |
|   592099 | 1021 | `	pGen->pIn++;` |
|   592099 | 1022 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|        - | 1023 | `		/* Invalid declaration */` |
|        - | 1024 | `		/* php reports the offending token here, expecting "," or ";". */` |
|        3 | 1025 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\",\" or \";\"");` |
|        3 | 1026 | `		if( rc == SXERR_ABORT ){` |
|        - | 1027 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1028 | `			return SXERR_ABORT;` |
|        - | 1029 | `		}` |
|        3 | 1030 | `		goto Synchronize;` |
|        - | 1031 | `	}` |
|        - | 1032 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|        - | 1033 | `	 * the read visibility must not be narrower than the set visibility. */` |
|   592097 | 1034 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|       13 | 1035 | `		const char *zAvErr = 0;` |
|       19 | 1036 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|       10 | 1037 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|        2 | 1038 | `			: PH7_CLASS_PROT_PUBLIC;` |
|       13 | 1039 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 | 1040 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|       13 | 1041 | `		}else if( iProtection > iSetLevel ){` |
|      ! 0 | 1042 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|      ! 0 | 1043 | `		}` |
|       13 | 1044 | `		if( zAvErr ){` |
|      ! 0 | 1045 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|      ! 0 | 1046 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1047 | `				return SXERR_ABORT;` |
|        - | 1048 | `			}` |
|      ! 0 | 1049 | `			goto Synchronize;` |
|        - | 1050 | `		}` |
|        6 | 1051 | `	}` |
|        - | 1052 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|        - | 1053 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   592097 | 1054 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       51 | 1055 | `		const char *zRoErr = 0;` |
|       51 | 1056 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        3 | 1057 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|       50 | 1058 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        6 | 1059 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|       47 | 1060 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|        6 | 1061 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|        2 | 1062 | `		}` |
|       51 | 1063 | `		if( zRoErr ){` |
|       13 | 1064 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|       13 | 1065 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1066 | `				return SXERR_ABORT;` |
|        - | 1067 | `			}` |
|       13 | 1068 | `			goto Synchronize;` |
|        - | 1069 | `		}` |
|       18 | 1070 | `	}` |
|        - | 1071 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|        - | 1072 | `	 * type atom or any union alternative. void/never are already rejected` |
|        - | 1073 | `	 * by the type parser. */` |
|   592087 | 1074 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|    23879 | 1075 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|        - | 1076 | `			&sTypeText,` |
|    15916 | 1077 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|     7958 | 1078 | `			"Property %z::$%z cannot have type %z",nLine);` |
|    15921 | 1079 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1080 | `			return SXERR_ABORT;` |
|    15921 | 1081 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 | 1082 | `			goto Synchronize;` |
|        - | 1083 | `		}` |
|     7958 | 1084 | `	}` |
|        - | 1085 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|        - | 1086 | `	/* Mirror of the class-constant path: same-name const + property is valid php` |
|        - | 1087 | `	 * that PHL cannot represent, so reject it loudly rather than resolve wrongly. */` |
|   592082 | 1088 | `	if( GenStateExtractProperty(pClass,pName->zString,pName->nByte) == 0` |
|   592086 | 1089 | `	 && GenStateExtractConstant(pClass,pName) != 0 ){` |
|      ! 0 | 1090 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1091 | `			"Property %z::$%z collides with a class constant of the same name"` |
|      ! 0 | 1092 | `			" (unsupported: PHL resolves both through one table)",&pClass->sName,pName);` |
|      ! 0 | 1093 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1094 | `			return SXERR_ABORT;` |
|        - | 1095 | `		}` |
|      ! 0 | 1096 | `		goto Synchronize;` |
|        - | 1097 | `	}` |
|   592087 | 1098 | `	if( GenStateExtractProperty(pClass,pName->zString,pName->nByte) != 0 ){` |
|        4 | 1099 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1100 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|        3 | 1101 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1102 | `			return SXERR_ABORT;` |
|        - | 1103 | `		}` |
|        3 | 1104 | `		goto Synchronize;` |
|        - | 1105 | `	}` |
|        - | 1106 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|        - | 1107 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|        - | 1108 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|        - | 1109 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|        - | 1110 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|        - | 1111 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|        - | 1112 | `	/* php: a property default may not CALL anything either. */` |
|   592085 | 1113 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && PH7_GenStateInitHasCallExpr(pGen) ){` |
|      ! 0 | 1114 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1115 | `			"Constant expression contains invalid operations");` |
|      ! 0 | 1116 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1117 | `			return SXERR_ABORT;` |
|        - | 1118 | `		}` |
|      ! 0 | 1119 | `		goto Synchronize;` |
|        - | 1120 | `	}` |
|   592085 | 1121 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|        6 | 1122 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1123 | `			"New expressions are not supported in this context");` |
|        6 | 1124 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1125 | `			return SXERR_ABORT;` |
|        - | 1126 | `		}` |
|        6 | 1127 | `		goto Synchronize;` |
|        - | 1128 | `	}` |
|        - | 1129 | `	/* Allocate a new class attribute */` |
|   592081 | 1130 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   592081 | 1131 | `	if( pAttr ){` |
|   592081 | 1132 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|   592081 | 1133 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 1134 | `			return SXERR_ABORT;` |
|        - | 1135 | `		}` |
|   296038 | 1136 | `	}` |
|   592081 | 1137 | `	if( pAttr == 0 ){` |
|      ! 0 | 1138 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1139 | `		return SXERR_ABORT;` |
|        - | 1140 | `	}` |
|   592081 | 1141 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|    15919 | 1142 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|     7957 | 1143 | `	}` |
|   592081 | 1144 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|        - | 1145 | `		SySet *pInstrContainer;` |
|   381805 | 1146 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|   381805 | 1147 | `		pGen->pIn++; /*Jump the equal sign */` |
|        - | 1148 | `		{` |
|        - | 1149 | `			/* Delimit the default expression: it ends at the declaration's` |
|        - | 1150 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|        - | 1151 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|        - | 1152 | `			 * compiler would otherwise run into the hook tokens. */` |
|   381805 | 1153 | `			SyToken *pScan = pGen->pIn;` |
|   381805 | 1154 | `			sxi32 iNest = 0;` |
|   841947 | 1155 | `			while( pScan < pGen->pEnd ){` |
|   841947 | 1156 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|    50637 | 1157 | `					iNest++;` |
|   816631 | 1158 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|    50637 | 1159 | `					iNest--;` |
|   765999 | 1160 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|   381805 | 1161 | `					break;` |
|        - | 1162 | `				}` |
|   460147 | 1163 | `				pScan++;` |
|        5 | 1164 | `			}` |
|   381805 | 1165 | `			pGen->pEnd = pScan;` |
|        - | 1166 | `		}` |
|        - | 1167 | `		/* Swap bytecode container */` |
|   381805 | 1168 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   381805 | 1169 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|        - | 1170 | `		/* Compile attribute value.` |
|        - | 1171 | `		 */` |
|   381805 | 1172 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   381805 | 1173 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 1174 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|      ! 0 | 1175 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1176 | `				return SXERR_ABORT;` |
|        - | 1177 | `			}` |
|      ! 0 | 1178 | `		}` |
|        - | 1179 | `		/* Emit the done instruction */` |
|   381805 | 1180 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   381805 | 1181 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   381805 | 1182 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|   381805 | 1183 | `		pGen->pEnd = pSavedDefEnd;` |
|   190900 | 1184 | `	}` |
|        - | 1185 | `	/* All done,install the attribute */` |
|   592081 | 1186 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   592081 | 1187 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1188 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1189 | `		return SXERR_ABORT;` |
|        - | 1190 | `	}` |
|   592081 | 1191 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|        - | 1192 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|        - | 1193 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|       95 | 1194 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|       95 | 1195 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1196 | `			return SXERR_ABORT;` |
|        - | 1197 | `		}` |
|       95 | 1198 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1199 | `			goto Synchronize;` |
|        - | 1200 | `		}` |
|       95 | 1201 | `		SySetRelease(&aUnionAlts);` |
|       95 | 1202 | `		return SXRET_OK;` |
|        - | 1203 | `	}` |
|   591987 | 1204 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - | 1205 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|        - | 1206 | `		 * wording differs per declaration site) */` |
|      ! 0 | 1207 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 1208 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|        - | 1209 | `				? "Interfaces may only include hooked properties"` |
|        - | 1210 | `				: "Only hooked properties may be declared abstract");` |
|      ! 0 | 1211 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1212 | `			return SXERR_ABORT;` |
|        - | 1213 | `		}` |
|      ! 0 | 1214 | `		goto Synchronize;` |
|        - | 1215 | `	}` |
|   591987 | 1216 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - | 1217 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|        5 | 1218 | `		pGen->pIn++; /* Jump the comma */` |
|        5 | 1219 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 | 1220 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 | 1221 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 | 1222 | `				pTok--;` |
|      ! 0 | 1223 | `			}` |
|      ! 0 | 1224 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 1225 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|      ! 0 | 1226 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 | 1227 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1228 | `				return SXERR_ABORT;` |
|        - | 1229 | `			}` |
|      ! 0 | 1230 | `		}else{` |
|        5 | 1231 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        5 | 1232 | `				goto loop;` |
|        - | 1233 | `			}` |
|        - | 1234 | `		}` |
|      ! 0 | 1235 | `	}` |
|   591983 | 1236 | `	SySetRelease(&aUnionAlts);` |
|   591983 | 1237 | `	return SXRET_OK;` |
|        9 | 1238 | `Synchronize:` |
|        - | 1239 | `	/* Synchronize with the first semi-colon */` |
|       56 | 1240 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       38 | 1241 | `		pGen->pIn++;` |
|        4 | 1242 | `	}` |
|       22 | 1243 | `	SySetRelease(&aUnionAlts);` |
|       22 | 1244 | `	return SXERR_CORRUPT;` |
|   296050 | 1245 | `}` |
|        - | 1246 | `/*` |
|        - | 1247 | ` * Compile a class method.` |
|        - | 1248 | ` *` |
|        - | 1249 | ` * Refer to the official documentation for more information` |
|        - | 1250 | ` * on the powerful extension introduced by the PH7 engine` |
|        - | 1251 | ` * to the OO subsystem such as full type hinting,method` |
|        - | 1252 | ` * overloading and many more.` |
|        - | 1253 | ` */` |
|  2902338 | 1254 | `static sxi32 GenStateCompileClassMethod(` |
|        - | 1255 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 1256 | `	sxi32 iProtection,   /* Visibility level */` |
|        - | 1257 | `	sxi32 iFlags,        /* Configuration flags */` |
|        - | 1258 | `	int doBody,          /* TRUE to process method body */` |
|        - | 1259 | `	ph7_class *pClass    /* Class this method belongs */` |
|        - | 1260 | `	)` |
|        5 | 1261 | `{` |
|  2902343 | 1262 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  2902343 | 1263 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|        - | 1264 | `	ph7_class_method *pMeth;` |
|        - | 1265 | `	sxi32 iFuncFlags;` |
|        - | 1266 | `	SyString *pName;` |
|        - | 1267 | `	SyToken *pEnd;` |
|        - | 1268 | `	sxi32 rc;` |
|        - | 1269 | `	/* Extract visibility level */` |
|  2902343 | 1270 | `	iProtection = GetProtectionLevel(iProtection);` |
|  2902343 | 1271 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  2902343 | 1272 | `	iFuncFlags = 0;` |
|  2902343 | 1273 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1274 | `		/* Invalid method name */` |
|      ! 0 | 1275 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|      ! 0 | 1276 | `		if( rc == SXERR_ABORT ){` |
|        - | 1277 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1278 | `			return SXERR_ABORT;` |
|        - | 1279 | `		}` |
|      ! 0 | 1280 | `		goto Synchronize;` |
|        - | 1281 | `	}` |
|  2902343 | 1282 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - | 1283 | `		/* Return by reference,remember that */` |
|      ! 0 | 1284 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|        - | 1285 | `		/* Jump the '&' token */` |
|      ! 0 | 1286 | `		pGen->pIn++;` |
|      ! 0 | 1287 | `	}` |
|  2902343 | 1288 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 1289 | `		/* Invalid method name */` |
|      ! 0 | 1290 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|      ! 0 | 1291 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1292 | `			return SXERR_ABORT;` |
|        - | 1293 | `		}` |
|      ! 0 | 1294 | `		goto Synchronize;` |
|        - | 1295 | `	}` |
|        - | 1296 | `	/* Peek method name */` |
|  2902343 | 1297 | `	pName = &pGen->pIn->sData;` |
|  2902343 | 1298 | `	nLine = pGen->pIn->nLine;` |
|        - | 1299 | `	/* Jump the method name */` |
|  2902343 | 1300 | `	pGen->pIn++;` |
|  2902343 | 1301 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - | 1302 | `		/* Abstract method */` |
|   140049 | 1303 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|      ! 0 | 1304 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1305 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|      ! 0 | 1306 | `				&pClass->sName,pName);` |
|      ! 0 | 1307 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1308 | `				return SXERR_ABORT;` |
|        - | 1309 | `			}` |
|      ! 0 | 1310 | `		}` |
|        - | 1311 | `		/* Assemble method signature only */` |
|   140049 | 1312 | `		doBody = FALSE;` |
|    70022 | 1313 | `	}` |
|  2902343 | 1314 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1315 | `		/* Syntax error */` |
|      ! 0 | 1316 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|      ! 0 | 1317 | `		if( rc == SXERR_ABORT ){` |
|        - | 1318 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1319 | `			return SXERR_ABORT;` |
|        - | 1320 | `		}` |
|      ! 0 | 1321 | `		goto Synchronize;` |
|        - | 1322 | `	}` |
|        - | 1323 | `	/* Allocate a new class_method instance */` |
|  2902343 | 1324 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  2902343 | 1325 | `	if( pMeth == 0 ){` |
|      ! 0 | 1326 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1327 | `		return SXERR_ABORT;` |
|        - | 1328 | `	}` |
|  2902343 | 1329 | `	pMeth->sFunc.nLine = nKwLine;` |
|  2902343 | 1330 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|  2902343 | 1331 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 1332 | `		return SXERR_ABORT;` |
|        - | 1333 | `	}` |
|        - | 1334 | `	/* Jump the left parenthesis '(' */` |
|  2902343 | 1335 | `	pGen->pIn++;` |
|  2902343 | 1336 | `	pEnd = 0; /* cc warning */` |
|        - | 1337 | `	/* Delimit the method signature */` |
|  2902343 | 1338 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  2902343 | 1339 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 1340 | `		/* Syntax error */` |
|        3 | 1341 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|        3 | 1342 | `		if( rc == SXERR_ABORT ){` |
|        - | 1343 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1344 | `			return SXERR_ABORT;` |
|        - | 1345 | `		}` |
|        3 | 1346 | `		goto Synchronize;` |
|        - | 1347 | `	}` |
|        - | 1348 | `	{` |
|  2902341 | 1349 | `		int bIsCtor = 0;` |
|  2902341 | 1350 | `		int bAbstractCtor = 0;` |
|  2902336 | 1351 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  1704059 | 1352 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  2799196 | 1353 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   206295 | 1354 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        3 | 1355 | `				bAbstractCtor = 1;` |
|        2 | 1356 | `			}else{` |
|   206293 | 1357 | `				bIsCtor = 1;` |
|        - | 1358 | `			}` |
|   103145 | 1359 | `		}` |
|  2902341 | 1360 | `		if( pGen->pIn < pEnd ){` |
|        - | 1361 | `			/* Collect method arguments */` |
|  1116601 | 1362 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|  1116601 | 1363 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1364 | `				return SXERR_ABORT;` |
|        - | 1365 | `			}` |
|   558298 | 1366 | `		}` |
|        - | 1367 | `	}` |
|        - | 1368 | `	/* Point past ')' and parse optional return type ': type' */` |
|  2902341 | 1369 | `	pGen->pIn = &pEnd[1];` |
|        - | 1370 | `	{` |
|  2902341 | 1371 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  2902341 | 1372 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 | 1373 | `			return SXERR_ABORT;` |
|  2902341 | 1374 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|      ! 0 | 1375 | `			goto Synchronize;` |
|        - | 1376 | `		}` |
|        - | 1377 | `	}` |
|        - | 1378 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|        - | 1379 | `	 * property init/typecheck is handled by the generic typed-property path` |
|        - | 1380 | `	 * since we mint real ph7_class_attr entries. */` |
|        - | 1381 | `	{` |
|  2902341 | 1382 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|        - | 1383 | `		sxu32 i;` |
|  4571227 | 1384 | `		for( i = 0; i < nArg; i++ ){` |
|  1668901 | 1385 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|        - | 1386 | `			ph7_class_attr *pAttr;` |
|  1668901 | 1387 | `			sxi32 iAttrFlags = 0;` |
|        - | 1388 | `			int bArgTyped;` |
|  1668901 | 1389 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  1668815 | 1390 | `				continue;` |
|        - | 1391 | `			}` |
|        - | 1392 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|        - | 1393 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|        - | 1394 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|       60 | 1395 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|       92 | 1396 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|       91 | 1397 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        3 | 1398 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1399 | `					"Cannot declare variadic promoted property");` |
|        3 | 1400 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1401 | `					return SXERR_ABORT;` |
|        - | 1402 | `				}` |
|        3 | 1403 | `				goto Synchronize;` |
|        - | 1404 | `			}` |
|        - | 1405 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|        - | 1406 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|        - | 1407 | `			 * appear as an alternative of a union type. */` |
|       89 | 1408 | `			if( bArgTyped ){` |
|      125 | 1409 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|       80 | 1410 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|       80 | 1411 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|       40 | 1412 | `					"Property %z::$%z cannot have type %z",nLine);` |
|       85 | 1413 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1414 | `					return SXERR_ABORT;` |
|       85 | 1415 | `				}else if( rc != SXRET_OK ){` |
|        6 | 1416 | `					goto Synchronize;` |
|        - | 1417 | `				}` |
|       38 | 1418 | `			}` |
|        - | 1419 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|       85 | 1420 | `			if( GenStateExtractProperty(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|        4 | 1421 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1422 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|        3 | 1423 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1424 | `					return SXERR_ABORT;` |
|        - | 1425 | `				}` |
|        3 | 1426 | `				goto Synchronize;` |
|        - | 1427 | `			}` |
|       83 | 1428 | `			if( bArgTyped ){` |
|       79 | 1429 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       37 | 1430 | `			}` |
|       83 | 1431 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|        3 | 1432 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|        1 | 1433 | `			}` |
|       83 | 1434 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|        8 | 1435 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|        3 | 1436 | `			}` |
|       83 | 1437 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|        - | 1438 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|        - | 1439 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|       26 | 1440 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        4 | 1441 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1442 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|        3 | 1443 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1444 | `						return SXERR_ABORT;` |
|        - | 1445 | `					}` |
|        3 | 1446 | `					goto Synchronize;` |
|        - | 1447 | `				}` |
|       24 | 1448 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       10 | 1449 | `			}` |
|       81 | 1450 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|        - | 1451 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|        5 | 1452 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 | 1453 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1454 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|      ! 0 | 1455 | `						&pClass->sName,&pArg->sName);` |
|      ! 0 | 1456 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1457 | `						return SXERR_ABORT;` |
|        - | 1458 | `					}` |
|      ! 0 | 1459 | `					goto Synchronize;` |
|        - | 1460 | `				}` |
|        5 | 1461 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|        2 | 1462 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|        2 | 1463 | `			}` |
|       81 | 1464 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|       81 | 1465 | `			if( pAttr == 0 ){` |
|      ! 0 | 1466 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1467 | `				return SXERR_ABORT;` |
|        - | 1468 | `			}` |
|       81 | 1469 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|       79 | 1470 | `				pAttr->nType = pArg->nType;` |
|       79 | 1471 | `				pAttr->sClass = pArg->sClass;` |
|       79 | 1472 | `				pAttr->sTypeName = pArg->sTypeName;` |
|       79 | 1473 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|        - | 1474 | `					sxu32 k;` |
|       20 | 1475 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|       14 | 1476 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|       14 | 1477 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|        8 | 1478 | `					}` |
|        3 | 1479 | `				}` |
|       37 | 1480 | `			}` |
|       81 | 1481 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|       81 | 1482 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1483 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1484 | `				return SXERR_ABORT;` |
|        - | 1485 | `			}` |
|       43 | 1486 | `		}` |
|        - | 1487 | `	}` |
|  2902331 | 1488 | `	if( doBody ){` |
|        - | 1489 | `		/* Compile method body */` |
|  2762287 | 1490 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  2762287 | 1491 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1492 | `			return SXERR_ABORT;` |
|        - | 1493 | `		}` |
|        - | 1494 | `		/* The cursor sits just past the body's closing brace */` |
|  2762287 | 1495 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|  1381146 | 1496 | `	}else{` |
|        - | 1497 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|   140049 | 1498 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|   140049 | 1499 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|    70022 | 1500 | `		}` |
|        - | 1501 | `		/* Only method signature is allowed */` |
|   140049 | 1502 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|      ! 0 | 1503 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 1504 | `				"Expected ';' after method signature '%z'",pName);` |
|      ! 0 | 1505 | `				if( rc == SXERR_ABORT ){` |
|        - | 1506 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 | 1507 | `					return SXERR_ABORT;` |
|        - | 1508 | `				}` |
|      ! 0 | 1509 | `				return SXERR_CORRUPT;` |
|        - | 1510 | `			}` |
|        - | 1511 | `	}` |
|        - | 1512 | `	/* php: an abstract method in a class that was not DECLARED abstract is a fatal,` |
|        - | 1513 | `	 * and it names the method -- distinct from the "contains N abstract methods"` |
|        - | 1514 | `	 * wording php uses for an unimplemented INHERITED one, which` |
|        - | 1515 | `	 * GenStateCheckAbstractMethods already handles. Interfaces and traits may carry` |
|        - | 1516 | `	 * abstract methods freely. */` |
|  2902326 | 1517 | `	if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT)` |
|  1521190 | 1518 | `	 && (pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|        4 | 1519 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1520 | `			"Class %z declares abstract method %z() and must therefore be declared abstract",` |
|        1 | 1521 | `			&pClass->sName,pName);` |
|        3 | 1522 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1523 | `			return SXERR_ABORT;` |
|        - | 1524 | `		}` |
|        3 | 1525 | `		return SXRET_OK;` |
|        - | 1526 | `	}` |
|        - | 1527 | `	/* php: two methods of the same name in one class body is a fatal, and the` |
|        - | 1528 | ``	 * comparison is case-INSENSITIVE (hMethod now matches that way), so `f` and`` |
|        - | 1529 | ``	 * `F` collide. php names the offending declaration with the spelling used at`` |
|        - | 1530 | `	 * the SECOND site. */` |
|  2902329 | 1531 | `	if( PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte) != 0 ){` |
|        8 | 1532 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        2 | 1533 | `			"Cannot redeclare %z::%z()",&pClass->sName,pName);` |
|        6 | 1534 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1535 | `			return SXERR_ABORT;` |
|        - | 1536 | `		}` |
|        6 | 1537 | `		return SXRET_OK;` |
|        - | 1538 | `	}` |
|        - | 1539 | `	/* All done,install the method */` |
|  2902325 | 1540 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  2902325 | 1541 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1542 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1543 | `		return SXERR_ABORT;` |
|        - | 1544 | `	}` |
|  2902325 | 1545 | `	return SXRET_OK;` |
|        6 | 1546 | `Synchronize:` |
|        - | 1547 | `	/* Synchronize with the first semi-colon */` |
|       40 | 1548 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       28 | 1549 | `		pGen->pIn++;` |
|        4 | 1550 | `	}` |
|       16 | 1551 | `	return SXERR_CORRUPT;` |
|  1451174 | 1552 | `}` |
|        - | 1553 | `/*` |
|        - | 1554 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|        - | 1555 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|        - | 1556 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|        - | 1557 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|        - | 1558 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|        - | 1559 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|        - | 1560 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|        - | 1561 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|        - | 1562 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|        - | 1563 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|        - | 1564 | `` * implicit `$value` formal.`` |
|        - | 1565 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|        - | 1566 | ` */` |
|        - | 1567 | `/*` |
|        - | 1568 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|        - | 1569 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|        - | 1570 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|        - | 1571 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|        - | 1572 | ` * allowed, excluded from the raw object surfaces.` |
|        - | 1573 | ` */` |
|       94 | 1574 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|        1 | 1575 | `{` |
|        - | 1576 | `	SyToken *p;` |
|      345 | 1577 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|      303 | 1578 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      223 | 1579 | `			continue;` |
|        - | 1580 | `		}` |
|        - | 1581 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|       80 | 1582 | `		if( p + 3 < pEnd` |
|       80 | 1583 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       80 | 1584 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|       73 | 1585 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|       66 | 1586 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|       66 | 1587 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       66 | 1588 | `		 && p[3].sData.nByte == pName->nByte` |
|       60 | 1589 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       51 | 1590 | `			return 1;` |
|        - | 1591 | `		}` |
|        - | 1592 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|        - | 1593 | `		 * hook operates on the shared per-instance backing store, so the` |
|        - | 1594 | `		 * property is backed (php compiles a default alongside it). */` |
|       30 | 1595 | `		if( p > pStart` |
|       26 | 1596 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|       12 | 1597 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        2 | 1598 | `		 && p[1].sData.nByte == pName->nByte` |
|        3 | 1599 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        3 | 1600 | `			return 1;` |
|        - | 1601 | `		}` |
|       15 | 1602 | `	}` |
|       43 | 1603 | `	return 0;` |
|       48 | 1604 | `}` |
|        - | 1605 | `/*` |
|        - | 1606 | ` * True when p opens php 8.4's parent-hook call form` |
|        - | 1607 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|        - | 1608 | ` */` |
|      990 | 1609 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|        1 | 1610 | `{` |
|     1167 | 1611 | `	return p + 6 < pEnd` |
|      671 | 1612 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|      250 | 1613 | `	 && p->sData.nByte == sizeof("parent")-1` |
|       81 | 1614 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|       11 | 1615 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|        8 | 1616 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|        8 | 1617 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        8 | 1618 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|        8 | 1619 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        8 | 1620 | `	 && p[5].sData.nByte == 3` |
|        8 | 1621 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|        6 | 1622 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|     1166 | 1623 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|        1 | 1624 | `}` |
|        - | 1625 | `/*` |
|        - | 1626 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|        - | 1627 | ` * hook body into calls of the parent class's synthesized hook method` |
|        - | 1628 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|        - | 1629 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|        - | 1630 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|        - | 1631 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|        - | 1632 | ` * or SXERR_MEM.` |
|        - | 1633 | ` */` |
|        4 | 1634 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|        - | 1635 | `	SyToken *pStart,SyToken *pEnd)` |
|        1 | 1636 | `{` |
|        5 | 1637 | `	SyToken *p = pStart;` |
|       35 | 1638 | `	while( p < pEnd ){` |
|       31 | 1639 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|        - | 1640 | `			SyToken sTok;` |
|        - | 1641 | `			char zName[384];` |
|        - | 1642 | `			sxu32 nName;` |
|        - | 1643 | `			char *zDup;` |
|        - | 1644 | ``			/* `parent` `::` */`` |
|        5 | 1645 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|        5 | 1646 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|        7 | 1647 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|        4 | 1648 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|        5 | 1649 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|        5 | 1650 | `			if( zDup == 0 ){` |
|      ! 0 | 1651 | `				return SXERR_MEM;` |
|        - | 1652 | `			}` |
|        5 | 1653 | `			sTok = p[3]; /* keep the line info of the property name */` |
|        5 | 1654 | `			sTok.nType = PH7_TK_ID;` |
|        5 | 1655 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|        5 | 1656 | `			sTok.pUserData = 0;` |
|        5 | 1657 | `			SySetPut(pCopy,(const void *)&sTok);` |
|        5 | 1658 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|        5 | 1659 | `			continue;` |
|        - | 1660 | `		}` |
|       27 | 1661 | `		SySetPut(pCopy,(const void *)p);` |
|       27 | 1662 | `		p++;` |
|        1 | 1663 | `	}` |
|        5 | 1664 | `	return SXRET_OK;` |
|        3 | 1665 | `}` |
|       94 | 1666 | `PH7_PRIVATE sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 | 1667 | `{` |
|       95 | 1668 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 1669 | `	sxi32 rc;` |
|       95 | 1670 | `	int bRefsSelf = 0;` |
|       95 | 1671 | `	pGen->pIn++; /* Jump '{' */` |
|      253 | 1672 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|        - | 1673 | `		char zHook[384];` |
|        - | 1674 | `		SyString sHookName;` |
|        - | 1675 | `		ph7_class_method *pMeth;` |
|        - | 1676 | `		int bGet;` |
|      159 | 1677 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|      159 | 1678 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       15 | 1679 | `			pGen->pIn++; /* stray ';' between hooks */` |
|       22 | 1680 | `			continue;` |
|        - | 1681 | `		}` |
|      145 | 1682 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - | 1683 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|      ! 0 | 1684 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 1685 | `				"By-reference property hooks are not supported for %z::$%z",` |
|      ! 0 | 1686 | `				&pClass->sName,&pAttr->sName);` |
|      ! 0 | 1687 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1688 | `				return SXERR_ABORT;` |
|        - | 1689 | `			}` |
|      ! 0 | 1690 | `			return SXERR_CORRUPT;` |
|        - | 1691 | `		}` |
|      145 | 1692 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 1693 | `			goto HookSyntax;` |
|        - | 1694 | `		}` |
|      144 | 1695 | `		if( pGen->pIn->sData.nByte == 3` |
|      145 | 1696 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|       79 | 1697 | `			bGet = 1;` |
|      106 | 1698 | `		}else if( pGen->pIn->sData.nByte == 3` |
|       67 | 1699 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|       67 | 1700 | `			bGet = 0;` |
|       34 | 1701 | `		}else{` |
|      ! 0 | 1702 | `			goto HookSyntax;` |
|        - | 1703 | `		}` |
|      145 | 1704 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|      145 | 1705 | `		sHookName.zString = zHook;` |
|      217 | 1706 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|       72 | 1707 | `			bGet ? "get" : "set",&pAttr->sName);` |
|      145 | 1708 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|        - | 1709 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|        - | 1710 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|        - | 1711 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|        - | 1712 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|        - | 1713 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|       14 | 1714 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|        8 | 1715 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 1716 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 1717 | `					"Non-abstract property hook must have a body");` |
|      ! 0 | 1718 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1719 | `					return SXERR_ABORT;` |
|        - | 1720 | `				}` |
|      ! 0 | 1721 | `				return SXERR_CORRUPT;` |
|        - | 1722 | `			}` |
|       15 | 1723 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|        - | 1724 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|       15 | 1725 | `			if( pMeth == 0 ){` |
|      ! 0 | 1726 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1727 | `				return SXERR_ABORT;` |
|        - | 1728 | `			}` |
|       15 | 1729 | `			pMeth->sFunc.nLine = nHLine;` |
|       15 | 1730 | `			if( !bGet ){` |
|        - | 1731 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|        - | 1732 | `				 * compatible with concrete set-hook implementations (which` |
|        - | 1733 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|        - | 1734 | `				 * type (php: the abstract set's parameter type IS the property` |
|        - | 1735 | `				 * type), so the override contravariance check accepts a typed` |
|        - | 1736 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|        - | 1737 | `				ph7_vm_func_arg sVArg;` |
|        7 | 1738 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        7 | 1739 | `				if( zVName == 0 ){` |
|      ! 0 | 1740 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1741 | `					return SXERR_ABORT;` |
|        - | 1742 | `				}` |
|        7 | 1743 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        7 | 1744 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        7 | 1745 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        7 | 1746 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        7 | 1747 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        7 | 1748 | `				sVArg.nType = pAttr->nType;` |
|        7 | 1749 | `				sVArg.sClass = pAttr->sClass;` |
|        7 | 1750 | `				sVArg.sTypeName = pAttr->sTypeName;` |
|        7 | 1751 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|      ! 0 | 1752 | `					sVArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|      ! 0 | 1753 | `				}` |
|        7 | 1754 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        3 | 1755 | `			}` |
|       15 | 1756 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       15 | 1757 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1758 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1759 | `				return SXERR_ABORT;` |
|        - | 1760 | `			}` |
|       15 | 1761 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|       15 | 1762 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|        - | 1763 | `		}` |
|      130 | 1764 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|      131 | 1765 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|        - | 1766 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|      ! 0 | 1767 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 1768 | `				"Abstract property hook cannot have body");` |
|      ! 0 | 1769 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1770 | `				return SXERR_ABORT;` |
|        - | 1771 | `			}` |
|      ! 0 | 1772 | `			return SXERR_CORRUPT;` |
|        - | 1773 | `		}` |
|      131 | 1774 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|        - | 1775 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|      131 | 1776 | `		if( pMeth == 0 ){` |
|      ! 0 | 1777 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1778 | `			return SXERR_ABORT;` |
|        - | 1779 | `		}` |
|      131 | 1780 | `		pMeth->sFunc.nLine = nHLine;` |
|      131 | 1781 | `		if( !bGet ){` |
|        - | 1782 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|       61 | 1783 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       17 | 1784 | `				SyToken *pRp = 0;` |
|       17 | 1785 | `				pGen->pIn++;` |
|       17 | 1786 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|       17 | 1787 | `				if( pRp >= pGen->pEnd ){` |
|      ! 0 | 1788 | `					goto HookSyntax;` |
|        - | 1789 | `				}` |
|       17 | 1790 | `				if( pGen->pIn < pRp ){` |
|       17 | 1791 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|       17 | 1792 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1793 | `						return SXERR_ABORT;` |
|        - | 1794 | `					}` |
|        8 | 1795 | `				}` |
|       17 | 1796 | `				pGen->pIn = &pRp[1];` |
|        8 | 1797 | `			}` |
|       61 | 1798 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|        - | 1799 | `				/* Implicit $value formal */` |
|        - | 1800 | `				ph7_vm_func_arg sVArg;` |
|       45 | 1801 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|       45 | 1802 | `				if( zVName == 0 ){` |
|      ! 0 | 1803 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1804 | `					return SXERR_ABORT;` |
|        - | 1805 | `				}` |
|       45 | 1806 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|       45 | 1807 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|       45 | 1808 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       45 | 1809 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       45 | 1810 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|       45 | 1811 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|       45 | 1812 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|       22 | 1813 | `			}` |
|       30 | 1814 | `		}` |
|      165 | 1815 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - | 1816 | `			/* Block body */` |
|       69 | 1817 | `			SyToken *pBodyStart = pGen->pIn;` |
|       69 | 1818 | `			SyToken *pCloser = 0;` |
|       69 | 1819 | `			int bParentCall = 0;` |
|       69 | 1820 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|       69 | 1821 | `			if( pCloser < pGen->pEnd ){` |
|        - | 1822 | `				SyToken *pScan;` |
|      753 | 1823 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|      687 | 1824 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|        3 | 1825 | `						bParentCall = 1;` |
|        3 | 1826 | `						break;` |
|        - | 1827 | `					}` |
|      343 | 1828 | `				}` |
|       34 | 1829 | `			}` |
|       69 | 1830 | `			if( bParentCall ){` |
|        - | 1831 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|        - | 1832 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|        - | 1833 | `				 * hook method), then continue past the original body. */` |
|        - | 1834 | `				SySet sBody;` |
|        3 | 1835 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|        3 | 1836 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        3 | 1837 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|        3 | 1838 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1839 | `					SySetRelease(&sBody);` |
|      ! 0 | 1840 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1841 | `					return SXERR_ABORT;` |
|        - | 1842 | `				}` |
|        3 | 1843 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|        3 | 1844 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|        3 | 1845 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        3 | 1846 | `				pGen->pIn = &pCloser[1];` |
|        3 | 1847 | `				pGen->pEnd = pSavedEnd;` |
|        3 | 1848 | `				SySetRelease(&sBody);` |
|        3 | 1849 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1850 | `					return SXERR_ABORT;` |
|        - | 1851 | `				}` |
|        3 | 1852 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|        2 | 1853 | `			}else{` |
|       67 | 1854 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|       67 | 1855 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1856 | `					return SXERR_ABORT;` |
|        - | 1857 | `				}` |
|       67 | 1858 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|        - | 1859 | `			}` |
|       69 | 1860 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|       17 | 1861 | `				bRefsSelf = 1;` |
|        9 | 1862 | `			}` |
|      128 | 1863 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|        - | 1864 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|        - | 1865 | `			GenBlock *pBlock;` |
|        - | 1866 | `			SySet *pInstrContainer;` |
|        - | 1867 | `			SyToken *pBodyStart;` |
|        - | 1868 | `			SyToken *pExprEnd;` |
|       63 | 1869 | `			SyToken *pSavedEnd = 0;` |
|        - | 1870 | `			SySet sBody;` |
|       63 | 1871 | `			int bParentCall = 0;` |
|       63 | 1872 | `			pGen->pIn++; /* Jump '=>' */` |
|       63 | 1873 | `			pBodyStart = pGen->pIn;` |
|        - | 1874 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|        - | 1875 | `			 * would end the enclosing hook list) and rewrite any` |
|        - | 1876 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|        - | 1877 | `			 * method on a token copy. */` |
|        - | 1878 | `			{` |
|       63 | 1879 | `				sxi32 iNest = 0;` |
|       63 | 1880 | `				pExprEnd = pBodyStart;` |
|      355 | 1881 | `				while( pExprEnd < pGen->pEnd ){` |
|      355 | 1882 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        9 | 1883 | `						iNest++;` |
|      351 | 1884 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        9 | 1885 | `						if( iNest <= 0 ){` |
|      ! 0 | 1886 | `							break;` |
|        - | 1887 | `						}` |
|        9 | 1888 | `						iNest--;` |
|      343 | 1889 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|       63 | 1890 | `						break;` |
|        - | 1891 | `					}` |
|      293 | 1892 | `					pExprEnd++;` |
|        1 | 1893 | `				}` |
|        - | 1894 | `			}` |
|        - | 1895 | `			{` |
|        - | 1896 | `				SyToken *pScan;` |
|      335 | 1897 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|      275 | 1898 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|        3 | 1899 | `						bParentCall = 1;` |
|        3 | 1900 | `						break;` |
|        - | 1901 | `					}` |
|      137 | 1902 | `				}` |
|        - | 1903 | `			}` |
|       63 | 1904 | `			if( bParentCall ){` |
|        3 | 1905 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        3 | 1906 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|        3 | 1907 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1908 | `					SySetRelease(&sBody);` |
|      ! 0 | 1909 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1910 | `					return SXERR_ABORT;` |
|        - | 1911 | `				}` |
|        3 | 1912 | `				pSavedEnd = pGen->pEnd;` |
|        3 | 1913 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|        3 | 1914 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|        1 | 1915 | `			}` |
|       94 | 1916 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       62 | 1917 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|       63 | 1918 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1919 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|      ! 0 | 1920 | `				return SXERR_ABORT;` |
|        - | 1921 | `			}` |
|       63 | 1922 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       63 | 1923 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|       63 | 1924 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|       63 | 1925 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       63 | 1926 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       63 | 1927 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       63 | 1928 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       63 | 1929 | `			GenStateLeaveBlock(&(*pGen),0);` |
|       63 | 1930 | `			if( bParentCall ){` |
|        3 | 1931 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|        3 | 1932 | `				pGen->pEnd = pSavedEnd;` |
|        3 | 1933 | `				SySetRelease(&sBody);` |
|        1 | 1934 | `			}` |
|       63 | 1935 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1936 | `				return SXERR_ABORT;` |
|        - | 1937 | `			}` |
|       63 | 1938 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|       63 | 1939 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|       37 | 1940 | `				bRefsSelf = 1;` |
|       18 | 1941 | `			}` |
|       63 | 1942 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       63 | 1943 | `				pGen->pIn++; /* Jump ';' */` |
|       31 | 1944 | `			}` |
|       63 | 1945 | `			if( !bGet ){` |
|        - | 1946 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|        - | 1947 | `				 * the dispatcher consumes the implicit return value — which` |
|        - | 1948 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|        - | 1949 | ``				 * for `$this->NAME = expr`). */`` |
|        3 | 1950 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|        3 | 1951 | `				bRefsSelf = 1;` |
|        1 | 1952 | `			}` |
|       32 | 1953 | `		}else{` |
|      ! 0 | 1954 | `			goto HookSyntax;` |
|        - | 1955 | `		}` |
|      131 | 1956 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|      131 | 1957 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1958 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1959 | `			return SXERR_ABORT;` |
|        - | 1960 | `		}` |
|      131 | 1961 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        1 | 1962 | `	}` |
|       95 | 1963 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|      ! 0 | 1964 | `		goto HookSyntax;` |
|        - | 1965 | `	}` |
|       95 | 1966 | `	pGen->pIn++; /* Jump '}' */` |
|       95 | 1967 | `	if( !bRefsSelf ){` |
|        - | 1968 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|        - | 1969 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|        - | 1970 | `		 * a default value (compile fatal, php's exact wording). */` |
|       41 | 1971 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|       41 | 1972 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      ! 0 | 1973 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1974 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|      ! 0 | 1975 | `				&pClass->sName,&pAttr->sName);` |
|      ! 0 | 1976 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1977 | `				return SXERR_ABORT;` |
|        - | 1978 | `			}` |
|      ! 0 | 1979 | `			return SXERR_CORRUPT;` |
|        - | 1980 | `		}` |
|       20 | 1981 | `	}` |
|       95 | 1982 | `	return SXRET_OK;` |
|      ! 0 | 1983 | `HookSyntax:` |
|      ! 0 | 1984 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1985 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|      ! 0 | 1986 | `		&pClass->sName,&pAttr->sName);` |
|      ! 0 | 1987 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1988 | `		return SXERR_ABORT;` |
|        - | 1989 | `	}` |
|      ! 0 | 1990 | `	return SXERR_CORRUPT;` |
|       48 | 1991 | `}` |
|        - | 1992 | `/*` |
|        - | 1993 | ` * Compile an object interface.` |
|        - | 1994 | ` *  According to the PHP language reference manual` |
|        - | 1995 | ` *   Object Interfaces:` |
|        - | 1996 | ` *   Object interfaces allow you to create code which specifies which methods` |
|        - | 1997 | ` *   a class must implement, without having to define how these methods are handled.` |
|        - | 1998 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - | 1999 | ` *   class, but without any of the methods having their contents defined.` |
|        - | 2000 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|        - | 2001 | ` */` |
|    70108 | 2002 | `PH7_PRIVATE sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|        5 | 2003 | `{` |
|    70113 | 2004 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2005 | `	ph7_class *pClass,*pBase;` |
|        - | 2006 | `	SyToken *pEnd,*pTmp;` |
|        - | 2007 | `	SyString *pName;` |
|        - | 2008 | `	sxi32 nKwrd;` |
|        - | 2009 | `	sxi32 rc;` |
|        - | 2010 | `	/* Jump the 'interface' keyword */` |
|    70113 | 2011 | `	pGen->pIn++;` |
|        - | 2012 | `	/* Extract interface name */` |
|    70113 | 2013 | `	pName = &pGen->pIn->sData;` |
|        - | 2014 | `	/* Advance the stream cursor */` |
|    70113 | 2015 | `	pGen->pIn++;` |
|        - | 2016 | `	/* Build FQN and obtain a raw class */ {` |
|        - | 2017 | `		SyBlob sFQN;` |
|        - | 2018 | `		SyString sFQNStr;` |
|    70113 | 2019 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    70113 | 2020 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    70113 | 2021 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    70113 | 2022 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    70113 | 2023 | `		SyBlobRelease(&sFQN);` |
|        - | 2024 | `	}` |
|    70113 | 2025 | `	if( pClass == 0 ){` |
|      ! 0 | 2026 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2027 | `		return SXERR_ABORT;` |
|        - | 2028 | `	}` |
|    70113 | 2029 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    70113 | 2030 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 2031 | `		return SXERR_ABORT;` |
|        - | 2032 | `	}` |
|        - | 2033 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|    70113 | 2034 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|        - | 2035 | `	/* Assume no base class is given */` |
|    70113 | 2036 | `	pBase = 0;` |
|    70113 | 2037 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    27233 | 2038 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    27233 | 2039 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a, c, … */ ){` |
|        - | 2040 | `			/* php lets an interface extend SEVERAL parent interfaces. The first` |
|        - | 2041 | `			 * becomes pBase (single-inheritance chain, hDerived); every extra one` |
|        - | 2042 | `			 * is recorded via PH7_ClassImplement so it lands in aInterface — the` |
|        - | 2043 | `			 * runtime subtype walk (VmInterfaceReaches) follows both. */` |
|    27233 | 2044 | `			pGen->pIn++;` |
|    13615 | 2045 | `			for(;;){` |
|        - | 2046 | `				SyBlob sResolved;` |
|        - | 2047 | `				SyString sBaseName;` |
|        - | 2048 | `				sxu32 nRefLine;` |
|        - | 2049 | `				ph7_class *pParent;` |
|    27235 | 2050 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    27235 | 2051 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    27235 | 2052 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 2053 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 2054 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2055 | `						"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|      ! 0 | 2056 | `						pName);` |
|      ! 0 | 2057 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 2058 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2059 | `						return SXERR_ABORT;` |
|        - | 2060 | `					}` |
|      ! 0 | 2061 | `					return SXRET_OK;` |
|        - | 2062 | `				}` |
|    40850 | 2063 | `				pParent = PH7_VmExtractClass(pGen->pVm,` |
|    27230 | 2064 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    27235 | 2065 | `				SyStringInitFromBuf(&sBaseName,` |
|        - | 2066 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 2067 | `				/* Only interfaces is allowed */` |
|    27235 | 2068 | `				while( pParent && (pParent->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 2069 | `					pParent = pParent->pNextName;` |
|      ! 0 | 2070 | `				}` |
|    27235 | 2071 | `				if( pParent == 0 ){` |
|      ! 0 | 2072 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 2073 | `						"Nonexistent base interface '%z'",&sBaseName);` |
|      ! 0 | 2074 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2075 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 2076 | `						return SXERR_ABORT;` |
|      ! 0 | 2077 | `					}` |
|    27235 | 2078 | `				}else if( pBase == 0 ){` |
|        - | 2079 | `					/* First parent → single-inheritance base */` |
|    27233 | 2080 | `					pBase = pParent;` |
|    13619 | 2081 | `				}else{` |
|        - | 2082 | `					/* Additional parent → record it in aInterface (+ copy its` |
|        - | 2083 | `					 * constants/method stubs) so instanceof reaches it too. */` |
|        3 | 2084 | `					PH7_ClassImplement(pClass,pParent);` |
|        - | 2085 | `				}` |
|    27235 | 2086 | `				SyBlobRelease(&sResolved);` |
|        - | 2087 | `				/* Continue on a comma-separated list */` |
|    27235 | 2088 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 | 2089 | `					pGen->pIn++;` |
|        3 | 2090 | `					continue;` |
|        - | 2091 | `				}` |
|    27233 | 2092 | `				break;` |
|      ! 0 | 2093 | `			}` |
|    13614 | 2094 | `		}` |
|    13614 | 2095 | `	}` |
|    70113 | 2096 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - | 2097 | `		/* Syntax error */` |
|      ! 0 | 2098 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|      ! 0 | 2099 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 2100 | `		if( rc == SXERR_ABORT ){` |
|        - | 2101 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 2102 | `			return SXERR_ABORT;` |
|        - | 2103 | `		}` |
|      ! 0 | 2104 | `		return SXRET_OK;` |
|        - | 2105 | `	}` |
|    70113 | 2106 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    70113 | 2107 | `	pEnd = 0; /* cc warning */` |
|        - | 2108 | `	/* Delimit the interface body */` |
|    70113 | 2109 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    70113 | 2110 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 2111 | `		/* Syntax error */` |
|      ! 0 | 2112 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|      ! 0 | 2113 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 2114 | `		if( rc == SXERR_ABORT ){` |
|        - | 2115 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 2116 | `			return SXERR_ABORT;` |
|        - | 2117 | `		}` |
|      ! 0 | 2118 | `		return SXRET_OK;` |
|        - | 2119 | `	}` |
|        - | 2120 | `	/* The delimiter token is the interface body's closing brace */` |
|    70113 | 2121 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 2122 | `	/* Swap token stream */` |
|    70113 | 2123 | `	pTmp = pGen->pEnd;` |
|    70113 | 2124 | `	pGen->pEnd = pEnd;` |
|        - | 2125 | `	/* Start the parse process` |
|        - | 2126 | `	 * Note (According to the PHP reference manual):` |
|        - | 2127 | `	 *  Only constants and function signatures(without body) are allowed.` |
|        - | 2128 | `	 *  Only 'public' visibility is allowed.` |
|        - | 2129 | `	 */` |
|   128399 | 2130 | `	for(;;){` |
|        - | 2131 | `		/* Jump leading/trailing semi-colons */` |
|   443497 | 2132 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   186695 | 2133 | `			pGen->pIn++;` |
|        5 | 2134 | `		}` |
|   256807 | 2135 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 2136 | `			/* End of interface body */` |
|    70109 | 2137 | `			break;` |
|        - | 2138 | `		}` |
|        - | 2139 | `		/* Bind a directly-preceding docblock to this member */` |
|   186703 | 2140 | `		GenStateSetPendingDoc(&(*pGen));` |
|   186703 | 2141 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 2142 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 2143 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|      ! 0 | 2144 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 2145 | `			if( rc == SXERR_ABORT ){` |
|        - | 2146 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 2147 | `				return SXERR_ABORT;` |
|        - | 2148 | `			}` |
|      ! 0 | 2149 | `			goto done;` |
|        - | 2150 | `		}` |
|        - | 2151 | `		/* Extract the current keyword */` |
|   186703 | 2152 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   186703 | 2153 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        - | 2154 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|        - | 2155 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|        3 | 2156 | `			const char *zKind = "member";` |
|        3 | 2157 | `			SyString *pMemberName = 0;` |
|        3 | 2158 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|        3 | 2159 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|        3 | 2160 | `				if( nNext == PH7_TKWRD_CONST ){` |
|        3 | 2161 | `					zKind = "constant";` |
|        3 | 2162 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|        3 | 2163 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|        2 | 2164 | `					}` |
|        1 | 2165 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 2166 | `					zKind = "method";` |
|      ! 0 | 2167 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|      ! 0 | 2168 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|      ! 0 | 2169 | `					}` |
|      ! 0 | 2170 | `				}` |
|        1 | 2171 | `			}` |
|        3 | 2172 | `			if( pMemberName ){` |
|        4 | 2173 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        1 | 2174 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|        2 | 2175 | `			}else{` |
|      ! 0 | 2176 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2177 | `					"Access type for interface %s must be public",zKind);` |
|        - | 2178 | `			}` |
|        3 | 2179 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2180 | `				return SXERR_ABORT;` |
|        - | 2181 | `			}` |
|        3 | 2182 | `			goto done;` |
|        - | 2183 | `		}` |
|   186701 | 2184 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|      ! 0 | 2185 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2186 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 | 2187 | `			if( rc == SXERR_ABORT ){` |
|        - | 2188 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 2189 | `				return SXERR_ABORT;` |
|        - | 2190 | `			}` |
|      ! 0 | 2191 | `			goto done;` |
|        - | 2192 | `		}` |
|   186701 | 2193 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|        - | 2194 | `			/* Advance the stream cursor */` |
|   132251 | 2195 | `			pGen->pIn++;` |
|   132246 | 2196 | `			if( pGen->pIn < pGen->pEnd` |
|   132251 | 2197 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|   132246 | 2198 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|        - | 2199 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|        - | 2200 | `				 * requirement. The attribute compiler + hook parser handle it` |
|        - | 2201 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|        - | 2202 | `				 * property without hooks is ITS "Interfaces may only include` |
|        - | 2203 | `				 * hooked properties" error). */` |
|      ! 0 | 2204 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|      ! 0 | 2205 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|      ! 0 | 2206 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 2207 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2208 | `						return SXERR_ABORT;` |
|        - | 2209 | `					}` |
|      ! 0 | 2210 | `					goto done;` |
|        - | 2211 | `				}` |
|      ! 0 | 2212 | `				continue;` |
|        - | 2213 | `			}` |
|   132251 | 2214 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|        - | 2215 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|        - | 2216 | `				 * '$' also opens a hooked-property requirement. */` |
|      ! 0 | 2217 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|      ! 0 | 2218 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|      ! 0 | 2219 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|      ! 0 | 2220 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|      ! 0 | 2221 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|      ! 0 | 2222 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 2223 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 2224 | `							return SXERR_ABORT;` |
|        - | 2225 | `						}` |
|      ! 0 | 2226 | `						goto done;` |
|        - | 2227 | `					}` |
|      ! 0 | 2228 | `					continue;` |
|        - | 2229 | `				}` |
|      ! 0 | 2230 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2231 | `					"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 | 2232 | `				if( rc == SXERR_ABORT ){` |
|        - | 2233 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 | 2234 | `					return SXERR_ABORT;` |
|        - | 2235 | `				}` |
|      ! 0 | 2236 | `				goto done;` |
|        - | 2237 | `			}` |
|   132251 | 2238 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   132251 | 2239 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|        - | 2240 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|        - | 2241 | `				 * hooked-property requirement (PHP 8.4). */` |
|        4 | 2242 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|        5 | 2243 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|        7 | 2244 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|        2 | 2245 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|        5 | 2246 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 2247 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 2248 | `							return SXERR_ABORT;` |
|        - | 2249 | `						}` |
|      ! 0 | 2250 | `						goto done;` |
|        - | 2251 | `					}` |
|        5 | 2252 | `					continue;` |
|        - | 2253 | `				}` |
|      ! 0 | 2254 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2255 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 | 2256 | `				if( rc == SXERR_ABORT ){` |
|        - | 2257 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 | 2258 | `					return SXERR_ABORT;` |
|        - | 2259 | `				}` |
|      ! 0 | 2260 | `				goto done;` |
|        - | 2261 | `			}` |
|    66121 | 2262 | `		}` |
|   186697 | 2263 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|        - | 2264 | `			/* Parse constant */` |
|    54451 | 2265 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|    54451 | 2266 | `			if( rc != SXRET_OK ){` |
|        3 | 2267 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2268 | `					return SXERR_ABORT;` |
|        - | 2269 | `				}` |
|        3 | 2270 | `				goto done;` |
|        - | 2271 | `			}` |
|    27227 | 2272 | `		}else{` |
|   132251 | 2273 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   132251 | 2274 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - | 2275 | `				/* Static method,record that */` |
|    11669 | 2276 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|        - | 2277 | `				/* Advance the stream cursor */` |
|    11669 | 2278 | `				pGen->pIn++;` |
|    11664 | 2279 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    11669 | 2280 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 2281 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2282 | `							"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 | 2283 | `						if( rc == SXERR_ABORT ){` |
|        - | 2284 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 2285 | `							return SXERR_ABORT;` |
|        - | 2286 | `						}` |
|      ! 0 | 2287 | `						goto done;` |
|        - | 2288 | `				}` |
|     5832 | 2289 | `			}` |
|        - | 2290 | `			/* Process method signature (no body for interface methods) */` |
|   132251 | 2291 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   132251 | 2292 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2293 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2294 | `					return SXERR_ABORT;` |
|        - | 2295 | `				}` |
|      ! 0 | 2296 | `				goto done;` |
|        - | 2297 | `			}` |
|        - | 2298 | `		}` |
|        5 | 2299 | `	}` |
|        - | 2300 | `	/* Reject a php-fatal redeclaration before hoisting the interface */` |
|    70109 | 2301 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|        3 | 2302 | `		return SXERR_ABORT;` |
|        - | 2303 | `	}` |
|        - | 2304 | `	/* Install the interface */` |
|    70107 | 2305 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    70107 | 2306 | `	if( rc == SXRET_OK && pBase ){` |
|        - | 2307 | `		/* Inherit from the base interface */` |
|    27233 | 2308 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    13614 | 2309 | `	}` |
|    70107 | 2310 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2311 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2312 | `		return SXERR_ABORT;` |
|        - | 2313 | `	}` |
|    35051 | 2314 | `done:` |
|        - | 2315 | `	/* Point beyond the interface body */` |
|    70111 | 2316 | `	pGen->pIn  = &pEnd[1];` |
|    70111 | 2317 | `	pGen->pEnd = pTmp;` |
|    70111 | 2318 | `	return PH7_OK;` |
|    35059 | 2319 | `}` |
|        - | 2320 | `/*` |
|        - | 2321 | ` * Compile a user-defined class.` |
|        - | 2322 | ` * According to the PHP language reference manual` |
|        - | 2323 | ` *  class` |
|        - | 2324 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|        - | 2325 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|        - | 2326 | ` *  of the properties and methods belonging to the class.` |
|        - | 2327 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|        - | 2328 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|        - | 2329 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|        - | 2330 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - | 2331 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|        - | 2332 | ` *  (called "methods").` |
|        - | 2333 | ` */` |
|        - | 2334 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|        - | 2335 | `typedef struct TraitUseEntry TraitUseEntry;` |
|        - | 2336 | `struct TraitUseEntry {` |
|        - | 2337 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|        - | 2338 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|        - | 2339 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|        - | 2340 | `};` |
|        - | 2341 | `/*` |
|        - | 2342 | ` * Validate that methods implementing interface contracts have compatible` |
|        - | 2343 | ` * signatures: public visibility and at least as many parameters as declared.` |
|        - | 2344 | ` */` |
|   452954 | 2345 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 2346 | `{` |
|        - | 2347 | `	ph7_class **apIface;` |
|        - | 2348 | `	sxu32 nIface,i;` |
|        - | 2349 | `	sxi32 rc;` |
|   452959 | 2350 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      ! 0 | 2351 | `		return SXRET_OK;` |
|        - | 2352 | `	}` |
|   452959 | 2353 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   452959 | 2354 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   877105 | 2355 | `	for(i = 0; i < nIface; i++){` |
|   424151 | 2356 | `		ph7_class *pIface = apIface[i];` |
|        - | 2357 | `		SyHashEntry *pEntry;` |
|   424151 | 2358 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  1249013 | 2359 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   824867 | 2360 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - | 2361 | `			ph7_class_method *pImplMeth;` |
|   824867 | 2362 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|        - | 2363 | `			/* Find the implementing method in the class */` |
|   824867 | 2364 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   824867 | 2365 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       23 | 2366 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|        - | 2367 | `			}` |
|        - | 2368 | `			/* Check visibility: interface methods must be implemented as public */` |
|   824849 | 2369 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        4 | 2370 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - | 2371 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|        1 | 2372 | `					&pClass->sName,pMName,&pIface->sName);` |
|        3 | 2373 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2374 | `					return SXERR_ABORT;` |
|        - | 2375 | `				}` |
|        1 | 2376 | `			}` |
|        - | 2377 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|        - | 2378 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|        - | 2379 | `			 */` |
|        - | 2380 | `			{` |
|   824849 | 2381 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   824849 | 2382 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   824849 | 2383 | `				int sigError = 0;` |
|   824849 | 2384 | `				if( nImplArgs < nIfaceArgs ){` |
|        3 | 2385 | `					sigError = 1;` |
|   824848 | 2386 | `				}else if( nImplArgs > nIfaceArgs ){` |
|        - | 2387 | `					/* Extra parameters must all have default values */` |
|     3897 | 2388 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        - | 2389 | `					sxu32 k;` |
|     7787 | 2390 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|     3897 | 2391 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|        3 | 2392 | `							sigError = 1;` |
|        3 | 2393 | `							break;` |
|        - | 2394 | `						}` |
|     1950 | 2395 | `					}` |
|     1946 | 2396 | `				}` |
|   824849 | 2397 | `				if( sigError ){` |
|        - | 2398 | `					SyBlob sImplSig, sIfaceSig;` |
|        - | 2399 | `					ph7_vm_func_arg *aArgs;` |
|        - | 2400 | `					sxu32 j;` |
|        6 | 2401 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|        6 | 2402 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|        - | 2403 | `					/* Build implementing method signature */` |
|        6 | 2404 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       12 | 2405 | `					for(j = 0; j < nImplArgs; j++){` |
|        8 | 2406 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|        8 | 2407 | `						SyBlobAppend(&sImplSig,"$",1);` |
|        8 | 2408 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 | 2409 | `					}` |
|        - | 2410 | `					/* Build interface method signature */` |
|        6 | 2411 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|       12 | 2412 | `					for(j = 0; j < nIfaceArgs; j++){` |
|        8 | 2413 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|        8 | 2414 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|        8 | 2415 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 | 2416 | `					}` |
|        8 | 2417 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - | 2418 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|        2 | 2419 | `						&pClass->sName,pMName,` |
|        4 | 2420 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|        2 | 2421 | `						&pIface->sName,pMName,` |
|        4 | 2422 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|        6 | 2423 | `					SyBlobRelease(&sImplSig);` |
|        6 | 2424 | `					SyBlobRelease(&sIfaceSig);` |
|        6 | 2425 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2426 | `						return SXERR_ABORT;` |
|        - | 2427 | `					}` |
|        2 | 2428 | `				}` |
|        - | 2429 | `			}` |
|        5 | 2430 | `		}` |
|   212078 | 2431 | `	}` |
|   452959 | 2432 | `	return SXRET_OK;` |
|   226482 | 2433 | `}` |
|        - | 2434 | `/*` |
|        - | 2435 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|        - | 2436 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|        - | 2437 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|        - | 2438 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|        - | 2439 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|        - | 2440 | ` * means that specific hook is still missing.` |
|        - | 2441 | ` */` |
|       38 | 2442 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|        5 | 2443 | `{` |
|        - | 2444 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        - | 2445 | `	ph7_class_attr *pProp;` |
|       38 | 2446 | `	if( pMName->nByte <= nPfx` |
|       27 | 2447 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|        4 | 2448 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|       36 | 2449 | `		return 0; /* not a hook stub */` |
|        - | 2450 | `	}` |
|        7 | 2451 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|        7 | 2452 | `	return pProp != 0` |
|        6 | 2453 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|        3 | 2454 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|       24 | 2455 | `}` |
|        - | 2456 | `/*` |
|        - | 2457 | ` * Append an abstract member's display name to the message blob, translating a` |
|        - | 2458 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|        - | 2459 | ` */` |
|       16 | 2460 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|        4 | 2461 | `{` |
|        - | 2462 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|       16 | 2463 | `	if( pMName->nByte > nPfx` |
|       12 | 2464 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|      ! 0 | 2465 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|      ! 0 | 2466 | `		SyBlobAppend(pMsg,"$",1);` |
|      ! 0 | 2467 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|      ! 0 | 2468 | `		SyBlobAppend(pMsg,"::",2);` |
|      ! 0 | 2469 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|      ! 0 | 2470 | `		return;` |
|        - | 2471 | `	}` |
|       20 | 2472 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|       12 | 2473 | `}` |
|        - | 2474 | `/*` |
|        - | 2475 | ` * Check that a concrete class has no remaining abstract methods.` |
|        - | 2476 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|        - | 2477 | ` */` |
|   452954 | 2478 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 2479 | `{` |
|        - | 2480 | `	ph7_class_method *pMeth;` |
|        - | 2481 | `	SyHashEntry *pEntry;` |
|        - | 2482 | `	sxu32 nAbstract;` |
|        - | 2483 | `	SyBlob sMsg;` |
|        - | 2484 | `	sxi32 rc;` |
|        - | 2485 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   452959 | 2486 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|    19497 | 2487 | `		return SXRET_OK;` |
|        - | 2488 | `	}` |
|        - | 2489 | `	/* Count abstract methods */` |
|   433467 | 2490 | `	nAbstract = 0;` |
|   433467 | 2491 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  6477258 | 2492 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  5827065 | 2493 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  5827065 | 2494 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       27 | 2495 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|        7 | 2496 | `				continue; /* hook requirement met by a plain property (php) */` |
|        - | 2497 | `			}` |
|       20 | 2498 | `			nAbstract++;` |
|        8 | 2499 | `		}` |
|        5 | 2500 | `	}` |
|   433467 | 2501 | `	if( nAbstract == 0 ){` |
|   433453 | 2502 | `		return SXRET_OK;` |
|        - | 2503 | `	}` |
|        - | 2504 | `	/* Build the error message listing all abstract methods with origins */` |
|       18 | 2505 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       18 | 2506 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|        - | 2507 | `		"be declared abstract or implement the remaining method%s (",` |
|        7 | 2508 | `		&pClass->sName,nAbstract,` |
|        7 | 2509 | `		(nAbstract > 1 ? "s" : ""),` |
|        7 | 2510 | `		(nAbstract > 1 ? "s" : ""));` |
|        - | 2511 | `	/* Second pass: list methods with origins */` |
|        - | 2512 | `	{` |
|       18 | 2513 | `		sxu32 nListed = 0;` |
|       18 | 2514 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|       36 | 2515 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|       22 | 2516 | `			ph7_class *pOrigin = 0;` |
|        - | 2517 | `			SyString *pMName;` |
|       22 | 2518 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|       22 | 2519 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        3 | 2520 | `				continue;` |
|        - | 2521 | `			}` |
|       20 | 2522 | `			pMName = &pMeth->sFunc.sName;` |
|       20 | 2523 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|      ! 0 | 2524 | `				continue; /* hook requirement met by a plain property (php) */` |
|        - | 2525 | `			}` |
|       20 | 2526 | `			if( nListed > 0 ){` |
|        3 | 2527 | `				SyBlobAppend(&sMsg,", ",2);` |
|        1 | 2528 | `			}` |
|        - | 2529 | `			/* Find the origin of this abstract method.` |
|        - | 2530 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|        - | 2531 | `			 * inheritance chains) take precedence for interface-declared` |
|        - | 2532 | `			 * methods. Abstract class methods only win when the class` |
|        - | 2533 | `			 * itself declared the abstract method (not inherited from` |
|        - | 2534 | `			 * an interface). Trait methods are adopted into the using` |
|        - | 2535 | `			 * class's namespace.` |
|        - | 2536 | `			 */` |
|        - | 2537 | `			{` |
|        - | 2538 | `				ph7_class **apIface;` |
|        - | 2539 | `				ph7_class **apTrait;` |
|        - | 2540 | `				ph7_class *pWalk;` |
|        - | 2541 | `				sxu32 i;` |
|        - | 2542 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|        - | 2543 | `				 * (one that was written in the class body, not inherited from an` |
|        - | 2544 | `				 * interface). PHP attributes origin to the declaring class.` |
|        - | 2545 | `				 */` |
|       20 | 2546 | `				if( pClass->pBase ){` |
|       10 | 2547 | `					pWalk = pClass->pBase;` |
|       18 | 2548 | `					while( pWalk ){` |
|        - | 2549 | `						ph7_class_method *pParentMeth;` |
|       12 | 2550 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|       12 | 2551 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        - | 2552 | `							/* Exclude methods that came from an interface anywhere` |
|        - | 2553 | `							 * in this class's ancestor chain.` |
|        - | 2554 | `							 */` |
|       12 | 2555 | `							int fromIface = 0;` |
|       12 | 2556 | `							ph7_class *pAnc = pWalk;` |
|       16 | 2557 | `							while( pAnc ){` |
|        - | 2558 | `								ph7_class **apPI;` |
|        - | 2559 | `								sxu32 j;` |
|       14 | 2560 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|       14 | 2561 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       10 | 2562 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       10 | 2563 | `										fromIface = 1;` |
|       10 | 2564 | `										break;` |
|        - | 2565 | `									}` |
|      ! 0 | 2566 | `								}` |
|       14 | 2567 | `								if( fromIface ) break;` |
|        5 | 2568 | `								pAnc = pAnc->pBase;` |
|        1 | 2569 | `							}` |
|       12 | 2570 | `							if( !fromIface ){` |
|        3 | 2571 | `								pOrigin = pWalk;` |
|        3 | 2572 | `								break;` |
|        - | 2573 | `							}` |
|        4 | 2574 | `						}` |
|       10 | 2575 | `						pWalk = pWalk->pBase;` |
|        2 | 2576 | `					}` |
|        4 | 2577 | `				}` |
|        - | 2578 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|        - | 2579 | `				 * each interface's own parent chain for the deepest origin.` |
|        - | 2580 | `				 */` |
|       20 | 2581 | `				if( !pOrigin ){` |
|       18 | 2582 | `					pWalk = pClass;` |
|       40 | 2583 | `					while( pWalk && !pOrigin ){` |
|       26 | 2584 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|       26 | 2585 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|       16 | 2586 | `							ph7_class *pIface = apIface[i];` |
|       16 | 2587 | `							ph7_class *pDeepest = 0;` |
|       28 | 2588 | `							while( pIface ){` |
|       16 | 2589 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|       16 | 2590 | `									pDeepest = pIface;` |
|        6 | 2591 | `								}` |
|       16 | 2592 | `								pIface = pIface->pBase;` |
|        4 | 2593 | `							}` |
|       16 | 2594 | `							if( pDeepest ){` |
|       16 | 2595 | `								pOrigin = pDeepest;` |
|       16 | 2596 | `								break;` |
|        - | 2597 | `							}` |
|      ! 0 | 2598 | `						}` |
|       26 | 2599 | `						pWalk = pWalk->pBase;` |
|        4 | 2600 | `					}` |
|        7 | 2601 | `				}` |
|        - | 2602 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|       20 | 2603 | `				if( !pOrigin ){` |
|        3 | 2604 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        3 | 2605 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|        3 | 2606 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|        3 | 2607 | `							pOrigin = pClass;` |
|        3 | 2608 | `							break;` |
|        - | 2609 | `						}` |
|      ! 0 | 2610 | `					}` |
|        1 | 2611 | `				}` |
|        - | 2612 | `			}` |
|       20 | 2613 | `			if( pOrigin ){` |
|       20 | 2614 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|       12 | 2615 | `			}else{` |
|        - | 2616 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|      ! 0 | 2617 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|        - | 2618 | `			}` |
|       20 | 2619 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|       20 | 2620 | `			nListed++;` |
|        4 | 2621 | `		}` |
|        - | 2622 | `	}` |
|       18 | 2623 | `	SyBlobAppend(&sMsg,")",1);` |
|       25 | 2624 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|       14 | 2625 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|       18 | 2626 | `	SyBlobRelease(&sMsg);` |
|       18 | 2627 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2628 | `		return SXERR_ABORT;` |
|        - | 2629 | `	}` |
|       18 | 2630 | `	return SXRET_OK;` |
|   226482 | 2631 | `}` |
|        - | 2632 | `/*` |
|        - | 2633 | ` * Parse a class/interface name reference from the current token stream.` |
|        - | 2634 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|        - | 2635 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|        - | 2636 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|        - | 2637 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|        - | 2638 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|        - | 2639 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|        - | 2640 | ` */` |
|   515574 | 2641 | `PH7_PRIVATE sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|        5 | 2642 | `{` |
|   515579 | 2643 | `	int isAbsolute = 0;` |
|   515579 | 2644 | `	SyToken *pStart = pGen->pIn;` |
|        - | 2645 | `	SyBlob sName;` |
|   515579 | 2646 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|     4543 | 2647 | `		isAbsolute = 1;` |
|     4543 | 2648 | `		pGen->pIn++;` |
|     2269 | 2649 | `	}` |
|   515579 | 2650 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        9 | 2651 | `		pGen->pIn = pStart;` |
|        9 | 2652 | `		return SXERR_INVALID;` |
|        - | 2653 | `	}` |
|   515573 | 2654 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   515573 | 2655 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   515573 | 2656 | `	pGen->pIn++;` |
|   773391 | 2657 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   257828 | 2658 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       28 | 2659 | `		SyBlobAppend(&sName,"\\",1);` |
|       28 | 2660 | `		pGen->pIn++;` |
|       28 | 2661 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       28 | 2662 | `		pGen->pIn++;` |
|        2 | 2663 | `	}` |
|   515573 | 2664 | `	if( isAbsolute ){` |
|     4541 | 2665 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     2273 | 2666 | `	}else{` |
|        - | 2667 | `		SyString sRaw;` |
|   511037 | 2668 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   511037 | 2669 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|        - | 2670 | `	}` |
|   515573 | 2671 | `	SyBlobRelease(&sName);` |
|   515573 | 2672 | `	return SXRET_OK;` |
|   257792 | 2673 | `}` |
|        - | 2674 | `/*` |
|        - | 2675 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|        - | 2676 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|        - | 2677 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|        - | 2678 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|        - | 2679 | ` * either direction cannot run unbounded.` |
|        - | 2680 | ` */` |
|        - | 2681 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|   198500 | 2682 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|        5 | 2683 | `{` |
|        - | 2684 | `	ph7_class **apParent;` |
|        - | 2685 | `	sxu32 n;` |
|   525349 | 2686 | `	while( pInterface ){` |
|   334635 | 2687 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|      ! 0 | 2688 | `			return FALSE;` |
|        - | 2689 | `		}` |
|   373535 | 2690 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    77800 | 2691 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|     7791 | 2692 | `			return TRUE;` |
|        - | 2693 | `		}` |
|   326849 | 2694 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|   326851 | 2695 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|        3 | 2696 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|      ! 0 | 2697 | `				return TRUE;` |
|        - | 2698 | `			}` |
|        2 | 2699 | `		}` |
|   326849 | 2700 | `		pInterface = pInterface->pBase;` |
|   326849 | 2701 | `		iDepth++;` |
|        5 | 2702 | `	}` |
|   190719 | 2703 | `	return FALSE;` |
|    99255 | 2704 | `}` |
|   198498 | 2705 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|        5 | 2706 | `{` |
|   198503 | 2707 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|        5 | 2708 | `}` |
|        - | 2709 | `/*` |
|        - | 2710 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|        - | 2711 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|        - | 2712 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|        - | 2713 | ` */` |
|     7786 | 2714 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|        5 | 2715 | `{` |
|     7795 | 2716 | `	while( pBase ){` |
|       10 | 2717 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|        2 | 2718 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|        3 | 2719 | `			return TRUE;` |
|        - | 2720 | `		}` |
|       10 | 2721 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|        6 | 2722 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|        3 | 2723 | `			return TRUE;` |
|        - | 2724 | `		}` |
|        5 | 2725 | `		pBase = pBase->pBase;` |
|        1 | 2726 | `	}` |
|     7787 | 2727 | `	return FALSE;` |
|     3898 | 2728 | `}` |
|        - | 2729 | `/*` |
|        - | 2730 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|        - | 2731 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|        - | 2732 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|        - | 2733 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|        - | 2734 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|        - | 2735 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|        - | 2736 | ` * pClass->aEnumCases for cases().` |
|        - | 2737 | ` */` |
|     7830 | 2738 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 2739 | `{` |
|     7835 | 2740 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2741 | `	SySet *pInstrContainer;` |
|        - | 2742 | `	ph7_class_attr *pCase;` |
|        - | 2743 | `	SyString *pName;` |
|        - | 2744 | `	sxi32 rc;` |
|     7835 | 2745 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|     7835 | 2746 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2747 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 2748 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|      ! 0 | 2749 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2750 | `			return SXERR_ABORT;` |
|        - | 2751 | `		}` |
|      ! 0 | 2752 | `		goto Synchronize;` |
|        - | 2753 | `	}` |
|     7835 | 2754 | `	pName = &pGen->pIn->sData;` |
|        - | 2755 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|     7835 | 2756 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      ! 0 | 2757 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2758 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|      ! 0 | 2759 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2760 | `			return SXERR_ABORT;` |
|        - | 2761 | `		}` |
|      ! 0 | 2762 | `		goto Synchronize;` |
|        - | 2763 | `	}` |
|     7835 | 2764 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 2765 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|     7835 | 2766 | `	if( pCase == 0 ){` |
|      ! 0 | 2767 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2768 | `		return SXERR_ABORT;` |
|        - | 2769 | `	}` |
|     7835 | 2770 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|     7835 | 2771 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 2772 | `		return SXERR_ABORT;` |
|        - | 2773 | `	}` |
|     7835 | 2774 | `	pGen->pIn++; /* Jump the case name */` |
|     7835 | 2775 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|     7821 | 2776 | `		if( pClass->nEnumBacking == 0 ){` |
|        8 | 2777 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        2 | 2778 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|        6 | 2779 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2780 | `				return SXERR_ABORT;` |
|        - | 2781 | `			}` |
|        6 | 2782 | `			goto Synchronize;` |
|        - | 2783 | `		}` |
|     7817 | 2784 | `		pGen->pIn++; /* Jump the equal sign */` |
|        - | 2785 | `		/* Compile the backing value expression into the case's own container` |
|        - | 2786 | `		 * (same technique as class constants). */` |
|     7817 | 2787 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     7817 | 2788 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|     7817 | 2789 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     7817 | 2790 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2791 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 2792 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|      ! 0 | 2793 | `		}` |
|     7817 | 2794 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|     7817 | 2795 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     7817 | 2796 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2797 | `			return SXERR_ABORT;` |
|        - | 2798 | `		}` |
|     3911 | 2799 | `	}else{` |
|       17 | 2800 | `		if( pClass->nEnumBacking != 0 ){` |
|      ! 0 | 2801 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 2802 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|      ! 0 | 2803 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2804 | `				return SXERR_ABORT;` |
|        - | 2805 | `			}` |
|      ! 0 | 2806 | `			goto Synchronize;` |
|        - | 2807 | `		}` |
|        - | 2808 | `	}` |
|     7831 | 2809 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|     7831 | 2810 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2811 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2812 | `		return SXERR_ABORT;` |
|        - | 2813 | `	}` |
|     7831 | 2814 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|     7831 | 2815 | `	return SXRET_OK;` |
|        2 | 2816 | `Synchronize:` |
|        - | 2817 | `	/* Synchronize with the first semi-colon */` |
|       14 | 2818 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       10 | 2819 | `		pGen->pIn++;` |
|        2 | 2820 | `	}` |
|        6 | 2821 | `	return SXERR_CORRUPT;` |
|     3920 | 2822 | `}` |
|        - | 2823 | `/*` |
|        - | 2824 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|        - | 2825 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|        - | 2826 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|        - | 2827 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|        - | 2828 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|        - | 2829 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|        - | 2830 | ` * pointers into it (see the constructor-promotion precedent above).` |
|        - | 2831 | ` */` |
|     3920 | 2832 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 2833 | `{` |
|        - | 2834 | `	SyToken *pSaveIn,*pSaveEnd;` |
|        - | 2835 | `	const char *zBack;` |
|        - | 2836 | `	SySet sToken;` |
|        - | 2837 | `	char *zSrc;` |
|        - | 2838 | `	sxu32 nSrc,nMax;` |
|     3925 | 2839 | `	sxi32 rc = SXRET_OK;` |
|     3925 | 2840 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|     3920 | 2841 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|     3925 | 2842 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|     3925 | 2843 | `	if( zSrc == 0 ){` |
|      ! 0 | 2844 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2845 | `		return SXERR_ABORT;` |
|        - | 2846 | `	}` |
|     3925 | 2847 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|     3925 | 2848 | `	if( pClass->nEnumBacking != 0 ){` |
|     5858 | 2849 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        - | 2850 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|        - | 2851 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|        - | 2852 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|     1951 | 2853 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|     1956 | 2854 | `	}else{` |
|       30 | 2855 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        9 | 2856 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|        - | 2857 | `	}` |
|     3925 | 2858 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|     3925 | 2859 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|     3925 | 2860 | `	pSaveIn = pGen->pIn;` |
|     3925 | 2861 | `	pSaveEnd = pGen->pEnd;` |
|     3925 | 2862 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|     3925 | 2863 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|    15649 | 2864 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|    11729 | 2865 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|        5 | 2866 | `	}` |
|     3925 | 2867 | `	pGen->pIn = pSaveIn;` |
|     3925 | 2868 | `	pGen->pEnd = pSaveEnd;` |
|     3925 | 2869 | `	SySetRelease(&sToken);` |
|     3925 | 2870 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|     1965 | 2871 | `}` |
|        - | 2872 | `/*` |
|        - | 2873 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|        - | 2874 | ` * __call/__callStatic/__invoke stay allowed).` |
|        - | 2875 | ` */` |
|        - | 2876 | `static const char *azEnumBannedMagic[] = {` |
|        - | 2877 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|        - | 2878 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|        - | 2879 | `};` |
|        - | 2880 | `/*` |
|        - | 2881 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|        - | 2882 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|        - | 2883 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|        - | 2884 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|        - | 2885 | ` * and before the class is installed.` |
|        - | 2886 | ` */` |
|     3920 | 2887 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|        5 | 2888 | `{` |
|        - | 2889 | `	SyHashEntry *pEntry;` |
|        - | 2890 | `	sxi32 rc;` |
|        - | 2891 | `	sxu32 n;` |
|        - | 2892 | `	/* php: "Enum %s cannot include properties" */` |
|     3925 | 2893 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    11757 | 2894 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     7839 | 2895 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     7839 | 2896 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        3 | 2897 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|        1 | 2898 | `				"Enum %z cannot include properties",&pClass->sName);` |
|        3 | 2899 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2900 | `				return SXERR_ABORT;` |
|        - | 2901 | `			}` |
|        3 | 2902 | `			break;` |
|        - | 2903 | `		}` |
|        5 | 2904 | `	}` |
|        - | 2905 | `	/* php: "Enum %s cannot include magic method %s" */` |
|    54885 | 2906 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|    76440 | 2907 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|    50965 | 2908 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|      ! 0 | 2909 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 2910 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|      ! 0 | 2911 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2912 | `				return SXERR_ABORT;` |
|        - | 2913 | `			}` |
|      ! 0 | 2914 | `		}` |
|    25485 | 2915 | `	}` |
|        - | 2916 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|        - | 2917 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|        - | 2918 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|        - | 2919 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|        - | 2920 | `	{` |
|        - | 2921 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|        - | 2922 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|        - | 2923 | `		ph7_class_attr *pAttr;` |
|     3925 | 2924 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 2925 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|     3925 | 2926 | `		if( pAttr == 0 ){` |
|      ! 0 | 2927 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2928 | `			return SXERR_ABORT;` |
|        - | 2929 | `		}` |
|     3925 | 2930 | `		pAttr->nType = MEMOBJ_STRING;` |
|     3925 | 2931 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|     3925 | 2932 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|     3925 | 2933 | `		if( pClass->nEnumBacking != 0 ){` |
|     3907 | 2934 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 2935 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|     3907 | 2936 | `			if( pAttr == 0 ){` |
|      ! 0 | 2937 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2938 | `				return SXERR_ABORT;` |
|        - | 2939 | `			}` |
|     3907 | 2940 | `			pAttr->nType = pClass->nEnumBacking;` |
|     3907 | 2941 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        7 | 2942 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|        4 | 2943 | `			}else{` |
|     3901 | 2944 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|        - | 2945 | `			}` |
|     3907 | 2946 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|     1951 | 2947 | `		}` |
|        - | 2948 | `	}` |
|     3925 | 2949 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|     1965 | 2950 | `}` |
|        - | 2951 | `/*` |
|        - | 2952 | ` * Compile a class declaration, named or anonymous.` |
|        - | 2953 | ` *` |
|        - | 2954 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|        - | 2955 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|        - | 2956 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|        - | 2957 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|        - | 2958 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|        - | 2959 | ` * implements, body, install) is shared by both paths.` |
|        - | 2960 | ` */` |
|   453004 | 2961 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|        - | 2962 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|        5 | 2963 | `{` |
|   453009 | 2964 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2965 | `	ph7_class *pClass,*pBase;` |
|        - | 2966 | `	SyToken *pEnd,*pTmp;` |
|        - | 2967 | `	sxi32 iProtection;` |
|        - | 2968 | `	SySet aInterfaces;` |
|        - | 2969 | `	SySet aUseEntries;` |
|        - | 2970 | `	sxi32 iAttrflags;` |
|        - | 2971 | `	SyString *pName;` |
|        - | 2972 | `	sxi32 nKwrd;` |
|        - | 2973 | `	sxi32 rc;` |
|        - | 2974 | `	/* Jump the 'class' keyword */` |
|   453009 | 2975 | `	pGen->pIn++;` |
|   453009 | 2976 | `	if( pAnonName ){` |
|        - | 2977 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|        - | 2978 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|        - | 2979 | `		 * then use the synthesized name. */` |
|       34 | 2980 | `		*ppArgStart = *ppArgEnd = 0;` |
|       34 | 2981 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        7 | 2982 | `			pGen->pIn++; /* Jump '(' */` |
|        7 | 2983 | `			*ppArgStart = pGen->pIn;` |
|       10 | 2984 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|        3 | 2985 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|        7 | 2986 | `			pGen->pIn = *ppArgEnd;` |
|        7 | 2987 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|        3 | 2988 | `		}` |
|       34 | 2989 | `		pName = pAnonName;` |
|       34 | 2990 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|       19 | 2991 | `	}else{` |
|   452979 | 2992 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - | 2993 | `			/* Syntax error */` |
|      ! 0 | 2994 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|      ! 0 | 2995 | `			if( rc == SXERR_ABORT ){` |
|        - | 2996 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 2997 | `				return SXERR_ABORT;` |
|        - | 2998 | `			}` |
|        - | 2999 | `			/* Synchronize with the first semi-colon or curly braces */` |
|      ! 0 | 3000 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|      ! 0 | 3001 | `				pGen->pIn++;` |
|      ! 0 | 3002 | `			}` |
|      ! 0 | 3003 | `			return SXRET_OK;` |
|        - | 3004 | `		}` |
|        - | 3005 | `		/* Extract class name */` |
|   452979 | 3006 | `		pName = &pGen->pIn->sData;` |
|        - | 3007 | `		/* Advance the stream cursor */` |
|   452979 | 3008 | `		pGen->pIn++;` |
|        - | 3009 | `		/* Build FQN and obtain a raw class */ {` |
|        - | 3010 | `			SyBlob sFQN;` |
|        - | 3011 | `			SyString sFQNStr;` |
|   452979 | 3012 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   452979 | 3013 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|   452979 | 3014 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   452979 | 3015 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   452979 | 3016 | `			SyBlobRelease(&sFQN);` |
|        - | 3017 | `		}` |
|        - | 3018 | `	}` |
|   453009 | 3019 | `	if( pClass == 0 ){` |
|      ! 0 | 3020 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3021 | `		return SXERR_ABORT;` |
|        - | 3022 | `	}` |
|   453004 | 3023 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|     3929 | 3024 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|        - | 3025 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|     3909 | 3026 | `		pGen->pIn++; /* Jump ':' */` |
|     3904 | 3027 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     3909 | 3028 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|        7 | 3029 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|        7 | 3030 | `			pGen->pIn++;` |
|     3902 | 3031 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     3903 | 3032 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|     3901 | 3033 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|     3901 | 3034 | `			pGen->pIn++;` |
|     1953 | 3035 | `		}else{` |
|        3 | 3036 | `			SyToken *pTok = pGen->pIn;` |
|        3 | 3037 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|        4 | 3038 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|        1 | 3039 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|        3 | 3040 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3041 | `				return SXERR_ABORT;` |
|        - | 3042 | `			}` |
|        3 | 3043 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|        3 | 3044 | `				pGen->pIn++; /* Skip the bogus type token */` |
|        1 | 3045 | `			}` |
|        - | 3046 | `		}` |
|     1952 | 3047 | `	}` |
|   453009 | 3048 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|   453009 | 3049 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 3050 | `		return SXERR_ABORT;` |
|        - | 3051 | `	}` |
|        - | 3052 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   453009 | 3053 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   453009 | 3054 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|        - | 3055 | `	/* Assume a standalone class */` |
|   453009 | 3056 | `	pBase = 0;` |
|   453009 | 3057 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   369857 | 3058 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   369857 | 3059 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|        - | 3060 | `			SyBlob sResolved;` |
|        - | 3061 | `			SyString sBaseName;` |
|        - | 3062 | `			sxu32 nRefLine;` |
|   249149 | 3063 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|        - | 3064 | `				/* php parse-fatals here (enums have no inheritance) */` |
|      ! 0 | 3065 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 3066 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|      ! 0 | 3067 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3068 | `					return SXERR_ABORT;` |
|        - | 3069 | `				}` |
|      ! 0 | 3070 | `			}` |
|   249149 | 3071 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   249149 | 3072 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   249149 | 3073 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   249149 | 3074 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        3 | 3075 | `				SyBlobRelease(&sResolved);` |
|        4 | 3076 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 3077 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|        1 | 3078 | `					pName);` |
|        3 | 3079 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|        3 | 3080 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3081 | `					return SXERR_ABORT;` |
|        - | 3082 | `				}` |
|        3 | 3083 | `				return SXRET_OK;` |
|        - | 3084 | `			}` |
|   373718 | 3085 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   249142 | 3086 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   249147 | 3087 | `			SyStringInitFromBuf(&sBaseName,` |
|        - | 3088 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 3089 | `			/* Interfaces are not allowed */` |
|   249147 | 3090 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|      ! 0 | 3091 | `				pBase = pBase->pNextName;` |
|      ! 0 | 3092 | `			}` |
|   249147 | 3093 | `			if( pBase == 0 ){` |
|      ! 0 | 3094 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 3095 | `					"Nonexistent base class '%z'",&sBaseName);` |
|      ! 0 | 3096 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3097 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 3098 | `					return SXERR_ABORT;` |
|        - | 3099 | `				}` |
|      ! 0 | 3100 | `			}else{` |
|   249147 | 3101 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|        4 | 3102 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 3103 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|        3 | 3104 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3105 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 3106 | `						return SXERR_ABORT;` |
|        - | 3107 | `					}` |
|        3 | 3108 | `					pBase = 0; /* Never inherit from an enum */` |
|   249146 | 3109 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|      ! 0 | 3110 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 3111 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|      ! 0 | 3112 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3113 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 3114 | `						return SXERR_ABORT;` |
|        - | 3115 | `					}` |
|      ! 0 | 3116 | `				}` |
|        - | 3117 | `			}` |
|   249147 | 3118 | `			SyBlobRelease(&sResolved);` |
|   249147 | 3119 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 | 3120 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|      ! 0 | 3121 | `			}` |
|   124571 | 3122 | `		}` |
|   369855 | 3123 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|        - | 3124 | `			ph7_class *pInterface;` |
|        - | 3125 | `			/* Interface implementation */` |
|   136281 | 3126 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   130360 | 3127 | `			for(;;){` |
|        - | 3128 | `				SyBlob sResolved;` |
|        - | 3129 | `				SyString sIntName;` |
|        - | 3130 | `				sxu32 nRefLine;` |
|   198503 | 3131 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   198503 | 3132 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   198503 | 3133 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 3134 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 3135 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 3136 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|      ! 0 | 3137 | `						pName);` |
|      ! 0 | 3138 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3139 | `						return SXERR_ABORT;` |
|        - | 3140 | `					}` |
|      ! 0 | 3141 | `					break;` |
|        - | 3142 | `				}` |
|   397001 | 3143 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|   198498 | 3144 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   198503 | 3145 | `				SyStringInitFromBuf(&sIntName,` |
|        - | 3146 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 3147 | `				/* Only interfaces are allowed */` |
|   198503 | 3148 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 3149 | `					pInterface = pInterface->pNextName;` |
|      ! 0 | 3150 | `				}` |
|   198503 | 3151 | `				if( pInterface == 0 ){` |
|      ! 0 | 3152 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 3153 | `						"Nonexistent base interface '%z'",&sIntName);` |
|      ! 0 | 3154 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3155 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 3156 | `						return SXERR_ABORT;` |
|        - | 3157 | `					}` |
|      ! 0 | 3158 | `				}else{` |
|        - | 3159 | `					/* Reject user classes that try to implement Throwable` |
|        - | 3160 | `					 * directly (or via an interface that extends Throwable)` |
|        - | 3161 | `					 * unless they already extend Exception or Error.` |
|        - | 3162 | `					 * Exception and Error themselves are compiled from the` |
|        - | 3163 | `					 * built-in library and are exempt by FQN — a namespaced` |
|        - | 3164 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|   198503 | 3165 | `					SyString *pFqn = &pClass->sName;` |
|   198503 | 3166 | `					int bIsExceptionOrError =` |
|   103141 | 3167 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   299695 | 3168 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|   196561 | 3169 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|     3902 | 3170 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   202391 | 3171 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    11682 | 3172 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|     3891 | 3173 | `						!bIsExceptionOrError ){` |
|       12 | 3174 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3175 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|        3 | 3176 | `							&pClass->sName);` |
|        9 | 3177 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3178 | `							SyBlobRelease(&sResolved);` |
|      ! 0 | 3179 | `							return SXERR_ABORT;` |
|        - | 3180 | `						}` |
|        - | 3181 | `						/* Skip registration so the follow-up abstract-method` |
|        - | 3182 | `						 * check does not produce a duplicate fatal. */` |
|        6 | 3183 | `					}else{` |
|   198497 | 3184 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|        - | 3185 | `					}` |
|        - | 3186 | `				}` |
|   198503 | 3187 | `				SyBlobRelease(&sResolved);` |
|   198503 | 3188 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    68143 | 3189 | `					break;` |
|        - | 3190 | `				}` |
|    62227 | 3191 | `				pGen->pIn++;/* Jump the comma */` |
|        5 | 3192 | `			}` |
|    68138 | 3193 | `		}` |
|   184925 | 3194 | `	}` |
|   453007 | 3195 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - | 3196 | `		/* Syntax error */` |
|      ! 0 | 3197 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|      ! 0 | 3198 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 3199 | `		if( rc == SXERR_ABORT ){` |
|        - | 3200 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3201 | `			return SXERR_ABORT;` |
|        - | 3202 | `		}` |
|      ! 0 | 3203 | `		return SXRET_OK;` |
|        - | 3204 | `	}` |
|   453007 | 3205 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   453007 | 3206 | `	pEnd = 0; /* cc warning */` |
|        - | 3207 | `	/* Delimit the class body */` |
|   453007 | 3208 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   453007 | 3209 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 3210 | `		/* Syntax error */` |
|      ! 0 | 3211 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|      ! 0 | 3212 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 3213 | `		if( rc == SXERR_ABORT ){` |
|        - | 3214 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3215 | `			return SXERR_ABORT;` |
|        - | 3216 | `		}` |
|      ! 0 | 3217 | `		return SXRET_OK;` |
|        - | 3218 | `	}` |
|        - | 3219 | `	/* The delimiter token is the class body's closing brace */` |
|   453007 | 3220 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 3221 | `	/* Swap token stream */` |
|   453007 | 3222 | `	pTmp = pGen->pEnd;` |
|   453007 | 3223 | `	pGen->pEnd = pEnd;` |
|        - | 3224 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|   453007 | 3225 | `	pClass->iFlags \|= iFlags;` |
|        - | 3226 | `	/* Start the parse process */` |
|  1697086 | 3227 | `	for(;;){` |
|        - | 3228 | `		/* Jump leading/trailing semi-colons */` |
|  4874141 | 3229 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   887779 | 3230 | `			pGen->pIn++;` |
|        5 | 3231 | `		}` |
|  3986367 | 3232 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3233 | `			/* End of class body */` |
|   452965 | 3234 | `			break;` |
|        - | 3235 | `		}` |
|        - | 3236 | `		/* Bind a directly-preceding docblock to this member */` |
|  3533407 | 3237 | `		GenStateSetPendingDoc(&(*pGen));` |
|  3533402 | 3238 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  1766706 | 3239 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|      ! 0 | 3240 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3241 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 | 3242 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 3243 | `			if( rc == SXERR_ABORT ){` |
|        - | 3244 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 3245 | `				return SXERR_ABORT;` |
|        - | 3246 | `			}` |
|      ! 0 | 3247 | `			goto done;` |
|        - | 3248 | `		}` |
|        - | 3249 | `		/* Assume public visibility */` |
|  3533407 | 3250 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  3533407 | 3251 | `		iAttrflags = 0;` |
|        - | 3252 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|        - | 3253 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|        - | 3254 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|        - | 3255 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  3533407 | 3256 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 3257 | `			int bMod = 0;` |
|      ! 0 | 3258 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 | 3259 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        - | 3260 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|        - | 3261 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|        - | 3262 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|        - | 3263 | `			 * that the generic keyword dispatch would misread as a method. */` |
|      ! 0 | 3264 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      ! 0 | 3265 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 | 3266 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|      ! 0 | 3267 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|      ! 0 | 3268 | `			}` |
|      ! 0 | 3269 | `			if( !bMod ){` |
|      ! 0 | 3270 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 3271 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 3272 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3273 | `						return SXERR_ABORT;` |
|        - | 3274 | `					}` |
|      ! 0 | 3275 | `					goto done;` |
|        - | 3276 | `				}` |
|      ! 0 | 3277 | `				continue;` |
|        - | 3278 | `			}` |
|      ! 0 | 3279 | `		}` |
|  3533407 | 3280 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 3281 | `			/* Extract the current keyword */` |
|  3533407 | 3282 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  3533407 | 3283 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|        - | 3284 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|     7835 | 3285 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|     7835 | 3286 | `				if( rc != SXRET_OK ){` |
|        6 | 3287 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3288 | `						return SXERR_ABORT;` |
|        - | 3289 | `					}` |
|        6 | 3290 | `					goto done;` |
|        - | 3291 | `				}` |
|     7831 | 3292 | `				continue;` |
|        - | 3293 | `			}` |
|  3525577 | 3294 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 3295 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|        - | 3296 | `				TraitUseEntry sUse;` |
|    15647 | 3297 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    15647 | 3298 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|    15647 | 3299 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|     7829 | 3300 | `				for(;;){` |
|        - | 3301 | `					ph7_class *pTrait;` |
|        - | 3302 | `					SyBlob sResolved;` |
|        - | 3303 | `					SyString sTraitName;` |
|    15655 | 3304 | `					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|        - | 3305 | `					/* A trait name is a full class reference: it may be qualified or` |
|        - | 3306 | ``					 * fully-qualified (`use Foo\Bar\T;`, `use \Foo\Bar\T;`) — the generated`` |
|        - | 3307 | `					 * PHPUnit mocks name their traits absolutely. Parse it with the shared` |
|        - | 3308 | `					 * class-reference reader (handles the leading '\', every '\'-segment and` |
|        - | 3309 | `					 * namespace/import resolution) instead of a single-identifier read, which` |
|        - | 3310 | `					 * choked on the first '\'. */` |
|    15655 | 3311 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    15655 | 3312 | `					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 3313 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 3314 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|      ! 0 | 3315 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|      ! 0 | 3316 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3317 | `							return SXERR_ABORT;` |
|        - | 3318 | `						}` |
|      ! 0 | 3319 | `						break;` |
|        - | 3320 | `					}` |
|    31305 | 3321 | `					pTrait = PH7_VmExtractClass(pGen->pVm,` |
|    15650 | 3322 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    15655 | 3323 | `					SyStringInitFromBuf(&sTraitName,` |
|        - | 3324 | `						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 3325 | `					/* Only traits are allowed */` |
|    15655 | 3326 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 3327 | `						pTrait = pTrait->pNextName;` |
|      ! 0 | 3328 | `					}` |
|    15655 | 3329 | `					if( pTrait == 0 ){` |
|      ! 0 | 3330 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|        - | 3331 | `							"'%z' is not a trait",&sTraitName);` |
|      ! 0 | 3332 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3333 | `							SyBlobRelease(&sResolved);` |
|      ! 0 | 3334 | `							return SXERR_ABORT;` |
|        - | 3335 | `						}` |
|      ! 0 | 3336 | `					}else{` |
|    15655 | 3337 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|        - | 3338 | `					}` |
|    15655 | 3339 | `					SyBlobRelease(&sResolved);` |
|        - | 3340 | `					/* GenStateParseClassReference already advanced past the whole name —` |
|        - | 3341 | `					 * continue only across a comma-separated trait list. */` |
|    15655 | 3342 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     7826 | 3343 | `						break;` |
|        - | 3344 | `					}` |
|       10 | 3345 | `					pGen->pIn++; /* Jump the comma */` |
|        2 | 3346 | `				}` |
|        - | 3347 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|    15647 | 3348 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - | 3349 | `					SyToken *pBlock;` |
|       12 | 3350 | `					pGen->pIn++; /* Jump '{' */` |
|       12 | 3351 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       12 | 3352 | `					sUse.pResolvStart = pGen->pIn;` |
|       12 | 3353 | `					sUse.pResolvEnd = pBlock;` |
|       12 | 3354 | `					if( pBlock < pGen->pEnd ){` |
|       12 | 3355 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|        7 | 3356 | `					}else{` |
|      ! 0 | 3357 | `						pGen->pIn = pGen->pEnd;` |
|        - | 3358 | `					}` |
|        5 | 3359 | `				}` |
|    15647 | 3360 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|        - | 3361 | `				/* The semicolon will be consumed by the outer loop */` |
|    15647 | 3362 | `				continue;` |
|        - | 3363 | `			}` |
|  3509935 | 3364 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        - | 3365 | `				int nSetTok;` |
|  2965075 | 3366 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|  2965075 | 3367 | `				if( nSetVis ){` |
|        - | 3368 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|        - | 3369 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|        3 | 3370 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        3 | 3371 | `					pGen->pIn += nSetTok;` |
|        2 | 3372 | `				}else{` |
|  2965073 | 3373 | `					iProtection = nKwrd;` |
|  2965073 | 3374 | `					pGen->pIn++; /* Jump the visibility token */` |
|        - | 3375 | `					/* Optional asymmetric set-visibility after the read` |
|        - | 3376 | ``					 * visibility: `public private(set) int $x`. */`` |
|  2965073 | 3377 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|  2965073 | 3378 | `					if( nSetVis ){` |
|        9 | 3379 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        9 | 3380 | `						pGen->pIn += nSetTok;` |
|        4 | 3381 | `					}` |
|        - | 3382 | `				}` |
|        - | 3383 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|        - | 3384 | ``				 * `public private(set) readonly int $x`. */`` |
|  2965075 | 3385 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       28 | 3386 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       28 | 3387 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       12 | 3388 | `				}` |
|  2965070 | 3389 | `				if( pGen->pIn >= pGen->pEnd` |
|  2965075 | 3390 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 3391 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3392 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 | 3393 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 3394 | `					if( rc == SXERR_ABORT ){` |
|        - | 3395 | `						/* Error count limit reached,abort immediately */` |
|      ! 0 | 3396 | `						return SXERR_ABORT;` |
|        - | 3397 | `					}` |
|      ! 0 | 3398 | `					goto done;` |
|        - | 3399 | `				}` |
|  2965075 | 3400 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 3401 | `					/* Attribute declaration (untyped) */` |
|   525573 | 3402 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   525573 | 3403 | `					if( rc != SXRET_OK ){` |
|       11 | 3404 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3405 | `							return SXERR_ABORT;` |
|        - | 3406 | `						}` |
|       11 | 3407 | `						goto done;` |
|        - | 3408 | `					}` |
|   533504 | 3409 | `					continue;` |
|        - | 3410 | `				}` |
|  2439507 | 3411 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 3412 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|    15889 | 3413 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    15889 | 3414 | `					if( rc != SXRET_OK ){` |
|        8 | 3415 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3416 | `							return SXERR_ABORT;` |
|        - | 3417 | `						}` |
|        8 | 3418 | `						goto done;` |
|        - | 3419 | `					}` |
|    15883 | 3420 | `					continue;` |
|        - | 3421 | `				}` |
|        - | 3422 | `				/* Extract the keyword */` |
|  2423623 | 3423 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1211809 | 3424 | `			}` |
|  2968483 | 3425 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|        - | 3426 | `				/* Process constant declaration */` |
|   287899 | 3427 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|   287899 | 3428 | `				if( rc != SXRET_OK ){` |
|       11 | 3429 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3430 | `						return SXERR_ABORT;` |
|        - | 3431 | `					}` |
|       11 | 3432 | `					goto done;` |
|        - | 3433 | `				}` |
|   143948 | 3434 | `			}else{` |
|  2680589 | 3435 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - | 3436 | `					/* Static method or attribute,record that */` |
|   101289 | 3437 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|   101289 | 3438 | `					pGen->pIn++; /* Jump the static keyword */` |
|   101289 | 3439 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 3440 | `						int nSetTok;` |
|    74031 | 3441 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|    74031 | 3442 | `						if( nSetVis ){` |
|        - | 3443 | ``							/* `static private(set) int $x` — read side stays public */`` |
|        3 | 3444 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        3 | 3445 | `							pGen->pIn += nSetTok;` |
|        2 | 3446 | `						}else{` |
|        - | 3447 | `							/* Extract the keyword */` |
|    74029 | 3448 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    74029 | 3449 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 3450 | `								iProtection = nKwrd;` |
|      ! 0 | 3451 | `								pGen->pIn++; /* Jump the visibility token */` |
|      ! 0 | 3452 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|      ! 0 | 3453 | `								if( nSetVis ){` |
|      ! 0 | 3454 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|      ! 0 | 3455 | `									pGen->pIn += nSetTok;` |
|      ! 0 | 3456 | `								}` |
|      ! 0 | 3457 | `							}` |
|        - | 3458 | `						}` |
|    37013 | 3459 | `					}` |
|        - | 3460 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|        - | 3461 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|        - | 3462 | `					 * than a generic "expecting method" parse error. */` |
|   101289 | 3463 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 3464 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 | 3465 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|      ! 0 | 3466 | `					}` |
|   101284 | 3467 | `					if( pGen->pIn >= pGen->pEnd` |
|   101289 | 3468 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 3469 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3470 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|      ! 0 | 3471 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 3472 | `						if( rc == SXERR_ABORT ){` |
|        - | 3473 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 3474 | `							return SXERR_ABORT;` |
|        - | 3475 | `						}` |
|      ! 0 | 3476 | `						goto done;` |
|        - | 3477 | `					}` |
|   101289 | 3478 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 3479 | `						/* Attribute declaration */` |
|    27259 | 3480 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    27259 | 3481 | `						if( rc != SXRET_OK ){` |
|        3 | 3482 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 3483 | `								return SXERR_ABORT;` |
|        - | 3484 | `							}` |
|        3 | 3485 | `							goto done;` |
|        - | 3486 | `						}` |
|    27257 | 3487 | `						continue;` |
|        - | 3488 | `					}` |
|    74035 | 3489 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 3490 | `						/* Typed static attribute declaration */` |
|       19 | 3491 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       19 | 3492 | `						if( rc != SXRET_OK ){` |
|        3 | 3493 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 3494 | `								return SXERR_ABORT;` |
|        - | 3495 | `							}` |
|        3 | 3496 | `							goto done;` |
|        - | 3497 | `						}` |
|       17 | 3498 | `						continue;` |
|        - | 3499 | `					}` |
|        - | 3500 | `					/* Extract the keyword */` |
|    74019 | 3501 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  2616312 | 3502 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        - | 3503 | `					/* Abstract method,record that.` |
|        - | 3504 | `					 * PHL used to also mark the whole CLASS abstract here, silently` |
|        - | 3505 | ``					 * promoting `class C{abstract function m();}` -- which php rejects`` |
|        - | 3506 | `					 * outright -- into a valid abstract class. That promotion is why` |
|        - | 3507 | `					 * GenStateCheckAbstractMethods never fired for it: by the time the` |
|        - | 3508 | `					 * check ran, the class looked declared-abstract. The declaration is` |
|        - | 3509 | `					 * now diagnosed where the method name is known (see the install` |
|        - | 3510 | `					 * site), so the class flag stays what the SOURCE said. */` |
|     7803 | 3511 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        - | 3512 | `					/* Advance the stream cursor */` |
|     7803 | 3513 | `					pGen->pIn++;` |
|     7803 | 3514 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     7803 | 3515 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     7803 | 3516 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     7801 | 3517 | `							iProtection = nKwrd;` |
|     7801 | 3518 | `							pGen->pIn++; /* Jump the visibility token */` |
|     3898 | 3519 | `						}` |
|     3899 | 3520 | `					}` |
|     7803 | 3521 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     7798 | 3522 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 3523 | `							/* Static method */` |
|      ! 0 | 3524 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 3525 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 3526 | `					}` |
|     7803 | 3527 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|     7798 | 3528 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|        - | 3529 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|        - | 3530 | `							 * HOOKED property declaration. Route anything that is not a` |
|        - | 3531 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|        - | 3532 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|        - | 3533 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|        6 | 3534 | `							if( pGen->pIn < pGen->pEnd` |
|        7 | 3535 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|        3 | 3536 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|        7 | 3537 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        7 | 3538 | `								if( rc != SXRET_OK ){` |
|      ! 0 | 3539 | `									if( rc == SXERR_ABORT ){` |
|      ! 0 | 3540 | `										return SXERR_ABORT;` |
|        - | 3541 | `									}` |
|      ! 0 | 3542 | `									goto done;` |
|        - | 3543 | `								}` |
|        7 | 3544 | `								continue;` |
|        - | 3545 | `							}` |
|      ! 0 | 3546 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3547 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|      ! 0 | 3548 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 3549 | `							if( rc == SXERR_ABORT ){` |
|        - | 3550 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 3551 | `								return SXERR_ABORT;` |
|        - | 3552 | `							}` |
|      ! 0 | 3553 | `							goto done;` |
|        - | 3554 | `					}` |
|     7797 | 3555 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  2575403 | 3556 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|        - | 3557 | `					/* final method ,record that */` |
|       21 | 3558 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       21 | 3559 | `					pGen->pIn++; /* Jump the final keyword */` |
|       21 | 3560 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 3561 | `						/* Extract the keyword */` |
|       21 | 3562 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       21 | 3563 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       10 | 3564 | `							iProtection = nKwrd;` |
|       10 | 3565 | `							pGen->pIn++; /* Jump the visibility token */` |
|        4 | 3566 | `						}` |
|        9 | 3567 | `					}` |
|       21 | 3568 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       18 | 3569 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|        - | 3570 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|        - | 3571 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|        - | 3572 | `							 * child class is compiled (PH7_ClassInherit). */` |
|       14 | 3573 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|       14 | 3574 | `							if( rc != SXRET_OK ){` |
|      ! 0 | 3575 | `								if( rc == SXERR_ABORT ){` |
|      ! 0 | 3576 | `									return SXERR_ABORT;` |
|        - | 3577 | `								}` |
|      ! 0 | 3578 | `								goto done;` |
|        - | 3579 | `							}` |
|       14 | 3580 | `							continue;` |
|        - | 3581 | `					}` |
|        8 | 3582 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        6 | 3583 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 3584 | `							/* Static method */` |
|      ! 0 | 3585 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 3586 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 3587 | `					}` |
|        8 | 3588 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        6 | 3589 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 3590 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3591 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|      ! 0 | 3592 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 3593 | `							if( rc == SXERR_ABORT ){` |
|        - | 3594 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 3595 | `								return SXERR_ABORT;` |
|        - | 3596 | `							}` |
|      ! 0 | 3597 | `							goto done;` |
|        - | 3598 | `					}` |
|        8 | 3599 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        3 | 3600 | `				}` |
|  2653301 | 3601 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 3602 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3603 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|      ! 0 | 3604 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 3605 | `						if( rc == SXERR_ABORT ){` |
|        - | 3606 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 3607 | `							return SXERR_ABORT;` |
|        - | 3608 | `						}` |
|      ! 0 | 3609 | `						goto done;` |
|        - | 3610 | `				}` |
|  2653301 | 3611 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|        7 | 3612 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|        7 | 3613 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|      ! 0 | 3614 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3615 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 3616 | `						if( rc == SXERR_ABORT ){` |
|        - | 3617 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 3618 | `							return SXERR_ABORT;` |
|        - | 3619 | `						}` |
|      ! 0 | 3620 | `						goto done;` |
|        - | 3621 | `					}` |
|        - | 3622 | `					/* Attribute declaration */` |
|        7 | 3623 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        4 | 3624 | `				}else{` |
|        - | 3625 | `					/* Process method declaration */` |
|  2653295 | 3626 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 3627 | `				}` |
|  2653301 | 3628 | `				if( rc != SXRET_OK ){` |
|       16 | 3629 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3630 | `						return SXERR_ABORT;` |
|        - | 3631 | `					}` |
|       16 | 3632 | `					goto done;` |
|        - | 3633 | `				}` |
|        - | 3634 | `			}` |
|  1470590 | 3635 | `		}else{` |
|        - | 3636 | `			/* Attribute declaration */` |
|      ! 0 | 3637 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 3638 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 3639 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3640 | `					return SXERR_ABORT;` |
|        - | 3641 | `				}` |
|      ! 0 | 3642 | `				goto done;` |
|        - | 3643 | `			}` |
|        - | 3644 | `		}` |
|        5 | 3645 | `	}` |
|        - | 3646 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|        - | 3647 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|        - | 3648 | `	 */` |
|        - | 3649 | `	{` |
|        - | 3650 | `		TraitUseEntry *apUse;` |
|        - | 3651 | `		sxu32 nU;` |
|   452965 | 3652 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   468607 | 3653 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|    15647 | 3654 | `			TraitUseEntry *pUse = &apUse[nU];` |
|    15647 | 3655 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|    15647 | 3656 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|    15647 | 3657 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|        - | 3658 | `			sxu32 nT;` |
|    15647 | 3659 | `			if( !hasResolution ){` |
|        - | 3660 | `				/* No conflict resolution block: use standard trait application */` |
|    31275 | 3661 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|    15643 | 3662 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|    15643 | 3663 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 3664 | `						break;` |
|        - | 3665 | `					}` |
|     7824 | 3666 | `				}` |
|     7821 | 3667 | `			}else{` |
|        - | 3668 | `				/* With resolution block: copy attributes, record traits,` |
|        - | 3669 | `				 * then use the block to resolve method conflicts.` |
|        - | 3670 | `				 */` |
|        - | 3671 | `				SyToken *pR;` |
|       24 | 3672 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       14 | 3673 | `					ph7_class *pTR = apTrait[nT];` |
|        - | 3674 | `					ph7_class_attr *pAR;` |
|        - | 3675 | `					SyHashEntry *pER;` |
|        - | 3676 | `					SyString *pNR;` |
|       14 | 3677 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|       20 | 3678 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|      ! 0 | 3679 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|      ! 0 | 3680 | `						pNR = &pAR->sName;` |
|      ! 0 | 3681 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      ! 0 | 3682 | `							SyHashInsertTail(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|      ! 0 | 3683 | `						}` |
|      ! 0 | 3684 | `					}` |
|       14 | 3685 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|        8 | 3686 | `				}` |
|        - | 3687 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       12 | 3688 | `				pR = pUse->pResolvStart;` |
|       26 | 3689 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 3690 | `					SyString sTrait,sMethod;` |
|        - | 3691 | `					ph7_class *pSrcTrait;` |
|        - | 3692 | `					ph7_class_method *pMeth;` |
|        - | 3693 | `					sxi32 nRKwrd;` |
|       40 | 3694 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       26 | 3695 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       16 | 3696 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       16 | 3697 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       16 | 3698 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       16 | 3699 | `					sMethod = pR->sData;` |
|       16 | 3700 | `					pR++;` |
|       16 | 3701 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 3702 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 3703 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 3704 | `							sTrait = sMethod;` |
|        7 | 3705 | `							pR++;` |
|        7 | 3706 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 3707 | `							sMethod = pR->sData;` |
|        7 | 3708 | `							pR++;` |
|        3 | 3709 | `						}` |
|        3 | 3710 | `					}` |
|       16 | 3711 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 3712 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 3713 | `						continue;` |
|        - | 3714 | `					}` |
|       16 | 3715 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       16 | 3716 | `					pR++;` |
|       16 | 3717 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|        5 | 3718 | `						pSrcTrait = 0;` |
|        7 | 3719 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        7 | 3720 | `							SyString *pTN = &apTrait[nT]->sName;` |
|       10 | 3721 | `							if( pTN->nByte >= sTrait.nByte &&` |
|        6 | 3722 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        5 | 3723 | `								pSrcTrait = apTrait[nT];` |
|        5 | 3724 | `								break;` |
|        - | 3725 | `							}` |
|        2 | 3726 | `						}` |
|        5 | 3727 | `						if( pSrcTrait ){` |
|        5 | 3728 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        5 | 3729 | `							if( pMeth ){` |
|        5 | 3730 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|        5 | 3731 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|        5 | 3732 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|        2 | 3733 | `								}` |
|        2 | 3734 | `							}` |
|        2 | 3735 | `						}` |
|        2 | 3736 | `					}` |
|       34 | 3737 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        2 | 3738 | `				}` |
|        - | 3739 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|       24 | 3740 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        - | 3741 | `					ph7_class_method *pMR;` |
|        - | 3742 | `					SyHashEntry *pER;` |
|        - | 3743 | `					SyString *pNR;` |
|       14 | 3744 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|       40 | 3745 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|       22 | 3746 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|       22 | 3747 | `						pNR = &pMR->sFunc.sName;` |
|       22 | 3748 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       13 | 3749 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|        6 | 3750 | `						}` |
|        2 | 3751 | `					}` |
|        8 | 3752 | `				}` |
|        - | 3753 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       12 | 3754 | `				pR = pUse->pResolvStart;` |
|       26 | 3755 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 3756 | `					SyString sTrait,sMethod,sAlias;` |
|        - | 3757 | `					ph7_class *pSrcTrait;` |
|        - | 3758 | `					ph7_class_method *pMeth;` |
|       26 | 3759 | `					int hasQual = 0;` |
|        - | 3760 | `					sxi32 nRKwrd;` |
|       40 | 3761 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       26 | 3762 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       16 | 3763 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       16 | 3764 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       16 | 3765 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|       16 | 3766 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       16 | 3767 | `					sMethod = pR->sData;` |
|       16 | 3768 | `					pR++;` |
|       16 | 3769 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 3770 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 3771 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 3772 | `							sTrait = sMethod;` |
|        7 | 3773 | `							hasQual = 1;` |
|        7 | 3774 | `							pR++;` |
|        7 | 3775 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 3776 | `							sMethod = pR->sData;` |
|        7 | 3777 | `							pR++;` |
|        3 | 3778 | `						}` |
|        3 | 3779 | `					}` |
|       16 | 3780 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 3781 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 3782 | `						continue;` |
|        - | 3783 | `					}` |
|       16 | 3784 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       16 | 3785 | `					pR++;` |
|       16 | 3786 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       12 | 3787 | `						sxi32 iNewVis = -1;` |
|       12 | 3788 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|        7 | 3789 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|        7 | 3790 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|        7 | 3791 | `								iNewVis = nAK;` |
|        7 | 3792 | `								pR++;` |
|        3 | 3793 | `							}` |
|        3 | 3794 | `						}` |
|       12 | 3795 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       10 | 3796 | `							sAlias = pR->sData;` |
|       10 | 3797 | `							pR++;` |
|        4 | 3798 | `						}` |
|       12 | 3799 | `						pMeth = 0;` |
|       12 | 3800 | `						if( hasQual ){` |
|        3 | 3801 | `							pSrcTrait = 0;` |
|        5 | 3802 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        5 | 3803 | `								SyString *pTN = &apTrait[nT]->sName;` |
|        7 | 3804 | `								if( pTN->nByte >= sTrait.nByte &&` |
|        4 | 3805 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        3 | 3806 | `									pSrcTrait = apTrait[nT];` |
|        3 | 3807 | `									break;` |
|        - | 3808 | `								}` |
|        2 | 3809 | `							}` |
|        3 | 3810 | `							if( pSrcTrait ){` |
|        3 | 3811 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        1 | 3812 | `							}` |
|        2 | 3813 | `						}else{` |
|        9 | 3814 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|        - | 3815 | `						}` |
|       12 | 3816 | `						if( pMeth ){` |
|       12 | 3817 | `							if( sAlias.nByte > 0 ){` |
|        - | 3818 | `								/* Create a shallow copy of the method struct for the alias` |
|        - | 3819 | `								 * so it can carry its own visibility without affecting the original.` |
|        - | 3820 | `								 */` |
|        - | 3821 | `								ph7_class_method *pAlias;` |
|        - | 3822 | `								char *zAliasDup;` |
|       10 | 3823 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       10 | 3824 | `								if( pAlias ){` |
|       10 | 3825 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       10 | 3826 | `									if( iNewVis >= 0 ){` |
|        5 | 3827 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 3828 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 3829 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        2 | 3830 | `									}` |
|       10 | 3831 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       10 | 3832 | `									if( zAliasDup ){` |
|       10 | 3833 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|        4 | 3834 | `									}` |
|        6 | 3835 | `								}` |
|        7 | 3836 | `							}else if( iNewVis >= 0 ){` |
|        - | 3837 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|        - | 3838 | `								ph7_class_method *pCopy;` |
|        3 | 3839 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        3 | 3840 | `								if( pCopy ){` |
|        3 | 3841 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|        3 | 3842 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|        3 | 3843 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 3844 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 3845 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        - | 3846 | `									/* Replace the method in the class hash */` |
|        3 | 3847 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|        3 | 3848 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|        1 | 3849 | `								}` |
|        1 | 3850 | `							}` |
|        5 | 3851 | `						}` |
|        5 | 3852 | `						SXUNUSED(hasQual);` |
|        5 | 3853 | `					}` |
|       20 | 3854 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        2 | 3855 | `				}` |
|        - | 3856 | `			}` |
|    15647 | 3857 | `			SySetRelease(&pUse->aTraits);` |
|     7826 | 3858 | `		}` |
|        - | 3859 | `	}` |
|   452965 | 3860 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - | 3861 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|        - | 3862 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|     3925 | 3863 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|     3925 | 3864 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3865 | `			SySetRelease(&aUseEntries);` |
|      ! 0 | 3866 | `			SySetRelease(&aInterfaces);` |
|      ! 0 | 3867 | `			return SXERR_ABORT;` |
|        - | 3868 | `		}` |
|     1960 | 3869 | `	}` |
|        - | 3870 | `	/* Reject a php-fatal redeclaration before hoisting the class */` |
|   452965 | 3871 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|        9 | 3872 | `		return SXERR_ABORT;` |
|        - | 3873 | `	}` |
|        - | 3874 | `	/* Install the class */` |
|   452959 | 3875 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   452959 | 3876 | `	if( rc == SXRET_OK ){` |
|        - | 3877 | `		ph7_class **apInterface;` |
|        - | 3878 | `		sxu32 n;` |
|   452959 | 3879 | `		if( pBase ){` |
|        - | 3880 | `			/* Inherit from base class and mark as a subclass */` |
|   249145 | 3881 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   124570 | 3882 | `		}` |
|   452959 | 3883 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   651451 | 3884 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|        - | 3885 | `			/* Implements one or more interface */` |
|   198497 | 3886 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|   198497 | 3887 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 3888 | `				break;` |
|        - | 3889 | `			}` |
|    99251 | 3890 | `		}` |
|        - | 3891 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|        - | 3892 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|   452959 | 3893 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|     3923 | 3894 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|     3923 | 3895 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 3896 | `				pIntf = pIntf->pNextName;` |
|      ! 0 | 3897 | `			}` |
|     3923 | 3898 | `			if( pIntf ){` |
|     3923 | 3899 | `				PH7_ClassImplement(pClass,pIntf);` |
|     1959 | 3900 | `			}` |
|     3923 | 3901 | `			if( pClass->nEnumBacking != 0 ){` |
|     3907 | 3902 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|     3907 | 3903 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 3904 | `					pIntf = pIntf->pNextName;` |
|      ! 0 | 3905 | `				}` |
|     3907 | 3906 | `				if( pIntf ){` |
|     3907 | 3907 | `					PH7_ClassImplement(pClass,pIntf);` |
|     1951 | 3908 | `				}` |
|     1951 | 3909 | `			}` |
|     1959 | 3910 | `		}` |
|        - | 3911 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|        - | 3912 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|   452954 | 3913 | `		if( rc == SXRET_OK` |
|   452954 | 3914 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|   452959 | 3915 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   229505 | 3916 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|        - | 3917 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   229505 | 3918 | `			if( pStringable ){` |
|   229505 | 3919 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   229505 | 3920 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|        - | 3921 | `				sxu32 i;` |
|   229505 | 3922 | `				int bAlready = 0;` |
|   276165 | 3923 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    58331 | 3924 | `					if( apImpl[i] == pStringable ){` |
|    11671 | 3925 | `						bAlready = 1;` |
|    11671 | 3926 | `						break;` |
|        - | 3927 | `					}` |
|    23335 | 3928 | `				}` |
|   229505 | 3929 | `				if( !bAlready ){` |
|   217839 | 3930 | `					PH7_ClassImplement(pClass,pStringable);` |
|   108917 | 3931 | `				}` |
|   114750 | 3932 | `			}` |
|   114750 | 3933 | `		}` |
|        - | 3934 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   452959 | 3935 | `		if( rc == SXRET_OK ){` |
|   452959 | 3936 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   452959 | 3937 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 3938 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 3939 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 3940 | `				return SXERR_ABORT;` |
|        - | 3941 | `			}` |
|   226477 | 3942 | `		}` |
|        - | 3943 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   452959 | 3944 | `		if( rc == SXRET_OK ){` |
|   452959 | 3945 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   452959 | 3946 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 3947 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 3948 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 3949 | `				return SXERR_ABORT;` |
|        - | 3950 | `			}` |
|   226477 | 3951 | `		}` |
|   226477 | 3952 | `	}` |
|   452959 | 3953 | `	SySetRelease(&aUseEntries);` |
|   452959 | 3954 | `	SySetRelease(&aInterfaces);` |
|   452959 | 3955 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3956 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3957 | `		return SXERR_ABORT;` |
|        - | 3958 | `	}` |
|   226477 | 3959 | `done:` |
|        - | 3960 | `	/* Point beyond the class body */` |
|   453001 | 3961 | `	pGen->pIn = &pEnd[1];` |
|   453001 | 3962 | `	pGen->pEnd = pTmp;` |
|   453001 | 3963 | `	return PH7_OK;` |
|   226507 | 3964 | `}` |
|        - | 3965 | `/* Compile a named class declaration (the common case). */` |
|   452974 | 3966 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|        5 | 3967 | `{` |
|   452979 | 3968 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|        5 | 3969 | `}` |
|        - | 3970 | `/*` |
|        - | 3971 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|        - | 3972 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|        - | 3973 | ` * compile + install the class body once (at compile time, like every other` |
|        - | 3974 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|        - | 3975 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|        - | 3976 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|        - | 3977 | ` */` |
|       30 | 3978 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 | 3979 | `{` |
|        - | 3980 | `	char zName[128];         /* Synthesized class name */` |
|        - | 3981 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|        - | 3982 | `	SyString sName;` |
|        - | 3983 | `	SyToken *pArgStart,*pArgEnd;` |
|       34 | 3984 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|        - | 3985 | `	                              * is keyed to this 'class' token */` |
|        - | 3986 | `	ph7_value *pObj;` |
|       34 | 3987 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3988 | `	sxu32 nIdx,nLen;` |
|        - | 3989 | `	sxi32 nArg,rc;` |
|       15 | 3990 | `	SXUNUSED(iCompileFlag);` |
|        - | 3991 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|       34 | 3992 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       34 | 3993 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 | 3994 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      ! 0 | 3995 | `	}` |
|       34 | 3996 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - | 3997 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|        - | 3998 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|        - | 3999 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|       34 | 4000 | `	pArgStart = pArgEnd = 0;` |
|       34 | 4001 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|       34 | 4002 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 4003 | `		return rc;` |
|        - | 4004 | `	}` |
|        - | 4005 | `	{` |
|        - | 4006 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|       34 | 4007 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|       30 | 4008 | `		if( pAnonClass` |
|       34 | 4009 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 4010 | `			return SXERR_ABORT;` |
|        - | 4011 | `		}` |
|        - | 4012 | `	}` |
|        - | 4013 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|        - | 4014 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|       34 | 4015 | `	nArg = 0;` |
|       34 | 4016 | `	if( pArgStart < pArgEnd ){` |
|        7 | 4017 | `		SyToken *pSavedIn = pGen->pIn;` |
|        7 | 4018 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 4019 | `		SyToken *pArgNext;` |
|        7 | 4020 | `		pGen->pIn = pArgStart;` |
|        7 | 4021 | `		pGen->pEnd = pArgEnd;` |
|       13 | 4022 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|        7 | 4023 | `			if( pGen->pIn < pArgNext ){` |
|        7 | 4024 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|        7 | 4025 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 4026 | `					pGen->pIn = pSavedIn;` |
|      ! 0 | 4027 | `					pGen->pEnd = pSavedEnd;` |
|      ! 0 | 4028 | `					return SXERR_ABORT;` |
|        - | 4029 | `				}` |
|        7 | 4030 | `				nArg++;` |
|        3 | 4031 | `			}` |
|        7 | 4032 | `			pGen->pIn = &pArgNext[1];` |
|        1 | 4033 | `		}` |
|        7 | 4034 | `		pGen->pIn = pSavedIn;` |
|        7 | 4035 | `		pGen->pEnd = pSavedEnd;` |
|        3 | 4036 | `	}` |
|        - | 4037 | `	/* Load the synthesized class name */` |
|       34 | 4038 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       34 | 4039 | `	if( pObj == 0 ){` |
|      ! 0 | 4040 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 4041 | `		return SXERR_ABORT;` |
|        - | 4042 | `	}` |
|       34 | 4043 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       34 | 4044 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - | 4045 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|       34 | 4046 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       34 | 4047 | `	return SXRET_OK;` |
|       19 | 4048 | `}` |
|        - | 4049 | `/*` |
|        - | 4050 | ` * Compile a user-defined abstract class.` |
|        - | 4051 | ` *  According to the PHP language reference manual` |
|        - | 4052 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|        - | 4053 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|        - | 4054 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|        - | 4055 | ` *   the method's signature - they cannot define the implementation.` |
|        - | 4056 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|        - | 4057 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|        - | 4058 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|        - | 4059 | ` *   method is defined as protected, the function implementation must be defined as either` |
|        - | 4060 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|        - | 4061 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|        - | 4062 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|        - | 4063 | ` *   could differ.` |
|        - | 4064 | ` */` |
|        - | 4065 | `/*` |
|        - | 4066 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|        - | 4067 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|        - | 4068 | ` * receives the corresponding PH7_CLASS_* bit.` |
|        - | 4069 | ` */` |
| 14476430 | 4070 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|        5 | 4071 | `{` |
| 14476435 | 4072 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  8664411 | 4073 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  8664411 | 4074 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  8609933 | 4075 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  4285471 | 4076 | `	}` |
| 14382971 | 4077 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
| 14382911 | 4078 | `	return FALSE;` |
|  7238220 | 4079 | `}` |
|        - | 4080 | `/*` |
|        - | 4081 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|        - | 4082 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|        - | 4083 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|        - | 4084 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|        - | 4085 | ` */` |
| 14382906 | 4086 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|        5 | 4087 | `{` |
| 14382911 | 4088 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
| 14382911 | 4089 | `	sxi32 iFlags = 0,iFlag;` |
| 14476435 | 4090 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    93529 | 4091 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|        5 | 4092 | `			pDup = pIn;` |
|        2 | 4093 | `		}` |
|    93529 | 4094 | `		iFlags \|= iFlag;` |
|    93529 | 4095 | `		pIn++;` |
|        5 | 4096 | `	}` |
| 14382911 | 4097 | `	*ppIn = pIn;` |
| 14382911 | 4098 | `	if( ppDup ){ *ppDup = pDup; }` |
| 14382911 | 4099 | `	return iFlags;` |
|        5 | 4100 | `}` |
|        - | 4101 | `/*` |
|        - | 4102 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|        - | 4103 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|        - | 4104 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|        - | 4105 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|        - | 4106 | `` * `readonly`) to their existing handlers.`` |
|        - | 4107 | ` */` |
| 14340042 | 4108 | `PH7_PRIVATE int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|        5 | 4109 | `{` |
| 14340047 | 4110 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  7220668 | 4111 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
| 14365364 | 4112 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|        5 | 4113 | `}` |
|        - | 4114 | `/*` |
|        - | 4115 | ` * Compile a class declaration carrying one or more leading modifiers` |
|        - | 4116 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|        - | 4117 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|        - | 4118 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|        - | 4119 | `` * `abstract`+`final` pair, like PHP.`` |
|        - | 4120 | ` */` |
|    42864 | 4121 | `PH7_PRIVATE sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|        5 | 4122 | `{` |
|        - | 4123 | `	SyToken *pDup;` |
|    42869 | 4124 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|        - | 4125 | `	sxi32 rc;` |
|    42869 | 4126 | `	if( pDup ){` |
|        4 | 4127 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|        2 | 4128 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|        3 | 4129 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4130 | `			return SXERR_ABORT;` |
|        - | 4131 | `		}` |
|        1 | 4132 | `	}` |
|    42864 | 4133 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    21437 | 4134 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|        3 | 4135 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4136 | `			"Cannot use the final modifier on an abstract class");` |
|        3 | 4137 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4138 | `			return SXERR_ABORT;` |
|        - | 4139 | `		}` |
|        1 | 4140 | `	}` |
|    42869 | 4141 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    21437 | 4142 | `}` |
|        - | 4143 | `/*` |
|        - | 4144 | ` * Compile a user-defined trait.` |
|        - | 4145 | ` *  Traits are similar to classes, but only intended to group functionality` |
|        - | 4146 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|        - | 4147 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|        - | 4148 | ` */` |
|     7876 | 4149 | `PH7_PRIVATE sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|        5 | 4150 | `{` |
|     7881 | 4151 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 4152 | `	ph7_class *pClass;` |
|        - | 4153 | `	SyToken *pEnd,*pTmp;` |
|        - | 4154 | `	sxi32 iProtection;` |
|        - | 4155 | `	sxi32 iAttrflags;` |
|        - | 4156 | `	SyString *pName;` |
|        - | 4157 | `	sxi32 nKwrd;` |
|        - | 4158 | `	sxi32 rc;` |
|        - | 4159 | `	/* Jump the 'trait' keyword */` |
|     7881 | 4160 | `	pGen->pIn++;` |
|     7881 | 4161 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 4162 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|      ! 0 | 4163 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4164 | `			return SXERR_ABORT;` |
|        - | 4165 | `		}` |
|      ! 0 | 4166 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|      ! 0 | 4167 | `			pGen->pIn++;` |
|      ! 0 | 4168 | `		}` |
|      ! 0 | 4169 | `		return SXRET_OK;` |
|        - | 4170 | `	}` |
|        - | 4171 | `	/* Extract trait name */` |
|     7881 | 4172 | `	pName = &pGen->pIn->sData;` |
|     7881 | 4173 | `	pGen->pIn++;` |
|        - | 4174 | `	/* Build FQN and obtain a raw class */ {` |
|        - | 4175 | `		SyBlob sFQN;` |
|        - | 4176 | `		SyString sFQNStr;` |
|     7881 | 4177 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     7881 | 4178 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     7881 | 4179 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|     7881 | 4180 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     7881 | 4181 | `		SyBlobRelease(&sFQN);` |
|        - | 4182 | `	}` |
|     7881 | 4183 | `	if( pClass == 0 ){` |
|      ! 0 | 4184 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4185 | `		return SXERR_ABORT;` |
|        - | 4186 | `	}` |
|     7881 | 4187 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     7881 | 4188 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 4189 | `		return SXERR_ABORT;` |
|        - | 4190 | `	}` |
|        - | 4191 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|     7881 | 4192 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 | 4193 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|      ! 0 | 4194 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 4195 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4196 | `			return SXERR_ABORT;` |
|        - | 4197 | `		}` |
|      ! 0 | 4198 | `		return SXRET_OK;` |
|        - | 4199 | `	}` |
|     7881 | 4200 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     7881 | 4201 | `	pEnd = 0;` |
|     7881 | 4202 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|     7881 | 4203 | `	if( pEnd >= pGen->pEnd ){` |
|      ! 0 | 4204 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|      ! 0 | 4205 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 4206 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4207 | `			return SXERR_ABORT;` |
|        - | 4208 | `		}` |
|      ! 0 | 4209 | `		return SXRET_OK;` |
|        - | 4210 | `	}` |
|        - | 4211 | `	/* The delimiter token is the trait body's closing brace */` |
|     7881 | 4212 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 4213 | `	/* Swap token stream */` |
|     7881 | 4214 | `	pTmp = pGen->pEnd;` |
|     7881 | 4215 | `	pGen->pEnd = pEnd;` |
|        - | 4216 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|     7881 | 4217 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|        - | 4218 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|    56477 | 4219 | `	for(;;){` |
|   159677 | 4220 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|    23367 | 4221 | `			pGen->pIn++;` |
|        5 | 4222 | `		}` |
|   136315 | 4223 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     7881 | 4224 | `			break;` |
|        - | 4225 | `		}` |
|        - | 4226 | `		/* Bind a directly-preceding docblock to this member */` |
|   128439 | 4227 | `		GenStateSetPendingDoc(&(*pGen));` |
|   128439 | 4228 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|      ! 0 | 4229 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4230 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 4231 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 4232 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4233 | `				return SXERR_ABORT;` |
|        - | 4234 | `			}` |
|      ! 0 | 4235 | `			goto done;` |
|        - | 4236 | `		}` |
|   128439 | 4237 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   128439 | 4238 | `		iAttrflags = 0;` |
|   128439 | 4239 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   128439 | 4240 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   128439 | 4241 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 4242 | `				/* Trait uses another trait: use OtherTrait; */` |
|        5 | 4243 | `				pGen->pIn++; /* Jump 'use' */` |
|        2 | 4244 | `				for(;;){` |
|        - | 4245 | `					ph7_class *pUsedTrait;` |
|        - | 4246 | `					SyString *pUsedName;` |
|        5 | 4247 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 4248 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 4249 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|      ! 0 | 4250 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4251 | `							return SXERR_ABORT;` |
|        - | 4252 | `						}` |
|      ! 0 | 4253 | `						break;` |
|        - | 4254 | `					}` |
|        5 | 4255 | `					pUsedName = &pGen->pIn->sData;` |
|        - | 4256 | `					{` |
|        - | 4257 | `						SyBlob sResolved;` |
|        5 | 4258 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        5 | 4259 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|        7 | 4260 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|        4 | 4261 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|        5 | 4262 | `						SyBlobRelease(&sResolved);` |
|        - | 4263 | `					}` |
|        5 | 4264 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 4265 | `						pUsedTrait = pUsedTrait->pNextName;` |
|      ! 0 | 4266 | `					}` |
|        5 | 4267 | `					if( pUsedTrait == 0 ){` |
|        4 | 4268 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        1 | 4269 | `							"'%z' is not a trait",pUsedName);` |
|        3 | 4270 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4271 | `							return SXERR_ABORT;` |
|        - | 4272 | `						}` |
|        2 | 4273 | `					}else{` |
|        3 | 4274 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|        - | 4275 | `					}` |
|        5 | 4276 | `					pGen->pIn++;` |
|        5 | 4277 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|        3 | 4278 | `						break;` |
|        - | 4279 | `					}` |
|      ! 0 | 4280 | `					pGen->pIn++;` |
|      ! 0 | 4281 | `				}` |
|        5 | 4282 | `				continue;` |
|        - | 4283 | `			}` |
|   128435 | 4284 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|   128417 | 4285 | `				iProtection = nKwrd;` |
|   128417 | 4286 | `				pGen->pIn++;` |
|        - | 4287 | ``				/* Optional `readonly` after the visibility (PHP 8.1): `private readonly T`` |
|        - | 4288 | ``				 * $x` — the generated PHPUnit runtime traits (StubApi, …) declare their`` |
|        - | 4289 | `				 * readonly state this way. Mirrors the class-body attribute parser. */` |
|   128417 | 4290 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|        5 | 4291 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|        5 | 4292 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        2 | 4293 | `				}` |
|   128412 | 4294 | `				if( pGen->pIn >= pGen->pEnd` |
|   128417 | 4295 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 4296 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4297 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 4298 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 4299 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 4300 | `						return SXERR_ABORT;` |
|        - | 4301 | `					}` |
|      ! 0 | 4302 | `					goto done;` |
|        - | 4303 | `				}` |
|   128417 | 4304 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|    23347 | 4305 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    23347 | 4306 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 4307 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4308 | `							return SXERR_ABORT;` |
|        - | 4309 | `						}` |
|      ! 0 | 4310 | `						goto done;` |
|        - | 4311 | `					}` |
|    23347 | 4312 | `					continue;` |
|        - | 4313 | `				}` |
|   105075 | 4314 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        9 | 4315 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        9 | 4316 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 4317 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4318 | `							return SXERR_ABORT;` |
|        - | 4319 | `						}` |
|      ! 0 | 4320 | `						goto done;` |
|        - | 4321 | `					}` |
|        9 | 4322 | `					continue;` |
|        - | 4323 | `				}` |
|   105067 | 4324 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    52531 | 4325 | `			}` |
|   105085 | 4326 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|      ! 0 | 4327 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4328 | `					"Traits cannot have constants");` |
|      ! 0 | 4329 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 4330 | `					return SXERR_ABORT;` |
|        - | 4331 | `				}` |
|      ! 0 | 4332 | `				goto done;` |
|      ! 0 | 4333 | `			}else{` |
|   105085 | 4334 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|     7791 | 4335 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     7791 | 4336 | `					pGen->pIn++;` |
|     7791 | 4337 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     7789 | 4338 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     7789 | 4339 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 4340 | `							iProtection = nKwrd;` |
|      ! 0 | 4341 | `							pGen->pIn++;` |
|      ! 0 | 4342 | `						}` |
|     3892 | 4343 | `					}` |
|     7786 | 4344 | `					if( pGen->pIn >= pGen->pEnd` |
|     7791 | 4345 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 4346 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4347 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|      ! 0 | 4348 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 4349 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4350 | `							return SXERR_ABORT;` |
|        - | 4351 | `						}` |
|      ! 0 | 4352 | `						goto done;` |
|        - | 4353 | `					}` |
|     7791 | 4354 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        3 | 4355 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        3 | 4356 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 4357 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 4358 | `								return SXERR_ABORT;` |
|        - | 4359 | `							}` |
|      ! 0 | 4360 | `							goto done;` |
|        - | 4361 | `						}` |
|        3 | 4362 | `						continue;` |
|        - | 4363 | `					}` |
|     7789 | 4364 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|      ! 0 | 4365 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 4366 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 4367 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 4368 | `								return SXERR_ABORT;` |
|        - | 4369 | `							}` |
|      ! 0 | 4370 | `							goto done;` |
|        - | 4371 | `						}` |
|      ! 0 | 4372 | `						continue;` |
|        - | 4373 | `					}` |
|     7789 | 4374 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   101191 | 4375 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        9 | 4376 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        9 | 4377 | `					pGen->pIn++;` |
|        9 | 4378 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        9 | 4379 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        9 | 4380 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        9 | 4381 | `							iProtection = nKwrd;` |
|        9 | 4382 | `							pGen->pIn++;` |
|        3 | 4383 | `						}` |
|        3 | 4384 | `					}` |
|        9 | 4385 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        6 | 4386 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 4387 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4388 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|      ! 0 | 4389 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 4390 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4391 | `							return SXERR_ABORT;` |
|        - | 4392 | `						}` |
|      ! 0 | 4393 | `						goto done;` |
|        - | 4394 | `					}` |
|        9 | 4395 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        3 | 4396 | `				}` |
|   105083 | 4397 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 4398 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4399 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|      ! 0 | 4400 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 4401 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 4402 | `						return SXERR_ABORT;` |
|        - | 4403 | `					}` |
|      ! 0 | 4404 | `					goto done;` |
|        - | 4405 | `				}` |
|   105083 | 4406 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|      ! 0 | 4407 | `					pGen->pIn++;` |
|      ! 0 | 4408 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 4409 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4410 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 4411 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4412 | `							return SXERR_ABORT;` |
|        - | 4413 | `						}` |
|      ! 0 | 4414 | `						goto done;` |
|        - | 4415 | `					}` |
|      ! 0 | 4416 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 4417 | `				}else{` |
|   105083 | 4418 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 4419 | `				}` |
|   105083 | 4420 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 4421 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 4422 | `						return SXERR_ABORT;` |
|        - | 4423 | `					}` |
|      ! 0 | 4424 | `					goto done;` |
|        - | 4425 | `				}` |
|        - | 4426 | `			}` |
|    52544 | 4427 | `		}else{` |
|      ! 0 | 4428 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 4429 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 4430 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 4431 | `					return SXERR_ABORT;` |
|        - | 4432 | `				}` |
|      ! 0 | 4433 | `				goto done;` |
|        - | 4434 | `			}` |
|        - | 4435 | `		}` |
|        5 | 4436 | `	}` |
|        - | 4437 | `	/* Reject a php-fatal redeclaration before hoisting the trait */` |
|     7881 | 4438 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|        3 | 4439 | `		return SXERR_ABORT;` |
|        - | 4440 | `	}` |
|        - | 4441 | `	/* Install the trait */` |
|     7879 | 4442 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     7879 | 4443 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 4444 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4445 | `		return SXERR_ABORT;` |
|        - | 4446 | `	}` |
|     3937 | 4447 | `done:` |
|        - | 4448 | `	/* Point beyond the trait body */` |
|     7879 | 4449 | `	pGen->pIn = &pEnd[1];` |
|     7879 | 4450 | `	pGen->pEnd = pTmp;` |
|     7879 | 4451 | `	return PH7_OK;` |
|     3943 | 4452 | `}` |
|        - | 4453 | `/*` |
|        - | 4454 | ` * Compile a user-defined class.` |
|        - | 4455 | ` *  According to the PHP language reference manual` |
|        - | 4456 | ` *   Basic class definitions begin with the keyword class, followed` |
|        - | 4457 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|        - | 4458 | ` *   the definitions of the properties and methods belonging to the class.` |
|        - | 4459 | ` *   A class may contain its own constants, variables (called "properties")` |
|        - | 4460 | ` *   and functions (called "methods").` |
|        - | 4461 | ` */` |
|   406186 | 4462 | `PH7_PRIVATE sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|        5 | 4463 | `{` |
|        - | 4464 | `	sxi32 rc;` |
|   406191 | 4465 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   406191 | 4466 | `	return rc;` |
|        5 | 4467 | `}` |
|        - | 4468 | `/*` |
|        - | 4469 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|        - | 4470 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|        - | 4471 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|        - | 4472 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|        - | 4473 | `` * meaning; `enum Name` can never start a valid expression.`` |
|        - | 4474 | ` */` |
| 14289402 | 4475 | `PH7_PRIVATE int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|        5 | 4476 | `{` |
| 14505939 | 4477 | `	return (pIn->nType & PH7_TK_ID)` |
|  7361233 | 4478 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|   226399 | 4479 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
| 14505934 | 4480 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|        5 | 4481 | `}` |
|        - | 4482 | `/*` |
|        - | 4483 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|        - | 4484 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|        - | 4485 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|        - | 4486 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|        - | 4487 | ` */` |
|     3924 | 4488 | `PH7_PRIVATE sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|        5 | 4489 | `{` |
|     3929 | 4490 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|        5 | 4491 | `}` |
|        - | 4492 |  |
