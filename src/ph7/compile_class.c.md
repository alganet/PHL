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
|        - |  272 | ` * php keeps class CONSTANTS and PROPERTIES in separate namespaces: a class may` |
|        - |  273 | `` * declare `const C` and `public $C` together, and `$obj->C` never resolves to the`` |
|        - |  274 | ` * constant. PHL stores both in the single hAttr table (a constant is just an attr` |
|        - |  275 | ` * carrying PH7_CLASS_ATTR_CONSTANT), so a bare PH7_ClassExtractAttribute() collides` |
|        - |  276 | ` * across the two. These two lookups keep the namespaces apart.` |
|        - |  277 | ` */` |
|  1280048 |  278 | `static ph7_class_attr * GenStateExtractConstant(ph7_class *pClass,SyString *pName)` |
|        5 |  279 | `{` |
|  1280053 |  280 | `	ph7_class_attr *pAttr = PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte);` |
|  1280053 |  281 | `	return ( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) ) ? pAttr : 0;` |
|        5 |  282 | `}` |
|  1530508 |  283 | `static ph7_class_attr * GenStateExtractProperty(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  284 | `{` |
|  1530513 |  285 | `	ph7_class_attr *pAttr = PH7_ClassExtractAttribute(pClass,zName,nByte);` |
|  1530513 |  286 | `	return ( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ) ? pAttr : 0;` |
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
|   726062 |  309 | `PH7_PRIVATE int PH7_GenStateInitHasCallExpr(ph7_gen_state *pGen)` |
|        5 |  310 | `{` |
|   726067 |  311 | `	SyToken *p = pGen->pIn;` |
|   726067 |  312 | `	int iDepth = 0;` |
|        - |  313 | `	/* Conservative ternary bail-out (see the note above). */` |
|        - |  314 | `	{` |
|   726067 |  315 | `		SyToken *q = pGen->pIn;` |
|   726067 |  316 | `		int iQd = 0;` |
|  1918797 |  317 | `		while( q < pGen->pEnd ){` |
|  1918759 |  318 | `			if( iQd == 0 && (q->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   726023 |  319 | `				break;` |
|        - |  320 | `			}` |
|  1192741 |  321 | `			if( q->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    50915 |  322 | `				iQd++;` |
|  1167286 |  323 | `			}else if( q->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|    50915 |  324 | `				if( iQd > 0 ){ iQd--; }` |
|  1116376 |  325 | `			}else if( (q->nType & PH7_TK_OP) && q->pUserData` |
|   393025 |  326 | `				&& ((const ph7_expr_op *)q->pUserData)->iOp == EXPR_OP_QUESTY ){` |
|        8 |  327 | `				return 0;` |
|        - |  328 | `			}` |
|  1192735 |  329 | `			q++;` |
|        5 |  330 | `		}` |
|        - |  331 | `	}` |
|  1917643 |  332 | `	while( p < pGen->pEnd ){` |
|  1917643 |  333 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   726017 |  334 | `			break; /* end of this initializer */` |
|        - |  335 | `		}` |
|  1191626 |  336 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   603634 |  337 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|    15631 |  338 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
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
|  1191627 |  390 | `		if( p->nType & PH7_TK_OCB ){` |
|       43 |  391 | `			if( iDepth == 0 ){` |
|       43 |  392 | `				break; /* property-hook list: the default expression ends here */` |
|        - |  393 | `			}` |
|      ! 0 |  394 | `			iDepth++;` |
|  1191585 |  395 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|        - |  396 | `			/* A '(' directly after a NAME is a call. A name here is an identifier` |
|        - |  397 | ``			 * token; `new X(` is excluded by looking for the `new` operator, and`` |
|        - |  398 | ``			 * `f(...)` (first-class callable) by peeking for a lone ellipsis. */`` |
|    50794 |  399 | `			if( (p->nType & PH7_TK_LPAREN) && p > pGen->pIn` |
|    15622 |  400 | `				&& (p[-1].nType & PH7_TK_ID)` |
|     7823 |  401 | `				&& !((p[-1].nType & PH7_TK_OP) && p[-1].pUserData` |
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
|    50797 |  422 | `			iDepth++;` |
|  1166187 |  423 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|    50797 |  424 | `			if( iDepth > 0 ){` |
|    50797 |  425 | `				iDepth--;` |
|    25396 |  426 | `			}` |
|    25396 |  427 | `		}` |
|  1191583 |  428 | `		p++;` |
|        5 |  429 | `	}` |
|   726059 |  430 | `	return 0;` |
|   363036 |  431 | `}` |
|        - |  432 | `/*` |
|        - |  433 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|        - |  434 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|        - |  435 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|        - |  436 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|        - |  437 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|        - |  438 | ` * share the same backing.` |
|        - |  439 | ` */` |
|    15986 |  440 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|        - |  441 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|        5 |  442 | `{` |
|    15991 |  443 | `	pAttr->nType = nType;` |
|    15991 |  444 | `	pAttr->sClass = *pClass;` |
|    15991 |  445 | `	pAttr->sTypeName = *pTypeName;` |
|    15991 |  446 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  447 | `		sxu32 i;` |
|       72 |  448 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|       50 |  449 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|       50 |  450 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|       27 |  451 | `		}` |
|       11 |  452 | `	}` |
|    15991 |  453 | `}` |
|   343232 |  454 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  455 | `{` |
|   343237 |  456 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  457 | `	SySet *pInstrContainer;` |
|        - |  458 | `	ph7_class_attr *pCons;` |
|        - |  459 | `	SyString *pName;` |
|        - |  460 | `	sxi32 rc;` |
|   343237 |  461 | `	sxu32 nType = 0;` |
|        - |  462 | `	SyString sTypeClass;` |
|        - |  463 | `	SyString sTypeText;` |
|        - |  464 | `	SySet aUnionAlts;` |
|   343237 |  465 | `	sxi32 iTypeFlags = 0;` |
|   343237 |  466 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   343237 |  467 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   343237 |  468 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  469 | `	/* Extract visibility level */` |
|   343237 |  470 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  471 | `	/* Mark as constant */` |
|   343237 |  472 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|   343237 |  473 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        - |  474 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|        - |  475 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|   343256 |  476 | `	if( GenStateClassConstHasType(pGen) ){` |
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
|   171616 |  498 | `loop:` |
|   343239 |  499 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - |  500 | `		/* Invalid constant name */` |
|      ! 0 |  501 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|      ! 0 |  502 | `		if( rc == SXERR_ABORT ){` |
|        - |  503 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  504 | `			return SXERR_ABORT;` |
|        - |  505 | `		}` |
|      ! 0 |  506 | `		goto Synchronize;` |
|        - |  507 | `	}` |
|        - |  508 | `	/* Peek constant name */` |
|   343239 |  509 | `	pName = &pGen->pIn->sData;` |
|        - |  510 | `	/* Make sure the constant name isn't reserved */` |
|   343239 |  511 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  512 | `		/* Reserved constant name */` |
|      ! 0 |  513 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|      ! 0 |  514 | `		if( rc == SXERR_ABORT ){` |
|        - |  515 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  516 | `			return SXERR_ABORT;` |
|        - |  517 | `		}` |
|      ! 0 |  518 | `		goto Synchronize;` |
|        - |  519 | `	}` |
|        - |  520 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|   343239 |  521 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
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
|   343237 |  532 | `	pGen->pIn++;` |
|   343237 |  533 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  534 | `		/* Invalid declaration */` |
|      ! 0 |  535 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|      ! 0 |  536 | `		if( rc == SXERR_ABORT ){` |
|        - |  537 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  538 | `			return SXERR_ABORT;` |
|        - |  539 | `		}` |
|      ! 0 |  540 | `		goto Synchronize;` |
|        - |  541 | `	}` |
|   343237 |  542 | `	pGen->pIn++; /* Jump the equal sign */` |
|        - |  543 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|        - |  544 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|        - |  545 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|        - |  546 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|   343232 |  547 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
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
|   343233 |  559 | `	if( PH7_GenStateInitHasCallExpr(pGen) ){` |
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
|   343233 |  570 | `	if( GenStateInitHasNewExpr(pGen) ){` |
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
|   343224 |  586 | `	if( GenStateExtractConstant(pClass,pName) == 0` |
|   343229 |  587 | `	 && GenStateExtractProperty(pClass,pName->zString,pName->nByte) != 0 ){` |
|      ! 0 |  588 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  589 | `			"Class constant %z::%z collides with a property of the same name"` |
|      ! 0 |  590 | `			" (unsupported: PHL resolves both through one table)",&pClass->sName,pName);` |
|      ! 0 |  591 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  592 | `			return SXERR_ABORT;` |
|        - |  593 | `		}` |
|      ! 0 |  594 | `		goto Synchronize;` |
|        - |  595 | `	}` |
|   343229 |  596 | `	if( GenStateExtractConstant(pClass,pName) != 0 ){` |
|      ! 0 |  597 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  598 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|      ! 0 |  599 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  600 | `			return SXERR_ABORT;` |
|        - |  601 | `		}` |
|      ! 0 |  602 | `		goto Synchronize;` |
|        - |  603 | `	}` |
|        - |  604 | `	/* Allocate a new class attribute */` |
|   343229 |  605 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   343229 |  606 | `	if( pCons ){` |
|   343229 |  607 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|   343229 |  608 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  609 | `			return SXERR_ABORT;` |
|        - |  610 | `		}` |
|   171612 |  611 | `	}` |
|   343229 |  612 | `	if( pCons == 0 ){` |
|      ! 0 |  613 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  614 | `		return SXERR_ABORT;` |
|        - |  615 | `	}` |
|   343229 |  616 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       35 |  617 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       16 |  618 | `	}` |
|        - |  619 | `	/* Swap bytecode container */` |
|   343229 |  620 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   343229 |  621 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|        - |  622 | `	/* Compile constant value.` |
|        - |  623 | `	 */` |
|   343229 |  624 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   343229 |  625 | `	if( rc == SXERR_EMPTY ){` |
|        3 |  626 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|        3 |  627 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  628 | `			return SXERR_ABORT;` |
|        - |  629 | `		}` |
|        1 |  630 | `	}` |
|        - |  631 | `	/* Emit the done instruction */` |
|   343229 |  632 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   343229 |  633 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   343229 |  634 | `	if( rc == SXERR_ABORT ){` |
|        - |  635 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  636 | `		return SXERR_ABORT;` |
|        - |  637 | `	}` |
|        - |  638 | `	/* All done,install the constant */` |
|   343229 |  639 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|   343229 |  640 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  641 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  642 | `		return SXERR_ABORT;` |
|        - |  643 | `	}` |
|   343229 |  644 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
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
|   343227 |  664 | `	SySetRelease(&aUnionAlts);` |
|   343227 |  665 | `	return SXRET_OK;` |
|        5 |  666 | `Synchronize:` |
|       13 |  667 | `	SySetRelease(&aUnionAlts);` |
|        - |  668 | `	/* Synchronize with the first semi-colon */` |
|       45 |  669 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       35 |  670 | `		pGen->pIn++;` |
|        3 |  671 | `	}` |
|       13 |  672 | `	return SXERR_CORRUPT;` |
|   171621 |  673 | `}` |
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
|  2633136 |  703 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|        5 |  704 | `{` |
|  2633141 |  705 | `	SyToken *p = pStart;` |
|  2633141 |  706 | `	int bFirst = 1;` |
|  2633141 |  707 | `	if( p >= pEnd ) return 0;` |
|        - |  708 | ``	/* Optional nullable `?` shorthand. */`` |
|  2633141 |  709 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|       41 |  710 | `		p++;` |
|       41 |  711 | `		if( p >= pEnd ) return 0;` |
|       19 |  712 | `	}` |
|        - |  713 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|        - |  714 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|        - |  715 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|        - |  716 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|  1316568 |  717 | `	for(;;){` |
|  2633161 |  718 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|        - |  719 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|        3 |  720 | `			p++;` |
|        9 |  721 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|        3 |  722 | `			if( p >= pEnd ) return 0;` |
|        3 |  723 | `			p++; /* skip ')' */` |
|        2 |  724 | `		}else{` |
|        - |  725 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|        - |  726 | ``			 * then any `&`-joined intersection members. */`` |
|  2633159 |  727 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  2633159 |  728 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  729 | `				return 0;` |
|        - |  730 | `			}` |
|        - |  731 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|        - |  732 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|        - |  733 | `			 * may still appear at the initial dispatch site). */` |
|  2633159 |  734 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  2633103 |  735 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  2633098 |  736 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|   125238 |  737 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  2617193 |  738 | `					return 0;` |
|        - |  739 | `				}` |
|     7955 |  740 | `			}` |
|    15971 |  741 | `			p++;` |
|    15973 |  742 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  743 | `				p += 2;` |
|        1 |  744 | `			}` |
|    23952 |  745 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|    15974 |  746 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  747 | `				p++; /* skip '&' */` |
|        3 |  748 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|        3 |  749 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|        3 |  750 | `				p++;` |
|        3 |  751 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      ! 0 |  752 | `					p += 2;` |
|      ! 0 |  753 | `				}` |
|        1 |  754 | `			}` |
|        - |  755 | `		}` |
|    15973 |  756 | `		bFirst = 0;` |
|    15968 |  757 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       25 |  758 | `			&& p->sData.zString[0] == '\|' ){` |
|       24 |  759 | ``			p++; /* next `\|`-separated part */`` |
|       24 |  760 | `			continue;` |
|        - |  761 | `		}` |
|    15953 |  762 | `		break;` |
|      ! 0 |  763 | `	}` |
|    15953 |  764 | `	if( p >= pEnd ) return 0;` |
|    15953 |  765 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|  1316573 |  766 | `}` |
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
|    15958 |  786 | `static sxi32 GenStateParsePropertyType(` |
|        - |  787 | `	ph7_gen_state *pGen,` |
|        - |  788 | `	sxu32 *pnType,` |
|        - |  789 | `	SyString *pClass,` |
|        - |  790 | `	sxi32 *piTypeFlags,` |
|        - |  791 | `	SyString *pTypeText,` |
|        - |  792 | `	SySet *pAlts` |
|        5 |  793 | `){` |
|    15963 |  794 | `	sxi32 iFlags = 0;` |
|        - |  795 | `	sxi32 rc;` |
|    15963 |  796 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  797 | `		return SXRET_OK;` |
|        - |  798 | `	}` |
|        - |  799 | `	/* If the first token is '$', there's no type */` |
|    15963 |  800 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      ! 0 |  801 | `		return SXRET_OK;` |
|        - |  802 | `	}` |
|    15963 |  803 | `	rc = GenStateParseUnionTypeDecl(` |
|     7979 |  804 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|        - |  805 | `		PH7_CLASS_ATTR_NULLABLE,` |
|        - |  806 | `		PH7_CLASS_ATTR_UNION,` |
|        - |  807 | `		/* bAllowVoid */ 0,` |
|    15958 |  808 | `		pGen->pIn->nLine);` |
|    15963 |  809 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  810 | `		return rc;` |
|        - |  811 | `	}` |
|        - |  812 | `	/* Verify next token is '$' (start of property name) */` |
|    15963 |  813 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  814 | `		return SXERR_SYNTAX;` |
|        - |  815 | `	}` |
|    15963 |  816 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|    15963 |  817 | `	return SXRET_OK;` |
|     7984 |  818 | `}` |
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
|    16136 |  831 | `static int GenStateIsDisallowedPropertyAtom(` |
|        - |  832 | `	sxu32 nType,` |
|        - |  833 | `	const SyString *pClass,` |
|        - |  834 | `	const char **pzName,` |
|        - |  835 | `	sxu32 *pnName)` |
|        5 |  836 | `{` |
|        - |  837 | `	const char *z;` |
|        - |  838 | `	sxu32 n;` |
|    16141 |  839 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|    16075 |  840 | `		return 0;` |
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
|     8073 |  851 | `}` |
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
|    16074 |  864 | `PH7_PRIVATE sxi32 GenStateValidateMemberType(` |
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
|    16079 |  875 | `	const char *zBad = 0;` |
|    16079 |  876 | `	sxu32 nBad = 0;` |
|        - |  877 | `	SyString sFallback;` |
|        - |  878 | `	const SyString *pBad;` |
|        - |  879 | `	sxi32 rc;` |
|    16079 |  880 | `	int bDisallowed = 0;` |
|    16079 |  881 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|        5 |  882 | `		bDisallowed = 1;` |
|    16077 |  883 | `	}else if( pUnionAlts ){` |
|        - |  884 | `		sxu32 i;` |
|       94 |  885 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|       66 |  886 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|       66 |  887 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|        3 |  888 | `				bDisallowed = 1;` |
|        3 |  889 | `				break;` |
|        - |  890 | `			}` |
|       34 |  891 | `		}` |
|       15 |  892 | `	}` |
|    16079 |  893 | `	if( !bDisallowed ){` |
|    16073 |  894 | `		return SXRET_OK;` |
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
|     8042 |  912 | `}` |
|        - |  913 | `/*` |
|        - |  914 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|        - |  915 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|        - |  916 | ` * matched as a plain identifier in the class-member modifier position rather` |
|        - |  917 | ` * than promoted to a lexer keyword.` |
|        - |  918 | ` */` |
| 23631588 |  919 | `PH7_PRIVATE int GenStateIsReadonly(SyToken *pTok)` |
|        5 |  920 | `{` |
| 23858535 |  921 | `	return (pTok->nType & PH7_TK_ID)` |
| 12042736 |  922 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 23858530 |  923 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|        5 |  924 | `}` |
|        - |  925 | `/*` |
|        - |  926 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|        - |  927 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|        - |  928 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|        - |  929 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|        - |  930 | ` */` |
|  8485888 |  931 | `PH7_PRIVATE sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|        5 |  932 | `{` |
|  8485893 |  933 | `	*pnTok = 0;` |
|  8485888 |  934 | `	if( &pTok[3] < pEnd` |
|  7933468 |  935 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|  6470079 |  936 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|  2779563 |  937 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       16 |  938 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|       16 |  939 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|       21 |  940 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|       17 |  941 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|       17 |  942 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|       17 |  943 | `			*pnTok = 4;` |
|       17 |  944 | `			return nKw;` |
|        - |  945 | `		}` |
|      ! 0 |  946 | `	}` |
|  8485877 |  947 | `	return 0;` |
|  4242949 |  948 | `}` |
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
|   593610 |  960 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  961 | `{` |
|   593615 |  962 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  963 | `	ph7_class_attr *pAttr;` |
|        - |  964 | `	SyString *pName;` |
|        - |  965 | `	sxi32 rc;` |
|   593615 |  966 | `	sxu32 nType = 0;` |
|        - |  967 | `	SyString sTypeClass;` |
|        - |  968 | `	SyString sTypeText;` |
|        - |  969 | `	SySet aUnionAlts;` |
|   593615 |  970 | `	sxi32 iTypeFlags = 0;` |
|   593615 |  971 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   593615 |  972 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   593615 |  973 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  974 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|        - |  975 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|        - |  976 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   593615 |  977 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|       21 |  978 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        9 |  979 | `	}` |
|        - |  980 | `	/* Extract visibility level */` |
|   593615 |  981 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  982 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   601594 |  983 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|    15963 |  984 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|    15963 |  985 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  986 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  987 | `			goto Synchronize;` |
|    15963 |  988 | `		}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  989 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  990 | `				"Invalid property type or declaration near '%z'",` |
|      ! 0 |  991 | `				&pGen->pIn->sData);` |
|      ! 0 |  992 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  993 | `				return SXERR_ABORT;` |
|        - |  994 | `			}` |
|      ! 0 |  995 | `			goto Synchronize;` |
|    15963 |  996 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  997 | `			return SXERR_ABORT;` |
|        - |  998 | `		}` |
|     7979 |  999 | `	}` |
|      ! 0 | 1000 | `loop:` |
|   593619 | 1001 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 1002 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|      ! 0 | 1003 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1004 | `			return SXERR_ABORT;` |
|        - | 1005 | `		}` |
|      ! 0 | 1006 | `		goto Synchronize;` |
|        - | 1007 | `	}` |
|   593619 | 1008 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   593619 | 1009 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        - | 1010 | `		/* Invalid attribute name */` |
|      ! 0 | 1011 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|      ! 0 | 1012 | `		if( rc == SXERR_ABORT ){` |
|        - | 1013 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1014 | `			return SXERR_ABORT;` |
|        - | 1015 | `		}` |
|      ! 0 | 1016 | `		goto Synchronize;` |
|        - | 1017 | `	}` |
|        - | 1018 | `	/* Peek attribute name */` |
|   593619 | 1019 | `	pName = &pGen->pIn->sData;` |
|        - | 1020 | `	/* Advance the stream cursor */` |
|   593619 | 1021 | `	pGen->pIn++;` |
|   593619 | 1022 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|        - | 1023 | `		/* Invalid declaration */` |
|        3 | 1024 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|        3 | 1025 | `		if( rc == SXERR_ABORT ){` |
|        - | 1026 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1027 | `			return SXERR_ABORT;` |
|        - | 1028 | `		}` |
|        3 | 1029 | `		goto Synchronize;` |
|        - | 1030 | `	}` |
|        - | 1031 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|        - | 1032 | `	 * the read visibility must not be narrower than the set visibility. */` |
|   593617 | 1033 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|       13 | 1034 | `		const char *zAvErr = 0;` |
|       19 | 1035 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|       10 | 1036 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|        2 | 1037 | `			: PH7_CLASS_PROT_PUBLIC;` |
|       13 | 1038 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 | 1039 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|       13 | 1040 | `		}else if( iProtection > iSetLevel ){` |
|      ! 0 | 1041 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|      ! 0 | 1042 | `		}` |
|       13 | 1043 | `		if( zAvErr ){` |
|      ! 0 | 1044 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|      ! 0 | 1045 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1046 | `				return SXERR_ABORT;` |
|        - | 1047 | `			}` |
|      ! 0 | 1048 | `			goto Synchronize;` |
|        - | 1049 | `		}` |
|        6 | 1050 | `	}` |
|        - | 1051 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|        - | 1052 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   593617 | 1053 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       51 | 1054 | `		const char *zRoErr = 0;` |
|       51 | 1055 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        3 | 1056 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|       50 | 1057 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        6 | 1058 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|       47 | 1059 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|        6 | 1060 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|        2 | 1061 | `		}` |
|       51 | 1062 | `		if( zRoErr ){` |
|       13 | 1063 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|       13 | 1064 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1065 | `				return SXERR_ABORT;` |
|        - | 1066 | `			}` |
|       13 | 1067 | `			goto Synchronize;` |
|        - | 1068 | `		}` |
|       18 | 1069 | `	}` |
|        - | 1070 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|        - | 1071 | `	 * type atom or any union alternative. void/never are already rejected` |
|        - | 1072 | `	 * by the type parser. */` |
|   593607 | 1073 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|    23939 | 1074 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|        - | 1075 | `			&sTypeText,` |
|    15956 | 1076 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|     7978 | 1077 | `			"Property %z::$%z cannot have type %z",nLine);` |
|    15961 | 1078 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1079 | `			return SXERR_ABORT;` |
|    15961 | 1080 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 | 1081 | `			goto Synchronize;` |
|        - | 1082 | `		}` |
|     7978 | 1083 | `	}` |
|        - | 1084 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|        - | 1085 | `	/* Mirror of the class-constant path: same-name const + property is valid php` |
|        - | 1086 | `	 * that PHL cannot represent, so reject it loudly rather than resolve wrongly. */` |
|   593602 | 1087 | `	if( GenStateExtractProperty(pClass,pName->zString,pName->nByte) == 0` |
|   593606 | 1088 | `	 && GenStateExtractConstant(pClass,pName) != 0 ){` |
|      ! 0 | 1089 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1090 | `			"Property %z::$%z collides with a class constant of the same name"` |
|      ! 0 | 1091 | `			" (unsupported: PHL resolves both through one table)",&pClass->sName,pName);` |
|      ! 0 | 1092 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1093 | `			return SXERR_ABORT;` |
|        - | 1094 | `		}` |
|      ! 0 | 1095 | `		goto Synchronize;` |
|        - | 1096 | `	}` |
|   593607 | 1097 | `	if( GenStateExtractProperty(pClass,pName->zString,pName->nByte) != 0 ){` |
|        4 | 1098 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1099 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|        3 | 1100 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1101 | `			return SXERR_ABORT;` |
|        - | 1102 | `		}` |
|        3 | 1103 | `		goto Synchronize;` |
|        - | 1104 | `	}` |
|        - | 1105 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|        - | 1106 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|        - | 1107 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|        - | 1108 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|        - | 1109 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|        - | 1110 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|        - | 1111 | `	/* php: a property default may not CALL anything either. */` |
|   593605 | 1112 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && PH7_GenStateInitHasCallExpr(pGen) ){` |
|      ! 0 | 1113 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1114 | `			"Constant expression contains invalid operations");` |
|      ! 0 | 1115 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1116 | `			return SXERR_ABORT;` |
|        - | 1117 | `		}` |
|      ! 0 | 1118 | `		goto Synchronize;` |
|        - | 1119 | `	}` |
|   593605 | 1120 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|        6 | 1121 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1122 | `			"New expressions are not supported in this context");` |
|        6 | 1123 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1124 | `			return SXERR_ABORT;` |
|        - | 1125 | `		}` |
|        6 | 1126 | `		goto Synchronize;` |
|        - | 1127 | `	}` |
|        - | 1128 | `	/* Allocate a new class attribute */` |
|   593601 | 1129 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   593601 | 1130 | `	if( pAttr ){` |
|   593601 | 1131 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|   593601 | 1132 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 1133 | `			return SXERR_ABORT;` |
|        - | 1134 | `		}` |
|   296798 | 1135 | `	}` |
|   593601 | 1136 | `	if( pAttr == 0 ){` |
|      ! 0 | 1137 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1138 | `		return SXERR_ABORT;` |
|        - | 1139 | `	}` |
|   593601 | 1140 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|    15959 | 1141 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|     7977 | 1142 | `	}` |
|   593601 | 1143 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|        - | 1144 | `		SySet *pInstrContainer;` |
|   382785 | 1145 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|   382785 | 1146 | `		pGen->pIn++; /*Jump the equal sign */` |
|        - | 1147 | `		{` |
|        - | 1148 | `			/* Delimit the default expression: it ends at the declaration's` |
|        - | 1149 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|        - | 1150 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|        - | 1151 | `			 * compiler would otherwise run into the hook tokens. */` |
|   382785 | 1152 | `			SyToken *pScan = pGen->pIn;` |
|   382785 | 1153 | `			sxi32 iNest = 0;` |
|   844087 | 1154 | `			while( pScan < pGen->pEnd ){` |
|   844087 | 1155 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|    50767 | 1156 | `					iNest++;` |
|   818706 | 1157 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|    50767 | 1158 | `					iNest--;` |
|   767944 | 1159 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|   382785 | 1160 | `					break;` |
|        - | 1161 | `				}` |
|   461307 | 1162 | `				pScan++;` |
|        5 | 1163 | `			}` |
|   382785 | 1164 | `			pGen->pEnd = pScan;` |
|        - | 1165 | `		}` |
|        - | 1166 | `		/* Swap bytecode container */` |
|   382785 | 1167 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   382785 | 1168 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|        - | 1169 | `		/* Compile attribute value.` |
|        - | 1170 | `		 */` |
|   382785 | 1171 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   382785 | 1172 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 1173 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|      ! 0 | 1174 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1175 | `				return SXERR_ABORT;` |
|        - | 1176 | `			}` |
|      ! 0 | 1177 | `		}` |
|        - | 1178 | `		/* Emit the done instruction */` |
|   382785 | 1179 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   382785 | 1180 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   382785 | 1181 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|   382785 | 1182 | `		pGen->pEnd = pSavedDefEnd;` |
|   191390 | 1183 | `	}` |
|        - | 1184 | `	/* All done,install the attribute */` |
|   593601 | 1185 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   593601 | 1186 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1187 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1188 | `		return SXERR_ABORT;` |
|        - | 1189 | `	}` |
|   593601 | 1190 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|        - | 1191 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|        - | 1192 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|       95 | 1193 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|       95 | 1194 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1195 | `			return SXERR_ABORT;` |
|        - | 1196 | `		}` |
|       95 | 1197 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1198 | `			goto Synchronize;` |
|        - | 1199 | `		}` |
|       95 | 1200 | `		SySetRelease(&aUnionAlts);` |
|       95 | 1201 | `		return SXRET_OK;` |
|        - | 1202 | `	}` |
|   593507 | 1203 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - | 1204 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|        - | 1205 | `		 * wording differs per declaration site) */` |
|      ! 0 | 1206 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 1207 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|        - | 1208 | `				? "Interfaces may only include hooked properties"` |
|        - | 1209 | `				: "Only hooked properties may be declared abstract");` |
|      ! 0 | 1210 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1211 | `			return SXERR_ABORT;` |
|        - | 1212 | `		}` |
|      ! 0 | 1213 | `		goto Synchronize;` |
|        - | 1214 | `	}` |
|   593507 | 1215 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - | 1216 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|        5 | 1217 | `		pGen->pIn++; /* Jump the comma */` |
|        5 | 1218 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 | 1219 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 | 1220 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 | 1221 | `				pTok--;` |
|      ! 0 | 1222 | `			}` |
|      ! 0 | 1223 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 1224 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|      ! 0 | 1225 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 | 1226 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1227 | `				return SXERR_ABORT;` |
|        - | 1228 | `			}` |
|      ! 0 | 1229 | `		}else{` |
|        5 | 1230 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        5 | 1231 | `				goto loop;` |
|        - | 1232 | `			}` |
|        - | 1233 | `		}` |
|      ! 0 | 1234 | `	}` |
|   593503 | 1235 | `	SySetRelease(&aUnionAlts);` |
|   593503 | 1236 | `	return SXRET_OK;` |
|        9 | 1237 | `Synchronize:` |
|        - | 1238 | `	/* Synchronize with the first semi-colon */` |
|       56 | 1239 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       38 | 1240 | `		pGen->pIn++;` |
|        4 | 1241 | `	}` |
|       22 | 1242 | `	SySetRelease(&aUnionAlts);` |
|       22 | 1243 | `	return SXERR_CORRUPT;` |
|   296810 | 1244 | `}` |
|        - | 1245 | `/*` |
|        - | 1246 | ` * Compile a class method.` |
|        - | 1247 | ` *` |
|        - | 1248 | ` * Refer to the official documentation for more information` |
|        - | 1249 | ` * on the powerful extension introduced by the PH7 engine` |
|        - | 1250 | ` * to the OO subsystem such as full type hinting,method` |
|        - | 1251 | ` * overloading and many more.` |
|        - | 1252 | ` */` |
|  2909798 | 1253 | `static sxi32 GenStateCompileClassMethod(` |
|        - | 1254 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 1255 | `	sxi32 iProtection,   /* Visibility level */` |
|        - | 1256 | `	sxi32 iFlags,        /* Configuration flags */` |
|        - | 1257 | `	int doBody,          /* TRUE to process method body */` |
|        - | 1258 | `	ph7_class *pClass    /* Class this method belongs */` |
|        - | 1259 | `	)` |
|        5 | 1260 | `{` |
|  2909803 | 1261 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  2909803 | 1262 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|        - | 1263 | `	ph7_class_method *pMeth;` |
|        - | 1264 | `	sxi32 iFuncFlags;` |
|        - | 1265 | `	SyString *pName;` |
|        - | 1266 | `	SyToken *pEnd;` |
|        - | 1267 | `	sxi32 rc;` |
|        - | 1268 | `	/* Extract visibility level */` |
|  2909803 | 1269 | `	iProtection = GetProtectionLevel(iProtection);` |
|  2909803 | 1270 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  2909803 | 1271 | `	iFuncFlags = 0;` |
|  2909803 | 1272 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1273 | `		/* Invalid method name */` |
|      ! 0 | 1274 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|      ! 0 | 1275 | `		if( rc == SXERR_ABORT ){` |
|        - | 1276 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1277 | `			return SXERR_ABORT;` |
|        - | 1278 | `		}` |
|      ! 0 | 1279 | `		goto Synchronize;` |
|        - | 1280 | `	}` |
|  2909803 | 1281 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - | 1282 | `		/* Return by reference,remember that */` |
|      ! 0 | 1283 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|        - | 1284 | `		/* Jump the '&' token */` |
|      ! 0 | 1285 | `		pGen->pIn++;` |
|      ! 0 | 1286 | `	}` |
|  2909803 | 1287 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 1288 | `		/* Invalid method name */` |
|      ! 0 | 1289 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|      ! 0 | 1290 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1291 | `			return SXERR_ABORT;` |
|        - | 1292 | `		}` |
|      ! 0 | 1293 | `		goto Synchronize;` |
|        - | 1294 | `	}` |
|        - | 1295 | `	/* Peek method name */` |
|  2909803 | 1296 | `	pName = &pGen->pIn->sData;` |
|  2909803 | 1297 | `	nLine = pGen->pIn->nLine;` |
|        - | 1298 | `	/* Jump the method name */` |
|  2909803 | 1299 | `	pGen->pIn++;` |
|  2909803 | 1300 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - | 1301 | `		/* Abstract method */` |
|   140409 | 1302 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|      ! 0 | 1303 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1304 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|      ! 0 | 1305 | `				&pClass->sName,pName);` |
|      ! 0 | 1306 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1307 | `				return SXERR_ABORT;` |
|        - | 1308 | `			}` |
|      ! 0 | 1309 | `		}` |
|        - | 1310 | `		/* Assemble method signature only */` |
|   140409 | 1311 | `		doBody = FALSE;` |
|    70202 | 1312 | `	}` |
|  2909803 | 1313 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1314 | `		/* Syntax error */` |
|      ! 0 | 1315 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|      ! 0 | 1316 | `		if( rc == SXERR_ABORT ){` |
|        - | 1317 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1318 | `			return SXERR_ABORT;` |
|        - | 1319 | `		}` |
|      ! 0 | 1320 | `		goto Synchronize;` |
|        - | 1321 | `	}` |
|        - | 1322 | `	/* Allocate a new class_method instance */` |
|  2909803 | 1323 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  2909803 | 1324 | `	if( pMeth == 0 ){` |
|      ! 0 | 1325 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1326 | `		return SXERR_ABORT;` |
|        - | 1327 | `	}` |
|  2909803 | 1328 | `	pMeth->sFunc.nLine = nKwLine;` |
|  2909803 | 1329 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|  2909803 | 1330 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 1331 | `		return SXERR_ABORT;` |
|        - | 1332 | `	}` |
|        - | 1333 | `	/* Jump the left parenthesis '(' */` |
|  2909803 | 1334 | `	pGen->pIn++;` |
|  2909803 | 1335 | `	pEnd = 0; /* cc warning */` |
|        - | 1336 | `	/* Delimit the method signature */` |
|  2909803 | 1337 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  2909803 | 1338 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 1339 | `		/* Syntax error */` |
|        3 | 1340 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|        3 | 1341 | `		if( rc == SXERR_ABORT ){` |
|        - | 1342 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1343 | `			return SXERR_ABORT;` |
|        - | 1344 | `		}` |
|        3 | 1345 | `		goto Synchronize;` |
|        - | 1346 | `	}` |
|        - | 1347 | `	{` |
|  2909801 | 1348 | `		int bIsCtor = 0;` |
|  2909801 | 1349 | `		int bAbstractCtor = 0;` |
|  2909796 | 1350 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  1708439 | 1351 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  2806391 | 1352 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   206825 | 1353 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        3 | 1354 | `				bAbstractCtor = 1;` |
|        2 | 1355 | `			}else{` |
|   206823 | 1356 | `				bIsCtor = 1;` |
|        - | 1357 | `			}` |
|   103410 | 1358 | `		}` |
|  2909801 | 1359 | `		if( pGen->pIn < pEnd ){` |
|        - | 1360 | `			/* Collect method arguments */` |
|  1119471 | 1361 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|  1119471 | 1362 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1363 | `				return SXERR_ABORT;` |
|        - | 1364 | `			}` |
|   559733 | 1365 | `		}` |
|        - | 1366 | `	}` |
|        - | 1367 | `	/* Point past ')' and parse optional return type ': type' */` |
|  2909801 | 1368 | `	pGen->pIn = &pEnd[1];` |
|        - | 1369 | `	{` |
|  2909801 | 1370 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  2909801 | 1371 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 | 1372 | `			return SXERR_ABORT;` |
|  2909801 | 1373 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|      ! 0 | 1374 | `			goto Synchronize;` |
|        - | 1375 | `		}` |
|        - | 1376 | `	}` |
|        - | 1377 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|        - | 1378 | `	 * property init/typecheck is handled by the generic typed-property path` |
|        - | 1379 | `	 * since we mint real ph7_class_attr entries. */` |
|        - | 1380 | `	{` |
|  2909801 | 1381 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|        - | 1382 | `		sxu32 i;` |
|  4582977 | 1383 | `		for( i = 0; i < nArg; i++ ){` |
|  1673191 | 1384 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|        - | 1385 | `			ph7_class_attr *pAttr;` |
|  1673191 | 1386 | `			sxi32 iAttrFlags = 0;` |
|        - | 1387 | `			int bArgTyped;` |
|  1673191 | 1388 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  1673105 | 1389 | `				continue;` |
|        - | 1390 | `			}` |
|        - | 1391 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|        - | 1392 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|        - | 1393 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|       60 | 1394 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|       92 | 1395 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|       91 | 1396 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        3 | 1397 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1398 | `					"Cannot declare variadic promoted property");` |
|        3 | 1399 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1400 | `					return SXERR_ABORT;` |
|        - | 1401 | `				}` |
|        3 | 1402 | `				goto Synchronize;` |
|        - | 1403 | `			}` |
|        - | 1404 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|        - | 1405 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|        - | 1406 | `			 * appear as an alternative of a union type. */` |
|       89 | 1407 | `			if( bArgTyped ){` |
|      125 | 1408 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|       80 | 1409 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|       80 | 1410 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|       40 | 1411 | `					"Property %z::$%z cannot have type %z",nLine);` |
|       85 | 1412 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1413 | `					return SXERR_ABORT;` |
|       85 | 1414 | `				}else if( rc != SXRET_OK ){` |
|        6 | 1415 | `					goto Synchronize;` |
|        - | 1416 | `				}` |
|       38 | 1417 | `			}` |
|        - | 1418 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|       85 | 1419 | `			if( GenStateExtractProperty(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|        4 | 1420 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1421 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|        3 | 1422 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1423 | `					return SXERR_ABORT;` |
|        - | 1424 | `				}` |
|        3 | 1425 | `				goto Synchronize;` |
|        - | 1426 | `			}` |
|       83 | 1427 | `			if( bArgTyped ){` |
|       79 | 1428 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       37 | 1429 | `			}` |
|       83 | 1430 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|        3 | 1431 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|        1 | 1432 | `			}` |
|       83 | 1433 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|        8 | 1434 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|        3 | 1435 | `			}` |
|       83 | 1436 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|        - | 1437 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|        - | 1438 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|       26 | 1439 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        4 | 1440 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1441 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|        3 | 1442 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1443 | `						return SXERR_ABORT;` |
|        - | 1444 | `					}` |
|        3 | 1445 | `					goto Synchronize;` |
|        - | 1446 | `				}` |
|       24 | 1447 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       10 | 1448 | `			}` |
|       81 | 1449 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|        - | 1450 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|        5 | 1451 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 | 1452 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1453 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|      ! 0 | 1454 | `						&pClass->sName,&pArg->sName);` |
|      ! 0 | 1455 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1456 | `						return SXERR_ABORT;` |
|        - | 1457 | `					}` |
|      ! 0 | 1458 | `					goto Synchronize;` |
|        - | 1459 | `				}` |
|        5 | 1460 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|        2 | 1461 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|        2 | 1462 | `			}` |
|       81 | 1463 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|       81 | 1464 | `			if( pAttr == 0 ){` |
|      ! 0 | 1465 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1466 | `				return SXERR_ABORT;` |
|        - | 1467 | `			}` |
|       81 | 1468 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|       79 | 1469 | `				pAttr->nType = pArg->nType;` |
|       79 | 1470 | `				pAttr->sClass = pArg->sClass;` |
|       79 | 1471 | `				pAttr->sTypeName = pArg->sTypeName;` |
|       79 | 1472 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|        - | 1473 | `					sxu32 k;` |
|       20 | 1474 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|       14 | 1475 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|       14 | 1476 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|        8 | 1477 | `					}` |
|        3 | 1478 | `				}` |
|       37 | 1479 | `			}` |
|       81 | 1480 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|       81 | 1481 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1482 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1483 | `				return SXERR_ABORT;` |
|        - | 1484 | `			}` |
|       43 | 1485 | `		}` |
|        - | 1486 | `	}` |
|  2909791 | 1487 | `	if( doBody ){` |
|        - | 1488 | `		/* Compile method body */` |
|  2769387 | 1489 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  2769387 | 1490 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1491 | `			return SXERR_ABORT;` |
|        - | 1492 | `		}` |
|        - | 1493 | `		/* The cursor sits just past the body's closing brace */` |
|  2769387 | 1494 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|  1384696 | 1495 | `	}else{` |
|        - | 1496 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|   140409 | 1497 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|   140409 | 1498 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|    70202 | 1499 | `		}` |
|        - | 1500 | `		/* Only method signature is allowed */` |
|   140409 | 1501 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|      ! 0 | 1502 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 1503 | `				"Expected ';' after method signature '%z'",pName);` |
|      ! 0 | 1504 | `				if( rc == SXERR_ABORT ){` |
|        - | 1505 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 | 1506 | `					return SXERR_ABORT;` |
|        - | 1507 | `				}` |
|      ! 0 | 1508 | `				return SXERR_CORRUPT;` |
|        - | 1509 | `			}` |
|        - | 1510 | `	}` |
|        - | 1511 | `	/* php: an abstract method in a class that was not DECLARED abstract is a fatal,` |
|        - | 1512 | `	 * and it names the method -- distinct from the "contains N abstract methods"` |
|        - | 1513 | `	 * wording php uses for an unimplemented INHERITED one, which` |
|        - | 1514 | `	 * GenStateCheckAbstractMethods already handles. Interfaces and traits may carry` |
|        - | 1515 | `	 * abstract methods freely. */` |
|  2909786 | 1516 | `	if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT)` |
|  1525100 | 1517 | `	 && (pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|        4 | 1518 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1519 | `			"Class %z declares abstract method %z() and must therefore be declared abstract",` |
|        1 | 1520 | `			&pClass->sName,pName);` |
|        3 | 1521 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1522 | `			return SXERR_ABORT;` |
|        - | 1523 | `		}` |
|        3 | 1524 | `		return SXRET_OK;` |
|        - | 1525 | `	}` |
|        - | 1526 | `	/* php: two methods of the same name in one class body is a fatal, and the` |
|        - | 1527 | ``	 * comparison is case-INSENSITIVE (hMethod now matches that way), so `f` and`` |
|        - | 1528 | ``	 * `F` collide. php names the offending declaration with the spelling used at`` |
|        - | 1529 | `	 * the SECOND site. */` |
|  2909789 | 1530 | `	if( PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte) != 0 ){` |
|        8 | 1531 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        2 | 1532 | `			"Cannot redeclare %z::%z()",&pClass->sName,pName);` |
|        6 | 1533 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1534 | `			return SXERR_ABORT;` |
|        - | 1535 | `		}` |
|        6 | 1536 | `		return SXRET_OK;` |
|        - | 1537 | `	}` |
|        - | 1538 | `	/* All done,install the method */` |
|  2909785 | 1539 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  2909785 | 1540 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1541 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1542 | `		return SXERR_ABORT;` |
|        - | 1543 | `	}` |
|  2909785 | 1544 | `	return SXRET_OK;` |
|        6 | 1545 | `Synchronize:` |
|        - | 1546 | `	/* Synchronize with the first semi-colon */` |
|       40 | 1547 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       28 | 1548 | `		pGen->pIn++;` |
|        4 | 1549 | `	}` |
|       16 | 1550 | `	return SXERR_CORRUPT;` |
|  1454904 | 1551 | `}` |
|        - | 1552 | `/*` |
|        - | 1553 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|        - | 1554 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|        - | 1555 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|        - | 1556 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|        - | 1557 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|        - | 1558 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|        - | 1559 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|        - | 1560 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|        - | 1561 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|        - | 1562 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|        - | 1563 | `` * implicit `$value` formal.`` |
|        - | 1564 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|        - | 1565 | ` */` |
|        - | 1566 | `/*` |
|        - | 1567 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|        - | 1568 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|        - | 1569 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|        - | 1570 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|        - | 1571 | ` * allowed, excluded from the raw object surfaces.` |
|        - | 1572 | ` */` |
|       94 | 1573 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|        1 | 1574 | `{` |
|        - | 1575 | `	SyToken *p;` |
|      345 | 1576 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|      303 | 1577 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      223 | 1578 | `			continue;` |
|        - | 1579 | `		}` |
|        - | 1580 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|       80 | 1581 | `		if( p + 3 < pEnd` |
|       80 | 1582 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       80 | 1583 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|       73 | 1584 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|       66 | 1585 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|       66 | 1586 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       66 | 1587 | `		 && p[3].sData.nByte == pName->nByte` |
|       60 | 1588 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       51 | 1589 | `			return 1;` |
|        - | 1590 | `		}` |
|        - | 1591 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|        - | 1592 | `		 * hook operates on the shared per-instance backing store, so the` |
|        - | 1593 | `		 * property is backed (php compiles a default alongside it). */` |
|       30 | 1594 | `		if( p > pStart` |
|       26 | 1595 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|       12 | 1596 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        2 | 1597 | `		 && p[1].sData.nByte == pName->nByte` |
|        3 | 1598 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        3 | 1599 | `			return 1;` |
|        - | 1600 | `		}` |
|       15 | 1601 | `	}` |
|       43 | 1602 | `	return 0;` |
|       48 | 1603 | `}` |
|        - | 1604 | `/*` |
|        - | 1605 | ` * True when p opens php 8.4's parent-hook call form` |
|        - | 1606 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|        - | 1607 | ` */` |
|      990 | 1608 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|        1 | 1609 | `{` |
|     1167 | 1610 | `	return p + 6 < pEnd` |
|      671 | 1611 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|      250 | 1612 | `	 && p->sData.nByte == sizeof("parent")-1` |
|       81 | 1613 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|       11 | 1614 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|        8 | 1615 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|        8 | 1616 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        8 | 1617 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|        8 | 1618 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        8 | 1619 | `	 && p[5].sData.nByte == 3` |
|        8 | 1620 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|        6 | 1621 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|     1166 | 1622 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|        1 | 1623 | `}` |
|        - | 1624 | `/*` |
|        - | 1625 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|        - | 1626 | ` * hook body into calls of the parent class's synthesized hook method` |
|        - | 1627 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|        - | 1628 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|        - | 1629 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|        - | 1630 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|        - | 1631 | ` * or SXERR_MEM.` |
|        - | 1632 | ` */` |
|        4 | 1633 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|        - | 1634 | `	SyToken *pStart,SyToken *pEnd)` |
|        1 | 1635 | `{` |
|        5 | 1636 | `	SyToken *p = pStart;` |
|       35 | 1637 | `	while( p < pEnd ){` |
|       31 | 1638 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|        - | 1639 | `			SyToken sTok;` |
|        - | 1640 | `			char zName[384];` |
|        - | 1641 | `			sxu32 nName;` |
|        - | 1642 | `			char *zDup;` |
|        - | 1643 | ``			/* `parent` `::` */`` |
|        5 | 1644 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|        5 | 1645 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|        7 | 1646 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|        4 | 1647 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|        5 | 1648 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|        5 | 1649 | `			if( zDup == 0 ){` |
|      ! 0 | 1650 | `				return SXERR_MEM;` |
|        - | 1651 | `			}` |
|        5 | 1652 | `			sTok = p[3]; /* keep the line info of the property name */` |
|        5 | 1653 | `			sTok.nType = PH7_TK_ID;` |
|        5 | 1654 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|        5 | 1655 | `			sTok.pUserData = 0;` |
|        5 | 1656 | `			SySetPut(pCopy,(const void *)&sTok);` |
|        5 | 1657 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|        5 | 1658 | `			continue;` |
|        - | 1659 | `		}` |
|       27 | 1660 | `		SySetPut(pCopy,(const void *)p);` |
|       27 | 1661 | `		p++;` |
|        1 | 1662 | `	}` |
|        5 | 1663 | `	return SXRET_OK;` |
|        3 | 1664 | `}` |
|       94 | 1665 | `PH7_PRIVATE sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 | 1666 | `{` |
|       95 | 1667 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 1668 | `	sxi32 rc;` |
|       95 | 1669 | `	int bRefsSelf = 0;` |
|       95 | 1670 | `	pGen->pIn++; /* Jump '{' */` |
|      253 | 1671 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|        - | 1672 | `		char zHook[384];` |
|        - | 1673 | `		SyString sHookName;` |
|        - | 1674 | `		ph7_class_method *pMeth;` |
|        - | 1675 | `		int bGet;` |
|      159 | 1676 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|      159 | 1677 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       15 | 1678 | `			pGen->pIn++; /* stray ';' between hooks */` |
|       22 | 1679 | `			continue;` |
|        - | 1680 | `		}` |
|      145 | 1681 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - | 1682 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|      ! 0 | 1683 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 1684 | `				"By-reference property hooks are not supported for %z::$%z",` |
|      ! 0 | 1685 | `				&pClass->sName,&pAttr->sName);` |
|      ! 0 | 1686 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1687 | `				return SXERR_ABORT;` |
|        - | 1688 | `			}` |
|      ! 0 | 1689 | `			return SXERR_CORRUPT;` |
|        - | 1690 | `		}` |
|      145 | 1691 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 1692 | `			goto HookSyntax;` |
|        - | 1693 | `		}` |
|      144 | 1694 | `		if( pGen->pIn->sData.nByte == 3` |
|      145 | 1695 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|       79 | 1696 | `			bGet = 1;` |
|      106 | 1697 | `		}else if( pGen->pIn->sData.nByte == 3` |
|       67 | 1698 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|       67 | 1699 | `			bGet = 0;` |
|       34 | 1700 | `		}else{` |
|      ! 0 | 1701 | `			goto HookSyntax;` |
|        - | 1702 | `		}` |
|      145 | 1703 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|      145 | 1704 | `		sHookName.zString = zHook;` |
|      217 | 1705 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|       72 | 1706 | `			bGet ? "get" : "set",&pAttr->sName);` |
|      145 | 1707 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|        - | 1708 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|        - | 1709 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|        - | 1710 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|        - | 1711 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|        - | 1712 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|       14 | 1713 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|        8 | 1714 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 1715 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 1716 | `					"Non-abstract property hook must have a body");` |
|      ! 0 | 1717 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1718 | `					return SXERR_ABORT;` |
|        - | 1719 | `				}` |
|      ! 0 | 1720 | `				return SXERR_CORRUPT;` |
|        - | 1721 | `			}` |
|       15 | 1722 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|        - | 1723 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|       15 | 1724 | `			if( pMeth == 0 ){` |
|      ! 0 | 1725 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1726 | `				return SXERR_ABORT;` |
|        - | 1727 | `			}` |
|       15 | 1728 | `			pMeth->sFunc.nLine = nHLine;` |
|       15 | 1729 | `			if( !bGet ){` |
|        - | 1730 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|        - | 1731 | `				 * compatible with concrete set-hook implementations (which` |
|        - | 1732 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|        - | 1733 | `				 * type (php: the abstract set's parameter type IS the property` |
|        - | 1734 | `				 * type), so the override contravariance check accepts a typed` |
|        - | 1735 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|        - | 1736 | `				ph7_vm_func_arg sVArg;` |
|        7 | 1737 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        7 | 1738 | `				if( zVName == 0 ){` |
|      ! 0 | 1739 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1740 | `					return SXERR_ABORT;` |
|        - | 1741 | `				}` |
|        7 | 1742 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        7 | 1743 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        7 | 1744 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        7 | 1745 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        7 | 1746 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        7 | 1747 | `				sVArg.nType = pAttr->nType;` |
|        7 | 1748 | `				sVArg.sClass = pAttr->sClass;` |
|        7 | 1749 | `				sVArg.sTypeName = pAttr->sTypeName;` |
|        7 | 1750 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|      ! 0 | 1751 | `					sVArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|      ! 0 | 1752 | `				}` |
|        7 | 1753 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        3 | 1754 | `			}` |
|       15 | 1755 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       15 | 1756 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1757 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1758 | `				return SXERR_ABORT;` |
|        - | 1759 | `			}` |
|       15 | 1760 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|       15 | 1761 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|        - | 1762 | `		}` |
|      130 | 1763 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|      131 | 1764 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|        - | 1765 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|      ! 0 | 1766 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 1767 | `				"Abstract property hook cannot have body");` |
|      ! 0 | 1768 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1769 | `				return SXERR_ABORT;` |
|        - | 1770 | `			}` |
|      ! 0 | 1771 | `			return SXERR_CORRUPT;` |
|        - | 1772 | `		}` |
|      131 | 1773 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|        - | 1774 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|      131 | 1775 | `		if( pMeth == 0 ){` |
|      ! 0 | 1776 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1777 | `			return SXERR_ABORT;` |
|        - | 1778 | `		}` |
|      131 | 1779 | `		pMeth->sFunc.nLine = nHLine;` |
|      131 | 1780 | `		if( !bGet ){` |
|        - | 1781 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|       61 | 1782 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       17 | 1783 | `				SyToken *pRp = 0;` |
|       17 | 1784 | `				pGen->pIn++;` |
|       17 | 1785 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|       17 | 1786 | `				if( pRp >= pGen->pEnd ){` |
|      ! 0 | 1787 | `					goto HookSyntax;` |
|        - | 1788 | `				}` |
|       17 | 1789 | `				if( pGen->pIn < pRp ){` |
|       17 | 1790 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|       17 | 1791 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1792 | `						return SXERR_ABORT;` |
|        - | 1793 | `					}` |
|        8 | 1794 | `				}` |
|       17 | 1795 | `				pGen->pIn = &pRp[1];` |
|        8 | 1796 | `			}` |
|       61 | 1797 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|        - | 1798 | `				/* Implicit $value formal */` |
|        - | 1799 | `				ph7_vm_func_arg sVArg;` |
|       45 | 1800 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|       45 | 1801 | `				if( zVName == 0 ){` |
|      ! 0 | 1802 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1803 | `					return SXERR_ABORT;` |
|        - | 1804 | `				}` |
|       45 | 1805 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|       45 | 1806 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|       45 | 1807 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       45 | 1808 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       45 | 1809 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|       45 | 1810 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|       45 | 1811 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|       22 | 1812 | `			}` |
|       30 | 1813 | `		}` |
|      165 | 1814 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - | 1815 | `			/* Block body */` |
|       69 | 1816 | `			SyToken *pBodyStart = pGen->pIn;` |
|       69 | 1817 | `			SyToken *pCloser = 0;` |
|       69 | 1818 | `			int bParentCall = 0;` |
|       69 | 1819 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|       69 | 1820 | `			if( pCloser < pGen->pEnd ){` |
|        - | 1821 | `				SyToken *pScan;` |
|      753 | 1822 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|      687 | 1823 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|        3 | 1824 | `						bParentCall = 1;` |
|        3 | 1825 | `						break;` |
|        - | 1826 | `					}` |
|      343 | 1827 | `				}` |
|       34 | 1828 | `			}` |
|       69 | 1829 | `			if( bParentCall ){` |
|        - | 1830 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|        - | 1831 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|        - | 1832 | `				 * hook method), then continue past the original body. */` |
|        - | 1833 | `				SySet sBody;` |
|        3 | 1834 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|        3 | 1835 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        3 | 1836 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|        3 | 1837 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1838 | `					SySetRelease(&sBody);` |
|      ! 0 | 1839 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1840 | `					return SXERR_ABORT;` |
|        - | 1841 | `				}` |
|        3 | 1842 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|        3 | 1843 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|        3 | 1844 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        3 | 1845 | `				pGen->pIn = &pCloser[1];` |
|        3 | 1846 | `				pGen->pEnd = pSavedEnd;` |
|        3 | 1847 | `				SySetRelease(&sBody);` |
|        3 | 1848 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1849 | `					return SXERR_ABORT;` |
|        - | 1850 | `				}` |
|        3 | 1851 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|        2 | 1852 | `			}else{` |
|       67 | 1853 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|       67 | 1854 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1855 | `					return SXERR_ABORT;` |
|        - | 1856 | `				}` |
|       67 | 1857 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|        - | 1858 | `			}` |
|       69 | 1859 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|       17 | 1860 | `				bRefsSelf = 1;` |
|        9 | 1861 | `			}` |
|      128 | 1862 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|        - | 1863 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|        - | 1864 | `			GenBlock *pBlock;` |
|        - | 1865 | `			SySet *pInstrContainer;` |
|        - | 1866 | `			SyToken *pBodyStart;` |
|        - | 1867 | `			SyToken *pExprEnd;` |
|       63 | 1868 | `			SyToken *pSavedEnd = 0;` |
|        - | 1869 | `			SySet sBody;` |
|       63 | 1870 | `			int bParentCall = 0;` |
|       63 | 1871 | `			pGen->pIn++; /* Jump '=>' */` |
|       63 | 1872 | `			pBodyStart = pGen->pIn;` |
|        - | 1873 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|        - | 1874 | `			 * would end the enclosing hook list) and rewrite any` |
|        - | 1875 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|        - | 1876 | `			 * method on a token copy. */` |
|        - | 1877 | `			{` |
|       63 | 1878 | `				sxi32 iNest = 0;` |
|       63 | 1879 | `				pExprEnd = pBodyStart;` |
|      355 | 1880 | `				while( pExprEnd < pGen->pEnd ){` |
|      355 | 1881 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        9 | 1882 | `						iNest++;` |
|      351 | 1883 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        9 | 1884 | `						if( iNest <= 0 ){` |
|      ! 0 | 1885 | `							break;` |
|        - | 1886 | `						}` |
|        9 | 1887 | `						iNest--;` |
|      343 | 1888 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|       63 | 1889 | `						break;` |
|        - | 1890 | `					}` |
|      293 | 1891 | `					pExprEnd++;` |
|        1 | 1892 | `				}` |
|        - | 1893 | `			}` |
|        - | 1894 | `			{` |
|        - | 1895 | `				SyToken *pScan;` |
|      335 | 1896 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|      275 | 1897 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|        3 | 1898 | `						bParentCall = 1;` |
|        3 | 1899 | `						break;` |
|        - | 1900 | `					}` |
|      137 | 1901 | `				}` |
|        - | 1902 | `			}` |
|       63 | 1903 | `			if( bParentCall ){` |
|        3 | 1904 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        3 | 1905 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|        3 | 1906 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1907 | `					SySetRelease(&sBody);` |
|      ! 0 | 1908 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1909 | `					return SXERR_ABORT;` |
|        - | 1910 | `				}` |
|        3 | 1911 | `				pSavedEnd = pGen->pEnd;` |
|        3 | 1912 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|        3 | 1913 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|        1 | 1914 | `			}` |
|       94 | 1915 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       62 | 1916 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|       63 | 1917 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1918 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|      ! 0 | 1919 | `				return SXERR_ABORT;` |
|        - | 1920 | `			}` |
|       63 | 1921 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       63 | 1922 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|       63 | 1923 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|       63 | 1924 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       63 | 1925 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       63 | 1926 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       63 | 1927 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       63 | 1928 | `			GenStateLeaveBlock(&(*pGen),0);` |
|       63 | 1929 | `			if( bParentCall ){` |
|        3 | 1930 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|        3 | 1931 | `				pGen->pEnd = pSavedEnd;` |
|        3 | 1932 | `				SySetRelease(&sBody);` |
|        1 | 1933 | `			}` |
|       63 | 1934 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1935 | `				return SXERR_ABORT;` |
|        - | 1936 | `			}` |
|       63 | 1937 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|       63 | 1938 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|       37 | 1939 | `				bRefsSelf = 1;` |
|       18 | 1940 | `			}` |
|       63 | 1941 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       63 | 1942 | `				pGen->pIn++; /* Jump ';' */` |
|       31 | 1943 | `			}` |
|       63 | 1944 | `			if( !bGet ){` |
|        - | 1945 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|        - | 1946 | `				 * the dispatcher consumes the implicit return value — which` |
|        - | 1947 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|        - | 1948 | ``				 * for `$this->NAME = expr`). */`` |
|        3 | 1949 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|        3 | 1950 | `				bRefsSelf = 1;` |
|        1 | 1951 | `			}` |
|       32 | 1952 | `		}else{` |
|      ! 0 | 1953 | `			goto HookSyntax;` |
|        - | 1954 | `		}` |
|      131 | 1955 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|      131 | 1956 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1957 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1958 | `			return SXERR_ABORT;` |
|        - | 1959 | `		}` |
|      131 | 1960 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        1 | 1961 | `	}` |
|       95 | 1962 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|      ! 0 | 1963 | `		goto HookSyntax;` |
|        - | 1964 | `	}` |
|       95 | 1965 | `	pGen->pIn++; /* Jump '}' */` |
|       95 | 1966 | `	if( !bRefsSelf ){` |
|        - | 1967 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|        - | 1968 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|        - | 1969 | `		 * a default value (compile fatal, php's exact wording). */` |
|       41 | 1970 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|       41 | 1971 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      ! 0 | 1972 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1973 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|      ! 0 | 1974 | `				&pClass->sName,&pAttr->sName);` |
|      ! 0 | 1975 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1976 | `				return SXERR_ABORT;` |
|        - | 1977 | `			}` |
|      ! 0 | 1978 | `			return SXERR_CORRUPT;` |
|        - | 1979 | `		}` |
|       20 | 1980 | `	}` |
|       95 | 1981 | `	return SXRET_OK;` |
|      ! 0 | 1982 | `HookSyntax:` |
|      ! 0 | 1983 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1984 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|      ! 0 | 1985 | `		&pClass->sName,&pAttr->sName);` |
|      ! 0 | 1986 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1987 | `		return SXERR_ABORT;` |
|        - | 1988 | `	}` |
|      ! 0 | 1989 | `	return SXERR_CORRUPT;` |
|       48 | 1990 | `}` |
|        - | 1991 | `/*` |
|        - | 1992 | ` * Compile an object interface.` |
|        - | 1993 | ` *  According to the PHP language reference manual` |
|        - | 1994 | ` *   Object Interfaces:` |
|        - | 1995 | ` *   Object interfaces allow you to create code which specifies which methods` |
|        - | 1996 | ` *   a class must implement, without having to define how these methods are handled.` |
|        - | 1997 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - | 1998 | ` *   class, but without any of the methods having their contents defined.` |
|        - | 1999 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|        - | 2000 | ` */` |
|    70288 | 2001 | `PH7_PRIVATE sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|        5 | 2002 | `{` |
|    70293 | 2003 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2004 | `	ph7_class *pClass,*pBase;` |
|        - | 2005 | `	SyToken *pEnd,*pTmp;` |
|        - | 2006 | `	SyString *pName;` |
|        - | 2007 | `	sxi32 nKwrd;` |
|        - | 2008 | `	sxi32 rc;` |
|        - | 2009 | `	/* Jump the 'interface' keyword */` |
|    70293 | 2010 | `	pGen->pIn++;` |
|        - | 2011 | `	/* Extract interface name */` |
|    70293 | 2012 | `	pName = &pGen->pIn->sData;` |
|        - | 2013 | `	/* Advance the stream cursor */` |
|    70293 | 2014 | `	pGen->pIn++;` |
|        - | 2015 | `	/* Build FQN and obtain a raw class */ {` |
|        - | 2016 | `		SyBlob sFQN;` |
|        - | 2017 | `		SyString sFQNStr;` |
|    70293 | 2018 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    70293 | 2019 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    70293 | 2020 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    70293 | 2021 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    70293 | 2022 | `		SyBlobRelease(&sFQN);` |
|        - | 2023 | `	}` |
|    70293 | 2024 | `	if( pClass == 0 ){` |
|      ! 0 | 2025 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2026 | `		return SXERR_ABORT;` |
|        - | 2027 | `	}` |
|    70293 | 2028 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    70293 | 2029 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 2030 | `		return SXERR_ABORT;` |
|        - | 2031 | `	}` |
|        - | 2032 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|    70293 | 2033 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|        - | 2034 | `	/* Assume no base class is given */` |
|    70293 | 2035 | `	pBase = 0;` |
|    70293 | 2036 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    27303 | 2037 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    27303 | 2038 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a, c, … */ ){` |
|        - | 2039 | `			/* php lets an interface extend SEVERAL parent interfaces. The first` |
|        - | 2040 | `			 * becomes pBase (single-inheritance chain, hDerived); every extra one` |
|        - | 2041 | `			 * is recorded via PH7_ClassImplement so it lands in aInterface — the` |
|        - | 2042 | `			 * runtime subtype walk (VmInterfaceReaches) follows both. */` |
|    27303 | 2043 | `			pGen->pIn++;` |
|    13650 | 2044 | `			for(;;){` |
|        - | 2045 | `				SyBlob sResolved;` |
|        - | 2046 | `				SyString sBaseName;` |
|        - | 2047 | `				sxu32 nRefLine;` |
|        - | 2048 | `				ph7_class *pParent;` |
|    27305 | 2049 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    27305 | 2050 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    27305 | 2051 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 2052 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 2053 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2054 | `						"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|      ! 0 | 2055 | `						pName);` |
|      ! 0 | 2056 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 2057 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2058 | `						return SXERR_ABORT;` |
|        - | 2059 | `					}` |
|      ! 0 | 2060 | `					return SXRET_OK;` |
|        - | 2061 | `				}` |
|    40955 | 2062 | `				pParent = PH7_VmExtractClass(pGen->pVm,` |
|    27300 | 2063 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    27305 | 2064 | `				SyStringInitFromBuf(&sBaseName,` |
|        - | 2065 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 2066 | `				/* Only interfaces is allowed */` |
|    27305 | 2067 | `				while( pParent && (pParent->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 2068 | `					pParent = pParent->pNextName;` |
|      ! 0 | 2069 | `				}` |
|    27305 | 2070 | `				if( pParent == 0 ){` |
|      ! 0 | 2071 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 2072 | `						"Nonexistent base interface '%z'",&sBaseName);` |
|      ! 0 | 2073 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2074 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 2075 | `						return SXERR_ABORT;` |
|      ! 0 | 2076 | `					}` |
|    27305 | 2077 | `				}else if( pBase == 0 ){` |
|        - | 2078 | `					/* First parent → single-inheritance base */` |
|    27303 | 2079 | `					pBase = pParent;` |
|    13654 | 2080 | `				}else{` |
|        - | 2081 | `					/* Additional parent → record it in aInterface (+ copy its` |
|        - | 2082 | `					 * constants/method stubs) so instanceof reaches it too. */` |
|        3 | 2083 | `					PH7_ClassImplement(pClass,pParent);` |
|        - | 2084 | `				}` |
|    27305 | 2085 | `				SyBlobRelease(&sResolved);` |
|        - | 2086 | `				/* Continue on a comma-separated list */` |
|    27305 | 2087 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 | 2088 | `					pGen->pIn++;` |
|        3 | 2089 | `					continue;` |
|        - | 2090 | `				}` |
|    27303 | 2091 | `				break;` |
|      ! 0 | 2092 | `			}` |
|    13649 | 2093 | `		}` |
|    13649 | 2094 | `	}` |
|    70293 | 2095 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - | 2096 | `		/* Syntax error */` |
|      ! 0 | 2097 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|      ! 0 | 2098 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 2099 | `		if( rc == SXERR_ABORT ){` |
|        - | 2100 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 2101 | `			return SXERR_ABORT;` |
|        - | 2102 | `		}` |
|      ! 0 | 2103 | `		return SXRET_OK;` |
|        - | 2104 | `	}` |
|    70293 | 2105 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    70293 | 2106 | `	pEnd = 0; /* cc warning */` |
|        - | 2107 | `	/* Delimit the interface body */` |
|    70293 | 2108 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    70293 | 2109 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 2110 | `		/* Syntax error */` |
|      ! 0 | 2111 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|      ! 0 | 2112 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 2113 | `		if( rc == SXERR_ABORT ){` |
|        - | 2114 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 2115 | `			return SXERR_ABORT;` |
|        - | 2116 | `		}` |
|      ! 0 | 2117 | `		return SXRET_OK;` |
|        - | 2118 | `	}` |
|        - | 2119 | `	/* The delimiter token is the interface body's closing brace */` |
|    70293 | 2120 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 2121 | `	/* Swap token stream */` |
|    70293 | 2122 | `	pTmp = pGen->pEnd;` |
|    70293 | 2123 | `	pGen->pEnd = pEnd;` |
|        - | 2124 | `	/* Start the parse process` |
|        - | 2125 | `	 * Note (According to the PHP reference manual):` |
|        - | 2126 | `	 *  Only constants and function signatures(without body) are allowed.` |
|        - | 2127 | `	 *  Only 'public' visibility is allowed.` |
|        - | 2128 | `	 */` |
|   128729 | 2129 | `	for(;;){` |
|        - | 2130 | `		/* Jump leading/trailing semi-colons */` |
|   444637 | 2131 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   187175 | 2132 | `			pGen->pIn++;` |
|        5 | 2133 | `		}` |
|   257467 | 2134 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 2135 | `			/* End of interface body */` |
|    70289 | 2136 | `			break;` |
|        - | 2137 | `		}` |
|        - | 2138 | `		/* Bind a directly-preceding docblock to this member */` |
|   187183 | 2139 | `		GenStateSetPendingDoc(&(*pGen));` |
|   187183 | 2140 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 2141 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 2142 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|      ! 0 | 2143 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 2144 | `			if( rc == SXERR_ABORT ){` |
|        - | 2145 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 2146 | `				return SXERR_ABORT;` |
|        - | 2147 | `			}` |
|      ! 0 | 2148 | `			goto done;` |
|        - | 2149 | `		}` |
|        - | 2150 | `		/* Extract the current keyword */` |
|   187183 | 2151 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   187183 | 2152 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        - | 2153 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|        - | 2154 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|        3 | 2155 | `			const char *zKind = "member";` |
|        3 | 2156 | `			SyString *pMemberName = 0;` |
|        3 | 2157 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|        3 | 2158 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|        3 | 2159 | `				if( nNext == PH7_TKWRD_CONST ){` |
|        3 | 2160 | `					zKind = "constant";` |
|        3 | 2161 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|        3 | 2162 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|        2 | 2163 | `					}` |
|        1 | 2164 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 2165 | `					zKind = "method";` |
|      ! 0 | 2166 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|      ! 0 | 2167 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|      ! 0 | 2168 | `					}` |
|      ! 0 | 2169 | `				}` |
|        1 | 2170 | `			}` |
|        3 | 2171 | `			if( pMemberName ){` |
|        4 | 2172 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        1 | 2173 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|        2 | 2174 | `			}else{` |
|      ! 0 | 2175 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2176 | `					"Access type for interface %s must be public",zKind);` |
|        - | 2177 | `			}` |
|        3 | 2178 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2179 | `				return SXERR_ABORT;` |
|        - | 2180 | `			}` |
|        3 | 2181 | `			goto done;` |
|        - | 2182 | `		}` |
|   187181 | 2183 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|      ! 0 | 2184 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2185 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 | 2186 | `			if( rc == SXERR_ABORT ){` |
|        - | 2187 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 2188 | `				return SXERR_ABORT;` |
|        - | 2189 | `			}` |
|      ! 0 | 2190 | `			goto done;` |
|        - | 2191 | `		}` |
|   187181 | 2192 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|        - | 2193 | `			/* Advance the stream cursor */` |
|   132591 | 2194 | `			pGen->pIn++;` |
|   132586 | 2195 | `			if( pGen->pIn < pGen->pEnd` |
|   132591 | 2196 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|   132586 | 2197 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|        - | 2198 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|        - | 2199 | `				 * requirement. The attribute compiler + hook parser handle it` |
|        - | 2200 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|        - | 2201 | `				 * property without hooks is ITS "Interfaces may only include` |
|        - | 2202 | `				 * hooked properties" error). */` |
|      ! 0 | 2203 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|      ! 0 | 2204 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|      ! 0 | 2205 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 2206 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2207 | `						return SXERR_ABORT;` |
|        - | 2208 | `					}` |
|      ! 0 | 2209 | `					goto done;` |
|        - | 2210 | `				}` |
|      ! 0 | 2211 | `				continue;` |
|        - | 2212 | `			}` |
|   132591 | 2213 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|        - | 2214 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|        - | 2215 | `				 * '$' also opens a hooked-property requirement. */` |
|      ! 0 | 2216 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|      ! 0 | 2217 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|      ! 0 | 2218 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|      ! 0 | 2219 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|      ! 0 | 2220 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|      ! 0 | 2221 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 2222 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 2223 | `							return SXERR_ABORT;` |
|        - | 2224 | `						}` |
|      ! 0 | 2225 | `						goto done;` |
|        - | 2226 | `					}` |
|      ! 0 | 2227 | `					continue;` |
|        - | 2228 | `				}` |
|      ! 0 | 2229 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2230 | `					"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 | 2231 | `				if( rc == SXERR_ABORT ){` |
|        - | 2232 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 | 2233 | `					return SXERR_ABORT;` |
|        - | 2234 | `				}` |
|      ! 0 | 2235 | `				goto done;` |
|        - | 2236 | `			}` |
|   132591 | 2237 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   132591 | 2238 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|        - | 2239 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|        - | 2240 | `				 * hooked-property requirement (PHP 8.4). */` |
|        4 | 2241 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|        5 | 2242 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|        7 | 2243 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|        2 | 2244 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|        5 | 2245 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 2246 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 2247 | `							return SXERR_ABORT;` |
|        - | 2248 | `						}` |
|      ! 0 | 2249 | `						goto done;` |
|        - | 2250 | `					}` |
|        5 | 2251 | `					continue;` |
|        - | 2252 | `				}` |
|      ! 0 | 2253 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2254 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 | 2255 | `				if( rc == SXERR_ABORT ){` |
|        - | 2256 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 | 2257 | `					return SXERR_ABORT;` |
|        - | 2258 | `				}` |
|      ! 0 | 2259 | `				goto done;` |
|        - | 2260 | `			}` |
|    66291 | 2261 | `		}` |
|   187177 | 2262 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|        - | 2263 | `			/* Parse constant */` |
|    54591 | 2264 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|    54591 | 2265 | `			if( rc != SXRET_OK ){` |
|        3 | 2266 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2267 | `					return SXERR_ABORT;` |
|        - | 2268 | `				}` |
|        3 | 2269 | `				goto done;` |
|        - | 2270 | `			}` |
|    27297 | 2271 | `		}else{` |
|   132591 | 2272 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   132591 | 2273 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - | 2274 | `				/* Static method,record that */` |
|    11699 | 2275 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|        - | 2276 | `				/* Advance the stream cursor */` |
|    11699 | 2277 | `				pGen->pIn++;` |
|    11694 | 2278 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    11699 | 2279 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 2280 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2281 | `							"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 | 2282 | `						if( rc == SXERR_ABORT ){` |
|        - | 2283 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 2284 | `							return SXERR_ABORT;` |
|        - | 2285 | `						}` |
|      ! 0 | 2286 | `						goto done;` |
|        - | 2287 | `				}` |
|     5847 | 2288 | `			}` |
|        - | 2289 | `			/* Process method signature (no body for interface methods) */` |
|   132591 | 2290 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   132591 | 2291 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2292 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2293 | `					return SXERR_ABORT;` |
|        - | 2294 | `				}` |
|      ! 0 | 2295 | `				goto done;` |
|        - | 2296 | `			}` |
|        - | 2297 | `		}` |
|        5 | 2298 | `	}` |
|        - | 2299 | `	/* Reject a php-fatal redeclaration before hoisting the interface */` |
|    70289 | 2300 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|        3 | 2301 | `		return SXERR_ABORT;` |
|        - | 2302 | `	}` |
|        - | 2303 | `	/* Install the interface */` |
|    70287 | 2304 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    70287 | 2305 | `	if( rc == SXRET_OK && pBase ){` |
|        - | 2306 | `		/* Inherit from the base interface */` |
|    27303 | 2307 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    13649 | 2308 | `	}` |
|    70287 | 2309 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2310 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2311 | `		return SXERR_ABORT;` |
|        - | 2312 | `	}` |
|    35141 | 2313 | `done:` |
|        - | 2314 | `	/* Point beyond the interface body */` |
|    70291 | 2315 | `	pGen->pIn  = &pEnd[1];` |
|    70291 | 2316 | `	pGen->pEnd = pTmp;` |
|    70291 | 2317 | `	return PH7_OK;` |
|    35149 | 2318 | `}` |
|        - | 2319 | `/*` |
|        - | 2320 | ` * Compile a user-defined class.` |
|        - | 2321 | ` * According to the PHP language reference manual` |
|        - | 2322 | ` *  class` |
|        - | 2323 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|        - | 2324 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|        - | 2325 | ` *  of the properties and methods belonging to the class.` |
|        - | 2326 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|        - | 2327 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|        - | 2328 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|        - | 2329 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - | 2330 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|        - | 2331 | ` *  (called "methods").` |
|        - | 2332 | ` */` |
|        - | 2333 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|        - | 2334 | `typedef struct TraitUseEntry TraitUseEntry;` |
|        - | 2335 | `struct TraitUseEntry {` |
|        - | 2336 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|        - | 2337 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|        - | 2338 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|        - | 2339 | `};` |
|        - | 2340 | `/*` |
|        - | 2341 | ` * Validate that methods implementing interface contracts have compatible` |
|        - | 2342 | ` * signatures: public visibility and at least as many parameters as declared.` |
|        - | 2343 | ` */` |
|   454114 | 2344 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 2345 | `{` |
|        - | 2346 | `	ph7_class **apIface;` |
|        - | 2347 | `	sxu32 nIface,i;` |
|        - | 2348 | `	sxi32 rc;` |
|   454119 | 2349 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      ! 0 | 2350 | `		return SXRET_OK;` |
|        - | 2351 | `	}` |
|   454119 | 2352 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   454119 | 2353 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   879355 | 2354 | `	for(i = 0; i < nIface; i++){` |
|   425241 | 2355 | `		ph7_class *pIface = apIface[i];` |
|        - | 2356 | `		SyHashEntry *pEntry;` |
|   425241 | 2357 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  1252223 | 2358 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   826987 | 2359 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - | 2360 | `			ph7_class_method *pImplMeth;` |
|   826987 | 2361 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|        - | 2362 | `			/* Find the implementing method in the class */` |
|   826987 | 2363 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   826987 | 2364 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       23 | 2365 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|        - | 2366 | `			}` |
|        - | 2367 | `			/* Check visibility: interface methods must be implemented as public */` |
|   826969 | 2368 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        4 | 2369 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - | 2370 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|        1 | 2371 | `					&pClass->sName,pMName,&pIface->sName);` |
|        3 | 2372 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2373 | `					return SXERR_ABORT;` |
|        - | 2374 | `				}` |
|        1 | 2375 | `			}` |
|        - | 2376 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|        - | 2377 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|        - | 2378 | `			 */` |
|        - | 2379 | `			{` |
|   826969 | 2380 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   826969 | 2381 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   826969 | 2382 | `				int sigError = 0;` |
|   826969 | 2383 | `				if( nImplArgs < nIfaceArgs ){` |
|        3 | 2384 | `					sigError = 1;` |
|   826968 | 2385 | `				}else if( nImplArgs > nIfaceArgs ){` |
|        - | 2386 | `					/* Extra parameters must all have default values */` |
|     3907 | 2387 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        - | 2388 | `					sxu32 k;` |
|     7807 | 2389 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|     3907 | 2390 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|        3 | 2391 | `							sigError = 1;` |
|        3 | 2392 | `							break;` |
|        - | 2393 | `						}` |
|     1955 | 2394 | `					}` |
|     1951 | 2395 | `				}` |
|   826969 | 2396 | `				if( sigError ){` |
|        - | 2397 | `					SyBlob sImplSig, sIfaceSig;` |
|        - | 2398 | `					ph7_vm_func_arg *aArgs;` |
|        - | 2399 | `					sxu32 j;` |
|        6 | 2400 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|        6 | 2401 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|        - | 2402 | `					/* Build implementing method signature */` |
|        6 | 2403 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       12 | 2404 | `					for(j = 0; j < nImplArgs; j++){` |
|        8 | 2405 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|        8 | 2406 | `						SyBlobAppend(&sImplSig,"$",1);` |
|        8 | 2407 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 | 2408 | `					}` |
|        - | 2409 | `					/* Build interface method signature */` |
|        6 | 2410 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|       12 | 2411 | `					for(j = 0; j < nIfaceArgs; j++){` |
|        8 | 2412 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|        8 | 2413 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|        8 | 2414 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 | 2415 | `					}` |
|        8 | 2416 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - | 2417 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|        2 | 2418 | `						&pClass->sName,pMName,` |
|        4 | 2419 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|        2 | 2420 | `						&pIface->sName,pMName,` |
|        4 | 2421 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|        6 | 2422 | `					SyBlobRelease(&sImplSig);` |
|        6 | 2423 | `					SyBlobRelease(&sIfaceSig);` |
|        6 | 2424 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2425 | `						return SXERR_ABORT;` |
|        - | 2426 | `					}` |
|        2 | 2427 | `				}` |
|        - | 2428 | `			}` |
|        5 | 2429 | `		}` |
|   212623 | 2430 | `	}` |
|   454119 | 2431 | `	return SXRET_OK;` |
|   227062 | 2432 | `}` |
|        - | 2433 | `/*` |
|        - | 2434 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|        - | 2435 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|        - | 2436 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|        - | 2437 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|        - | 2438 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|        - | 2439 | ` * means that specific hook is still missing.` |
|        - | 2440 | ` */` |
|       38 | 2441 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|        5 | 2442 | `{` |
|        - | 2443 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        - | 2444 | `	ph7_class_attr *pProp;` |
|       38 | 2445 | `	if( pMName->nByte <= nPfx` |
|       27 | 2446 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|        4 | 2447 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|       36 | 2448 | `		return 0; /* not a hook stub */` |
|        - | 2449 | `	}` |
|        7 | 2450 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|        7 | 2451 | `	return pProp != 0` |
|        6 | 2452 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|        3 | 2453 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|       24 | 2454 | `}` |
|        - | 2455 | `/*` |
|        - | 2456 | ` * Append an abstract member's display name to the message blob, translating a` |
|        - | 2457 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|        - | 2458 | ` */` |
|       16 | 2459 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|        4 | 2460 | `{` |
|        - | 2461 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|       16 | 2462 | `	if( pMName->nByte > nPfx` |
|       12 | 2463 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|      ! 0 | 2464 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|      ! 0 | 2465 | `		SyBlobAppend(pMsg,"$",1);` |
|      ! 0 | 2466 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|      ! 0 | 2467 | `		SyBlobAppend(pMsg,"::",2);` |
|      ! 0 | 2468 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|      ! 0 | 2469 | `		return;` |
|        - | 2470 | `	}` |
|       20 | 2471 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|       12 | 2472 | `}` |
|        - | 2473 | `/*` |
|        - | 2474 | ` * Check that a concrete class has no remaining abstract methods.` |
|        - | 2475 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|        - | 2476 | ` */` |
|   454114 | 2477 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 2478 | `{` |
|        - | 2479 | `	ph7_class_method *pMeth;` |
|        - | 2480 | `	SyHashEntry *pEntry;` |
|        - | 2481 | `	sxu32 nAbstract;` |
|        - | 2482 | `	SyBlob sMsg;` |
|        - | 2483 | `	sxi32 rc;` |
|        - | 2484 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   454119 | 2485 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|    19547 | 2486 | `		return SXRET_OK;` |
|        - | 2487 | `	}` |
|        - | 2488 | `	/* Count abstract methods */` |
|   434577 | 2489 | `	nAbstract = 0;` |
|   434577 | 2490 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  6493903 | 2491 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  5842045 | 2492 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  5842045 | 2493 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       27 | 2494 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|        7 | 2495 | `				continue; /* hook requirement met by a plain property (php) */` |
|        - | 2496 | `			}` |
|       20 | 2497 | `			nAbstract++;` |
|        8 | 2498 | `		}` |
|        5 | 2499 | `	}` |
|   434577 | 2500 | `	if( nAbstract == 0 ){` |
|   434563 | 2501 | `		return SXRET_OK;` |
|        - | 2502 | `	}` |
|        - | 2503 | `	/* Build the error message listing all abstract methods with origins */` |
|       18 | 2504 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       18 | 2505 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|        - | 2506 | `		"be declared abstract or implement the remaining method%s (",` |
|        7 | 2507 | `		&pClass->sName,nAbstract,` |
|        7 | 2508 | `		(nAbstract > 1 ? "s" : ""),` |
|        7 | 2509 | `		(nAbstract > 1 ? "s" : ""));` |
|        - | 2510 | `	/* Second pass: list methods with origins */` |
|        - | 2511 | `	{` |
|       18 | 2512 | `		sxu32 nListed = 0;` |
|       18 | 2513 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|       36 | 2514 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|       22 | 2515 | `			ph7_class *pOrigin = 0;` |
|        - | 2516 | `			SyString *pMName;` |
|       22 | 2517 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|       22 | 2518 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        3 | 2519 | `				continue;` |
|        - | 2520 | `			}` |
|       20 | 2521 | `			pMName = &pMeth->sFunc.sName;` |
|       20 | 2522 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|      ! 0 | 2523 | `				continue; /* hook requirement met by a plain property (php) */` |
|        - | 2524 | `			}` |
|       20 | 2525 | `			if( nListed > 0 ){` |
|        3 | 2526 | `				SyBlobAppend(&sMsg,", ",2);` |
|        1 | 2527 | `			}` |
|        - | 2528 | `			/* Find the origin of this abstract method.` |
|        - | 2529 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|        - | 2530 | `			 * inheritance chains) take precedence for interface-declared` |
|        - | 2531 | `			 * methods. Abstract class methods only win when the class` |
|        - | 2532 | `			 * itself declared the abstract method (not inherited from` |
|        - | 2533 | `			 * an interface). Trait methods are adopted into the using` |
|        - | 2534 | `			 * class's namespace.` |
|        - | 2535 | `			 */` |
|        - | 2536 | `			{` |
|        - | 2537 | `				ph7_class **apIface;` |
|        - | 2538 | `				ph7_class **apTrait;` |
|        - | 2539 | `				ph7_class *pWalk;` |
|        - | 2540 | `				sxu32 i;` |
|        - | 2541 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|        - | 2542 | `				 * (one that was written in the class body, not inherited from an` |
|        - | 2543 | `				 * interface). PHP attributes origin to the declaring class.` |
|        - | 2544 | `				 */` |
|       20 | 2545 | `				if( pClass->pBase ){` |
|       10 | 2546 | `					pWalk = pClass->pBase;` |
|       18 | 2547 | `					while( pWalk ){` |
|        - | 2548 | `						ph7_class_method *pParentMeth;` |
|       12 | 2549 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|       12 | 2550 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        - | 2551 | `							/* Exclude methods that came from an interface anywhere` |
|        - | 2552 | `							 * in this class's ancestor chain.` |
|        - | 2553 | `							 */` |
|       12 | 2554 | `							int fromIface = 0;` |
|       12 | 2555 | `							ph7_class *pAnc = pWalk;` |
|       16 | 2556 | `							while( pAnc ){` |
|        - | 2557 | `								ph7_class **apPI;` |
|        - | 2558 | `								sxu32 j;` |
|       14 | 2559 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|       14 | 2560 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       10 | 2561 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       10 | 2562 | `										fromIface = 1;` |
|       10 | 2563 | `										break;` |
|        - | 2564 | `									}` |
|      ! 0 | 2565 | `								}` |
|       14 | 2566 | `								if( fromIface ) break;` |
|        5 | 2567 | `								pAnc = pAnc->pBase;` |
|        1 | 2568 | `							}` |
|       12 | 2569 | `							if( !fromIface ){` |
|        3 | 2570 | `								pOrigin = pWalk;` |
|        3 | 2571 | `								break;` |
|        - | 2572 | `							}` |
|        4 | 2573 | `						}` |
|       10 | 2574 | `						pWalk = pWalk->pBase;` |
|        2 | 2575 | `					}` |
|        4 | 2576 | `				}` |
|        - | 2577 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|        - | 2578 | `				 * each interface's own parent chain for the deepest origin.` |
|        - | 2579 | `				 */` |
|       20 | 2580 | `				if( !pOrigin ){` |
|       18 | 2581 | `					pWalk = pClass;` |
|       40 | 2582 | `					while( pWalk && !pOrigin ){` |
|       26 | 2583 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|       26 | 2584 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|       16 | 2585 | `							ph7_class *pIface = apIface[i];` |
|       16 | 2586 | `							ph7_class *pDeepest = 0;` |
|       28 | 2587 | `							while( pIface ){` |
|       16 | 2588 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|       16 | 2589 | `									pDeepest = pIface;` |
|        6 | 2590 | `								}` |
|       16 | 2591 | `								pIface = pIface->pBase;` |
|        4 | 2592 | `							}` |
|       16 | 2593 | `							if( pDeepest ){` |
|       16 | 2594 | `								pOrigin = pDeepest;` |
|       16 | 2595 | `								break;` |
|        - | 2596 | `							}` |
|      ! 0 | 2597 | `						}` |
|       26 | 2598 | `						pWalk = pWalk->pBase;` |
|        4 | 2599 | `					}` |
|        7 | 2600 | `				}` |
|        - | 2601 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|       20 | 2602 | `				if( !pOrigin ){` |
|        3 | 2603 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        3 | 2604 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|        3 | 2605 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|        3 | 2606 | `							pOrigin = pClass;` |
|        3 | 2607 | `							break;` |
|        - | 2608 | `						}` |
|      ! 0 | 2609 | `					}` |
|        1 | 2610 | `				}` |
|        - | 2611 | `			}` |
|       20 | 2612 | `			if( pOrigin ){` |
|       20 | 2613 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|       12 | 2614 | `			}else{` |
|        - | 2615 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|      ! 0 | 2616 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|        - | 2617 | `			}` |
|       20 | 2618 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|       20 | 2619 | `			nListed++;` |
|        4 | 2620 | `		}` |
|        - | 2621 | `	}` |
|       18 | 2622 | `	SyBlobAppend(&sMsg,")",1);` |
|       25 | 2623 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|       14 | 2624 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|       18 | 2625 | `	SyBlobRelease(&sMsg);` |
|       18 | 2626 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2627 | `		return SXERR_ABORT;` |
|        - | 2628 | `	}` |
|       18 | 2629 | `	return SXRET_OK;` |
|   227062 | 2630 | `}` |
|        - | 2631 | `/*` |
|        - | 2632 | ` * Parse a class/interface name reference from the current token stream.` |
|        - | 2633 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|        - | 2634 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|        - | 2635 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|        - | 2636 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|        - | 2637 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|        - | 2638 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|        - | 2639 | ` */` |
|   516894 | 2640 | `PH7_PRIVATE sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|        5 | 2641 | `{` |
|   516899 | 2642 | `	int isAbsolute = 0;` |
|   516899 | 2643 | `	SyToken *pStart = pGen->pIn;` |
|        - | 2644 | `	SyBlob sName;` |
|   516899 | 2645 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|     4553 | 2646 | `		isAbsolute = 1;` |
|     4553 | 2647 | `		pGen->pIn++;` |
|     2274 | 2648 | `	}` |
|   516899 | 2649 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        9 | 2650 | `		pGen->pIn = pStart;` |
|        9 | 2651 | `		return SXERR_INVALID;` |
|        - | 2652 | `	}` |
|   516893 | 2653 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   516893 | 2654 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   516893 | 2655 | `	pGen->pIn++;` |
|   775371 | 2656 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   258488 | 2657 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       28 | 2658 | `		SyBlobAppend(&sName,"\\",1);` |
|       28 | 2659 | `		pGen->pIn++;` |
|       28 | 2660 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       28 | 2661 | `		pGen->pIn++;` |
|        2 | 2662 | `	}` |
|   516893 | 2663 | `	if( isAbsolute ){` |
|     4551 | 2664 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     2278 | 2665 | `	}else{` |
|        - | 2666 | `		SyString sRaw;` |
|   512347 | 2667 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   512347 | 2668 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|        - | 2669 | `	}` |
|   516893 | 2670 | `	SyBlobRelease(&sName);` |
|   516893 | 2671 | `	return SXRET_OK;` |
|   258452 | 2672 | `}` |
|        - | 2673 | `/*` |
|        - | 2674 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|        - | 2675 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|        - | 2676 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|        - | 2677 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|        - | 2678 | ` * either direction cannot run unbounded.` |
|        - | 2679 | ` */` |
|        - | 2680 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|   199010 | 2681 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|        5 | 2682 | `{` |
|        - | 2683 | `	ph7_class **apParent;` |
|        - | 2684 | `	sxu32 n;` |
|   526699 | 2685 | `	while( pInterface ){` |
|   335495 | 2686 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|      ! 0 | 2687 | `			return FALSE;` |
|        - | 2688 | `		}` |
|   374495 | 2689 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    78000 | 2690 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|     7811 | 2691 | `			return TRUE;` |
|        - | 2692 | `		}` |
|   327689 | 2693 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|   327691 | 2694 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|        3 | 2695 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|      ! 0 | 2696 | `				return TRUE;` |
|        - | 2697 | `			}` |
|        2 | 2698 | `		}` |
|   327689 | 2699 | `		pInterface = pInterface->pBase;` |
|   327689 | 2700 | `		iDepth++;` |
|        5 | 2701 | `	}` |
|   191209 | 2702 | `	return FALSE;` |
|    99510 | 2703 | `}` |
|   199008 | 2704 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|        5 | 2705 | `{` |
|   199013 | 2706 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|        5 | 2707 | `}` |
|        - | 2708 | `/*` |
|        - | 2709 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|        - | 2710 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|        - | 2711 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|        - | 2712 | ` */` |
|     7806 | 2713 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|        5 | 2714 | `{` |
|     7815 | 2715 | `	while( pBase ){` |
|       10 | 2716 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|        2 | 2717 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|        3 | 2718 | `			return TRUE;` |
|        - | 2719 | `		}` |
|       10 | 2720 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|        6 | 2721 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|        3 | 2722 | `			return TRUE;` |
|        - | 2723 | `		}` |
|        5 | 2724 | `		pBase = pBase->pBase;` |
|        1 | 2725 | `	}` |
|     7807 | 2726 | `	return FALSE;` |
|     3908 | 2727 | `}` |
|        - | 2728 | `/*` |
|        - | 2729 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|        - | 2730 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|        - | 2731 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|        - | 2732 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|        - | 2733 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|        - | 2734 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|        - | 2735 | ` * pClass->aEnumCases for cases().` |
|        - | 2736 | ` */` |
|     7850 | 2737 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 2738 | `{` |
|     7855 | 2739 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2740 | `	SySet *pInstrContainer;` |
|        - | 2741 | `	ph7_class_attr *pCase;` |
|        - | 2742 | `	SyString *pName;` |
|        - | 2743 | `	sxi32 rc;` |
|     7855 | 2744 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|     7855 | 2745 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2746 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 2747 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|      ! 0 | 2748 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2749 | `			return SXERR_ABORT;` |
|        - | 2750 | `		}` |
|      ! 0 | 2751 | `		goto Synchronize;` |
|        - | 2752 | `	}` |
|     7855 | 2753 | `	pName = &pGen->pIn->sData;` |
|        - | 2754 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|     7855 | 2755 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      ! 0 | 2756 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2757 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|      ! 0 | 2758 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2759 | `			return SXERR_ABORT;` |
|        - | 2760 | `		}` |
|      ! 0 | 2761 | `		goto Synchronize;` |
|        - | 2762 | `	}` |
|     7855 | 2763 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 2764 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|     7855 | 2765 | `	if( pCase == 0 ){` |
|      ! 0 | 2766 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2767 | `		return SXERR_ABORT;` |
|        - | 2768 | `	}` |
|     7855 | 2769 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|     7855 | 2770 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 2771 | `		return SXERR_ABORT;` |
|        - | 2772 | `	}` |
|     7855 | 2773 | `	pGen->pIn++; /* Jump the case name */` |
|     7855 | 2774 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|     7841 | 2775 | `		if( pClass->nEnumBacking == 0 ){` |
|        8 | 2776 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        2 | 2777 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|        6 | 2778 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2779 | `				return SXERR_ABORT;` |
|        - | 2780 | `			}` |
|        6 | 2781 | `			goto Synchronize;` |
|        - | 2782 | `		}` |
|     7837 | 2783 | `		pGen->pIn++; /* Jump the equal sign */` |
|        - | 2784 | `		/* Compile the backing value expression into the case's own container` |
|        - | 2785 | `		 * (same technique as class constants). */` |
|     7837 | 2786 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     7837 | 2787 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|     7837 | 2788 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     7837 | 2789 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2790 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 2791 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|      ! 0 | 2792 | `		}` |
|     7837 | 2793 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|     7837 | 2794 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     7837 | 2795 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2796 | `			return SXERR_ABORT;` |
|        - | 2797 | `		}` |
|     3921 | 2798 | `	}else{` |
|       17 | 2799 | `		if( pClass->nEnumBacking != 0 ){` |
|      ! 0 | 2800 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 2801 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|      ! 0 | 2802 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2803 | `				return SXERR_ABORT;` |
|        - | 2804 | `			}` |
|      ! 0 | 2805 | `			goto Synchronize;` |
|        - | 2806 | `		}` |
|        - | 2807 | `	}` |
|     7851 | 2808 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|     7851 | 2809 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2810 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2811 | `		return SXERR_ABORT;` |
|        - | 2812 | `	}` |
|     7851 | 2813 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|     7851 | 2814 | `	return SXRET_OK;` |
|        2 | 2815 | `Synchronize:` |
|        - | 2816 | `	/* Synchronize with the first semi-colon */` |
|       14 | 2817 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       10 | 2818 | `		pGen->pIn++;` |
|        2 | 2819 | `	}` |
|        6 | 2820 | `	return SXERR_CORRUPT;` |
|     3930 | 2821 | `}` |
|        - | 2822 | `/*` |
|        - | 2823 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|        - | 2824 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|        - | 2825 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|        - | 2826 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|        - | 2827 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|        - | 2828 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|        - | 2829 | ` * pointers into it (see the constructor-promotion precedent above).` |
|        - | 2830 | ` */` |
|     3930 | 2831 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 2832 | `{` |
|        - | 2833 | `	SyToken *pSaveIn,*pSaveEnd;` |
|        - | 2834 | `	const char *zBack;` |
|        - | 2835 | `	SySet sToken;` |
|        - | 2836 | `	char *zSrc;` |
|        - | 2837 | `	sxu32 nSrc,nMax;` |
|     3935 | 2838 | `	sxi32 rc = SXRET_OK;` |
|     3935 | 2839 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|     3930 | 2840 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|     3935 | 2841 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|     3935 | 2842 | `	if( zSrc == 0 ){` |
|      ! 0 | 2843 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2844 | `		return SXERR_ABORT;` |
|        - | 2845 | `	}` |
|     3935 | 2846 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|     3935 | 2847 | `	if( pClass->nEnumBacking != 0 ){` |
|     5873 | 2848 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        - | 2849 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|        - | 2850 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|        - | 2851 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|     1956 | 2852 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|     1961 | 2853 | `	}else{` |
|       30 | 2854 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        9 | 2855 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|        - | 2856 | `	}` |
|     3935 | 2857 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|     3935 | 2858 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|     3935 | 2859 | `	pSaveIn = pGen->pIn;` |
|     3935 | 2860 | `	pSaveEnd = pGen->pEnd;` |
|     3935 | 2861 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|     3935 | 2862 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|    15689 | 2863 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|    11759 | 2864 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|        5 | 2865 | `	}` |
|     3935 | 2866 | `	pGen->pIn = pSaveIn;` |
|     3935 | 2867 | `	pGen->pEnd = pSaveEnd;` |
|     3935 | 2868 | `	SySetRelease(&sToken);` |
|     3935 | 2869 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|     1970 | 2870 | `}` |
|        - | 2871 | `/*` |
|        - | 2872 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|        - | 2873 | ` * __call/__callStatic/__invoke stay allowed).` |
|        - | 2874 | ` */` |
|        - | 2875 | `static const char *azEnumBannedMagic[] = {` |
|        - | 2876 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|        - | 2877 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|        - | 2878 | `};` |
|        - | 2879 | `/*` |
|        - | 2880 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|        - | 2881 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|        - | 2882 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|        - | 2883 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|        - | 2884 | ` * and before the class is installed.` |
|        - | 2885 | ` */` |
|     3930 | 2886 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|        5 | 2887 | `{` |
|        - | 2888 | `	SyHashEntry *pEntry;` |
|        - | 2889 | `	sxi32 rc;` |
|        - | 2890 | `	sxu32 n;` |
|        - | 2891 | `	/* php: "Enum %s cannot include properties" */` |
|     3935 | 2892 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    11787 | 2893 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     7859 | 2894 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     7859 | 2895 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        3 | 2896 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|        1 | 2897 | `				"Enum %z cannot include properties",&pClass->sName);` |
|        3 | 2898 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2899 | `				return SXERR_ABORT;` |
|        - | 2900 | `			}` |
|        3 | 2901 | `			break;` |
|        - | 2902 | `		}` |
|        5 | 2903 | `	}` |
|        - | 2904 | `	/* php: "Enum %s cannot include magic method %s" */` |
|    55025 | 2905 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|    76635 | 2906 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|    51095 | 2907 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|      ! 0 | 2908 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 2909 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|      ! 0 | 2910 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2911 | `				return SXERR_ABORT;` |
|        - | 2912 | `			}` |
|      ! 0 | 2913 | `		}` |
|    25550 | 2914 | `	}` |
|        - | 2915 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|        - | 2916 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|        - | 2917 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|        - | 2918 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|        - | 2919 | `	{` |
|        - | 2920 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|        - | 2921 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|        - | 2922 | `		ph7_class_attr *pAttr;` |
|     3935 | 2923 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 2924 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|     3935 | 2925 | `		if( pAttr == 0 ){` |
|      ! 0 | 2926 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2927 | `			return SXERR_ABORT;` |
|        - | 2928 | `		}` |
|     3935 | 2929 | `		pAttr->nType = MEMOBJ_STRING;` |
|     3935 | 2930 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|     3935 | 2931 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|     3935 | 2932 | `		if( pClass->nEnumBacking != 0 ){` |
|     3917 | 2933 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 2934 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|     3917 | 2935 | `			if( pAttr == 0 ){` |
|      ! 0 | 2936 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2937 | `				return SXERR_ABORT;` |
|        - | 2938 | `			}` |
|     3917 | 2939 | `			pAttr->nType = pClass->nEnumBacking;` |
|     3917 | 2940 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        7 | 2941 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|        4 | 2942 | `			}else{` |
|     3911 | 2943 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|        - | 2944 | `			}` |
|     3917 | 2945 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|     1956 | 2946 | `		}` |
|        - | 2947 | `	}` |
|     3935 | 2948 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|     1970 | 2949 | `}` |
|        - | 2950 | `/*` |
|        - | 2951 | ` * Compile a class declaration, named or anonymous.` |
|        - | 2952 | ` *` |
|        - | 2953 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|        - | 2954 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|        - | 2955 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|        - | 2956 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|        - | 2957 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|        - | 2958 | ` * implements, body, install) is shared by both paths.` |
|        - | 2959 | ` */` |
|   454164 | 2960 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|        - | 2961 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|        5 | 2962 | `{` |
|   454169 | 2963 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2964 | `	ph7_class *pClass,*pBase;` |
|        - | 2965 | `	SyToken *pEnd,*pTmp;` |
|        - | 2966 | `	sxi32 iProtection;` |
|        - | 2967 | `	SySet aInterfaces;` |
|        - | 2968 | `	SySet aUseEntries;` |
|        - | 2969 | `	sxi32 iAttrflags;` |
|        - | 2970 | `	SyString *pName;` |
|        - | 2971 | `	sxi32 nKwrd;` |
|        - | 2972 | `	sxi32 rc;` |
|        - | 2973 | `	/* Jump the 'class' keyword */` |
|   454169 | 2974 | `	pGen->pIn++;` |
|   454169 | 2975 | `	if( pAnonName ){` |
|        - | 2976 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|        - | 2977 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|        - | 2978 | `		 * then use the synthesized name. */` |
|       34 | 2979 | `		*ppArgStart = *ppArgEnd = 0;` |
|       34 | 2980 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        7 | 2981 | `			pGen->pIn++; /* Jump '(' */` |
|        7 | 2982 | `			*ppArgStart = pGen->pIn;` |
|       10 | 2983 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|        3 | 2984 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|        7 | 2985 | `			pGen->pIn = *ppArgEnd;` |
|        7 | 2986 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|        3 | 2987 | `		}` |
|       34 | 2988 | `		pName = pAnonName;` |
|       34 | 2989 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|       19 | 2990 | `	}else{` |
|   454139 | 2991 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - | 2992 | `			/* Syntax error */` |
|      ! 0 | 2993 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|      ! 0 | 2994 | `			if( rc == SXERR_ABORT ){` |
|        - | 2995 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 2996 | `				return SXERR_ABORT;` |
|        - | 2997 | `			}` |
|        - | 2998 | `			/* Synchronize with the first semi-colon or curly braces */` |
|      ! 0 | 2999 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|      ! 0 | 3000 | `				pGen->pIn++;` |
|      ! 0 | 3001 | `			}` |
|      ! 0 | 3002 | `			return SXRET_OK;` |
|        - | 3003 | `		}` |
|        - | 3004 | `		/* Extract class name */` |
|   454139 | 3005 | `		pName = &pGen->pIn->sData;` |
|        - | 3006 | `		/* Advance the stream cursor */` |
|   454139 | 3007 | `		pGen->pIn++;` |
|        - | 3008 | `		/* Build FQN and obtain a raw class */ {` |
|        - | 3009 | `			SyBlob sFQN;` |
|        - | 3010 | `			SyString sFQNStr;` |
|   454139 | 3011 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   454139 | 3012 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|   454139 | 3013 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   454139 | 3014 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   454139 | 3015 | `			SyBlobRelease(&sFQN);` |
|        - | 3016 | `		}` |
|        - | 3017 | `	}` |
|   454169 | 3018 | `	if( pClass == 0 ){` |
|      ! 0 | 3019 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3020 | `		return SXERR_ABORT;` |
|        - | 3021 | `	}` |
|   454164 | 3022 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|     3939 | 3023 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|        - | 3024 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|     3919 | 3025 | `		pGen->pIn++; /* Jump ':' */` |
|     3914 | 3026 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     3919 | 3027 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|        7 | 3028 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|        7 | 3029 | `			pGen->pIn++;` |
|     3912 | 3030 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     3913 | 3031 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|     3911 | 3032 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|     3911 | 3033 | `			pGen->pIn++;` |
|     1958 | 3034 | `		}else{` |
|        3 | 3035 | `			SyToken *pTok = pGen->pIn;` |
|        3 | 3036 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|        4 | 3037 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|        1 | 3038 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|        3 | 3039 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3040 | `				return SXERR_ABORT;` |
|        - | 3041 | `			}` |
|        3 | 3042 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|        3 | 3043 | `				pGen->pIn++; /* Skip the bogus type token */` |
|        1 | 3044 | `			}` |
|        - | 3045 | `		}` |
|     1957 | 3046 | `	}` |
|   454169 | 3047 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|   454169 | 3048 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 3049 | `		return SXERR_ABORT;` |
|        - | 3050 | `	}` |
|        - | 3051 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   454169 | 3052 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   454169 | 3053 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|        - | 3054 | `	/* Assume a standalone class */` |
|   454169 | 3055 | `	pBase = 0;` |
|   454169 | 3056 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   370807 | 3057 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   370807 | 3058 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|        - | 3059 | `			SyBlob sResolved;` |
|        - | 3060 | `			SyString sBaseName;` |
|        - | 3061 | `			sxu32 nRefLine;` |
|   249789 | 3062 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|        - | 3063 | `				/* php parse-fatals here (enums have no inheritance) */` |
|      ! 0 | 3064 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 3065 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|      ! 0 | 3066 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3067 | `					return SXERR_ABORT;` |
|        - | 3068 | `				}` |
|      ! 0 | 3069 | `			}` |
|   249789 | 3070 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   249789 | 3071 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   249789 | 3072 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   249789 | 3073 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        3 | 3074 | `				SyBlobRelease(&sResolved);` |
|        4 | 3075 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 3076 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|        1 | 3077 | `					pName);` |
|        3 | 3078 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|        3 | 3079 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3080 | `					return SXERR_ABORT;` |
|        - | 3081 | `				}` |
|        3 | 3082 | `				return SXRET_OK;` |
|        - | 3083 | `			}` |
|   374678 | 3084 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   249782 | 3085 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   249787 | 3086 | `			SyStringInitFromBuf(&sBaseName,` |
|        - | 3087 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 3088 | `			/* Interfaces are not allowed */` |
|   249787 | 3089 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|      ! 0 | 3090 | `				pBase = pBase->pNextName;` |
|      ! 0 | 3091 | `			}` |
|   249787 | 3092 | `			if( pBase == 0 ){` |
|      ! 0 | 3093 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 3094 | `					"Nonexistent base class '%z'",&sBaseName);` |
|      ! 0 | 3095 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3096 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 3097 | `					return SXERR_ABORT;` |
|        - | 3098 | `				}` |
|      ! 0 | 3099 | `			}else{` |
|   249787 | 3100 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|        4 | 3101 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 3102 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|        3 | 3103 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3104 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 3105 | `						return SXERR_ABORT;` |
|        - | 3106 | `					}` |
|        3 | 3107 | `					pBase = 0; /* Never inherit from an enum */` |
|   249786 | 3108 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|      ! 0 | 3109 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 3110 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|      ! 0 | 3111 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3112 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 3113 | `						return SXERR_ABORT;` |
|        - | 3114 | `					}` |
|      ! 0 | 3115 | `				}` |
|        - | 3116 | `			}` |
|   249787 | 3117 | `			SyBlobRelease(&sResolved);` |
|   249787 | 3118 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 | 3119 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|      ! 0 | 3120 | `			}` |
|   124891 | 3121 | `		}` |
|   370805 | 3122 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|        - | 3123 | `			ph7_class *pInterface;` |
|        - | 3124 | `			/* Interface implementation */` |
|   136631 | 3125 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   130695 | 3126 | `			for(;;){` |
|        - | 3127 | `				SyBlob sResolved;` |
|        - | 3128 | `				SyString sIntName;` |
|        - | 3129 | `				sxu32 nRefLine;` |
|   199013 | 3130 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   199013 | 3131 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   199013 | 3132 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 3133 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 3134 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 3135 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|      ! 0 | 3136 | `						pName);` |
|      ! 0 | 3137 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3138 | `						return SXERR_ABORT;` |
|        - | 3139 | `					}` |
|      ! 0 | 3140 | `					break;` |
|        - | 3141 | `				}` |
|   398021 | 3142 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|   199008 | 3143 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   199013 | 3144 | `				SyStringInitFromBuf(&sIntName,` |
|        - | 3145 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 3146 | `				/* Only interfaces are allowed */` |
|   199013 | 3147 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 3148 | `					pInterface = pInterface->pNextName;` |
|      ! 0 | 3149 | `				}` |
|   199013 | 3150 | `				if( pInterface == 0 ){` |
|      ! 0 | 3151 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 3152 | `						"Nonexistent base interface '%z'",&sIntName);` |
|      ! 0 | 3153 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3154 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 3155 | `						return SXERR_ABORT;` |
|        - | 3156 | `					}` |
|      ! 0 | 3157 | `				}else{` |
|        - | 3158 | `					/* Reject user classes that try to implement Throwable` |
|        - | 3159 | `					 * directly (or via an interface that extends Throwable)` |
|        - | 3160 | `					 * unless they already extend Exception or Error.` |
|        - | 3161 | `					 * Exception and Error themselves are compiled from the` |
|        - | 3162 | `					 * built-in library and are exempt by FQN — a namespaced` |
|        - | 3163 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|   199013 | 3164 | `					SyString *pFqn = &pClass->sName;` |
|   199013 | 3165 | `					int bIsExceptionOrError =` |
|   103406 | 3166 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   300465 | 3167 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|   197066 | 3168 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|     3912 | 3169 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   202911 | 3170 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    11712 | 3171 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|     3901 | 3172 | `						!bIsExceptionOrError ){` |
|       12 | 3173 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3174 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|        3 | 3175 | `							&pClass->sName);` |
|        9 | 3176 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3177 | `							SyBlobRelease(&sResolved);` |
|      ! 0 | 3178 | `							return SXERR_ABORT;` |
|        - | 3179 | `						}` |
|        - | 3180 | `						/* Skip registration so the follow-up abstract-method` |
|        - | 3181 | `						 * check does not produce a duplicate fatal. */` |
|        6 | 3182 | `					}else{` |
|   199007 | 3183 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|        - | 3184 | `					}` |
|        - | 3185 | `				}` |
|   199013 | 3186 | `				SyBlobRelease(&sResolved);` |
|   199013 | 3187 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    68318 | 3188 | `					break;` |
|        - | 3189 | `				}` |
|    62387 | 3190 | `				pGen->pIn++;/* Jump the comma */` |
|        5 | 3191 | `			}` |
|    68313 | 3192 | `		}` |
|   185400 | 3193 | `	}` |
|   454167 | 3194 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - | 3195 | `		/* Syntax error */` |
|      ! 0 | 3196 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|      ! 0 | 3197 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 3198 | `		if( rc == SXERR_ABORT ){` |
|        - | 3199 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3200 | `			return SXERR_ABORT;` |
|        - | 3201 | `		}` |
|      ! 0 | 3202 | `		return SXRET_OK;` |
|        - | 3203 | `	}` |
|   454167 | 3204 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   454167 | 3205 | `	pEnd = 0; /* cc warning */` |
|        - | 3206 | `	/* Delimit the class body */` |
|   454167 | 3207 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   454167 | 3208 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 3209 | `		/* Syntax error */` |
|      ! 0 | 3210 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|      ! 0 | 3211 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 3212 | `		if( rc == SXERR_ABORT ){` |
|        - | 3213 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3214 | `			return SXERR_ABORT;` |
|        - | 3215 | `		}` |
|      ! 0 | 3216 | `		return SXRET_OK;` |
|        - | 3217 | `	}` |
|        - | 3218 | `	/* The delimiter token is the class body's closing brace */` |
|   454167 | 3219 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 3220 | `	/* Swap token stream */` |
|   454167 | 3221 | `	pTmp = pGen->pEnd;` |
|   454167 | 3222 | `	pGen->pEnd = pEnd;` |
|        - | 3223 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|   454167 | 3224 | `	pClass->iFlags \|= iFlags;` |
|        - | 3225 | `	/* Start the parse process */` |
|  1701446 | 3226 | `	for(;;){` |
|        - | 3227 | `		/* Jump leading/trailing semi-colons */` |
|  4886661 | 3228 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   890059 | 3229 | `			pGen->pIn++;` |
|        5 | 3230 | `		}` |
|  3996607 | 3231 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3232 | `			/* End of class body */` |
|   454125 | 3233 | `			break;` |
|        - | 3234 | `		}` |
|        - | 3235 | `		/* Bind a directly-preceding docblock to this member */` |
|  3542487 | 3236 | `		GenStateSetPendingDoc(&(*pGen));` |
|  3542482 | 3237 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  1771246 | 3238 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|      ! 0 | 3239 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3240 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 | 3241 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 3242 | `			if( rc == SXERR_ABORT ){` |
|        - | 3243 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 3244 | `				return SXERR_ABORT;` |
|        - | 3245 | `			}` |
|      ! 0 | 3246 | `			goto done;` |
|        - | 3247 | `		}` |
|        - | 3248 | `		/* Assume public visibility */` |
|  3542487 | 3249 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  3542487 | 3250 | `		iAttrflags = 0;` |
|        - | 3251 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|        - | 3252 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|        - | 3253 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|        - | 3254 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  3542487 | 3255 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 3256 | `			int bMod = 0;` |
|      ! 0 | 3257 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 | 3258 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        - | 3259 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|        - | 3260 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|        - | 3261 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|        - | 3262 | `			 * that the generic keyword dispatch would misread as a method. */` |
|      ! 0 | 3263 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      ! 0 | 3264 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 | 3265 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|      ! 0 | 3266 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|      ! 0 | 3267 | `			}` |
|      ! 0 | 3268 | `			if( !bMod ){` |
|      ! 0 | 3269 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 3270 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 3271 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3272 | `						return SXERR_ABORT;` |
|        - | 3273 | `					}` |
|      ! 0 | 3274 | `					goto done;` |
|        - | 3275 | `				}` |
|      ! 0 | 3276 | `				continue;` |
|        - | 3277 | `			}` |
|      ! 0 | 3278 | `		}` |
|  3542487 | 3279 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 3280 | `			/* Extract the current keyword */` |
|  3542487 | 3281 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  3542487 | 3282 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|        - | 3283 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|     7855 | 3284 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|     7855 | 3285 | `				if( rc != SXRET_OK ){` |
|        6 | 3286 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3287 | `						return SXERR_ABORT;` |
|        - | 3288 | `					}` |
|        6 | 3289 | `					goto done;` |
|        - | 3290 | `				}` |
|     7851 | 3291 | `				continue;` |
|        - | 3292 | `			}` |
|  3534637 | 3293 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 3294 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|        - | 3295 | `				TraitUseEntry sUse;` |
|    15687 | 3296 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    15687 | 3297 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|    15687 | 3298 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|     7849 | 3299 | `				for(;;){` |
|        - | 3300 | `					ph7_class *pTrait;` |
|        - | 3301 | `					SyBlob sResolved;` |
|        - | 3302 | `					SyString sTraitName;` |
|    15695 | 3303 | `					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|        - | 3304 | `					/* A trait name is a full class reference: it may be qualified or` |
|        - | 3305 | ``					 * fully-qualified (`use Foo\Bar\T;`, `use \Foo\Bar\T;`) — the generated`` |
|        - | 3306 | `					 * PHPUnit mocks name their traits absolutely. Parse it with the shared` |
|        - | 3307 | `					 * class-reference reader (handles the leading '\', every '\'-segment and` |
|        - | 3308 | `					 * namespace/import resolution) instead of a single-identifier read, which` |
|        - | 3309 | `					 * choked on the first '\'. */` |
|    15695 | 3310 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    15695 | 3311 | `					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 3312 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 3313 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|      ! 0 | 3314 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|      ! 0 | 3315 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3316 | `							return SXERR_ABORT;` |
|        - | 3317 | `						}` |
|      ! 0 | 3318 | `						break;` |
|        - | 3319 | `					}` |
|    31385 | 3320 | `					pTrait = PH7_VmExtractClass(pGen->pVm,` |
|    15690 | 3321 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    15695 | 3322 | `					SyStringInitFromBuf(&sTraitName,` |
|        - | 3323 | `						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 3324 | `					/* Only traits are allowed */` |
|    15695 | 3325 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 3326 | `						pTrait = pTrait->pNextName;` |
|      ! 0 | 3327 | `					}` |
|    15695 | 3328 | `					if( pTrait == 0 ){` |
|      ! 0 | 3329 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|        - | 3330 | `							"'%z' is not a trait",&sTraitName);` |
|      ! 0 | 3331 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3332 | `							SyBlobRelease(&sResolved);` |
|      ! 0 | 3333 | `							return SXERR_ABORT;` |
|        - | 3334 | `						}` |
|      ! 0 | 3335 | `					}else{` |
|    15695 | 3336 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|        - | 3337 | `					}` |
|    15695 | 3338 | `					SyBlobRelease(&sResolved);` |
|        - | 3339 | `					/* GenStateParseClassReference already advanced past the whole name —` |
|        - | 3340 | `					 * continue only across a comma-separated trait list. */` |
|    15695 | 3341 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     7846 | 3342 | `						break;` |
|        - | 3343 | `					}` |
|       10 | 3344 | `					pGen->pIn++; /* Jump the comma */` |
|        2 | 3345 | `				}` |
|        - | 3346 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|    15687 | 3347 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - | 3348 | `					SyToken *pBlock;` |
|       12 | 3349 | `					pGen->pIn++; /* Jump '{' */` |
|       12 | 3350 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       12 | 3351 | `					sUse.pResolvStart = pGen->pIn;` |
|       12 | 3352 | `					sUse.pResolvEnd = pBlock;` |
|       12 | 3353 | `					if( pBlock < pGen->pEnd ){` |
|       12 | 3354 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|        7 | 3355 | `					}else{` |
|      ! 0 | 3356 | `						pGen->pIn = pGen->pEnd;` |
|        - | 3357 | `					}` |
|        5 | 3358 | `				}` |
|    15687 | 3359 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|        - | 3360 | `				/* The semicolon will be consumed by the outer loop */` |
|    15687 | 3361 | `				continue;` |
|        - | 3362 | `			}` |
|  3518955 | 3363 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        - | 3364 | `				int nSetTok;` |
|  2972695 | 3365 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|  2972695 | 3366 | `				if( nSetVis ){` |
|        - | 3367 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|        - | 3368 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|        3 | 3369 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        3 | 3370 | `					pGen->pIn += nSetTok;` |
|        2 | 3371 | `				}else{` |
|  2972693 | 3372 | `					iProtection = nKwrd;` |
|  2972693 | 3373 | `					pGen->pIn++; /* Jump the visibility token */` |
|        - | 3374 | `					/* Optional asymmetric set-visibility after the read` |
|        - | 3375 | ``					 * visibility: `public private(set) int $x`. */`` |
|  2972693 | 3376 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|  2972693 | 3377 | `					if( nSetVis ){` |
|        9 | 3378 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        9 | 3379 | `						pGen->pIn += nSetTok;` |
|        4 | 3380 | `					}` |
|        - | 3381 | `				}` |
|        - | 3382 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|        - | 3383 | ``				 * `public private(set) readonly int $x`. */`` |
|  2972695 | 3384 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       28 | 3385 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       28 | 3386 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       12 | 3387 | `				}` |
|  2972690 | 3388 | `				if( pGen->pIn >= pGen->pEnd` |
|  2972695 | 3389 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 3390 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3391 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 | 3392 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 3393 | `					if( rc == SXERR_ABORT ){` |
|        - | 3394 | `						/* Error count limit reached,abort immediately */` |
|      ! 0 | 3395 | `						return SXERR_ABORT;` |
|        - | 3396 | `					}` |
|      ! 0 | 3397 | `					goto done;` |
|        - | 3398 | `				}` |
|  2972695 | 3399 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 3400 | `					/* Attribute declaration (untyped) */` |
|   526923 | 3401 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   526923 | 3402 | `					if( rc != SXRET_OK ){` |
|       11 | 3403 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3404 | `							return SXERR_ABORT;` |
|        - | 3405 | `						}` |
|       11 | 3406 | `						goto done;` |
|        - | 3407 | `					}` |
|   534874 | 3408 | `					continue;` |
|        - | 3409 | `				}` |
|  2445777 | 3410 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 3411 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|    15929 | 3412 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    15929 | 3413 | `					if( rc != SXRET_OK ){` |
|        8 | 3414 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3415 | `							return SXERR_ABORT;` |
|        - | 3416 | `						}` |
|        8 | 3417 | `						goto done;` |
|        - | 3418 | `					}` |
|    15923 | 3419 | `					continue;` |
|        - | 3420 | `				}` |
|        - | 3421 | `				/* Extract the keyword */` |
|  2429853 | 3422 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1214924 | 3423 | `			}` |
|  2976113 | 3424 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|        - | 3425 | `				/* Process constant declaration */` |
|   288639 | 3426 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|   288639 | 3427 | `				if( rc != SXRET_OK ){` |
|       11 | 3428 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3429 | `						return SXERR_ABORT;` |
|        - | 3430 | `					}` |
|       11 | 3431 | `					goto done;` |
|        - | 3432 | `				}` |
|   144318 | 3433 | `			}else{` |
|  2687479 | 3434 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - | 3435 | `					/* Static method or attribute,record that */` |
|   101549 | 3436 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|   101549 | 3437 | `					pGen->pIn++; /* Jump the static keyword */` |
|   101549 | 3438 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 3439 | `						int nSetTok;` |
|    74221 | 3440 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|    74221 | 3441 | `						if( nSetVis ){` |
|        - | 3442 | ``							/* `static private(set) int $x` — read side stays public */`` |
|        3 | 3443 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        3 | 3444 | `							pGen->pIn += nSetTok;` |
|        2 | 3445 | `						}else{` |
|        - | 3446 | `							/* Extract the keyword */` |
|    74219 | 3447 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    74219 | 3448 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 3449 | `								iProtection = nKwrd;` |
|      ! 0 | 3450 | `								pGen->pIn++; /* Jump the visibility token */` |
|      ! 0 | 3451 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|      ! 0 | 3452 | `								if( nSetVis ){` |
|      ! 0 | 3453 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|      ! 0 | 3454 | `									pGen->pIn += nSetTok;` |
|      ! 0 | 3455 | `								}` |
|      ! 0 | 3456 | `							}` |
|        - | 3457 | `						}` |
|    37108 | 3458 | `					}` |
|        - | 3459 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|        - | 3460 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|        - | 3461 | `					 * than a generic "expecting method" parse error. */` |
|   101549 | 3462 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 3463 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 | 3464 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|      ! 0 | 3465 | `					}` |
|   101544 | 3466 | `					if( pGen->pIn >= pGen->pEnd` |
|   101549 | 3467 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 3468 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3469 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|      ! 0 | 3470 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 3471 | `						if( rc == SXERR_ABORT ){` |
|        - | 3472 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 3473 | `							return SXERR_ABORT;` |
|        - | 3474 | `						}` |
|      ! 0 | 3475 | `						goto done;` |
|        - | 3476 | `					}` |
|   101549 | 3477 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 3478 | `						/* Attribute declaration */` |
|    27329 | 3479 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    27329 | 3480 | `						if( rc != SXRET_OK ){` |
|        3 | 3481 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 3482 | `								return SXERR_ABORT;` |
|        - | 3483 | `							}` |
|        3 | 3484 | `							goto done;` |
|        - | 3485 | `						}` |
|    27327 | 3486 | `						continue;` |
|        - | 3487 | `					}` |
|    74225 | 3488 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 3489 | `						/* Typed static attribute declaration */` |
|       19 | 3490 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       19 | 3491 | `						if( rc != SXRET_OK ){` |
|        3 | 3492 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 3493 | `								return SXERR_ABORT;` |
|        - | 3494 | `							}` |
|        3 | 3495 | `							goto done;` |
|        - | 3496 | `						}` |
|       17 | 3497 | `						continue;` |
|        - | 3498 | `					}` |
|        - | 3499 | `					/* Extract the keyword */` |
|    74209 | 3500 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  2623037 | 3501 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        - | 3502 | `					/* Abstract method,record that.` |
|        - | 3503 | `					 * PHL used to also mark the whole CLASS abstract here, silently` |
|        - | 3504 | ``					 * promoting `class C{abstract function m();}` -- which php rejects`` |
|        - | 3505 | `					 * outright -- into a valid abstract class. That promotion is why` |
|        - | 3506 | `					 * GenStateCheckAbstractMethods never fired for it: by the time the` |
|        - | 3507 | `					 * check ran, the class looked declared-abstract. The declaration is` |
|        - | 3508 | `					 * now diagnosed where the method name is known (see the install` |
|        - | 3509 | `					 * site), so the class flag stays what the SOURCE said. */` |
|     7823 | 3510 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        - | 3511 | `					/* Advance the stream cursor */` |
|     7823 | 3512 | `					pGen->pIn++;` |
|     7823 | 3513 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     7823 | 3514 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     7823 | 3515 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     7821 | 3516 | `							iProtection = nKwrd;` |
|     7821 | 3517 | `							pGen->pIn++; /* Jump the visibility token */` |
|     3908 | 3518 | `						}` |
|     3909 | 3519 | `					}` |
|     7823 | 3520 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     7818 | 3521 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 3522 | `							/* Static method */` |
|      ! 0 | 3523 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 3524 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 3525 | `					}` |
|     7823 | 3526 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|     7818 | 3527 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|        - | 3528 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|        - | 3529 | `							 * HOOKED property declaration. Route anything that is not a` |
|        - | 3530 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|        - | 3531 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|        - | 3532 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|        6 | 3533 | `							if( pGen->pIn < pGen->pEnd` |
|        7 | 3534 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|        3 | 3535 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|        7 | 3536 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        7 | 3537 | `								if( rc != SXRET_OK ){` |
|      ! 0 | 3538 | `									if( rc == SXERR_ABORT ){` |
|      ! 0 | 3539 | `										return SXERR_ABORT;` |
|        - | 3540 | `									}` |
|      ! 0 | 3541 | `									goto done;` |
|        - | 3542 | `								}` |
|        7 | 3543 | `								continue;` |
|        - | 3544 | `							}` |
|      ! 0 | 3545 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3546 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|      ! 0 | 3547 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 3548 | `							if( rc == SXERR_ABORT ){` |
|        - | 3549 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 3550 | `								return SXERR_ABORT;` |
|        - | 3551 | `							}` |
|      ! 0 | 3552 | `							goto done;` |
|        - | 3553 | `					}` |
|     7817 | 3554 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  2582023 | 3555 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|        - | 3556 | `					/* final method ,record that */` |
|       21 | 3557 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       21 | 3558 | `					pGen->pIn++; /* Jump the final keyword */` |
|       21 | 3559 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 3560 | `						/* Extract the keyword */` |
|       21 | 3561 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       21 | 3562 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       10 | 3563 | `							iProtection = nKwrd;` |
|       10 | 3564 | `							pGen->pIn++; /* Jump the visibility token */` |
|        4 | 3565 | `						}` |
|        9 | 3566 | `					}` |
|       21 | 3567 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       18 | 3568 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|        - | 3569 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|        - | 3570 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|        - | 3571 | `							 * child class is compiled (PH7_ClassInherit). */` |
|       14 | 3572 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|       14 | 3573 | `							if( rc != SXRET_OK ){` |
|      ! 0 | 3574 | `								if( rc == SXERR_ABORT ){` |
|      ! 0 | 3575 | `									return SXERR_ABORT;` |
|        - | 3576 | `								}` |
|      ! 0 | 3577 | `								goto done;` |
|        - | 3578 | `							}` |
|       14 | 3579 | `							continue;` |
|        - | 3580 | `					}` |
|        8 | 3581 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        6 | 3582 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 3583 | `							/* Static method */` |
|      ! 0 | 3584 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 3585 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 3586 | `					}` |
|        8 | 3587 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        6 | 3588 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 3589 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3590 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|      ! 0 | 3591 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 3592 | `							if( rc == SXERR_ABORT ){` |
|        - | 3593 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 3594 | `								return SXERR_ABORT;` |
|        - | 3595 | `							}` |
|      ! 0 | 3596 | `							goto done;` |
|        - | 3597 | `					}` |
|        8 | 3598 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        3 | 3599 | `				}` |
|  2660121 | 3600 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 3601 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3602 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|      ! 0 | 3603 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 3604 | `						if( rc == SXERR_ABORT ){` |
|        - | 3605 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 3606 | `							return SXERR_ABORT;` |
|        - | 3607 | `						}` |
|      ! 0 | 3608 | `						goto done;` |
|        - | 3609 | `				}` |
|  2660121 | 3610 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|        7 | 3611 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|        7 | 3612 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|      ! 0 | 3613 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3614 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 3615 | `						if( rc == SXERR_ABORT ){` |
|        - | 3616 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 3617 | `							return SXERR_ABORT;` |
|        - | 3618 | `						}` |
|      ! 0 | 3619 | `						goto done;` |
|        - | 3620 | `					}` |
|        - | 3621 | `					/* Attribute declaration */` |
|        7 | 3622 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        4 | 3623 | `				}else{` |
|        - | 3624 | `					/* Process method declaration */` |
|  2660115 | 3625 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 3626 | `				}` |
|  2660121 | 3627 | `				if( rc != SXRET_OK ){` |
|       16 | 3628 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3629 | `						return SXERR_ABORT;` |
|        - | 3630 | `					}` |
|       16 | 3631 | `					goto done;` |
|        - | 3632 | `				}` |
|        - | 3633 | `			}` |
|  1474370 | 3634 | `		}else{` |
|        - | 3635 | `			/* Attribute declaration */` |
|      ! 0 | 3636 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 3637 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 3638 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3639 | `					return SXERR_ABORT;` |
|        - | 3640 | `				}` |
|      ! 0 | 3641 | `				goto done;` |
|        - | 3642 | `			}` |
|        - | 3643 | `		}` |
|        5 | 3644 | `	}` |
|        - | 3645 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|        - | 3646 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|        - | 3647 | `	 */` |
|        - | 3648 | `	{` |
|        - | 3649 | `		TraitUseEntry *apUse;` |
|        - | 3650 | `		sxu32 nU;` |
|   454125 | 3651 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   469807 | 3652 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|    15687 | 3653 | `			TraitUseEntry *pUse = &apUse[nU];` |
|    15687 | 3654 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|    15687 | 3655 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|    15687 | 3656 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|        - | 3657 | `			sxu32 nT;` |
|    15687 | 3658 | `			if( !hasResolution ){` |
|        - | 3659 | `				/* No conflict resolution block: use standard trait application */` |
|    31355 | 3660 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|    15683 | 3661 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|    15683 | 3662 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 3663 | `						break;` |
|        - | 3664 | `					}` |
|     7844 | 3665 | `				}` |
|     7841 | 3666 | `			}else{` |
|        - | 3667 | `				/* With resolution block: copy attributes, record traits,` |
|        - | 3668 | `				 * then use the block to resolve method conflicts.` |
|        - | 3669 | `				 */` |
|        - | 3670 | `				SyToken *pR;` |
|       24 | 3671 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       14 | 3672 | `					ph7_class *pTR = apTrait[nT];` |
|        - | 3673 | `					ph7_class_attr *pAR;` |
|        - | 3674 | `					SyHashEntry *pER;` |
|        - | 3675 | `					SyString *pNR;` |
|       14 | 3676 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|       20 | 3677 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|      ! 0 | 3678 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|      ! 0 | 3679 | `						pNR = &pAR->sName;` |
|      ! 0 | 3680 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      ! 0 | 3681 | `							SyHashInsertTail(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|      ! 0 | 3682 | `						}` |
|      ! 0 | 3683 | `					}` |
|       14 | 3684 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|        8 | 3685 | `				}` |
|        - | 3686 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       12 | 3687 | `				pR = pUse->pResolvStart;` |
|       26 | 3688 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 3689 | `					SyString sTrait,sMethod;` |
|        - | 3690 | `					ph7_class *pSrcTrait;` |
|        - | 3691 | `					ph7_class_method *pMeth;` |
|        - | 3692 | `					sxi32 nRKwrd;` |
|       40 | 3693 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       26 | 3694 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       16 | 3695 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       16 | 3696 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       16 | 3697 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       16 | 3698 | `					sMethod = pR->sData;` |
|       16 | 3699 | `					pR++;` |
|       16 | 3700 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 3701 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 3702 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 3703 | `							sTrait = sMethod;` |
|        7 | 3704 | `							pR++;` |
|        7 | 3705 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 3706 | `							sMethod = pR->sData;` |
|        7 | 3707 | `							pR++;` |
|        3 | 3708 | `						}` |
|        3 | 3709 | `					}` |
|       16 | 3710 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 3711 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 3712 | `						continue;` |
|        - | 3713 | `					}` |
|       16 | 3714 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       16 | 3715 | `					pR++;` |
|       16 | 3716 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|        5 | 3717 | `						pSrcTrait = 0;` |
|        7 | 3718 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        7 | 3719 | `							SyString *pTN = &apTrait[nT]->sName;` |
|       10 | 3720 | `							if( pTN->nByte >= sTrait.nByte &&` |
|        6 | 3721 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        5 | 3722 | `								pSrcTrait = apTrait[nT];` |
|        5 | 3723 | `								break;` |
|        - | 3724 | `							}` |
|        2 | 3725 | `						}` |
|        5 | 3726 | `						if( pSrcTrait ){` |
|        5 | 3727 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        5 | 3728 | `							if( pMeth ){` |
|        5 | 3729 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|        5 | 3730 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|        5 | 3731 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|        2 | 3732 | `								}` |
|        2 | 3733 | `							}` |
|        2 | 3734 | `						}` |
|        2 | 3735 | `					}` |
|       34 | 3736 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        2 | 3737 | `				}` |
|        - | 3738 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|       24 | 3739 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        - | 3740 | `					ph7_class_method *pMR;` |
|        - | 3741 | `					SyHashEntry *pER;` |
|        - | 3742 | `					SyString *pNR;` |
|       14 | 3743 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|       40 | 3744 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|       22 | 3745 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|       22 | 3746 | `						pNR = &pMR->sFunc.sName;` |
|       22 | 3747 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       13 | 3748 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|        6 | 3749 | `						}` |
|        2 | 3750 | `					}` |
|        8 | 3751 | `				}` |
|        - | 3752 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       12 | 3753 | `				pR = pUse->pResolvStart;` |
|       26 | 3754 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 3755 | `					SyString sTrait,sMethod,sAlias;` |
|        - | 3756 | `					ph7_class *pSrcTrait;` |
|        - | 3757 | `					ph7_class_method *pMeth;` |
|       26 | 3758 | `					int hasQual = 0;` |
|        - | 3759 | `					sxi32 nRKwrd;` |
|       40 | 3760 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       26 | 3761 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       16 | 3762 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       16 | 3763 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       16 | 3764 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|       16 | 3765 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       16 | 3766 | `					sMethod = pR->sData;` |
|       16 | 3767 | `					pR++;` |
|       16 | 3768 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 3769 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 3770 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 3771 | `							sTrait = sMethod;` |
|        7 | 3772 | `							hasQual = 1;` |
|        7 | 3773 | `							pR++;` |
|        7 | 3774 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 3775 | `							sMethod = pR->sData;` |
|        7 | 3776 | `							pR++;` |
|        3 | 3777 | `						}` |
|        3 | 3778 | `					}` |
|       16 | 3779 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 3780 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 3781 | `						continue;` |
|        - | 3782 | `					}` |
|       16 | 3783 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       16 | 3784 | `					pR++;` |
|       16 | 3785 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       12 | 3786 | `						sxi32 iNewVis = -1;` |
|       12 | 3787 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|        7 | 3788 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|        7 | 3789 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|        7 | 3790 | `								iNewVis = nAK;` |
|        7 | 3791 | `								pR++;` |
|        3 | 3792 | `							}` |
|        3 | 3793 | `						}` |
|       12 | 3794 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       10 | 3795 | `							sAlias = pR->sData;` |
|       10 | 3796 | `							pR++;` |
|        4 | 3797 | `						}` |
|       12 | 3798 | `						pMeth = 0;` |
|       12 | 3799 | `						if( hasQual ){` |
|        3 | 3800 | `							pSrcTrait = 0;` |
|        5 | 3801 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        5 | 3802 | `								SyString *pTN = &apTrait[nT]->sName;` |
|        7 | 3803 | `								if( pTN->nByte >= sTrait.nByte &&` |
|        4 | 3804 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        3 | 3805 | `									pSrcTrait = apTrait[nT];` |
|        3 | 3806 | `									break;` |
|        - | 3807 | `								}` |
|        2 | 3808 | `							}` |
|        3 | 3809 | `							if( pSrcTrait ){` |
|        3 | 3810 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        1 | 3811 | `							}` |
|        2 | 3812 | `						}else{` |
|        9 | 3813 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|        - | 3814 | `						}` |
|       12 | 3815 | `						if( pMeth ){` |
|       12 | 3816 | `							if( sAlias.nByte > 0 ){` |
|        - | 3817 | `								/* Create a shallow copy of the method struct for the alias` |
|        - | 3818 | `								 * so it can carry its own visibility without affecting the original.` |
|        - | 3819 | `								 */` |
|        - | 3820 | `								ph7_class_method *pAlias;` |
|        - | 3821 | `								char *zAliasDup;` |
|       10 | 3822 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       10 | 3823 | `								if( pAlias ){` |
|       10 | 3824 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       10 | 3825 | `									if( iNewVis >= 0 ){` |
|        5 | 3826 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 3827 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 3828 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        2 | 3829 | `									}` |
|       10 | 3830 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       10 | 3831 | `									if( zAliasDup ){` |
|       10 | 3832 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|        4 | 3833 | `									}` |
|        6 | 3834 | `								}` |
|        7 | 3835 | `							}else if( iNewVis >= 0 ){` |
|        - | 3836 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|        - | 3837 | `								ph7_class_method *pCopy;` |
|        3 | 3838 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        3 | 3839 | `								if( pCopy ){` |
|        3 | 3840 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|        3 | 3841 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|        3 | 3842 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 3843 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 3844 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        - | 3845 | `									/* Replace the method in the class hash */` |
|        3 | 3846 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|        3 | 3847 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|        1 | 3848 | `								}` |
|        1 | 3849 | `							}` |
|        5 | 3850 | `						}` |
|        5 | 3851 | `						SXUNUSED(hasQual);` |
|        5 | 3852 | `					}` |
|       20 | 3853 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        2 | 3854 | `				}` |
|        - | 3855 | `			}` |
|    15687 | 3856 | `			SySetRelease(&pUse->aTraits);` |
|     7846 | 3857 | `		}` |
|        - | 3858 | `	}` |
|   454125 | 3859 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - | 3860 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|        - | 3861 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|     3935 | 3862 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|     3935 | 3863 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3864 | `			SySetRelease(&aUseEntries);` |
|      ! 0 | 3865 | `			SySetRelease(&aInterfaces);` |
|      ! 0 | 3866 | `			return SXERR_ABORT;` |
|        - | 3867 | `		}` |
|     1965 | 3868 | `	}` |
|        - | 3869 | `	/* Reject a php-fatal redeclaration before hoisting the class */` |
|   454125 | 3870 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|        9 | 3871 | `		return SXERR_ABORT;` |
|        - | 3872 | `	}` |
|        - | 3873 | `	/* Install the class */` |
|   454119 | 3874 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   454119 | 3875 | `	if( rc == SXRET_OK ){` |
|        - | 3876 | `		ph7_class **apInterface;` |
|        - | 3877 | `		sxu32 n;` |
|   454119 | 3878 | `		if( pBase ){` |
|        - | 3879 | `			/* Inherit from base class and mark as a subclass */` |
|   249785 | 3880 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   124890 | 3881 | `		}` |
|   454119 | 3882 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   653121 | 3883 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|        - | 3884 | `			/* Implements one or more interface */` |
|   199007 | 3885 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|   199007 | 3886 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 3887 | `				break;` |
|        - | 3888 | `			}` |
|    99506 | 3889 | `		}` |
|        - | 3890 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|        - | 3891 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|   454119 | 3892 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|     3933 | 3893 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|     3933 | 3894 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 3895 | `				pIntf = pIntf->pNextName;` |
|      ! 0 | 3896 | `			}` |
|     3933 | 3897 | `			if( pIntf ){` |
|     3933 | 3898 | `				PH7_ClassImplement(pClass,pIntf);` |
|     1964 | 3899 | `			}` |
|     3933 | 3900 | `			if( pClass->nEnumBacking != 0 ){` |
|     3917 | 3901 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|     3917 | 3902 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 3903 | `					pIntf = pIntf->pNextName;` |
|      ! 0 | 3904 | `				}` |
|     3917 | 3905 | `				if( pIntf ){` |
|     3917 | 3906 | `					PH7_ClassImplement(pClass,pIntf);` |
|     1956 | 3907 | `				}` |
|     1956 | 3908 | `			}` |
|     1964 | 3909 | `		}` |
|        - | 3910 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|        - | 3911 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|   454114 | 3912 | `		if( rc == SXRET_OK` |
|   454114 | 3913 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|   454119 | 3914 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   230095 | 3915 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|        - | 3916 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   230095 | 3917 | `			if( pStringable ){` |
|   230095 | 3918 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   230095 | 3919 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|        - | 3920 | `				sxu32 i;` |
|   230095 | 3921 | `				int bAlready = 0;` |
|   276875 | 3922 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    58481 | 3923 | `					if( apImpl[i] == pStringable ){` |
|    11701 | 3924 | `						bAlready = 1;` |
|    11701 | 3925 | `						break;` |
|        - | 3926 | `					}` |
|    23395 | 3927 | `				}` |
|   230095 | 3928 | `				if( !bAlready ){` |
|   218399 | 3929 | `					PH7_ClassImplement(pClass,pStringable);` |
|   109197 | 3930 | `				}` |
|   115045 | 3931 | `			}` |
|   115045 | 3932 | `		}` |
|        - | 3933 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   454119 | 3934 | `		if( rc == SXRET_OK ){` |
|   454119 | 3935 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   454119 | 3936 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 3937 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 3938 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 3939 | `				return SXERR_ABORT;` |
|        - | 3940 | `			}` |
|   227057 | 3941 | `		}` |
|        - | 3942 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   454119 | 3943 | `		if( rc == SXRET_OK ){` |
|   454119 | 3944 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   454119 | 3945 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 3946 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 3947 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 3948 | `				return SXERR_ABORT;` |
|        - | 3949 | `			}` |
|   227057 | 3950 | `		}` |
|   227057 | 3951 | `	}` |
|   454119 | 3952 | `	SySetRelease(&aUseEntries);` |
|   454119 | 3953 | `	SySetRelease(&aInterfaces);` |
|   454119 | 3954 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3955 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3956 | `		return SXERR_ABORT;` |
|        - | 3957 | `	}` |
|   227057 | 3958 | `done:` |
|        - | 3959 | `	/* Point beyond the class body */` |
|   454161 | 3960 | `	pGen->pIn = &pEnd[1];` |
|   454161 | 3961 | `	pGen->pEnd = pTmp;` |
|   454161 | 3962 | `	return PH7_OK;` |
|   227087 | 3963 | `}` |
|        - | 3964 | `/* Compile a named class declaration (the common case). */` |
|   454134 | 3965 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|        5 | 3966 | `{` |
|   454139 | 3967 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|        5 | 3968 | `}` |
|        - | 3969 | `/*` |
|        - | 3970 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|        - | 3971 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|        - | 3972 | ` * compile + install the class body once (at compile time, like every other` |
|        - | 3973 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|        - | 3974 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|        - | 3975 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|        - | 3976 | ` */` |
|       30 | 3977 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 | 3978 | `{` |
|        - | 3979 | `	char zName[128];         /* Synthesized class name */` |
|        - | 3980 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|        - | 3981 | `	SyString sName;` |
|        - | 3982 | `	SyToken *pArgStart,*pArgEnd;` |
|       34 | 3983 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|        - | 3984 | `	                              * is keyed to this 'class' token */` |
|        - | 3985 | `	ph7_value *pObj;` |
|       34 | 3986 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3987 | `	sxu32 nIdx,nLen;` |
|        - | 3988 | `	sxi32 nArg,rc;` |
|       15 | 3989 | `	SXUNUSED(iCompileFlag);` |
|        - | 3990 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|       34 | 3991 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       34 | 3992 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 | 3993 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      ! 0 | 3994 | `	}` |
|       34 | 3995 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - | 3996 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|        - | 3997 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|        - | 3998 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|       34 | 3999 | `	pArgStart = pArgEnd = 0;` |
|       34 | 4000 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|       34 | 4001 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 4002 | `		return rc;` |
|        - | 4003 | `	}` |
|        - | 4004 | `	{` |
|        - | 4005 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|       34 | 4006 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|       30 | 4007 | `		if( pAnonClass` |
|       34 | 4008 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 4009 | `			return SXERR_ABORT;` |
|        - | 4010 | `		}` |
|        - | 4011 | `	}` |
|        - | 4012 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|        - | 4013 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|       34 | 4014 | `	nArg = 0;` |
|       34 | 4015 | `	if( pArgStart < pArgEnd ){` |
|        7 | 4016 | `		SyToken *pSavedIn = pGen->pIn;` |
|        7 | 4017 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 4018 | `		SyToken *pArgNext;` |
|        7 | 4019 | `		pGen->pIn = pArgStart;` |
|        7 | 4020 | `		pGen->pEnd = pArgEnd;` |
|       13 | 4021 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|        7 | 4022 | `			if( pGen->pIn < pArgNext ){` |
|        7 | 4023 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|        7 | 4024 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 4025 | `					pGen->pIn = pSavedIn;` |
|      ! 0 | 4026 | `					pGen->pEnd = pSavedEnd;` |
|      ! 0 | 4027 | `					return SXERR_ABORT;` |
|        - | 4028 | `				}` |
|        7 | 4029 | `				nArg++;` |
|        3 | 4030 | `			}` |
|        7 | 4031 | `			pGen->pIn = &pArgNext[1];` |
|        1 | 4032 | `		}` |
|        7 | 4033 | `		pGen->pIn = pSavedIn;` |
|        7 | 4034 | `		pGen->pEnd = pSavedEnd;` |
|        3 | 4035 | `	}` |
|        - | 4036 | `	/* Load the synthesized class name */` |
|       34 | 4037 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       34 | 4038 | `	if( pObj == 0 ){` |
|      ! 0 | 4039 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 4040 | `		return SXERR_ABORT;` |
|        - | 4041 | `	}` |
|       34 | 4042 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       34 | 4043 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - | 4044 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|       34 | 4045 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       34 | 4046 | `	return SXRET_OK;` |
|       19 | 4047 | `}` |
|        - | 4048 | `/*` |
|        - | 4049 | ` * Compile a user-defined abstract class.` |
|        - | 4050 | ` *  According to the PHP language reference manual` |
|        - | 4051 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|        - | 4052 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|        - | 4053 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|        - | 4054 | ` *   the method's signature - they cannot define the implementation.` |
|        - | 4055 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|        - | 4056 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|        - | 4057 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|        - | 4058 | ` *   method is defined as protected, the function implementation must be defined as either` |
|        - | 4059 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|        - | 4060 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|        - | 4061 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|        - | 4062 | ` *   could differ.` |
|        - | 4063 | ` */` |
|        - | 4064 | `/*` |
|        - | 4065 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|        - | 4066 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|        - | 4067 | ` * receives the corresponding PH7_CLASS_* bit.` |
|        - | 4068 | ` */` |
| 14513538 | 4069 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|        5 | 4070 | `{` |
| 14513543 | 4071 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  8686631 | 4072 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  8686631 | 4073 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  8632013 | 4074 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  4296461 | 4075 | `	}` |
| 14419839 | 4076 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
| 14419779 | 4077 | `	return FALSE;` |
|  7256774 | 4078 | `}` |
|        - | 4079 | `/*` |
|        - | 4080 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|        - | 4081 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|        - | 4082 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|        - | 4083 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|        - | 4084 | ` */` |
| 14419774 | 4085 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|        5 | 4086 | `{` |
| 14419779 | 4087 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
| 14419779 | 4088 | `	sxi32 iFlags = 0,iFlag;` |
| 14513543 | 4089 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    93769 | 4090 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|        5 | 4091 | `			pDup = pIn;` |
|        2 | 4092 | `		}` |
|    93769 | 4093 | `		iFlags \|= iFlag;` |
|    93769 | 4094 | `		pIn++;` |
|        5 | 4095 | `	}` |
| 14419779 | 4096 | `	*ppIn = pIn;` |
| 14419779 | 4097 | `	if( ppDup ){ *ppDup = pDup; }` |
| 14419779 | 4098 | `	return iFlags;` |
|        5 | 4099 | `}` |
|        - | 4100 | `/*` |
|        - | 4101 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|        - | 4102 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|        - | 4103 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|        - | 4104 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|        - | 4105 | `` * `readonly`) to their existing handlers.`` |
|        - | 4106 | ` */` |
| 14376800 | 4107 | `PH7_PRIVATE int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|        5 | 4108 | `{` |
| 14376805 | 4109 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  7239177 | 4110 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
| 14402187 | 4111 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|        5 | 4112 | `}` |
|        - | 4113 | `/*` |
|        - | 4114 | ` * Compile a class declaration carrying one or more leading modifiers` |
|        - | 4115 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|        - | 4116 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|        - | 4117 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|        - | 4118 | `` * `abstract`+`final` pair, like PHP.`` |
|        - | 4119 | ` */` |
|    42974 | 4120 | `PH7_PRIVATE sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|        5 | 4121 | `{` |
|        - | 4122 | `	SyToken *pDup;` |
|    42979 | 4123 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|        - | 4124 | `	sxi32 rc;` |
|    42979 | 4125 | `	if( pDup ){` |
|        4 | 4126 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|        2 | 4127 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|        3 | 4128 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4129 | `			return SXERR_ABORT;` |
|        - | 4130 | `		}` |
|        1 | 4131 | `	}` |
|    42974 | 4132 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    21492 | 4133 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|        3 | 4134 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4135 | `			"Cannot use the final modifier on an abstract class");` |
|        3 | 4136 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4137 | `			return SXERR_ABORT;` |
|        - | 4138 | `		}` |
|        1 | 4139 | `	}` |
|    42979 | 4140 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    21492 | 4141 | `}` |
|        - | 4142 | `/*` |
|        - | 4143 | ` * Compile a user-defined trait.` |
|        - | 4144 | ` *  Traits are similar to classes, but only intended to group functionality` |
|        - | 4145 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|        - | 4146 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|        - | 4147 | ` */` |
|     7896 | 4148 | `PH7_PRIVATE sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|        5 | 4149 | `{` |
|     7901 | 4150 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 4151 | `	ph7_class *pClass;` |
|        - | 4152 | `	SyToken *pEnd,*pTmp;` |
|        - | 4153 | `	sxi32 iProtection;` |
|        - | 4154 | `	sxi32 iAttrflags;` |
|        - | 4155 | `	SyString *pName;` |
|        - | 4156 | `	sxi32 nKwrd;` |
|        - | 4157 | `	sxi32 rc;` |
|        - | 4158 | `	/* Jump the 'trait' keyword */` |
|     7901 | 4159 | `	pGen->pIn++;` |
|     7901 | 4160 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 4161 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|      ! 0 | 4162 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4163 | `			return SXERR_ABORT;` |
|        - | 4164 | `		}` |
|      ! 0 | 4165 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|      ! 0 | 4166 | `			pGen->pIn++;` |
|      ! 0 | 4167 | `		}` |
|      ! 0 | 4168 | `		return SXRET_OK;` |
|        - | 4169 | `	}` |
|        - | 4170 | `	/* Extract trait name */` |
|     7901 | 4171 | `	pName = &pGen->pIn->sData;` |
|     7901 | 4172 | `	pGen->pIn++;` |
|        - | 4173 | `	/* Build FQN and obtain a raw class */ {` |
|        - | 4174 | `		SyBlob sFQN;` |
|        - | 4175 | `		SyString sFQNStr;` |
|     7901 | 4176 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     7901 | 4177 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     7901 | 4178 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|     7901 | 4179 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     7901 | 4180 | `		SyBlobRelease(&sFQN);` |
|        - | 4181 | `	}` |
|     7901 | 4182 | `	if( pClass == 0 ){` |
|      ! 0 | 4183 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4184 | `		return SXERR_ABORT;` |
|        - | 4185 | `	}` |
|     7901 | 4186 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     7901 | 4187 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 4188 | `		return SXERR_ABORT;` |
|        - | 4189 | `	}` |
|        - | 4190 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|     7901 | 4191 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 | 4192 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|      ! 0 | 4193 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 4194 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4195 | `			return SXERR_ABORT;` |
|        - | 4196 | `		}` |
|      ! 0 | 4197 | `		return SXRET_OK;` |
|        - | 4198 | `	}` |
|     7901 | 4199 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     7901 | 4200 | `	pEnd = 0;` |
|     7901 | 4201 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|     7901 | 4202 | `	if( pEnd >= pGen->pEnd ){` |
|      ! 0 | 4203 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|      ! 0 | 4204 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 4205 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4206 | `			return SXERR_ABORT;` |
|        - | 4207 | `		}` |
|      ! 0 | 4208 | `		return SXRET_OK;` |
|        - | 4209 | `	}` |
|        - | 4210 | `	/* The delimiter token is the trait body's closing brace */` |
|     7901 | 4211 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 4212 | `	/* Swap token stream */` |
|     7901 | 4213 | `	pTmp = pGen->pEnd;` |
|     7901 | 4214 | `	pGen->pEnd = pEnd;` |
|        - | 4215 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|     7901 | 4216 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|        - | 4217 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|    56622 | 4218 | `	for(;;){` |
|   160087 | 4219 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|    23427 | 4220 | `			pGen->pIn++;` |
|        5 | 4221 | `		}` |
|   136665 | 4222 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     7901 | 4223 | `			break;` |
|        - | 4224 | `		}` |
|        - | 4225 | `		/* Bind a directly-preceding docblock to this member */` |
|   128769 | 4226 | `		GenStateSetPendingDoc(&(*pGen));` |
|   128769 | 4227 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|      ! 0 | 4228 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4229 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 4230 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 4231 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4232 | `				return SXERR_ABORT;` |
|        - | 4233 | `			}` |
|      ! 0 | 4234 | `			goto done;` |
|        - | 4235 | `		}` |
|   128769 | 4236 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   128769 | 4237 | `		iAttrflags = 0;` |
|   128769 | 4238 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   128769 | 4239 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   128769 | 4240 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 4241 | `				/* Trait uses another trait: use OtherTrait; */` |
|        5 | 4242 | `				pGen->pIn++; /* Jump 'use' */` |
|        2 | 4243 | `				for(;;){` |
|        - | 4244 | `					ph7_class *pUsedTrait;` |
|        - | 4245 | `					SyString *pUsedName;` |
|        5 | 4246 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 4247 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 4248 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|      ! 0 | 4249 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4250 | `							return SXERR_ABORT;` |
|        - | 4251 | `						}` |
|      ! 0 | 4252 | `						break;` |
|        - | 4253 | `					}` |
|        5 | 4254 | `					pUsedName = &pGen->pIn->sData;` |
|        - | 4255 | `					{` |
|        - | 4256 | `						SyBlob sResolved;` |
|        5 | 4257 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        5 | 4258 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|        7 | 4259 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|        4 | 4260 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|        5 | 4261 | `						SyBlobRelease(&sResolved);` |
|        - | 4262 | `					}` |
|        5 | 4263 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 4264 | `						pUsedTrait = pUsedTrait->pNextName;` |
|      ! 0 | 4265 | `					}` |
|        5 | 4266 | `					if( pUsedTrait == 0 ){` |
|        4 | 4267 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        1 | 4268 | `							"'%z' is not a trait",pUsedName);` |
|        3 | 4269 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4270 | `							return SXERR_ABORT;` |
|        - | 4271 | `						}` |
|        2 | 4272 | `					}else{` |
|        3 | 4273 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|        - | 4274 | `					}` |
|        5 | 4275 | `					pGen->pIn++;` |
|        5 | 4276 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|        3 | 4277 | `						break;` |
|        - | 4278 | `					}` |
|      ! 0 | 4279 | `					pGen->pIn++;` |
|      ! 0 | 4280 | `				}` |
|        5 | 4281 | `				continue;` |
|        - | 4282 | `			}` |
|   128765 | 4283 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|   128747 | 4284 | `				iProtection = nKwrd;` |
|   128747 | 4285 | `				pGen->pIn++;` |
|        - | 4286 | ``				/* Optional `readonly` after the visibility (PHP 8.1): `private readonly T`` |
|        - | 4287 | ``				 * $x` — the generated PHPUnit runtime traits (StubApi, …) declare their`` |
|        - | 4288 | `				 * readonly state this way. Mirrors the class-body attribute parser. */` |
|   128747 | 4289 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|        5 | 4290 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|        5 | 4291 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        2 | 4292 | `				}` |
|   128742 | 4293 | `				if( pGen->pIn >= pGen->pEnd` |
|   128747 | 4294 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 4295 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4296 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 4297 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 4298 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 4299 | `						return SXERR_ABORT;` |
|        - | 4300 | `					}` |
|      ! 0 | 4301 | `					goto done;` |
|        - | 4302 | `				}` |
|   128747 | 4303 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|    23407 | 4304 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    23407 | 4305 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 4306 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4307 | `							return SXERR_ABORT;` |
|        - | 4308 | `						}` |
|      ! 0 | 4309 | `						goto done;` |
|        - | 4310 | `					}` |
|    23407 | 4311 | `					continue;` |
|        - | 4312 | `				}` |
|   105345 | 4313 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        9 | 4314 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        9 | 4315 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 4316 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4317 | `							return SXERR_ABORT;` |
|        - | 4318 | `						}` |
|      ! 0 | 4319 | `						goto done;` |
|        - | 4320 | `					}` |
|        9 | 4321 | `					continue;` |
|        - | 4322 | `				}` |
|   105337 | 4323 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    52666 | 4324 | `			}` |
|   105355 | 4325 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|      ! 0 | 4326 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4327 | `					"Traits cannot have constants");` |
|      ! 0 | 4328 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 4329 | `					return SXERR_ABORT;` |
|        - | 4330 | `				}` |
|      ! 0 | 4331 | `				goto done;` |
|      ! 0 | 4332 | `			}else{` |
|   105355 | 4333 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|     7811 | 4334 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     7811 | 4335 | `					pGen->pIn++;` |
|     7811 | 4336 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     7809 | 4337 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     7809 | 4338 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 4339 | `							iProtection = nKwrd;` |
|      ! 0 | 4340 | `							pGen->pIn++;` |
|      ! 0 | 4341 | `						}` |
|     3902 | 4342 | `					}` |
|     7806 | 4343 | `					if( pGen->pIn >= pGen->pEnd` |
|     7811 | 4344 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 4345 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4346 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|      ! 0 | 4347 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 4348 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4349 | `							return SXERR_ABORT;` |
|        - | 4350 | `						}` |
|      ! 0 | 4351 | `						goto done;` |
|        - | 4352 | `					}` |
|     7811 | 4353 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        3 | 4354 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        3 | 4355 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 4356 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 4357 | `								return SXERR_ABORT;` |
|        - | 4358 | `							}` |
|      ! 0 | 4359 | `							goto done;` |
|        - | 4360 | `						}` |
|        3 | 4361 | `						continue;` |
|        - | 4362 | `					}` |
|     7809 | 4363 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|      ! 0 | 4364 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 4365 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 4366 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 4367 | `								return SXERR_ABORT;` |
|        - | 4368 | `							}` |
|      ! 0 | 4369 | `							goto done;` |
|        - | 4370 | `						}` |
|      ! 0 | 4371 | `						continue;` |
|        - | 4372 | `					}` |
|     7809 | 4373 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   101451 | 4374 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        9 | 4375 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        9 | 4376 | `					pGen->pIn++;` |
|        9 | 4377 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        9 | 4378 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        9 | 4379 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        9 | 4380 | `							iProtection = nKwrd;` |
|        9 | 4381 | `							pGen->pIn++;` |
|        3 | 4382 | `						}` |
|        3 | 4383 | `					}` |
|        9 | 4384 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        6 | 4385 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 4386 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4387 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|      ! 0 | 4388 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 4389 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4390 | `							return SXERR_ABORT;` |
|        - | 4391 | `						}` |
|      ! 0 | 4392 | `						goto done;` |
|        - | 4393 | `					}` |
|        9 | 4394 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        3 | 4395 | `				}` |
|   105353 | 4396 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 4397 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4398 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|      ! 0 | 4399 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 4400 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 4401 | `						return SXERR_ABORT;` |
|        - | 4402 | `					}` |
|      ! 0 | 4403 | `					goto done;` |
|        - | 4404 | `				}` |
|   105353 | 4405 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|      ! 0 | 4406 | `					pGen->pIn++;` |
|      ! 0 | 4407 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 4408 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4409 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 4410 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4411 | `							return SXERR_ABORT;` |
|        - | 4412 | `						}` |
|      ! 0 | 4413 | `						goto done;` |
|        - | 4414 | `					}` |
|      ! 0 | 4415 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 4416 | `				}else{` |
|   105353 | 4417 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 4418 | `				}` |
|   105353 | 4419 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 4420 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 4421 | `						return SXERR_ABORT;` |
|        - | 4422 | `					}` |
|      ! 0 | 4423 | `					goto done;` |
|        - | 4424 | `				}` |
|        - | 4425 | `			}` |
|    52679 | 4426 | `		}else{` |
|      ! 0 | 4427 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 4428 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 4429 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 4430 | `					return SXERR_ABORT;` |
|        - | 4431 | `				}` |
|      ! 0 | 4432 | `				goto done;` |
|        - | 4433 | `			}` |
|        - | 4434 | `		}` |
|        5 | 4435 | `	}` |
|        - | 4436 | `	/* Reject a php-fatal redeclaration before hoisting the trait */` |
|     7901 | 4437 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|        3 | 4438 | `		return SXERR_ABORT;` |
|        - | 4439 | `	}` |
|        - | 4440 | `	/* Install the trait */` |
|     7899 | 4441 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     7899 | 4442 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 4443 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4444 | `		return SXERR_ABORT;` |
|        - | 4445 | `	}` |
|     3947 | 4446 | `done:` |
|        - | 4447 | `	/* Point beyond the trait body */` |
|     7899 | 4448 | `	pGen->pIn = &pEnd[1];` |
|     7899 | 4449 | `	pGen->pEnd = pTmp;` |
|     7899 | 4450 | `	return PH7_OK;` |
|     3953 | 4451 | `}` |
|        - | 4452 | `/*` |
|        - | 4453 | ` * Compile a user-defined class.` |
|        - | 4454 | ` *  According to the PHP language reference manual` |
|        - | 4455 | ` *   Basic class definitions begin with the keyword class, followed` |
|        - | 4456 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|        - | 4457 | ` *   the definitions of the properties and methods belonging to the class.` |
|        - | 4458 | ` *   A class may contain its own constants, variables (called "properties")` |
|        - | 4459 | ` *   and functions (called "methods").` |
|        - | 4460 | ` */` |
|   407226 | 4461 | `PH7_PRIVATE sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|        5 | 4462 | `{` |
|        - | 4463 | `	sxi32 rc;` |
|   407231 | 4464 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   407231 | 4465 | `	return rc;` |
|        5 | 4466 | `}` |
|        - | 4467 | `/*` |
|        - | 4468 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|        - | 4469 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|        - | 4470 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|        - | 4471 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|        - | 4472 | `` * meaning; `enum Name` can never start a valid expression.`` |
|        - | 4473 | ` */` |
| 14326030 | 4474 | `PH7_PRIVATE int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|        5 | 4475 | `{` |
| 14543111 | 4476 | `	return (pIn->nType & PH7_TK_ID)` |
|  7380091 | 4477 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|   226968 | 4478 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
| 14543106 | 4479 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|        5 | 4480 | `}` |
|        - | 4481 | `/*` |
|        - | 4482 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|        - | 4483 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|        - | 4484 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|        - | 4485 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|        - | 4486 | ` */` |
|     3934 | 4487 | `PH7_PRIVATE sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|        5 | 4488 | `{` |
|     3939 | 4489 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|        5 | 4490 | `}` |
|        - | 4491 |  |
