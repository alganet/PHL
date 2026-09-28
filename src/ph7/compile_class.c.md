# src/ph7/compile_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2781/3583 lines (77.62%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `#include "compile_int.h"` |
|       - |    8 | `/* Forward declaration — deferred class declarations (defined with the deferral` |
|       - |    9 | ` * helpers ahead of GenStateCompileClassEx; used by the interface/trait` |
|       - |   10 | ` * compilers that precede them in this file). */` |
|       - |   11 | `static int GenStateMaybeDeferClass(ph7_gen_state *pGen,sxi32 iFlags,int iSelfKind,sxi32 *pRc);` |
|       - |   12 | `/*` |
|       - |   13 | ` * Section:` |
|       - |   14 | ` *    Class/OO compilation: classes, interfaces, traits, enums, anonymous` |
|       - |   15 | ` *    classes, class constants, typed properties, property hooks and methods.` |
|       - |   16 | ` * Status:` |
|       - |   17 | ` *    Stable.` |
|       - |   18 | ` */` |
|      10 |   19 | `static const char * GenStateClassKind(const ph7_class *pClass)` |
|       4 |   20 | `{` |
|      14 |   21 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){ return "interface"; }` |
|      12 |   22 | `	if( pClass->iFlags & PH7_CLASS_TRAIT ){ return "trait"; }` |
|       9 |   23 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){ return "enum"; }` |
|       6 |   24 | `	return "class";` |
|       9 |   25 | `}` |
|       - |   26 | `/*` |
|       - |   27 | ` * Guard a class/interface/trait/enum about to be installed. Returns SXERR_ABORT` |
|       - |   28 | ` * (after emitting the fatal) if it redeclares an already-bound type; otherwise` |
|       - |   29 | ` * marks it bound (when unconditional & top-level) and returns SXRET_OK.` |
|       - |   30 | ` */` |
|    4422 |   31 | `static sxi32 GenStateGuardClassRedeclaration(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |   32 | `{` |
|       - |   33 | `	SyHashEntry *pEntry;` |
|    4427 |   34 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|      63 |   35 | `		return SXRET_OK; /* conditional/nested: keep hoisting */` |
|       - |   36 | `	}` |
|    4369 |   37 | `	pClass->iFlags \|= PH7_CLASS_BOUND;` |
|    4369 |   38 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|     ! 0 |   39 | `		return SXRET_OK; /* the prelude installs each builtin exactly once */` |
|       - |   40 | `	}` |
|    4369 |   41 | `	pEntry = SyHashGet(&pGen->pVm->hClass,(const void *)pClass->sName.zString,pClass->sName.nByte);` |
|    4369 |   42 | `	if( pEntry ){` |
|      17 |   43 | `		ph7_class *pPrev = (ph7_class *)pEntry->pUserData;` |
|      19 |   44 | `		while( pPrev ){` |
|      17 |   45 | `			if( pPrev->iFlags & PH7_CLASS_BOUND ){` |
|       - |   46 | `				/* php names the entity by the PREVIOUS declaration's kind and omits` |
|       - |   47 | `				 * the "(previously declared in ...)" clause for internal symbols. */` |
|      14 |   48 | `				if( pPrev->sFile.nByte > 0 ){` |
|      16 |   49 | `					PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|       - |   50 | `						"Cannot redeclare %s %z (previously declared in %.*s:%u)",` |
|       4 |   51 | `						GenStateClassKind(pPrev),&pClass->sName,` |
|       4 |   52 | `						pPrev->sFile.nByte,pPrev->sFile.zString,pPrev->nLine);` |
|       8 |   53 | `				}else{` |
|       4 |   54 | `					PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|       1 |   55 | `						"Cannot redeclare %s %z",GenStateClassKind(pPrev),&pClass->sName);` |
|       - |   56 | `				}` |
|      14 |   57 | `				return SXERR_ABORT;` |
|       - |   58 | `			}` |
|       3 |   59 | `			pPrev = pPrev->pNextName;` |
|       1 |   60 | `		}` |
|       1 |   61 | `	}` |
|    4359 |   62 | `	return SXRET_OK;` |
|    2216 |   63 | `}` |
|       - |   64 | `/*` |
|       - |   65 | ` * Extract the visibility level associated with a given keyword.` |
|       - |   66 | ` * According to the PHP language reference manual` |
|       - |   67 | ` *  Visibility:` |
|       - |   68 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |   69 | ` *  the declaration with the keywords public, protected or private.` |
|       - |   70 | ` *  Class members declared public can be accessed everywhere.` |
|       - |   71 | ` *  Members declared protected can be accessed only within the class` |
|       - |   72 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |   73 | ` *  may only be accessed by the class that defines the member.` |
|       - |   74 | ` */` |
|    6554 |   75 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |   76 | `{` |
|    6559 |   77 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|     499 |   78 | `		return PH7_CLASS_PROT_PRIVATE;` |
|    6065 |   79 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|     187 |   80 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |   81 | `	}` |
|       - |   82 | `	/* Assume public by default */` |
|    5883 |   83 | `	return PH7_CLASS_PROT_PUBLIC;` |
|    3282 |   84 | `}` |
|       - |   85 | `/*` |
|       - |   86 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|       - |   87 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|       - |   88 | ` * token immediately followed by '='. Anything else with a leading type token` |
|       - |   89 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|       - |   90 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|       - |   91 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|       - |   92 | ` */` |
|     492 |   93 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|       5 |   94 | `{` |
|       - |   95 | `	SyToken *p0, *p1;` |
|     497 |   96 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |   97 | `		return 0;` |
|       - |   98 | `	}` |
|     497 |   99 | `	p0 = pGen->pIn;` |
|       - |  100 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|     497 |  101 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|     ! 0 |  102 | `		return 1;` |
|       - |  103 | `	}` |
|     497 |  104 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|       5 |  105 | `		return 1;` |
|       - |  106 | `	}` |
|       - |  107 | `	/* A name-like first token begins a type only when followed by another` |
|       - |  108 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|       - |  109 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|     493 |  110 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     493 |  111 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|     493 |  112 | `		if( p1 ){` |
|     493 |  113 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|      47 |  114 | `				return 1;` |
|       - |  115 | `			}` |
|     451 |  116 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|       5 |  117 | `				return 1;` |
|       - |  118 | `			}` |
|     221 |  119 | `		}` |
|     221 |  120 | `	}` |
|     447 |  121 | `	return 0;` |
|     251 |  122 | `}` |
|       - |  123 | `/*` |
|       - |  124 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|       - |  125 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|       - |  126 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|       - |  127 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|       - |  128 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|       - |  129 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|       - |  130 | ` * Peek only; never consumes tokens.` |
|       - |  131 | ` */` |
|      34 |  132 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|       4 |  133 | `{` |
|      38 |  134 | `	SyToken *p = pGen->pIn;` |
|      54 |  135 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      25 |  136 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|       3 |  137 | `		p++; /* skip leading unary sign(s) */` |
|       1 |  138 | `	}` |
|      38 |  139 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|      33 |  140 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|       - |  141 | `	}` |
|       6 |  142 | `	p++;` |
|       - |  143 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|       6 |  144 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|      21 |  145 | `}` |
|       - |  146 | `/*` |
|       - |  147 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|       - |  148 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|       - |  149 | `` * `$o->new`), not a `new` expression.`` |
|       - |  150 | ` */` |
|     190 |  151 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|       5 |  152 | `{` |
|       - |  153 | `	sxi32 iOp;` |
|     195 |  154 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|      33 |  155 | `		return 0;` |
|       - |  156 | `	}` |
|     165 |  157 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|     165 |  158 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|     100 |  159 | `}` |
|       - |  160 | `/*` |
|       - |  161 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|       - |  162 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|       - |  163 | ` * interface-constant and (instance/static) property-default initializers` |
|       - |  164 | ` * ("New expressions are not supported in this context") while still allowing it` |
|       - |  165 | ` * in global constants, parameter defaults and static-local initializers (which` |
|       - |  166 | ` * are compiled by different functions and left untouched). The scan is` |
|       - |  167 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|       - |  168 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|       - |  169 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|       - |  170 | ` *` |
|       - |  171 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|       - |  172 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|       - |  173 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|       - |  174 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|       - |  175 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|       - |  176 | ` */` |
|    1958 |  177 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|       5 |  178 | `{` |
|    1963 |  179 | `	SyToken *p = pGen->pIn;` |
|    1963 |  180 | `	int iDepth = 0;` |
|   18171 |  181 | `	while( p < pGen->pEnd ){` |
|   18171 |  182 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    1901 |  183 | `			break; /* end of this initializer */` |
|       - |  184 | `		}` |
|   16270 |  185 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    8182 |  186 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      74 |  187 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|       - |  188 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|       - |  189 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|       - |  190 | `			 * expression. */` |
|      24 |  191 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|      24 |  192 | `			p++;` |
|      24 |  193 | `			if( bArrow ){` |
|       - |  194 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|       - |  195 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|     ! 0 |  196 | `				int iBase = iDepth;` |
|     ! 0 |  197 | `				while( p < pGen->pEnd ){` |
|     ! 0 |  198 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  199 | `						iDepth++;` |
|     ! 0 |  200 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  201 | `						if( iDepth <= iBase ){` |
|     ! 0 |  202 | `							break; /* closes an enclosing group, not the fn's own */` |
|       - |  203 | `						}` |
|     ! 0 |  204 | `						iDepth--;` |
|     ! 0 |  205 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|     ! 0 |  206 | `						break;` |
|       - |  207 | `					}` |
|     ! 0 |  208 | `					p++;` |
|     ! 0 |  209 | `				}` |
|     ! 0 |  210 | `			}else{` |
|       - |  211 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|       - |  212 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|       - |  213 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|       - |  214 | `				 * then skip the balanced brace block. */` |
|      24 |  215 | `				int iLocal = 0;` |
|      64 |  216 | `				while( p < pGen->pEnd ){` |
|      64 |  217 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|      24 |  218 | `						break; /* body brace */` |
|       - |  219 | `					}` |
|      44 |  220 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      24 |  221 | `						iLocal++;` |
|      34 |  222 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      24 |  223 | `						if( iLocal > 0 ){` |
|      24 |  224 | `							iLocal--;` |
|      10 |  225 | `						}` |
|      10 |  226 | `					}` |
|      44 |  227 | `					p++;` |
|       4 |  228 | `				}` |
|      24 |  229 | `				if( p < pGen->pEnd ){` |
|      24 |  230 | `					int iBrace = 0; /* p is on the body '{' */` |
|     170 |  231 | `					while( p < pGen->pEnd ){` |
|     170 |  232 | `						if( p->nType & PH7_TK_OCB ){` |
|      28 |  233 | `							iBrace++;` |
|     158 |  234 | `						}else if( p->nType & PH7_TK_CCB ){` |
|      28 |  235 | `							iBrace--;` |
|      28 |  236 | `							if( iBrace == 0 ){` |
|      24 |  237 | `								p++;` |
|      24 |  238 | `								break;` |
|       - |  239 | `							}` |
|       2 |  240 | `						}` |
|     150 |  241 | `						p++;` |
|       4 |  242 | `					}` |
|      10 |  243 | `				}` |
|       - |  244 | `			}` |
|      24 |  245 | `			continue;` |
|       - |  246 | `		}` |
|   16255 |  247 | `		if( p->nType & PH7_TK_OCB ){` |
|      57 |  248 | `			if( iDepth == 0 ){` |
|       - |  249 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|       - |  250 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|       - |  251 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|       - |  252 | `				 * is legal — don't scan into it. */` |
|      57 |  253 | `				break;` |
|       - |  254 | `			}` |
|     ! 0 |  255 | `			iDepth++;` |
|   16201 |  256 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     193 |  257 | `			iDepth++;` |
|   16107 |  258 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     191 |  259 | `			if( iDepth > 0 ){` |
|     191 |  260 | `				iDepth--;` |
|      93 |  261 | `			}` |
|   15920 |  262 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    7635 |  263 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|       - |  264 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|       - |  265 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|       - |  266 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|      11 |  267 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|      11 |  268 | `				return 1;` |
|       - |  269 | `			}` |
|     ! 0 |  270 | `		}` |
|   16193 |  271 | `		p++;` |
|       5 |  272 | `	}` |
|    1955 |  273 | `	return 0;` |
|     984 |  274 | `}` |
|       - |  275 | `/*` |
|       - |  276 | ` * php keeps class CONSTANTS and PROPERTIES in separate namespaces: a class may` |
|       - |  277 | `` * declare `const C` and `public $C` together, and `$obj->C` never resolves to the`` |
|       - |  278 | ` * constant. PHL stores each in its own table (constants in hConst, properties in` |
|       - |  279 | ` * hAttr), so these two lookups target the right namespace and never collide.` |
|       - |  280 | ` */` |
|     484 |  281 | `static ph7_class_attr * GenStateExtractConstant(ph7_class *pClass,SyString *pName)` |
|       5 |  282 | `{` |
|     489 |  283 | `	return PH7_ClassExtractConstant(pClass,pName->zString,pName->nByte);` |
|       5 |  284 | `}` |
|    2278 |  285 | `static ph7_class_attr * GenStateExtractProperty(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  286 | `{` |
|    2283 |  287 | `	return PH7_ClassExtractAttribute(pClass,zName,nByte);` |
|       5 |  288 | `}` |
|       - |  289 | `/*` |
|       - |  290 | ` * Return TRUE if the constant expression starting at the current token performs a` |
|       - |  291 | ` * FUNCTION CALL, which php rejects with "Constant expression contains invalid` |
|       - |  292 | `` * operations" in every constant-expression context (global `const`, class/interface`` |
|       - |  293 | ` * constants, property defaults, parameter defaults, attribute arguments).` |
|       - |  294 | ` *` |
|       - |  295 | ` * Shares GenStateInitHasNewExpr's walk: depth-aware so a nested call is caught` |
|       - |  296 | `` * (`[1, f()]`) and an inner comma does not end the scan, and skipping any`` |
|       - |  297 | `` * `function`/`fn` construct outright — a call inside a closure body runs when the`` |
|       - |  298 | ` * closure is invoked, so php allows it.` |
|       - |  299 | ` *` |
|       - |  300 | ` * Deliberately NOT rejected, because php accepts them:` |
|       - |  301 | `` *   - first-class callables, `strlen(...)` — the parens hold only the ellipsis;`` |
|       - |  302 | ``  *   - `new X(...)` — constructor calls are legal in the contexts that allow `new` `` |
|       - |  303 | ` *     at all, and GenStateInitHasNewExpr owns the contexts that do not;` |
|       - |  304 | ` *   - anything inside a ternary. php FOLDS a constant condition and only rejects a` |
|       - |  305 | `` *     call that survives, so `true ? 1 : f()` is legal while `false ? 1 : f()` is`` |
|       - |  306 | ` *     not. PHL does not constant-fold here, so rather than risk rejecting valid` |
|       - |  307 | `` *     code this scan skips an initializer containing a depth-0 `?` entirely. The`` |
|       - |  308 | ` *     residual is a call hiding in a TAKEN ternary branch, which stays accepted.` |
|       - |  309 | ` */` |
|    2126 |  310 | `PH7_PRIVATE int PH7_GenStateInitHasCallExpr(ph7_gen_state *pGen)` |
|       5 |  311 | `{` |
|    2131 |  312 | `	SyToken *p = pGen->pIn;` |
|    2131 |  313 | `	int iDepth = 0;` |
|       - |  314 | `	/* Conservative ternary bail-out (see the note above). */` |
|       - |  315 | `	{` |
|    2131 |  316 | `		SyToken *q = pGen->pIn;` |
|    2131 |  317 | `		int iQd = 0;` |
|   20251 |  318 | `		while( q < pGen->pEnd ){` |
|   20205 |  319 | `			if( iQd == 0 && (q->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    2079 |  320 | `				break;` |
|       - |  321 | `			}` |
|   18131 |  322 | `			if( q->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     423 |  323 | `				iQd++;` |
|   17922 |  324 | `			}else if( q->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     423 |  325 | `				if( iQd > 0 ){ iQd--; }` |
|   17504 |  326 | `			}else if( (q->nType & PH7_TK_OP) && q->pUserData` |
|    7921 |  327 | `				&& ((const ph7_expr_op *)q->pUserData)->iOp == EXPR_OP_QUESTY ){` |
|       8 |  328 | `				return 0;` |
|       - |  329 | `			}` |
|   18125 |  330 | `			q++;` |
|       5 |  331 | `		}` |
|       - |  332 | `	}` |
|   18685 |  333 | `	while( p < pGen->pEnd ){` |
|   18685 |  334 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    2069 |  335 | `			break; /* end of this initializer */` |
|       - |  336 | `		}` |
|   16616 |  337 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    8359 |  338 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      80 |  339 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|       - |  340 | `			/* A call inside a closure/arrow-fn is deferred to call time: skip the` |
|       - |  341 | `			 * whole construct. Delegating to the sibling scanner is not possible` |
|       - |  342 | ``			 * (it reports `new`), so mirror its bracket walk. */`` |
|      28 |  343 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|      28 |  344 | `			int iBase = iDepth;` |
|      28 |  345 | `			p++;` |
|      28 |  346 | `			if( bArrow ){` |
|     ! 0 |  347 | `				while( p < pGen->pEnd ){` |
|     ! 0 |  348 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  349 | `						iDepth++;` |
|     ! 0 |  350 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  351 | `						if( iDepth <= iBase ){` |
|     ! 0 |  352 | `							break;` |
|       - |  353 | `						}` |
|     ! 0 |  354 | `						iDepth--;` |
|     ! 0 |  355 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|     ! 0 |  356 | `						break;` |
|       - |  357 | `					}` |
|     ! 0 |  358 | `					p++;` |
|     ! 0 |  359 | `				}` |
|     ! 0 |  360 | `			}else{` |
|      28 |  361 | `				int iLocal = 0;` |
|      76 |  362 | `				while( p < pGen->pEnd ){` |
|      76 |  363 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|      28 |  364 | `						break;` |
|       - |  365 | `					}` |
|      52 |  366 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      28 |  367 | `						iLocal++;` |
|      40 |  368 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      28 |  369 | `						if( iLocal > 0 ){ iLocal--; }` |
|      12 |  370 | `					}` |
|      52 |  371 | `					p++;` |
|       4 |  372 | `				}` |
|      28 |  373 | `				if( p < pGen->pEnd ){` |
|      28 |  374 | `					int iBrace = 0;` |
|     196 |  375 | `					while( p < pGen->pEnd ){` |
|     196 |  376 | `						if( p->nType & PH7_TK_OCB ){` |
|      32 |  377 | `							iBrace++;` |
|     182 |  378 | `						}else if( p->nType & PH7_TK_CCB ){` |
|      32 |  379 | `							iBrace--;` |
|      32 |  380 | `							if( iBrace == 0 ){` |
|      28 |  381 | `								p++;` |
|      28 |  382 | `								break;` |
|       - |  383 | `							}` |
|       2 |  384 | `						}` |
|     172 |  385 | `						p++;` |
|       4 |  386 | `					}` |
|      12 |  387 | `				}` |
|       - |  388 | `			}` |
|      28 |  389 | `			continue;` |
|       - |  390 | `		}` |
|   16597 |  391 | `		if( p->nType & PH7_TK_OCB ){` |
|      55 |  392 | `			if( iDepth == 0 ){` |
|      55 |  393 | `				break; /* property-hook list: the default expression ends here */` |
|       - |  394 | `			}` |
|     ! 0 |  395 | `			iDepth++;` |
|   16545 |  396 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|       - |  397 | `			/* A '(' directly after a NAME is a call. A name here is an identifier` |
|       - |  398 | ``			 * token; `new X(` is excluded by looking for the `new` operator, and`` |
|       - |  399 | ``			 * `f(...)` (first-class callable) by peeking for a lone ellipsis. */`` |
|     226 |  400 | `			if( (p->nType & PH7_TK_LPAREN) && p > pGen->pIn` |
|      42 |  401 | `				&& (p[-1].nType & PH7_TK_ID)` |
|      39 |  402 | `				&& !((p[-1].nType & PH7_TK_OP) && p[-1].pUserData` |
|     ! 0 |  403 | `					&& ((const ph7_expr_op *)p[-1].pUserData)->iOp == EXPR_OP_NEW) ){` |
|      31 |  404 | `				int bNewCtor = 0;` |
|      31 |  405 | `				SyToken *q = &p[-1];` |
|       - |  406 | ``				/* Walk back over a qualified name (A\B, A::b, $o->m) to a `new`. */`` |
|      31 |  407 | `				while( q > pGen->pIn && (q[-1].nType & (PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) ){` |
|      20 |  408 | `					if( (q[-1].nType & PH7_TK_OP) && q[-1].pUserData` |
|      25 |  409 | `						&& ((const ph7_expr_op *)q[-1].pUserData)->iOp == EXPR_OP_NEW ){` |
|      25 |  410 | `						bNewCtor = 1;` |
|      25 |  411 | `						break;` |
|       - |  412 | `					}` |
|     ! 0 |  413 | `					if( !GenStateTokenIsMemberOp(&q[-1]) && (q[-1].nType & PH7_TK_NSSEP) == 0 ){` |
|     ! 0 |  414 | `						break;` |
|       - |  415 | `					}` |
|     ! 0 |  416 | `					q--;` |
|     ! 0 |  417 | `				}` |
|      26 |  418 | `				if( !bNewCtor` |
|      21 |  419 | `					&& !(&p[1] < pGen->pEnd && (p[1].nType & PH7_TK_ELLIPSIS)) ){` |
|       5 |  420 | `					return 1;` |
|       - |  421 | `				}` |
|      11 |  422 | `			}` |
|     227 |  423 | `			iDepth++;` |
|   16430 |  424 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     227 |  425 | `			if( iDepth > 0 ){` |
|     227 |  426 | `				iDepth--;` |
|     111 |  427 | `			}` |
|     111 |  428 | `		}` |
|   16541 |  429 | `		p++;` |
|       5 |  430 | `	}` |
|    2121 |  431 | `	return 0;` |
|    1068 |  432 | `}` |
|       - |  433 | `/*` |
|       - |  434 | ` * Scan a constant-expression initializer for a closure / arrow-function literal` |
|       - |  435 | ` * and report php's two compile-time rules the call scanner above does NOT: a` |
|       - |  436 | `` * NON-STATIC closure is `Fatal error: Closures in constant expressions must be`` |
|       - |  437 | `` * static`, and ANY arrow function is `Constant expression contains invalid`` |
|       - |  438 | `` * operations` (there is no static-`fn` escape -- `static fn()=>1` is rejected`` |
|       - |  439 | `` * too). Only `static function(){...}` is accepted; its body is regular runtime`` |
|       - |  440 | ` * code, so a closure NESTED inside it is skipped, not rejected.` |
|       - |  441 | ` *` |
|       - |  442 | ` * Returns 0 (clean), 1 (arrow fn -> "invalid operations") or 2 (non-static` |
|       - |  443 | ` * closure -> "must be static"). Mirrors PH7_GenStateInitHasCallExpr's construct` |
|       - |  444 | ` * skip and initializer-terminator tracking, but WITHOUT its conservative ternary` |
|       - |  445 | ` * bail-out: php rejects the closure even inside a branch it would fold away` |
|       - |  446 | `` * (`true ? function(){} : 1` still fatals), because nothing is constant-folded at`` |
|       - |  447 | `` * this stage. Wired beside every call-scanner site (global `const`, class /`` |
|       - |  448 | ` * interface constants, property defaults).` |
|       - |  449 | ` */` |
|    2132 |  450 | `PH7_PRIVATE int PH7_GenStateInitClosureError(ph7_gen_state *pGen)` |
|       5 |  451 | `{` |
|    2137 |  452 | `	SyToken *p = pGen->pIn;` |
|    2137 |  453 | `	int iDepth = 0;` |
|   18735 |  454 | `	while( p < pGen->pEnd ){` |
|   18735 |  455 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    2077 |  456 | `			break; /* end of this initializer */` |
|       - |  457 | `		}` |
|   16658 |  458 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    8384 |  459 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      86 |  460 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|      35 |  461 | `			if( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ){` |
|       - |  462 | `				/* An arrow function is never a valid constant expression. */` |
|       3 |  463 | `				return 1;` |
|       - |  464 | `			}` |
|       - |  465 | ``			/* A closure literal must be `static function`: the modifier sits in the`` |
|       - |  466 | ``			 * token immediately before `function`. `static::X` never reaches here`` |
|       - |  467 | ``			 * (its next token is `::`, not the keyword). */`` |
|      33 |  468 | `			if( !(p > pGen->pIn && (p[-1].nType & PH7_TK_KEYWORD)` |
|      24 |  469 | `				&& SX_PTR_TO_INT(p[-1].pUserData) == PH7_TKWRD_STATIC) ){` |
|       6 |  470 | `				return 2;` |
|       - |  471 | `			}` |
|       - |  472 | ``			/* `static function(){...}`: accepted -- skip the whole construct`` |
|       - |  473 | `			 * (parameter parens then the brace-balanced body) exactly like the call` |
|       - |  474 | `			 * scanner, then keep looking for a sibling closure in the initializer. */` |
|      28 |  475 | `			p++;` |
|       - |  476 | `			{` |
|      28 |  477 | `				int iLocal = 0;` |
|      76 |  478 | `				while( p < pGen->pEnd ){` |
|      76 |  479 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|      28 |  480 | `						break;` |
|       - |  481 | `					}` |
|      52 |  482 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      28 |  483 | `						iLocal++;` |
|      40 |  484 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      28 |  485 | `						if( iLocal > 0 ){ iLocal--; }` |
|      12 |  486 | `					}` |
|      52 |  487 | `					p++;` |
|       4 |  488 | `				}` |
|      28 |  489 | `				if( p < pGen->pEnd ){` |
|      28 |  490 | `					int iBrace = 0;` |
|     196 |  491 | `					while( p < pGen->pEnd ){` |
|     196 |  492 | `						if( p->nType & PH7_TK_OCB ){` |
|      32 |  493 | `							iBrace++;` |
|     182 |  494 | `						}else if( p->nType & PH7_TK_CCB ){` |
|      32 |  495 | `							iBrace--;` |
|      32 |  496 | `							if( iBrace == 0 ){` |
|      28 |  497 | `								p++;` |
|      28 |  498 | `								break;` |
|       - |  499 | `							}` |
|       2 |  500 | `						}` |
|     172 |  501 | `						p++;` |
|       4 |  502 | `					}` |
|      12 |  503 | `				}` |
|       - |  504 | `			}` |
|      28 |  505 | `			continue;` |
|       - |  506 | `		}` |
|   16633 |  507 | `		if( p->nType & PH7_TK_OCB ){` |
|      57 |  508 | `			if( iDepth == 0 ){` |
|      57 |  509 | `				break; /* property-hook list: the default expression ends here */` |
|       - |  510 | `			}` |
|     ! 0 |  511 | `			iDepth++;` |
|   16579 |  512 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     231 |  513 | `			iDepth++;` |
|   16466 |  514 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     231 |  515 | `			if( iDepth > 0 ){` |
|     231 |  516 | `				iDepth--;` |
|     113 |  517 | `			}` |
|     113 |  518 | `		}` |
|   16579 |  519 | `		p++;` |
|       5 |  520 | `	}` |
|    2131 |  521 | `	return 0;` |
|    1071 |  522 | `}` |
|       - |  523 | `/*` |
|       - |  524 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|       - |  525 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|       - |  526 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|       - |  527 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|       - |  528 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|       - |  529 | ` * share the same backing.` |
|       - |  530 | ` */` |
|     726 |  531 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|       - |  532 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|       5 |  533 | `{` |
|     731 |  534 | `	pAttr->nType = nType;` |
|     731 |  535 | `	pAttr->sClass = *pClass;` |
|     731 |  536 | `	pAttr->sTypeName = *pTypeName;` |
|     731 |  537 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  538 | `		sxu32 i;` |
|     139 |  539 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      95 |  540 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|      95 |  541 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      50 |  542 | `		}` |
|      22 |  543 | `	}` |
|     731 |  544 | `}` |
|     492 |  545 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  546 | `{` |
|     497 |  547 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  548 | `	SySet *pInstrContainer;` |
|       - |  549 | `	ph7_class_attr *pCons;` |
|       - |  550 | `	SyString *pName;` |
|       - |  551 | `	sxi32 rc;` |
|     497 |  552 | `	sxu32 nType = 0;` |
|       - |  553 | `	SyString sTypeClass;` |
|       - |  554 | `	SyString sTypeText;` |
|       - |  555 | `	SySet aUnionAlts;` |
|     497 |  556 | `	sxi32 iTypeFlags = 0;` |
|     497 |  557 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|     497 |  558 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|     497 |  559 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  560 | `	/* Extract visibility level */` |
|     497 |  561 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  562 | `	/* Mark as constant */` |
|     497 |  563 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|     497 |  564 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |  565 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|       - |  566 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|     522 |  567 | `	if( GenStateClassConstHasType(pGen) ){` |
|      80 |  568 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|      50 |  569 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|       - |  570 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|       - |  571 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|       - |  572 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|       - |  573 | `		 * and success paths release. */` |
|      55 |  574 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  575 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  576 | `			goto Synchronize;` |
|      55 |  577 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  578 | `			return SXERR_ABORT;` |
|      55 |  579 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  580 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  581 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|     ! 0 |  582 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  583 | `				return SXERR_ABORT;` |
|       - |  584 | `			}` |
|     ! 0 |  585 | `			goto Synchronize;` |
|       - |  586 | `		}` |
|      55 |  587 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      25 |  588 | `	}` |
|     246 |  589 | `loop:` |
|       - |  590 | ``	/* php 8 accepts EVERY reserved word as a class-constant name — `const list = 5`,`` |
|       - |  591 | ``	 * `const match`, `const function`, even `const true` — because a class constant is`` |
|       - |  592 | ``	 * addressed only through `C::name`, where no keyword can be ambiguous. The single`` |
|       - |  593 | ``	 * exception is `class`, reserved for `C::class`, and it gets its own message.`` |
|       - |  594 | `	 * (Method names already accept the whole set; this is the member-name side of the` |
|       - |  595 | ``	 * same rule. Global `const` is NOT the same rule: php rejects a reserved word there.)`` |
|       - |  596 | `	 * A keyword arrives as PH7_TK_KEYWORD, which this ID-only test rejected — so PHL` |
|       - |  597 | ``	 * accepted only the alpha-OPERATOR keywords (`const new`, `const and`), which the`` |
|       - |  598 | `	 * lexer marks PH7_TK_ID\|PH7_TK_OP, and that partial allow-list looked like a design. */` |
|     503 |  599 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  600 | `		/* Invalid constant name */` |
|     ! 0 |  601 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  602 | `		if( rc == SXERR_ABORT ){` |
|       - |  603 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  604 | `			return SXERR_ABORT;` |
|       - |  605 | `		}` |
|     ! 0 |  606 | `		goto Synchronize;` |
|       - |  607 | `	}` |
|       - |  608 | `	/* Peek constant name */` |
|     503 |  609 | `	pName = &pGen->pIn->sData;` |
|     498 |  610 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     289 |  611 | `		&& (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 |  612 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  613 | `			"A class constant must not be called 'class'; it is reserved for class name fetching");` |
|       3 |  614 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  615 | `			return SXERR_ABORT;` |
|       - |  616 | `		}` |
|       3 |  617 | `		goto Synchronize;` |
|       - |  618 | `	}` |
|       - |  619 | `	/* No reserved-CONSTANT check here: true/false/null are reserved GLOBAL constant` |
|       - |  620 | ``	 * names (compile_stmt.c still rejects `const true = 1`), but `C::true` addresses a`` |
|       - |  621 | `	 * class constant and php accepts the declaration like any other reserved word. The` |
|       - |  622 | `	 * member-name flag keeps the read from folding into the boolean literal. */` |
|       - |  623 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|     501 |  624 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      80 |  625 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|      50 |  626 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      25 |  627 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|      55 |  628 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  629 | `			return SXERR_ABORT;` |
|      55 |  630 | `		}else if( rc != SXRET_OK ){` |
|       3 |  631 | `			goto Synchronize;` |
|       - |  632 | `		}` |
|      24 |  633 | `	}` |
|       - |  634 | `	/* Advance the stream cursor */` |
|     499 |  635 | `	pGen->pIn++;` |
|     499 |  636 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  637 | `		/* Invalid declaration */` |
|     ! 0 |  638 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  639 | `		if( rc == SXERR_ABORT ){` |
|       - |  640 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  641 | `			return SXERR_ABORT;` |
|       - |  642 | `		}` |
|     ! 0 |  643 | `		goto Synchronize;` |
|       - |  644 | `	}` |
|     499 |  645 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  646 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|       - |  647 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|       - |  648 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|       - |  649 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|     494 |  650 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|      51 |  651 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|       8 |  652 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  653 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|       2 |  654 | `			&pClass->sName,pName,&sTypeText);` |
|       6 |  655 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  656 | `			return SXERR_ABORT;` |
|       - |  657 | `		}` |
|       6 |  658 | `		goto Synchronize;` |
|       - |  659 | `	}` |
|       - |  660 | ``	/* php: a closure in a class/interface constant must be `static function`;`` |
|       - |  661 | ``	 * same rule (and messages) as the global `const` path in compile_stmt.c. */`` |
|       - |  662 | `	{` |
|     495 |  663 | `		int iClo = PH7_GenStateInitClosureError(pGen);` |
|     495 |  664 | `		if( iClo ){` |
|       4 |  665 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  666 | `				iClo == 2 ? "Closures in constant expressions must be static"` |
|       - |  667 | `				          : "Constant expression contains invalid operations");` |
|       3 |  668 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  669 | `				return SXERR_ABORT;` |
|       - |  670 | `			}` |
|       3 |  671 | `			goto Synchronize;` |
|       - |  672 | `		}` |
|       - |  673 | `	}` |
|       - |  674 | `	/* php: a constant expression may not CALL anything. Same rule as the global` |
|       - |  675 | ``	 * `const` path in compile_stmt.c. */`` |
|     493 |  676 | `	if( PH7_GenStateInitHasCallExpr(pGen) ){` |
|     ! 0 |  677 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  678 | `			"Constant expression contains invalid operations");` |
|     ! 0 |  679 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  680 | `			return SXERR_ABORT;` |
|       - |  681 | `		}` |
|     ! 0 |  682 | `		goto Synchronize;` |
|       - |  683 | `	}` |
|       - |  684 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|       - |  685 | `	 * constant initializer ("New expressions are not supported in this context").` |
|       - |  686 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|     493 |  687 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|       6 |  688 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  689 | `			"New expressions are not supported in this context");` |
|       6 |  690 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  691 | `			return SXERR_ABORT;` |
|       - |  692 | `		}` |
|       6 |  693 | `		goto Synchronize;` |
|       - |  694 | `	}` |
|       - |  695 | `	/* php: a class constant may not be redefined in the same class body. The` |
|       - |  696 | `	 * property path already guarded this; the constant path did not, so` |
|       - |  697 | ``	 * `class C{const X=1; const X=2;}` silently kept one of them. */`` |
|       - |  698 | ``	/* php keeps constants and properties in SEPARATE namespaces, so `const C` and`` |
|       - |  699 | ``	 * `public $C` coexist. PHL now stores them in disjoint tables (hConst / hAttr),`` |
|       - |  700 | `	 * so no collision — only a genuine constant redefinition is rejected below. */` |
|     489 |  701 | `	if( GenStateExtractConstant(pClass,pName) != 0 ){` |
|     ! 0 |  702 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  703 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|     ! 0 |  704 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  705 | `			return SXERR_ABORT;` |
|       - |  706 | `		}` |
|     ! 0 |  707 | `		goto Synchronize;` |
|       - |  708 | `	}` |
|       - |  709 | `	/* Allocate a new class attribute */` |
|     489 |  710 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|     489 |  711 | `	if( pCons ){` |
|     489 |  712 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|     489 |  713 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|     ! 0 |  714 | `			return SXERR_ABORT;` |
|       - |  715 | `		}` |
|     489 |  716 | `		if( GenStateCheckAttrPlacement(&(*pGen),&pCons->aAttrs,16,16,0,0) == SXERR_ABORT ){` |
|     ! 0 |  717 | `			return SXERR_ABORT;` |
|       - |  718 | `		}` |
|     242 |  719 | `	}` |
|     489 |  720 | `	if( pCons == 0 ){` |
|     ! 0 |  721 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  722 | `		return SXERR_ABORT;` |
|       - |  723 | `	}` |
|     489 |  724 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      48 |  725 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      22 |  726 | `	}` |
|       - |  727 | `	/* Swap bytecode container */` |
|     489 |  728 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     489 |  729 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  730 | `	/* Compile constant value.` |
|       - |  731 | `	 */` |
|     489 |  732 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     489 |  733 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  734 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  735 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  736 | `			return SXERR_ABORT;` |
|       - |  737 | `		}` |
|       1 |  738 | `	}` |
|       - |  739 | `	/* Emit the done instruction */` |
|     489 |  740 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|     489 |  741 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     489 |  742 | `	if( rc == SXERR_ABORT ){` |
|       - |  743 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  744 | `		return SXERR_ABORT;` |
|       - |  745 | `	}` |
|       - |  746 | `	/* All done,install the constant */` |
|     489 |  747 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|     489 |  748 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  749 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  750 | `		return SXERR_ABORT;` |
|       - |  751 | `	}` |
|     489 |  752 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  753 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|       7 |  754 | `		pGen->pIn++; /* Jump the comma */` |
|       - |  755 | `		/* A reserved word is a valid name for EVERY constant in the declaration, not` |
|       - |  756 | ``		 * just the first (`const list = 1, match = 2`) — same allow-list as the head. */`` |
|       7 |  757 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  758 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  759 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  760 | `				pTok--;` |
|     ! 0 |  761 | `			}` |
|     ! 0 |  762 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  763 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  764 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  765 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  766 | `				return SXERR_ABORT;` |
|       - |  767 | `			}` |
|     ! 0 |  768 | `		}else{` |
|       7 |  769 | `			goto loop;` |
|       - |  770 | `		}` |
|     ! 0 |  771 | `	}` |
|     483 |  772 | `	SySetRelease(&aUnionAlts);` |
|     483 |  773 | `	return SXRET_OK;` |
|       7 |  774 | `Synchronize:` |
|      17 |  775 | `	SySetRelease(&aUnionAlts);` |
|       - |  776 | `	/* Synchronize with the first semi-colon */` |
|      67 |  777 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      53 |  778 | `		pGen->pIn++;` |
|       3 |  779 | `	}` |
|      17 |  780 | `	return SXERR_CORRUPT;` |
|     251 |  781 | `}` |
|       - |  782 | `/*` |
|       - |  783 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  784 | ` * According to the PHP language reference manual` |
|       - |  785 | ` *  Properties` |
|       - |  786 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  787 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  788 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  789 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  790 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  791 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  792 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  793 | ` * Symisc eXtension.` |
|       - |  794 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  795 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  796 | ` *  Example:` |
|       - |  797 | ` *   class Test{` |
|       - |  798 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  799 | ` *   };` |
|       - |  800 | ` *   var_dump(TEST::myVar);` |
|       - |  801 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  802 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  803 | ` */` |
|       - |  804 | `/*` |
|       - |  805 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  806 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  807 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  808 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  809 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  810 | ` */` |
|    4672 |  811 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  812 | `{` |
|    4677 |  813 | `	SyToken *p = pStart;` |
|    4677 |  814 | `	int bFirst = 1;` |
|    4677 |  815 | `	if( p >= pEnd ) return 0;` |
|       - |  816 | ``	/* Optional nullable `?` shorthand. */`` |
|    4677 |  817 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      68 |  818 | `		p++;` |
|      68 |  819 | `		if( p >= pEnd ) return 0;` |
|      32 |  820 | `	}` |
|       - |  821 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|       - |  822 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|       - |  823 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|       - |  824 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|    2336 |  825 | `	for(;;){` |
|    4717 |  826 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|       - |  827 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|       3 |  828 | `			p++;` |
|       9 |  829 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|       3 |  830 | `			if( p >= pEnd ) return 0;` |
|       3 |  831 | `			p++; /* skip ')' */` |
|       2 |  832 | `		}else{` |
|       - |  833 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|       - |  834 | ``			 * then any `&`-joined intersection members. */`` |
|    4715 |  835 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|    4715 |  836 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  837 | `				return 0;` |
|       - |  838 | `			}` |
|       - |  839 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|       - |  840 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|       - |  841 | `			 * may still appear at the initial dispatch site). */` |
|    4715 |  842 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|    4619 |  843 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|    4614 |  844 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    1227 |  845 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|    4003 |  846 | `					return 0;` |
|       - |  847 | `				}` |
|     308 |  848 | `			}` |
|     717 |  849 | `			p++;` |
|     719 |  850 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  851 | `				p += 2;` |
|       1 |  852 | `			}` |
|    1074 |  853 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|     723 |  854 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       6 |  855 | `				p++; /* skip '&' */` |
|       6 |  856 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|       6 |  857 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|       6 |  858 | `				p++;` |
|       6 |  859 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  860 | `					p += 2;` |
|     ! 0 |  861 | `				}` |
|       2 |  862 | `			}` |
|       - |  863 | `		}` |
|     719 |  864 | `		bFirst = 0;` |
|     714 |  865 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      45 |  866 | `			&& p->sData.zString[0] == '\|' ){` |
|      45 |  867 | ``			p++; /* next `\|`-separated part */`` |
|      45 |  868 | `			continue;` |
|       - |  869 | `		}` |
|     679 |  870 | `		break;` |
|     ! 0 |  871 | `	}` |
|     679 |  872 | `	if( p >= pEnd ) return 0;` |
|     679 |  873 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|    2341 |  874 | `}` |
|       - |  875 |  |
|       - |  876 | `/*` |
|       - |  877 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  878 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  879 | ` * if not). Recognized forms:` |
|       - |  880 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  881 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  882 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  883 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  884 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  885 | ` * on unrecoverable error.` |
|       - |  886 | ` *` |
|       - |  887 | ` * When a type is parsed:` |
|       - |  888 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  889 | ` *   *pClass is set to the class name (for class types)` |
|       - |  890 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  891 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  892 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  893 | ` */` |
|     686 |  894 | `static sxi32 GenStateParsePropertyType(` |
|       - |  895 | `	ph7_gen_state *pGen,` |
|       - |  896 | `	sxu32 *pnType,` |
|       - |  897 | `	SyString *pClass,` |
|       - |  898 | `	sxi32 *piTypeFlags,` |
|       - |  899 | `	SyString *pTypeText,` |
|       - |  900 | `	SySet *pAlts` |
|       5 |  901 | `){` |
|     691 |  902 | `	sxi32 iFlags = 0;` |
|       - |  903 | `	sxi32 rc;` |
|     691 |  904 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  905 | `		return SXRET_OK;` |
|       - |  906 | `	}` |
|       - |  907 | `	/* If the first token is '$', there's no type */` |
|     691 |  908 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  909 | `		return SXRET_OK;` |
|       - |  910 | `	}` |
|     691 |  911 | `	rc = GenStateParseUnionTypeDecl(` |
|     343 |  912 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  913 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  914 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  915 | `		/* bAllowVoid */ 0,` |
|     686 |  916 | `		pGen->pIn->nLine);` |
|     691 |  917 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  918 | `		return rc;` |
|       - |  919 | `	}` |
|       - |  920 | `	/* Verify next token is '$' (start of property name) */` |
|     691 |  921 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  922 | `		return SXERR_SYNTAX;` |
|       - |  923 | `	}` |
|     691 |  924 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     691 |  925 | `	return SXRET_OK;` |
|     348 |  926 | `}` |
|       - |  927 |  |
|       - |  928 | `/*` |
|       - |  929 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  930 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  931 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  932 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  933 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  934 | ` * by the type parser itself before reaching here.` |
|       - |  935 | ` *` |
|       - |  936 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  937 | ` * use in the error message.` |
|       - |  938 | ` */` |
|     950 |  939 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  940 | `	sxu32 nType,` |
|       - |  941 | `	const SyString *pClass,` |
|       - |  942 | `	const char **pzName,` |
|       - |  943 | `	sxu32 *pnName)` |
|       5 |  944 | `{` |
|       - |  945 | `	const char *z;` |
|       - |  946 | `	sxu32 n;` |
|     955 |  947 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     845 |  948 | `		return 0;` |
|       - |  949 | `	}` |
|     115 |  950 | `	z = pClass->zString;` |
|     115 |  951 | `	n = pClass->nByte;` |
|     115 |  952 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       9 |  953 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  954 | `	}` |
|       - |  955 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - |  956 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - |  957 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|     109 |  958 | `	return 0;` |
|     480 |  959 | `}` |
|       - |  960 |  |
|       - |  961 | `/*` |
|       - |  962 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|       - |  963 | ` * constant) — the main atom plus any union alternatives — against the` |
|       - |  964 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|       - |  965 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|       - |  966 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|       - |  967 | ` * type T" vs "Class constant C::X cannot have type T").` |
|       - |  968 | ` *` |
|       - |  969 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  970 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  971 | ` */` |
|     844 |  972 | `PH7_PRIVATE sxi32 GenStateValidateMemberType(` |
|       - |  973 | `	ph7_gen_state *pGen,` |
|       - |  974 | `	ph7_class *pClass,` |
|       - |  975 | `	const SyString *pMemberName,` |
|       - |  976 | `	sxu32 nType,` |
|       - |  977 | `	const SyString *pTypeClass,` |
|       - |  978 | `	const SyString *pTypeText,` |
|       - |  979 | `	SySet *pUnionAlts,` |
|       - |  980 | `	const char *zErrFmt,` |
|       - |  981 | `	sxu32 nLine)` |
|       5 |  982 | `{` |
|     849 |  983 | `	const char *zBad = 0;` |
|     849 |  984 | `	sxu32 nBad = 0;` |
|       - |  985 | `	SyString sFallback;` |
|       - |  986 | `	const SyString *pBad;` |
|       - |  987 | `	sxi32 rc;` |
|     849 |  988 | `	int bDisallowed = 0;` |
|     849 |  989 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       6 |  990 | `		bDisallowed = 1;` |
|     847 |  991 | `	}else if( pUnionAlts ){` |
|       - |  992 | `		sxu32 i;` |
|     161 |  993 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|     111 |  994 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|     111 |  995 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  996 | `				bDisallowed = 1;` |
|       3 |  997 | `				break;` |
|       - |  998 | `			}` |
|      57 |  999 | `		}` |
|      26 | 1000 | `	}` |
|     849 | 1001 | `	if( !bDisallowed ){` |
|     843 | 1002 | `		return SXRET_OK;` |
|       - | 1003 | `	}` |
|       - | 1004 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - | 1005 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - | 1006 | `	 * canonical spelling if the type text is unavailable. */` |
|       9 | 1007 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       9 | 1008 | `		pBad = pTypeText;` |
|       6 | 1009 | `	}else{` |
|     ! 0 | 1010 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 | 1011 | `		pBad = &sFallback;` |
|       - | 1012 | `	}` |
|      12 | 1013 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 | 1014 | `		zErrFmt,` |
|       3 | 1015 | `		&pClass->sName,pMemberName,pBad);` |
|       9 | 1016 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1017 | `		return SXERR_ABORT;` |
|       - | 1018 | `	}` |
|       9 | 1019 | `	return SXERR_SYNTAX;` |
|     427 | 1020 | `}` |
|       - | 1021 | `/*` |
|       - | 1022 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|       - | 1023 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|       - | 1024 | ` * matched as a plain identifier in the class-member modifier position rather` |
|       - | 1025 | ` * than promoted to a lexer keyword.` |
|       - | 1026 | ` */` |
| 2361222 | 1027 | `PH7_PRIVATE int GenStateIsReadonly(SyToken *pTok)` |
|       5 | 1028 | `{` |
| 2412408 | 1029 | `	return (pTok->nType & PH7_TK_ID)` |
| 1231792 | 1030 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 2412403 | 1031 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|       5 | 1032 | `}` |
|       - | 1033 | `/*` |
|       - | 1034 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|       - | 1035 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|       - | 1036 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|       - | 1037 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|       - | 1038 | ` */` |
|  300322 | 1039 | `PH7_PRIVATE sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|       5 | 1040 | `{` |
|  300327 | 1041 | `	*pnTok = 0;` |
|  300322 | 1042 | `	if( &pTok[3] < pEnd` |
|  252077 | 1043 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|  124209 | 1044 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|   22311 | 1045 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      36 | 1046 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|      36 | 1047 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|      41 | 1048 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|      37 | 1049 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|      37 | 1050 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|      37 | 1051 | `			*pnTok = 4;` |
|      37 | 1052 | `			return nKw;` |
|       - | 1053 | `		}` |
|     ! 0 | 1054 | `	}` |
|  300291 | 1055 | `	return 0;` |
|  150166 | 1056 | `}` |
|       - | 1057 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|      36 | 1058 | `PH7_PRIVATE sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|       1 | 1059 | `{` |
|      37 | 1060 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|      25 | 1061 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|       - | 1062 | `	}` |
|      13 | 1063 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|      11 | 1064 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|       - | 1065 | `	}` |
|       3 | 1066 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|      19 | 1067 | `}` |
|    2160 | 1068 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 | 1069 | `{` |
|    2165 | 1070 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 1071 | `	ph7_class_attr *pAttr;` |
|       - | 1072 | `	SyString *pName;` |
|       - | 1073 | `	sxi32 rc;` |
|    2165 | 1074 | `	sxu32 nType = 0;` |
|       - | 1075 | `	SyString sTypeClass;` |
|       - | 1076 | `	SyString sTypeText;` |
|       - | 1077 | `	SySet aUnionAlts;` |
|    2165 | 1078 | `	sxi32 iTypeFlags = 0;` |
|    2165 | 1079 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    2165 | 1080 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    2165 | 1081 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - | 1082 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|       - | 1083 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|       - | 1084 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|    2165 | 1085 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      23 | 1086 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|      10 | 1087 | `	}` |
|       - | 1088 | `	/* Extract visibility level */` |
|    2165 | 1089 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - | 1090 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|    2508 | 1091 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     691 | 1092 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     691 | 1093 | `		if( rc == SXERR_CORRUPT ){` |
|       - | 1094 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 | 1095 | `			goto Synchronize;` |
|     691 | 1096 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 | 1097 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1098 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 | 1099 | `				&pGen->pIn->sData);` |
|     ! 0 | 1100 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1101 | `				return SXERR_ABORT;` |
|       - | 1102 | `			}` |
|     ! 0 | 1103 | `			goto Synchronize;` |
|     691 | 1104 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 | 1105 | `			return SXERR_ABORT;` |
|       - | 1106 | `		}` |
|     343 | 1107 | `	}` |
|     ! 0 | 1108 | `loop:` |
|    2173 | 1109 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 1110 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 | 1111 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1112 | `			return SXERR_ABORT;` |
|       - | 1113 | `		}` |
|     ! 0 | 1114 | `		goto Synchronize;` |
|       - | 1115 | `	}` |
|    2173 | 1116 | `	pGen->pIn++; /* Jump the dollar sign */` |
|    2173 | 1117 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - | 1118 | `		/* Invalid attribute name */` |
|     ! 0 | 1119 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 | 1120 | `		if( rc == SXERR_ABORT ){` |
|       - | 1121 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1122 | `			return SXERR_ABORT;` |
|       - | 1123 | `		}` |
|     ! 0 | 1124 | `		goto Synchronize;` |
|       - | 1125 | `	}` |
|       - | 1126 | `	/* Peek attribute name */` |
|    2173 | 1127 | `	pName = &pGen->pIn->sData;` |
|       - | 1128 | `	/* Advance the stream cursor */` |
|    2173 | 1129 | `	pGen->pIn++;` |
|    2173 | 1130 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|       - | 1131 | `		/* Invalid declaration */` |
|       - | 1132 | `		/* php reports the offending token here, expecting "," or ";". */` |
|       3 | 1133 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\",\" or \";\"");` |
|       3 | 1134 | `		if( rc == SXERR_ABORT ){` |
|       - | 1135 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1136 | `			return SXERR_ABORT;` |
|       - | 1137 | `		}` |
|       3 | 1138 | `		goto Synchronize;` |
|       - | 1139 | `	}` |
|       - | 1140 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|       - | 1141 | `	 * the read visibility must not be narrower than the set visibility. */` |
|    2171 | 1142 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|      33 | 1143 | `		const char *zAvErr = 0;` |
|      49 | 1144 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|      28 | 1145 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|       6 | 1146 | `			: PH7_CLASS_PROT_PUBLIC;` |
|      33 | 1147 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|     ! 0 | 1148 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|      33 | 1149 | `		}else if( iProtection > iSetLevel ){` |
|     ! 0 | 1150 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|     ! 0 | 1151 | `		}` |
|      33 | 1152 | `		if( zAvErr ){` |
|     ! 0 | 1153 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|     ! 0 | 1154 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1155 | `				return SXERR_ABORT;` |
|       - | 1156 | `			}` |
|     ! 0 | 1157 | `			goto Synchronize;` |
|       - | 1158 | `		}` |
|      16 | 1159 | `	}` |
|       - | 1160 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|       - | 1161 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|    2171 | 1162 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      85 | 1163 | `		const char *zRoErr = 0;` |
|      85 | 1164 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       3 | 1165 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|      84 | 1166 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       6 | 1167 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|      81 | 1168 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|       6 | 1169 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|       2 | 1170 | `		}` |
|      85 | 1171 | `		if( zRoErr ){` |
|      13 | 1172 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|      13 | 1173 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1174 | `				return SXERR_ABORT;` |
|       - | 1175 | `			}` |
|      13 | 1176 | `			goto Synchronize;` |
|       - | 1177 | `		}` |
|      35 | 1178 | `	}` |
|       - | 1179 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - | 1180 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - | 1181 | `	 * by the type parser. */` |
|    2161 | 1182 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|    1031 | 1183 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - | 1184 | `			&sTypeText,` |
|     684 | 1185 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|     342 | 1186 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     689 | 1187 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1188 | `			return SXERR_ABORT;` |
|     689 | 1189 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 | 1190 | `			goto Synchronize;` |
|       - | 1191 | `		}` |
|     342 | 1192 | `	}` |
|       - | 1193 | `	/* Reject redeclaration (catches clash with an earlier promoted property).` |
|       - | 1194 | `	 * A same-name class CONSTANT is NOT a clash — php's separate namespaces let` |
|       - | 1195 | ``	 * `const C` and `public $C` coexist (stored in disjoint hConst / hAttr tables). */`` |
|    2161 | 1196 | `	if( GenStateExtractProperty(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 | 1197 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 | 1198 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 | 1199 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1200 | `			return SXERR_ABORT;` |
|       - | 1201 | `		}` |
|       3 | 1202 | `		goto Synchronize;` |
|       - | 1203 | `	}` |
|       - | 1204 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|       - | 1205 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|       - | 1206 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|       - | 1207 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|       - | 1208 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|       - | 1209 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|       - | 1210 | ``	/* php: a property default holding a closure must use `static function` too. */`` |
|    2159 | 1211 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|    1475 | 1212 | `		int iClo = PH7_GenStateInitClosureError(pGen);` |
|    1475 | 1213 | `		if( iClo ){` |
|     ! 0 | 1214 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 1215 | `				iClo == 2 ? "Closures in constant expressions must be static"` |
|       - | 1216 | `				          : "Constant expression contains invalid operations");` |
|     ! 0 | 1217 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1218 | `				return SXERR_ABORT;` |
|       - | 1219 | `			}` |
|     ! 0 | 1220 | `			goto Synchronize;` |
|       - | 1221 | `		}` |
|     735 | 1222 | `	}` |
|       - | 1223 | `	/* php: a property default may not CALL anything either. */` |
|    2159 | 1224 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && PH7_GenStateInitHasCallExpr(pGen) ){` |
|     ! 0 | 1225 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1226 | `			"Constant expression contains invalid operations");` |
|     ! 0 | 1227 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1228 | `			return SXERR_ABORT;` |
|       - | 1229 | `		}` |
|     ! 0 | 1230 | `		goto Synchronize;` |
|       - | 1231 | `	}` |
|    2159 | 1232 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|       6 | 1233 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1234 | `			"New expressions are not supported in this context");` |
|       6 | 1235 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1236 | `			return SXERR_ABORT;` |
|       - | 1237 | `		}` |
|       6 | 1238 | `		goto Synchronize;` |
|       - | 1239 | `	}` |
|       - | 1240 | `	/* Allocate a new class attribute */` |
|    2155 | 1241 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    2155 | 1242 | `	if( pAttr ){` |
|    2155 | 1243 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|    2155 | 1244 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|     ! 0 | 1245 | `			return SXERR_ABORT;` |
|       - | 1246 | `		}` |
|    2155 | 1247 | `		if( GenStateCheckAttrPlacement(&(*pGen),&pAttr->aAttrs,8,8,0,0) == SXERR_ABORT ){` |
|     ! 0 | 1248 | `			return SXERR_ABORT;` |
|       - | 1249 | `		}` |
|    1075 | 1250 | `	}` |
|    2155 | 1251 | `	if( pAttr == 0 ){` |
|     ! 0 | 1252 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1253 | `		return SXERR_ABORT;` |
|       - | 1254 | `	}` |
|    2155 | 1255 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     687 | 1256 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|     341 | 1257 | `	}` |
|    2155 | 1258 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - | 1259 | `		SySet *pInstrContainer;` |
|    1471 | 1260 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|    1471 | 1261 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - | 1262 | `		{` |
|       - | 1263 | `			/* Delimit the default expression: it ends at the declaration's` |
|       - | 1264 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|       - | 1265 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|       - | 1266 | `			 * compiler would otherwise run into the hook tokens. */` |
|    1471 | 1267 | `			SyToken *pScan = pGen->pIn;` |
|    1471 | 1268 | `			sxi32 iNest = 0;` |
|    1471 | 1269 | ``			int bFuncSeen = 0; /* a `function` keyword stands at depth 0 */`` |
|   11559 | 1270 | `			while( pScan < pGen->pEnd ){` |
|   11554 | 1271 | `				if( (pScan->nType & PH7_TK_KEYWORD) && iNest <= 0` |
|      44 | 1272 | `					&& SX_PTR_TO_INT(pScan->pUserData) == PH7_TKWRD_FUNCTION ){` |
|       - | 1273 | `					/* The next depth-0 '{' is this CLOSURE's body, not a hook list:` |
|       - | 1274 | ``					 * `public $p = static function(){ … };` is php-legal (a static`` |
|       - | 1275 | `					 * closure is a constant expression) and its brace was read as` |
|       - | 1276 | `					 * the hook-list opener, so the default was truncated at` |
|       - | 1277 | ``					 * `static function()` and the declaration died three errors`` |
|       - | 1278 | `					 * deep. A class CONSTANT never showed it — only the property` |
|       - | 1279 | `					 * path carries a hook list at all. */` |
|       9 | 1280 | `					bFuncSeen = 1;` |
|   11555 | 1281 | `				}else if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     177 | 1282 | `					iNest++;` |
|   11465 | 1283 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|     177 | 1284 | `					iNest--;` |
|   11293 | 1285 | `				}else if( iNest <= 0 && (pScan->nType & PH7_TK_OCB) ){` |
|      65 | 1286 | `					if( !bFuncSeen ){` |
|      57 | 1287 | `						break; /* the hook list */` |
|       - | 1288 | `					}` |
|       9 | 1289 | `					bFuncSeen = 0;` |
|       9 | 1290 | `					pScan++;` |
|       9 | 1291 | `					PH7_DelimitNestedTokens(pScan,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pScan);` |
|       9 | 1292 | `					if( pScan >= pGen->pEnd ){` |
|     ! 0 | 1293 | `						break;` |
|       1 | 1294 | `					}` |
|       - | 1295 | `					/* land on the closing '}', the loop's pScan++ steps past it */` |
|   11149 | 1296 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    1417 | 1297 | `					break;` |
|       - | 1298 | `				}` |
|   10093 | 1299 | `				pScan++;` |
|       5 | 1300 | `			}` |
|    1471 | 1301 | `			pGen->pEnd = pScan;` |
|       - | 1302 | `		}` |
|       - | 1303 | `		/* Swap bytecode container */` |
|    1471 | 1304 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    1471 | 1305 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - | 1306 | `		/* Compile attribute value. The default is a const-expression belonging to` |
|       - | 1307 | `		 * pClass (see iInMemberDefault) — __TRAIT__ in it reads pCurClass. */` |
|    1471 | 1308 | `		pGen->iInMemberDefault++;` |
|    1471 | 1309 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    1471 | 1310 | `		pGen->iInMemberDefault--;` |
|    1471 | 1311 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 1312 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 | 1313 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1314 | `				return SXERR_ABORT;` |
|       - | 1315 | `			}` |
|     ! 0 | 1316 | `		}` |
|       - | 1317 | `		/* Emit the done instruction */` |
|    1471 | 1318 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    1471 | 1319 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    1471 | 1320 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|    1471 | 1321 | `		pGen->pEnd = pSavedDefEnd;` |
|     733 | 1322 | `	}` |
|       - | 1323 | `	/* All done,install the attribute */` |
|    2155 | 1324 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|    2155 | 1325 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1326 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 1327 | `		return SXERR_ABORT;` |
|       - | 1328 | `	}` |
|    2155 | 1329 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|       - | 1330 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|       - | 1331 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|     179 | 1332 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|     179 | 1333 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1334 | `			return SXERR_ABORT;` |
|       - | 1335 | `		}` |
|     179 | 1336 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1337 | `			goto Synchronize;` |
|       - | 1338 | `		}` |
|     179 | 1339 | `		SySetRelease(&aUnionAlts);` |
|     179 | 1340 | `		return SXRET_OK;` |
|       - | 1341 | `	}` |
|    1981 | 1342 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 1343 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|       - | 1344 | `		 * wording differs per declaration site) */` |
|     ! 0 | 1345 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 1346 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|       - | 1347 | `				? "Interfaces may only include hooked properties"` |
|       - | 1348 | `				: "Only hooked properties may be declared abstract");` |
|     ! 0 | 1349 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1350 | `			return SXERR_ABORT;` |
|       - | 1351 | `		}` |
|     ! 0 | 1352 | `		goto Synchronize;` |
|       - | 1353 | `	}` |
|    1981 | 1354 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 1355 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       9 | 1356 | `		pGen->pIn++; /* Jump the comma */` |
|       9 | 1357 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 1358 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 1359 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 1360 | `				pTok--;` |
|     ! 0 | 1361 | `			}` |
|     ! 0 | 1362 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 1363 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 | 1364 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 1365 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1366 | `				return SXERR_ABORT;` |
|       - | 1367 | `			}` |
|     ! 0 | 1368 | `		}else{` |
|       9 | 1369 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       9 | 1370 | `				goto loop;` |
|       - | 1371 | `			}` |
|       - | 1372 | `		}` |
|     ! 0 | 1373 | `	}` |
|    1973 | 1374 | `	SySetRelease(&aUnionAlts);` |
|    1973 | 1375 | `	return SXRET_OK;` |
|       9 | 1376 | `Synchronize:` |
|       - | 1377 | `	/* Synchronize with the first semi-colon */` |
|      56 | 1378 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      38 | 1379 | `		pGen->pIn++;` |
|       4 | 1380 | `	}` |
|      22 | 1381 | `	SySetRelease(&aUnionAlts);` |
|      22 | 1382 | `	return SXERR_CORRUPT;` |
|    1085 | 1383 | `}` |
|       - | 1384 | `/*` |
|       - | 1385 | ` * php validates a magic method's DECLARATION at compile time` |
|       - | 1386 | ` * (zend_check_magic_method_implementation): the ENGINE builds the arguments and` |
|       - | 1387 | ` * calls these methods on its own, so a wrong shape is rejected where it is` |
|       - | 1388 | ` * written rather than discovered — or silently tolerated — at the dispatch.` |
|       - | 1389 | ` *` |
|       - | 1390 | ` * One row per magic name; its fields are the checks php makes for that name.` |
|       - | 1391 | ` * Arity is the first of them, in php's order — which is what decides the message` |
|       - | 1392 | ` * when a declaration breaks more than one of php's rules at once.` |
|       - | 1393 | ` */` |
|       - | 1394 | `typedef struct MagicMethodRule MagicMethodRule;` |
|       - | 1395 | `struct MagicMethodRule` |
|       - | 1396 | `{` |
|       - | 1397 | `	const char *zName; /* Magic method name */` |
|       - | 1398 | `	sxu32 nName;       /* Its length */` |
|       - | 1399 | `	int nArgs;         /* Declared arguments php requires, -1 when it does not check */` |
|       - | 1400 | `	int bStatic;       /* TRUE: must be static · FALSE: must NOT be static */` |
|       - | 1401 | `	int bPublic;       /* TRUE: must be public — a WARNING, and dispatched anyway */` |
|       - | 1402 | `	int bNoReturnType; /* TRUE: declaring ANY return type is a fatal */` |
|       - | 1403 | `};` |
|       - | 1404 | `#define MAGIC_METHOD_ROW(N,A,S,P,R) { N, sizeof(N)-1, A, S, P, R }` |
|       - | 1405 | `static const MagicMethodRule aMagicMethod[] = {` |
|       - | 1406 | `	MAGIC_METHOD_ROW("__construct",  -1, FALSE, FALSE, TRUE),` |
|       - | 1407 | `	MAGIC_METHOD_ROW("__destruct",    0, FALSE, FALSE, TRUE),` |
|       - | 1408 | `	MAGIC_METHOD_ROW("__clone",       0, FALSE, FALSE, FALSE),` |
|       - | 1409 | `	MAGIC_METHOD_ROW("__get",         1, FALSE, TRUE,  FALSE),` |
|       - | 1410 | `	MAGIC_METHOD_ROW("__set",         2, FALSE, TRUE,  FALSE),` |
|       - | 1411 | `	MAGIC_METHOD_ROW("__isset",       1, FALSE, TRUE,  FALSE),` |
|       - | 1412 | `	MAGIC_METHOD_ROW("__unset",       1, FALSE, TRUE,  FALSE),` |
|       - | 1413 | `	MAGIC_METHOD_ROW("__call",        2, FALSE, TRUE,  FALSE),` |
|       - | 1414 | `	MAGIC_METHOD_ROW("__callStatic",  2, TRUE,  TRUE,  FALSE),` |
|       - | 1415 | `	MAGIC_METHOD_ROW("__toString",    0, FALSE, TRUE,  FALSE),` |
|       - | 1416 | `	MAGIC_METHOD_ROW("__invoke",     -1, FALSE, TRUE,  FALSE),` |
|       - | 1417 | `	MAGIC_METHOD_ROW("__debugInfo",   0, FALSE, TRUE,  FALSE),` |
|       - | 1418 | `	MAGIC_METHOD_ROW("__serialize",   0, FALSE, TRUE,  FALSE),` |
|       - | 1419 | `	MAGIC_METHOD_ROW("__unserialize", 1, FALSE, TRUE,  FALSE),` |
|       - | 1420 | `	MAGIC_METHOD_ROW("__sleep",       0, FALSE, TRUE,  FALSE),` |
|       - | 1421 | `	MAGIC_METHOD_ROW("__wakeup",      0, FALSE, TRUE,  FALSE),` |
|       - | 1422 | `	MAGIC_METHOD_ROW("__set_state",   1, TRUE,  TRUE,  FALSE)` |
|       - | 1423 | `};` |
|       - | 1424 | `#undef MAGIC_METHOD_ROW` |
|       - | 1425 | `/*` |
|       - | 1426 | ` * Find the rule for a declared method name, or 0 when the name is not magic.` |
|       - | 1427 | `` * php matches method names case-insensitively everywhere, so `__GET` is `__get`;`` |
|       - | 1428 | ` * the two-underscore prefix test is php's own cheap reject.` |
|       - | 1429 | ` */` |
|  111347 | 1430 | `static const MagicMethodRule * GenStateMagicMethodRule(const SyString *pName)` |
|       5 | 1431 | `{` |
|       - | 1432 | `	sxu32 n;` |
|  111352 | 1433 | `	if( pName->nByte < sizeof("__x")-1 \|\| pName->zString[0] != '_' \|\| pName->zString[1] != '_' ){` |
|    2865 | 1434 | `		return 0;` |
|       - | 1435 | `	}` |
| 1143365 | 1436 | `	for( n = 0 ; n < SX_ARRAYSIZE(aMagicMethod) ; ++n ){` |
| 1143352 | 1437 | `		if( pName->nByte == aMagicMethod[n].nName` |
|  626732 | 1438 | `		 && SyStrnicmp(pName->zString,aMagicMethod[n].zName,aMagicMethod[n].nName) == 0 ){` |
|  108484 | 1439 | `			return &aMagicMethod[n];` |
|       - | 1440 | `		}` |
|  517443 | 1441 | `	}` |
|      10 | 1442 | `	return 0;` |
|   55679 | 1443 | `}` |
|       - | 1444 | `/*` |
|       - | 1445 | ` * TRUE when php requires this magic method to be PUBLIC: the rows it merely` |
|       - | 1446 | ` * WARNS about at the declaration and then dispatches regardless of what the` |
|       - | 1447 | ` * declaration said. The runtime's visibility gate reads this to let an` |
|       - | 1448 | ` * engine-built call through — a call the user WROTE stays denied.` |
|       - | 1449 | ` *` |
|       - | 1450 | `` * `__construct`/`__destruct`/`__clone` are deliberately not in the set: a`` |
|       - | 1451 | ` * private constructor is the singleton idiom, and php enforces those three at` |
|       - | 1452 | ` * the call like any other method.` |
|       - | 1453 | ` */` |
|  107447 | 1454 | `PH7_PRIVATE int PH7_MagicMethodMustBePublic(const SyString *pName)` |
|       5 | 1455 | `{` |
|  107452 | 1456 | `	const MagicMethodRule *pRule = GenStateMagicMethodRule(pName);` |
|  107452 | 1457 | `	return pRule != 0 && pRule->bPublic;` |
|       5 | 1458 | `}` |
|       - | 1459 | `/*` |
|       - | 1460 | ` * Enforce the rules of pRule against the declaration just parsed. pName is the` |
|       - | 1461 | ` * name AS WRITTEN — php quotes that spelling, not the canonical one.` |
|       - | 1462 | ` *` |
|       - | 1463 | ` * The diagnostic is FORMATTED, not reported: php decides these rules while the` |
|       - | 1464 | ` * signature is in hand (so the arity beats __toString's return-type rule) but` |
|       - | 1465 | ` * raises them only once the declaration has cleared the checks php makes` |
|       - | 1466 | ` * first — the redeclaration and abstract-placement rules, and any parse error` |
|       - | 1467 | ` * in the body php has already read. The caller reports the buffer at that` |
|       - | 1468 | ` * point.` |
|       - | 1469 | ` *` |
|       - | 1470 | ` * Returns the severity it wrote: E_ERROR, E_WARNING, or 0 for a clean` |
|       - | 1471 | ` * declaration.` |
|       - | 1472 | ` */` |
|    3900 | 1473 | `static sxi32 GenStateCheckMagicMethod(` |
|       - | 1474 | `	ph7_class *pClass,` |
|       - | 1475 | `	const SyString *pName,` |
|       - | 1476 | `	ph7_class_method *pMeth,` |
|       - | 1477 | `	char *zErr,` |
|       - | 1478 | `	int nErrBuf` |
|       - | 1479 | `	)` |
|       5 | 1480 | `{` |
|    3905 | 1481 | `	const MagicMethodRule *pRule = GenStateMagicMethodRule(pName);` |
|    3905 | 1482 | `	if( pRule == 0 ){` |
|    2873 | 1483 | `		return 0;` |
|       - | 1484 | `	}` |
|    1037 | 1485 | `	if( pRule->nArgs >= 0 ){` |
|       - | 1486 | `		/* php counts DECLARED parameters — an optional one counts` |
|       - | 1487 | ``		 * (`__destruct($a = null)` is rejected) and the variadic tail does not`` |
|       - | 1488 | ``		 * (`__clone(...$a)` declares zero and passes, `__get(...$a)` declares`` |
|       - | 1489 | `		 * zero where one is required and does not). */` |
|     531 | 1490 | `		sxu32 nDecl = SySetUsed(&pMeth->sFunc.aArgs);` |
|     531 | 1491 | `		sxu32 nGiven = 0;` |
|       - | 1492 | `		sxu32 n;` |
|     943 | 1493 | `		for( n = 0 ; n < nDecl ; ++n ){` |
|     417 | 1494 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,n);` |
|     417 | 1495 | `			if( pArg && (pArg->iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|     415 | 1496 | `				nGiven++;` |
|     205 | 1497 | `			}` |
|     211 | 1498 | `		}` |
|     531 | 1499 | `		if( nGiven != (sxu32)pRule->nArgs ){` |
|       9 | 1500 | `			if( pRule->nArgs == 0 ){` |
|       4 | 1501 | `				SyBufferFormat(zErr,nErrBuf,"Method %z::%z() cannot take arguments",` |
|       1 | 1502 | `					&pClass->sName,pName);` |
|       2 | 1503 | `			}else{` |
|       8 | 1504 | `				SyBufferFormat(zErr,nErrBuf,"Method %z::%z() must take exactly %d argument%s",` |
|       6 | 1505 | `					&pClass->sName,pName,pRule->nArgs,pRule->nArgs == 1 ? "" : "s");` |
|       - | 1506 | `			}` |
|       9 | 1507 | `			return E_ERROR;` |
|       - | 1508 | `		}` |
|       - | 1509 | `		/* None of the arguments the engine builds may be by-reference — there is` |
|       - | 1510 | `		 * no caller variable to write back to. php checks as many arguments as` |
|       - | 1511 | `		 * the rule counts, and the count above already skipped variadics, so` |
|       - | 1512 | `		 * this walk skips them the same way. */` |
|     927 | 1513 | `		for( n = 0 ; n < nDecl ; ++n ){` |
|     409 | 1514 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,n);` |
|     409 | 1515 | `			if( pArg == 0 \|\| (pArg->iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|       3 | 1516 | `				continue;` |
|       - | 1517 | `			}` |
|     407 | 1518 | `			if( pArg->iFlags & VM_FUNC_ARG_BY_REF ){` |
|       4 | 1519 | `				SyBufferFormat(zErr,nErrBuf,"Method %z::%z() cannot take arguments by reference",` |
|       1 | 1520 | `					&pClass->sName,pName);` |
|       3 | 1521 | `				return E_ERROR;` |
|       - | 1522 | `			}` |
|     205 | 1523 | `		}` |
|     259 | 1524 | `	}` |
|       - | 1525 | `	/* Static-ness. Whether the engine has a receiver for a magic method is not` |
|       - | 1526 | ``	 * the declaration's to choose: `__callStatic` and `__set_state` are reached`` |
|       - | 1527 | `	 * with a class and nothing else, every other row is reached through an` |
|       - | 1528 | `	 * object. PHL took the declaration at its word and then dispatched the` |
|       - | 1529 | ``	 * method anyway — a `static function __get()` ran with no `$this` at all,`` |
|       - | 1530 | `	 * so a hook property table or a lazy-loading accessor read whatever the` |
|       - | 1531 | `	 * unbound scope happened to hold. php checks this after the arity, which is` |
|       - | 1532 | ``	 * why `static function __get($a,$b)` reports the count first. */`` |
|    1029 | 1533 | `	if( pRule->bStatic != ((pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0) ){` |
|       8 | 1534 | `		SyBufferFormat(zErr,nErrBuf,"Method %z::%z() %s be static",` |
|       4 | 1535 | `			&pClass->sName,pName,pRule->bStatic ? "must" : "cannot");` |
|       6 | 1536 | `		return E_ERROR;` |
|       - | 1537 | `	}` |
|       - | 1538 | `	/* Visibility. This one is a WARNING: php names the declaration and then` |
|       - | 1539 | ``	 * dispatches the method anyway, because the engine calling `__get` is not`` |
|       - | 1540 | `	 * the outside world reaching for a private member. PHL was silent at the` |
|       - | 1541 | ``	 * declaration and threw `Call to private method C::__get()` at the ACCESS —`` |
|       - | 1542 | `	 * the one rule of this family that changed what a program php RUNS does,` |
|       - | 1543 | `	 * and it killed the script. The dispatch half is` |
|       - | 1544 | `	 * PH7_MagicMethodMustBePublic, read by the runtime visibility gate. */` |
|    1025 | 1545 | `	if( pRule->bPublic && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|      51 | 1546 | `		SyBufferFormat(zErr,nErrBuf,"The magic method %z::%z() must have public visibility",` |
|      16 | 1547 | `			&pClass->sName,pName);` |
|      35 | 1548 | `		return E_WARNING;` |
|       - | 1549 | `	}` |
|       - | 1550 | ``	/* A return type on the two methods that have no return VALUE. `new C` is the`` |
|       - | 1551 | `	 * instance, never whatever __construct returned, and __destruct is called by` |
|       - | 1552 | `	 * the engine at a point with nowhere to put an answer — so php rejects any` |
|       - | 1553 | ``	 * declared type on either, `void` and `never` included, rather than let a`` |
|       - | 1554 | ``	 * declaration promise something no caller can read. (`__clone` is NOT in`` |
|       - | 1555 | ``	 * this row: `: void` on it is valid php.) PHL enforced the declared type at`` |
|       - | 1556 | ``	 * runtime instead, so `__construct(): int` raised a TypeError at every`` |
|       - | 1557 | `	 * instantiation — a diagnostic on the CALL for a mistake in the` |
|       - | 1558 | `	 * declaration. */` |
|     988 | 1559 | `	if( pRule->bNoReturnType` |
|     935 | 1560 | `	 && (pMeth->sFunc.nReturnType > 0` |
|     436 | 1561 | `	  \|\| SyStringLength(&pMeth->sFunc.sReturnClass) > 0` |
|     434 | 1562 | `	  \|\| SySetUsed(&pMeth->sFunc.aReturnUnion) > 0) ){` |
|       8 | 1563 | `		SyBufferFormat(zErr,nErrBuf,"Method %z::%z() cannot declare a return type",` |
|       2 | 1564 | `			&pClass->sName,pName);` |
|       6 | 1565 | `		return E_ERROR;` |
|       - | 1566 | `	}` |
|     989 | 1567 | `	return 0;` |
|    1955 | 1568 | `}` |
|       - | 1569 | `/*` |
|       - | 1570 | ` * Raise the declaration diagnostic parked above (GenStateCheckMagicMethod's magic-method` |
|       - | 1571 | ` * rules, or the final-private one beside its call), once, and disarm it.` |
|       - | 1572 | ` * Suppressed when this declaration has already reported a fatal php decides` |
|       - | 1573 | ` * FIRST — a redeclaration, an abstract method in a non-abstract class, a parse` |
|       - | 1574 | ` * error in the body — since php stops at its own first fatal.` |
|       - | 1575 | ` */` |
|    3884 | 1576 | `static sxi32 GenStateRaiseMagicDiag(` |
|       - | 1577 | `	ph7_gen_state *pGen,` |
|       - | 1578 | `	sxi32 *pnSeverity,   /* IN/OUT: the parked severity, zeroed here */` |
|       - | 1579 | `	const char *zErr,` |
|       - | 1580 | `	sxu32 nLine,` |
|       - | 1581 | `	sxu32 nErrEntry      /* pGen->nErr when this declaration started */` |
|       - | 1582 | `	)` |
|       5 | 1583 | `{` |
|    3889 | 1584 | `	sxi32 rc = SXRET_OK;` |
|    3889 | 1585 | `	if( *pnSeverity != 0 ){` |
|      54 | 1586 | `		if( pGen->nErr == nErrEntry ){` |
|      54 | 1587 | `			rc = PH7_GenCompileError(pGen,*pnSeverity,nLine,"%s",zErr);` |
|      25 | 1588 | `		}` |
|      54 | 1589 | `		*pnSeverity = 0;` |
|      25 | 1590 | `	}` |
|    3889 | 1591 | `	return rc;` |
|       5 | 1592 | `}` |
|       - | 1593 | `/*` |
|       - | 1594 | ` * Compile a class method.` |
|       - | 1595 | ` *` |
|       - | 1596 | ` * Refer to the official documentation for more information` |
|       - | 1597 | ` * on the powerful extension introduced by the PH7 engine` |
|       - | 1598 | ` * to the OO subsystem such as full type hinting,method` |
|       - | 1599 | ` * overloading and many more.` |
|       - | 1600 | ` */` |
|    3902 | 1601 | `static sxi32 GenStateCompileClassMethod(` |
|       - | 1602 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 1603 | `	sxi32 iProtection,   /* Visibility level */` |
|       - | 1604 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - | 1605 | `	int doBody,          /* TRUE to process method body */` |
|       - | 1606 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - | 1607 | `	)` |
|       5 | 1608 | `{` |
|    3907 | 1609 | `	sxu32 nLine = pGen->pIn->nLine;` |
|    3907 | 1610 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    3907 | 1611 | `	sxu32 nErrEntry = pGen->nErr; /* Errors already reported when this declaration started */` |
|       - | 1612 | `	char zMagicErr[256];          /* Pending magic-method rule violation, reported at the end */` |
|    3907 | 1613 | `	sxi32 nMagicSeverity = 0;     /* E_ERROR / E_WARNING while zMagicErr is still unreported */` |
|    3907 | 1614 | `	int bMagicFatal = FALSE;      /* The parked diagnostic was a fatal: do not install the method */` |
|       - | 1615 | `	ph7_class_method *pMeth;` |
|       - | 1616 | `	sxi32 iFuncFlags;` |
|       - | 1617 | `	SyString *pName;` |
|       - | 1618 | `	SyToken *pEnd;` |
|       - | 1619 | `	sxi32 rc;` |
|       - | 1620 | `	/* Extract visibility level */` |
|    3907 | 1621 | `	iProtection = GetProtectionLevel(iProtection);` |
|    3907 | 1622 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|    3907 | 1623 | `	iFuncFlags = 0;` |
|    3907 | 1624 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 1625 | `		/* Invalid method name */` |
|     ! 0 | 1626 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 1627 | `		if( rc == SXERR_ABORT ){` |
|       - | 1628 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1629 | `			return SXERR_ABORT;` |
|       - | 1630 | `		}` |
|     ! 0 | 1631 | `		goto Synchronize;` |
|       - | 1632 | `	}` |
|    3907 | 1633 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 1634 | `		/* Return by reference,remember that */` |
|       6 | 1635 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 1636 | `		/* Jump the '&' token */` |
|       6 | 1637 | `		pGen->pIn++;` |
|       2 | 1638 | `	}` |
|    3907 | 1639 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 1640 | `		/* Invalid method name */` |
|     ! 0 | 1641 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 1642 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1643 | `			return SXERR_ABORT;` |
|       - | 1644 | `		}` |
|     ! 0 | 1645 | `		goto Synchronize;` |
|       - | 1646 | `	}` |
|       - | 1647 | `	/* Peek method name */` |
|    3907 | 1648 | `	pName = &pGen->pIn->sData;` |
|    3907 | 1649 | `	nLine = pGen->pIn->nLine;` |
|       - | 1650 | `	/* Jump the method name */` |
|    3907 | 1651 | `	pGen->pIn++;` |
|    3907 | 1652 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 1653 | `		/* Abstract method */` |
|     131 | 1654 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 1655 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1656 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 | 1657 | `				&pClass->sName,pName);` |
|     ! 0 | 1658 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1659 | `				return SXERR_ABORT;` |
|       - | 1660 | `			}` |
|     ! 0 | 1661 | `		}` |
|       - | 1662 | `		/* Assemble method signature only */` |
|     131 | 1663 | `		doBody = FALSE;` |
|      63 | 1664 | `	}` |
|    3907 | 1665 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 1666 | `		/* Syntax error */` |
|     ! 0 | 1667 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 | 1668 | `		if( rc == SXERR_ABORT ){` |
|       - | 1669 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1670 | `			return SXERR_ABORT;` |
|       - | 1671 | `		}` |
|     ! 0 | 1672 | `		goto Synchronize;` |
|       - | 1673 | `	}` |
|       - | 1674 | `	/* Allocate a new class_method instance */` |
|    3907 | 1675 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|    3907 | 1676 | `	if( pMeth == 0 ){` |
|     ! 0 | 1677 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 1678 | `		return SXERR_ABORT;` |
|       - | 1679 | `	}` |
|    3907 | 1680 | `	pMeth->sFunc.nLine = nKwLine;` |
|    3907 | 1681 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|    3907 | 1682 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|     ! 0 | 1683 | `		return SXERR_ABORT;` |
|       - | 1684 | `	}` |
|    3907 | 1685 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pMeth->sFunc.aAttrs,4,4,0,0) == SXERR_ABORT ){` |
|     ! 0 | 1686 | `		return SXERR_ABORT;` |
|       - | 1687 | `	}` |
|       - | 1688 | `	/* Jump the left parenthesis '(' */` |
|    3907 | 1689 | `	pGen->pIn++;` |
|    3907 | 1690 | `	pEnd = 0; /* cc warning */` |
|       - | 1691 | `	/* Delimit the method signature */` |
|    3907 | 1692 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3907 | 1693 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 1694 | `		/* Syntax error */` |
|       3 | 1695 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 | 1696 | `		if( rc == SXERR_ABORT ){` |
|       - | 1697 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1698 | `			return SXERR_ABORT;` |
|       - | 1699 | `		}` |
|       3 | 1700 | `		goto Synchronize;` |
|       - | 1701 | `	}` |
|       - | 1702 | `	{` |
|    3905 | 1703 | `		int bIsCtor = 0;` |
|    3905 | 1704 | `		int bAbstractCtor = 0;` |
|       - | 1705 | `		/* Only __construct is the constructor (PHP-4 class-name constructors removed` |
|       - | 1706 | `		 * in 8.0): a method named like the class is a plain method, so promoted` |
|       - | 1707 | `		 * properties in it are rejected exactly as php does elsewhere. */` |
|    3900 | 1708 | `		if( pName->nByte == sizeof("__construct") - 1` |
|    2288 | 1709 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0 ){` |
|     427 | 1710 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       8 | 1711 | `				bAbstractCtor = 1;` |
|       5 | 1712 | `			}else{` |
|     421 | 1713 | `				bIsCtor = 1;` |
|       - | 1714 | `			}` |
|     211 | 1715 | `		}` |
|    3905 | 1716 | `		if( pGen->pIn < pEnd ){` |
|       - | 1717 | `			/* Collect method arguments */` |
|    1477 | 1718 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|    1477 | 1719 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1720 | `				return SXERR_ABORT;` |
|       - | 1721 | `			}` |
|     736 | 1722 | `		}` |
|       - | 1723 | `	}` |
|       - | 1724 | `	/* Point past ')' and parse optional return type ': type' */` |
|    3905 | 1725 | `	pGen->pIn = &pEnd[1];` |
|       - | 1726 | `	{` |
|    3905 | 1727 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|    3905 | 1728 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 | 1729 | `			return SXERR_ABORT;` |
|    3905 | 1730 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 | 1731 | `			goto Synchronize;` |
|       - | 1732 | `		}` |
|       - | 1733 | `	}` |
|       - | 1734 | `	/* php's #[\NoDiscard] declaration rules, which want the return type. */` |
|    4233 | 1735 | `	if( GenStateApplyNoDiscard(&(*pGen),&pMeth->sFunc,pClass,` |
|    3900 | 1736 | `			pName->nByte == sizeof("__construct")-1` |
|    2283 | 1737 | `			 && SyStrnicmp(pName->zString,"__construct",sizeof("__construct")-1) == 0)` |
|    1955 | 1738 | `		== SXERR_ABORT ){` |
|     ! 0 | 1739 | `		return SXERR_ABORT;` |
|       - | 1740 | `	}` |
|       - | 1741 | `	/* php's compile-time magic-method declaration rules, DECIDED here — with the` |
|       - | 1742 | `	 * signature in hand and before the __toString return-type rule below, which` |
|       - | 1743 | ``	 * is php's own order (`static function __toString($a): int` reports the`` |
|       - | 1744 | `	 * arity). Reported at the end of this function; see zMagicErr there. */` |
|    3905 | 1745 | `	nMagicSeverity = GenStateCheckMagicMethod(pClass,pName,pMeth,zMagicErr,(int)sizeof(zMagicErr));` |
|    3900 | 1746 | `	if( nMagicSeverity == 0` |
|    3876 | 1747 | `	 && (pMeth->iFlags & PH7_CLASS_ATTR_FINAL)` |
|    1937 | 1748 | `	 && pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       - | 1749 | `		/* Not a magic rule, but the same KIND of rule and the same parking: php` |
|       - | 1750 | `		 * checks a declaration php itself decides the meaning of. A private method` |
|       - | 1751 | ``		 * is never overridden, so `final` on one says nothing — php WARNS here (8.0+)`` |
|       - | 1752 | `		 * and compiles the class. PHL was silent at the declaration and then fataled` |
|       - | 1753 | `		 * at the SUBCLASS that reused the name ("Cannot override final method"), a` |
|       - | 1754 | `		 * class php accepts; the inheritance half is in PH7_ClassInherit. */` |
|       3 | 1755 | `		SyBufferFormat(zMagicErr,sizeof(zMagicErr),` |
|       - | 1756 | `			"Private methods cannot be final as they are never overridden by other classes");` |
|       3 | 1757 | `		nMagicSeverity = E_WARNING;` |
|       1 | 1758 | `	}` |
|    3905 | 1759 | `	if( nMagicSeverity == E_ERROR ){` |
|       - | 1760 | `		/* Suppress the __toString rule below: php never reaches it on a` |
|       - | 1761 | `		 * declaration the magic rules already rejected. */` |
|      20 | 1762 | `		bMagicFatal = TRUE;` |
|      20 | 1763 | `		goto SkipToStringType;` |
|       - | 1764 | `	}` |
|       - | 1765 | `	/*` |
|       - | 1766 | ``	 * php gives __toString() an IMPLICIT `string` return type. That is what makes`` |
|       - | 1767 | ``	 * `return 42` coerce to "42" and `return null` / an array / an object / falling`` |
|       - | 1768 | `	 * off the end raise` |
|       - | 1769 | `	 *   C::__toString(): Return value must be of type string, X returned` |
|       - | 1770 | `	 * PHL enforced DECLARED return types only, so an undeclared __toString could` |
|       - | 1771 | `	 * answer anything and MemObjStringValue fell back to the "Object" placeholder` |
|       - | 1772 | `	 * for whatever was not a non-empty string. Installing the type here reuses the` |
|       - | 1773 | `	 * enforcement that already matches php byte for byte.` |
|       - | 1774 | `	 *` |
|       - | 1775 | `	 * sReturnTypeName is filled in as well, for two reasons: reflection reports the` |
|       - | 1776 | `	 * implicit type exactly as php does (hasReturnType() TRUE, getReturnType()` |
|       - | 1777 | `	 * "string" for an undeclared __toString), and the generator-return-type fatal` |
|       - | 1778 | ``	 * renders from it — so a __toString with a `yield` in it now reports php's`` |
|       - | 1779 | `	 * "Generator return type must be a supertype of Generator, string given".` |
|       - | 1780 | `	 *` |
|       - | 1781 | ``	 * Declaring any OTHER return type is php's own compile fatal, `?string`, a`` |
|       - | 1782 | ``	 * union, `mixed`, `static` and `void` included. (php checks`` |
|       - | 1783 | ``	 * "A void method must not return a value" FIRST when a `: void` __toString also`` |
|       - | 1784 | `	 * returns a value; PHL has no such check yet, so it reports this one instead —` |
|       - | 1785 | `	 * both reject, on doubly-invalid input only.)` |
|       - | 1786 | `	 */` |
|    3884 | 1787 | `	if( pName->nByte == sizeof("__toString")-1` |
|    2165 | 1788 | `	 && SyStrnicmp(pName->zString,"__toString",sizeof("__toString")-1) == 0 ){` |
|     171 | 1789 | `		ph7_vm_func *pTsFunc = &pMeth->sFunc;` |
|     171 | 1790 | `		int bTsDeclared = pTsFunc->nReturnType > 0 \|\| SySetUsed(&pTsFunc->aReturnUnion) > 0;` |
|     171 | 1791 | `		if( bTsDeclared ){` |
|      96 | 1792 | `			if( pTsFunc->nReturnType != MEMOBJ_STRING` |
|      95 | 1793 | `			 \|\| SySetUsed(&pTsFunc->aReturnUnion) > 0` |
|      99 | 1794 | `			 \|\| (pTsFunc->iFlags & VM_FUNC_RETURN_NULLABLE) ){` |
|       - | 1795 | `				/* php raises this one AFTER the magic rules, so a parked` |
|       - | 1796 | `				 * visibility warning is php's first line here rather than a` |
|       - | 1797 | `				 * casualty of the fatal about to be counted. */` |
|       6 | 1798 | `				if( GenStateRaiseMagicDiag(pGen,&nMagicSeverity,zMagicErr,` |
|       6 | 1799 | `						nKwLine,nErrEntry) == SXERR_ABORT ){` |
|     ! 0 | 1800 | `					return SXERR_ABORT;` |
|       - | 1801 | `				}` |
|       8 | 1802 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1803 | `					"%z::%z(): Return type must be string when declared",` |
|       2 | 1804 | `					&pClass->sName,pName);` |
|       6 | 1805 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1806 | `					return SXERR_ABORT;` |
|       - | 1807 | `				}` |
|       6 | 1808 | `				goto Synchronize;` |
|       - | 1809 | `			}` |
|      51 | 1810 | `		}else{` |
|      75 | 1811 | `			char *zTsType = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       - | 1812 | `				"string",sizeof("string")-1);` |
|      75 | 1813 | `			pTsFunc->nReturnType = MEMOBJ_STRING;` |
|      75 | 1814 | `			if( zTsType ){` |
|      75 | 1815 | `				SyStringInitFromBuf(&pTsFunc->sReturnTypeName,zTsType,sizeof("string")-1);` |
|      35 | 1816 | `			}` |
|       - | 1817 | `		}` |
|      81 | 1818 | `	}` |
|     ! 0 | 1819 | `SkipToStringType:` |
|       - | 1820 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - | 1821 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - | 1822 | `	 * since we mint real ph7_class_attr entries. */` |
|       - | 1823 | `	{` |
|    3901 | 1824 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - | 1825 | `		sxu32 i;` |
|    5935 | 1826 | `		for( i = 0; i < nArg; i++ ){` |
|    2049 | 1827 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - | 1828 | `			ph7_class_attr *pAttr;` |
|    2049 | 1829 | `			sxi32 iAttrFlags = 0;` |
|       - | 1830 | `			int bArgTyped;` |
|    2049 | 1831 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|    1921 | 1832 | `				continue;` |
|       - | 1833 | `			}` |
|       - | 1834 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|       - | 1835 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|       - | 1836 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|      93 | 1837 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|     140 | 1838 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|     133 | 1839 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 | 1840 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1841 | `					"Cannot declare variadic promoted property");` |
|       3 | 1842 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1843 | `					return SXERR_ABORT;` |
|       - | 1844 | `				}` |
|       3 | 1845 | `				goto Synchronize;` |
|       - | 1846 | `			}` |
|       - | 1847 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - | 1848 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - | 1849 | `			 * appear as an alternative of a union type. */` |
|     131 | 1850 | `			if( bArgTyped ){` |
|     170 | 1851 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|     110 | 1852 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|     110 | 1853 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      55 | 1854 | `					"Property %z::$%z cannot have type %z",nLine);` |
|     115 | 1855 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1856 | `					return SXERR_ABORT;` |
|     115 | 1857 | `				}else if( rc != SXRET_OK ){` |
|       6 | 1858 | `					goto Synchronize;` |
|       - | 1859 | `				}` |
|      53 | 1860 | `			}` |
|       - | 1861 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|     127 | 1862 | `			if( GenStateExtractProperty(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 | 1863 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 | 1864 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 | 1865 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1866 | `					return SXERR_ABORT;` |
|       - | 1867 | `				}` |
|       3 | 1868 | `				goto Synchronize;` |
|       - | 1869 | `			}` |
|     125 | 1870 | `			if( bArgTyped ){` |
|     108 | 1871 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      52 | 1872 | `			}` |
|     125 | 1873 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 | 1874 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 | 1875 | `			}` |
|     125 | 1876 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       8 | 1877 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       3 | 1878 | `			}` |
|     125 | 1879 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|       - | 1880 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|       - | 1881 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|      36 | 1882 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       4 | 1883 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 | 1884 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|       3 | 1885 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 1886 | `						return SXERR_ABORT;` |
|       - | 1887 | `					}` |
|       3 | 1888 | `					goto Synchronize;` |
|       - | 1889 | `				}` |
|      34 | 1890 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|      15 | 1891 | `			}` |
|     123 | 1892 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|       - | 1893 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|       5 | 1894 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|     ! 0 | 1895 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1896 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|     ! 0 | 1897 | `						&pClass->sName,&pArg->sName);` |
|     ! 0 | 1898 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 1899 | `						return SXERR_ABORT;` |
|       - | 1900 | `					}` |
|     ! 0 | 1901 | `					goto Synchronize;` |
|       - | 1902 | `				}` |
|       5 | 1903 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|       2 | 1904 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|       2 | 1905 | `			}` |
|     123 | 1906 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|     123 | 1907 | `			if( pAttr == 0 ){` |
|     ! 0 | 1908 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 1909 | `				return SXERR_ABORT;` |
|       - | 1910 | `			}` |
|     123 | 1911 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|     108 | 1912 | `				pAttr->nType = pArg->nType;` |
|     108 | 1913 | `				pAttr->sClass = pArg->sClass;` |
|     108 | 1914 | `				pAttr->sTypeName = pArg->sTypeName;` |
|     108 | 1915 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - | 1916 | `					sxu32 k;` |
|      20 | 1917 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|      14 | 1918 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|      14 | 1919 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|       8 | 1920 | `					}` |
|       3 | 1921 | `				}` |
|      52 | 1922 | `			}` |
|       - | 1923 | ``			/* A promoted parameter's `#[...]` belongs to BOTH members in php: the`` |
|       - | 1924 | `			 * ReflectionParameter reports it and so does the ReflectionProperty,` |
|       - | 1925 | ``			 * which is what makes `#[\Override] public $p` in a constructor`` |
|       - | 1926 | `			 * signature a PROPERTY claim. The records are shared, not copied --` |
|       - | 1927 | `			 * the parameter owns them for the VM's lifetime. */` |
|       - | 1928 | `			{` |
|     123 | 1929 | `				ph7_attribute *aSrc = (ph7_attribute *)SySetBasePtr(&pArg->aAttrs);` |
|       - | 1930 | `				sxu32 k;` |
|     135 | 1931 | `				for( k = 0 ; k < SySetUsed(&pArg->aAttrs) ; k++ ){` |
|      15 | 1932 | `					SySetPut(&pAttr->aAttrs,(const void *)&aSrc[k]);` |
|       9 | 1933 | `				}` |
|       - | 1934 | `			}` |
|     123 | 1935 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|     123 | 1936 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 1937 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 1938 | `				return SXERR_ABORT;` |
|       - | 1939 | `			}` |
|      64 | 1940 | `		}` |
|       - | 1941 | `	}` |
|    3891 | 1942 | `	if( doBody ){` |
|       - | 1943 | `		/* Compile method body */` |
|    3765 | 1944 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|    3765 | 1945 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1946 | `			return SXERR_ABORT;` |
|       - | 1947 | `		}` |
|       - | 1948 | `		/* The cursor sits just past the body's closing brace */` |
|    3765 | 1949 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|    1885 | 1950 | `	}else{` |
|       - | 1951 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|     131 | 1952 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|     131 | 1953 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|      63 | 1954 | `		}` |
|       - | 1955 | `		/* Only method signature is allowed */` |
|     131 | 1956 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 | 1957 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 1958 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 | 1959 | `				if( rc == SXERR_ABORT ){` |
|       - | 1960 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 1961 | `					return SXERR_ABORT;` |
|       - | 1962 | `				}` |
|     ! 0 | 1963 | `				return SXERR_CORRUPT;` |
|       - | 1964 | `			}` |
|       - | 1965 | `	}` |
|       - | 1966 | `	/* php: an abstract method in a class that was not DECLARED abstract is a fatal,` |
|       - | 1967 | `	 * and it names the method -- distinct from the "contains N abstract methods"` |
|       - | 1968 | `	 * wording php uses for an unimplemented INHERITED one, which` |
|       - | 1969 | `	 * GenStateCheckAbstractMethods already handles. Interfaces and traits may carry` |
|       - | 1970 | `	 * abstract methods freely. */` |
|    3886 | 1971 | `	if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT)` |
|    2011 | 1972 | `	 && (pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|       4 | 1973 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1974 | `			"Class %z declares abstract method %z() and must therefore be declared abstract",` |
|       1 | 1975 | `			&pClass->sName,pName);` |
|       3 | 1976 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1977 | `			return SXERR_ABORT;` |
|       - | 1978 | `		}` |
|       3 | 1979 | `		return SXRET_OK;` |
|       - | 1980 | `	}` |
|       - | 1981 | `	/* php: two methods of the same name in one class body is a fatal, and the` |
|       - | 1982 | ``	 * comparison is case-INSENSITIVE (hMethod now matches that way), so `f` and`` |
|       - | 1983 | ``	 * `F` collide. php names the offending declaration with the spelling used at`` |
|       - | 1984 | `	 * the SECOND site. */` |
|    3889 | 1985 | `	if( PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte) != 0 ){` |
|       8 | 1986 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       2 | 1987 | `			"Cannot redeclare %z::%z()",&pClass->sName,pName);` |
|       6 | 1988 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1989 | `			return SXERR_ABORT;` |
|       - | 1990 | `		}` |
|       6 | 1991 | `		return SXRET_OK;` |
|       - | 1992 | `	}` |
|       - | 1993 | `	/* The magic-method rule this declaration broke (decided above, with the` |
|       - | 1994 | `	 * signature in hand). It is raised HERE because php raises it last of the` |
|       - | 1995 | `	 * declaration's fatals: a redeclaration, an abstract method in a class that` |
|       - | 1996 | `	 * is not abstract, and any parse error inside the body php has already read` |
|       - | 1997 | `	 * all win — and each of them has, by now, either returned or bumped nErr.` |
|       - | 1998 | `	 * php stops at its first fatal, so one is all this declaration reports. The` |
|       - | 1999 | ``	 * line is the `function` KEYWORD's, which is where php points once a`` |
|       - | 2000 | `	 * signature wraps across lines. */` |
|    3885 | 2001 | `	if( GenStateRaiseMagicDiag(pGen,&nMagicSeverity,zMagicErr,nKwLine,nErrEntry) == SXERR_ABORT ){` |
|     ! 0 | 2002 | `		return SXERR_ABORT;` |
|       - | 2003 | `	}` |
|    3885 | 2004 | `	if( bMagicFatal ){` |
|       - | 2005 | `		/* Never install a method php refused to declare. A WARNING falls through:` |
|       - | 2006 | `		 * php keeps the method and calls it. */` |
|      20 | 2007 | `		return SXRET_OK;` |
|       - | 2008 | `	}` |
|       - | 2009 | `	/* All done,install the method */` |
|    3869 | 2010 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|    3869 | 2011 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2012 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2013 | `		return SXERR_ABORT;` |
|       - | 2014 | `	}` |
|    3869 | 2015 | `	return SXRET_OK;` |
|       8 | 2016 | `Synchronize:` |
|       - | 2017 | `	/* Synchronize with the first semi-colon */` |
|      56 | 2018 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      40 | 2019 | `		pGen->pIn++;` |
|       4 | 2020 | `	}` |
|      20 | 2021 | `	return SXERR_CORRUPT;` |
|    1956 | 2022 | `}` |
|       - | 2023 | `/*` |
|       - | 2024 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|       - | 2025 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|       - | 2026 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|       - | 2027 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|       - | 2028 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|       - | 2029 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|       - | 2030 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|       - | 2031 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|       - | 2032 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|       - | 2033 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|       - | 2034 | `` * implicit `$value` formal.`` |
|       - | 2035 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|       - | 2036 | ` */` |
|       - | 2037 | `/*` |
|       - | 2038 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|       - | 2039 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|       - | 2040 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|       - | 2041 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|       - | 2042 | ` * allowed, excluded from the raw object surfaces.` |
|       - | 2043 | ` */` |
|     180 | 2044 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|       5 | 2045 | `{` |
|       - | 2046 | `	SyToken *p;` |
|     827 | 2047 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|     713 | 2048 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|     581 | 2049 | `			continue;` |
|       - | 2050 | `		}` |
|       - | 2051 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|     132 | 2052 | `		if( p + 3 < pEnd` |
|     132 | 2053 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|     132 | 2054 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|     115 | 2055 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|      98 | 2056 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|      98 | 2057 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|      98 | 2058 | `		 && p[3].sData.nByte == pName->nByte` |
|      91 | 2059 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      65 | 2060 | `			return 1;` |
|       - | 2061 | `		}` |
|       - | 2062 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|       - | 2063 | `		 * hook operates on the shared per-instance backing store, so the` |
|       - | 2064 | `		 * property is backed (php compiles a default alongside it). */` |
|      70 | 2065 | `		if( p > pStart` |
|      66 | 2066 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|      33 | 2067 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       4 | 2068 | `		 && p[1].sData.nByte == pName->nByte` |
|       8 | 2069 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       6 | 2070 | `			return 1;` |
|       - | 2071 | `		}` |
|      36 | 2072 | `	}` |
|     119 | 2073 | `	return 0;` |
|      95 | 2074 | `}` |
|       - | 2075 | `/*` |
|       - | 2076 | ` * True when p opens php 8.4's parent-hook call form` |
|       - | 2077 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|       - | 2078 | ` */` |
|    1448 | 2079 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|       5 | 2080 | `{` |
|    1674 | 2081 | `	return p + 6 < pEnd` |
|     945 | 2082 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|     317 | 2083 | `	 && p->sData.nByte == sizeof("parent")-1` |
|     110 | 2084 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|      20 | 2085 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|      12 | 2086 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|      12 | 2087 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|      12 | 2088 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|      12 | 2089 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|      12 | 2090 | `	 && p[5].sData.nByte == 3` |
|      12 | 2091 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|       8 | 2092 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|    1669 | 2093 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|       5 | 2094 | `}` |
|       - | 2095 | `/*` |
|       - | 2096 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|       - | 2097 | ` * hook body into calls of the parent class's synthesized hook method` |
|       - | 2098 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|       - | 2099 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|       - | 2100 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|       - | 2101 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|       - | 2102 | ` * or SXERR_MEM.` |
|       - | 2103 | ` */` |
|       6 | 2104 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|       - | 2105 | `	SyToken *pStart,SyToken *pEnd)` |
|       2 | 2106 | `{` |
|       8 | 2107 | `	SyToken *p = pStart;` |
|      56 | 2108 | `	while( p < pEnd ){` |
|      50 | 2109 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|       - | 2110 | `			SyToken sTok;` |
|       - | 2111 | `			char zName[384];` |
|       - | 2112 | `			sxu32 nName;` |
|       - | 2113 | `			char *zDup;` |
|       - | 2114 | ``			/* `parent` `::` */`` |
|       8 | 2115 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|       8 | 2116 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|      11 | 2117 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|       6 | 2118 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|       8 | 2119 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|       8 | 2120 | `			if( zDup == 0 ){` |
|     ! 0 | 2121 | `				return SXERR_MEM;` |
|       - | 2122 | `			}` |
|       8 | 2123 | `			sTok = p[3]; /* keep the line info of the property name */` |
|       8 | 2124 | `			sTok.nType = PH7_TK_ID;` |
|       8 | 2125 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|       8 | 2126 | `			sTok.pUserData = 0;` |
|       8 | 2127 | `			SySetPut(pCopy,(const void *)&sTok);` |
|       8 | 2128 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|       8 | 2129 | `			continue;` |
|       - | 2130 | `		}` |
|      44 | 2131 | `		SySetPut(pCopy,(const void *)p);` |
|      44 | 2132 | `		p++;` |
|       2 | 2133 | `	}` |
|       8 | 2134 | `	return SXRET_OK;` |
|       5 | 2135 | `}` |
|       - | 2136 | `/*` |
|       - | 2137 | `` * A `get` hook's return type IS the property's declared type — php never lets a`` |
|       - | 2138 | ` * hook declare one, so there is nothing else it could be, and that is what makes` |
|       - | 2139 | `` * `public int $p { get { return "5"; } }` answer int(5) and a `get` returning`` |
|       - | 2140 | `` * "x" raise `C::$p::get(): Return value must be of type int, string returned`.`` |
|       - | 2141 | ` * Installing it on the synthesized method reuses the return enforcement that` |
|       - | 2142 | ` * already matches php byte for byte (the same move the __toString implicit` |
|       - | 2143 | `` * `string` type made), and lets the compile-time bare-`return;` check see the`` |
|       - | 2144 | ` * hook as the typed function php treats it as.` |
|       - | 2145 | ` *` |
|       - | 2146 | ` * The union alternatives are SHARED, not copied: their class-name SyStrings are` |
|       - | 2147 | ` * VM-allocator owned and outlive both records, which is the same contract` |
|       - | 2148 | ` * GenStateCopyTypeToAttr relies on.` |
|       - | 2149 | ` */` |
|     142 | 2150 | `static void GenStateHookGetReturnType(ph7_vm_func *pFunc,ph7_class_attr *pAttr)` |
|       5 | 2151 | `{` |
|     147 | 2152 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      19 | 2153 | `		return; /* untyped property: the hook is untyped too */` |
|       - | 2154 | `	}` |
|     131 | 2155 | `	pFunc->nReturnType = pAttr->nType;` |
|     131 | 2156 | `	pFunc->sReturnClass = pAttr->sClass;` |
|     131 | 2157 | `	pFunc->sReturnTypeName = pAttr->sTypeName;` |
|     131 | 2158 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|      14 | 2159 | `		pFunc->iFlags \|= VM_FUNC_RETURN_NULLABLE;` |
|       6 | 2160 | `	}` |
|     131 | 2161 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       - | 2162 | `		sxu32 i;` |
|     ! 0 | 2163 | `		for( i = 0 ; i < SySetUsed(&pAttr->aUnionAlts) ; i++ ){` |
|     ! 0 | 2164 | `			SySetPut(&pFunc->aReturnUnion,SySetAt(&pAttr->aUnionAlts,i));` |
|     ! 0 | 2165 | `		}` |
|     ! 0 | 2166 | `	}` |
|      76 | 2167 | `}` |
|       - | 2168 | `/*` |
|       - | 2169 | `` * The mirror for a `set` hook. php gives it two implicit pieces of signature:`` |
|       - | 2170 | `` * the implicit `$value` formal carries the PROPERTY's declared type (so`` |
|       - | 2171 | `` * `public int $p { set { ... } }` coerces `$o->p = "7"` to int(7) and rejects`` |
|       - | 2172 | `` * "abc" with `C::$p::set(): Argument #1 ($value) must be of type int, string`` |
|       - | 2173 | `` * given`), and the hook itself returns `void` — a set hook that returns a value`` |
|       - | 2174 | `` * is php's `A void method must not return a value`, on an untyped property too.`` |
|       - | 2175 | `` * An EXPLICIT `set(T $v)` keeps its own declared type; only the implicit formal`` |
|       - | 2176 | ` * is typed from the property, which is why the caller passes pValueArg only` |
|       - | 2177 | ` * when it synthesized one.` |
|       - | 2178 | ` */` |
|      94 | 2179 | `static void GenStateHookSetSignature(ph7_gen_state *pGen,ph7_vm_func *pFunc,` |
|       - | 2180 | `	ph7_class_attr *pAttr,ph7_vm_func_arg *pValueArg)` |
|       4 | 2181 | `{` |
|       - | 2182 | `	char *zVoid;` |
|      98 | 2183 | `	if( pValueArg && (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|      69 | 2184 | `		pValueArg->nType = pAttr->nType;` |
|      69 | 2185 | `		pValueArg->sClass = pAttr->sClass;` |
|      69 | 2186 | `		pValueArg->sTypeName = pAttr->sTypeName;` |
|      69 | 2187 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|      14 | 2188 | `			pValueArg->iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|       6 | 2189 | `		}` |
|      69 | 2190 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       - | 2191 | `			sxu32 i;` |
|       3 | 2192 | `			pValueArg->iFlags \|= VM_FUNC_ARG_UNION;` |
|       7 | 2193 | `			for( i = 0 ; i < SySetUsed(&pAttr->aUnionAlts) ; i++ ){` |
|       5 | 2194 | `				SySetPut(&pValueArg->aUnionAlts,SySetAt(&pAttr->aUnionAlts,i));` |
|       3 | 2195 | `			}` |
|       1 | 2196 | `		}` |
|      33 | 2197 | `	}` |
|      98 | 2198 | `	pFunc->nReturnType = MEMOBJ_VOID;` |
|      98 | 2199 | `	zVoid = SyMemBackendStrDup(&pGen->pVm->sAllocator,"void",sizeof("void")-1);` |
|      98 | 2200 | `	if( zVoid ){` |
|      98 | 2201 | `		SyStringInitFromBuf(&pFunc->sReturnTypeName,zVoid,sizeof("void")-1);` |
|      47 | 2202 | `	}` |
|      98 | 2203 | `}` |
|     174 | 2204 | `PH7_PRIVATE sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|       5 | 2205 | `{` |
|     179 | 2206 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 2207 | `	sxi32 rc;` |
|     179 | 2208 | `	int bRefsSelf = 0;` |
|     179 | 2209 | `	pGen->pIn++; /* Jump '{' */` |
|     431 | 2210 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|       - | 2211 | `		char zHook[384];` |
|       - | 2212 | `		SyString sHookName;` |
|       - | 2213 | `		ph7_class_method *pMeth;` |
|       - | 2214 | `		int bGet;` |
|     257 | 2215 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|     257 | 2216 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|      18 | 2217 | `			pGen->pIn++; /* stray ';' between hooks */` |
|      26 | 2218 | `			continue;` |
|       - | 2219 | `		}` |
|     241 | 2220 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - | 2221 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|     ! 0 | 2222 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|       - | 2223 | `				"By-reference property hooks are not supported for %z::$%z",` |
|     ! 0 | 2224 | `				&pClass->sName,&pAttr->sName);` |
|     ! 0 | 2225 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2226 | `				return SXERR_ABORT;` |
|       - | 2227 | `			}` |
|     ! 0 | 2228 | `			return SXERR_CORRUPT;` |
|       - | 2229 | `		}` |
|     241 | 2230 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 2231 | `			goto HookSyntax;` |
|       - | 2232 | `		}` |
|     236 | 2233 | `		if( pGen->pIn->sData.nByte == 3` |
|     241 | 2234 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|     147 | 2235 | `			bGet = 1;` |
|     170 | 2236 | `		}else if( pGen->pIn->sData.nByte == 3` |
|      98 | 2237 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|      98 | 2238 | `			bGet = 0;` |
|      51 | 2239 | `		}else{` |
|     ! 0 | 2240 | `			goto HookSyntax;` |
|       - | 2241 | `		}` |
|     241 | 2242 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|     241 | 2243 | `		sHookName.zString = zHook;` |
|     359 | 2244 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|     118 | 2245 | `			bGet ? "get" : "set",&pAttr->sName);` |
|     241 | 2246 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|       - | 2247 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|       - | 2248 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|       - | 2249 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|       - | 2250 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|       - | 2251 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|      16 | 2252 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|      10 | 2253 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 2254 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|       - | 2255 | `					"Non-abstract property hook must have a body");` |
|     ! 0 | 2256 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2257 | `					return SXERR_ABORT;` |
|       - | 2258 | `				}` |
|     ! 0 | 2259 | `				return SXERR_CORRUPT;` |
|       - | 2260 | `			}` |
|      18 | 2261 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|       - | 2262 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|      18 | 2263 | `			if( pMeth == 0 ){` |
|     ! 0 | 2264 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2265 | `				return SXERR_ABORT;` |
|       - | 2266 | `			}` |
|      18 | 2267 | `			pMeth->sFunc.nLine = nHLine;` |
|      18 | 2268 | `			if( bGet ){` |
|      12 | 2269 | `				GenStateHookGetReturnType(&pMeth->sFunc,pAttr);` |
|       5 | 2270 | `			}` |
|      18 | 2271 | `			if( !bGet ){` |
|       - | 2272 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|       - | 2273 | `				 * compatible with concrete set-hook implementations (which` |
|       - | 2274 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|       - | 2275 | `				 * type (php: the abstract set's parameter type IS the property` |
|       - | 2276 | `				 * type), so the override contravariance check accepts a typed` |
|       - | 2277 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|       - | 2278 | `				ph7_vm_func_arg sVArg;` |
|       7 | 2279 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|       7 | 2280 | `				if( zVName == 0 ){` |
|     ! 0 | 2281 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2282 | `					return SXERR_ABORT;` |
|       - | 2283 | `				}` |
|       7 | 2284 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|       7 | 2285 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|       7 | 2286 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       7 | 2287 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       7 | 2288 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|       7 | 2289 | `				GenStateHookSetSignature(&(*pGen),&pMeth->sFunc,pAttr,&sVArg);` |
|       7 | 2290 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|       3 | 2291 | `			}` |
|      18 | 2292 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|      18 | 2293 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 2294 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2295 | `				return SXERR_ABORT;` |
|       - | 2296 | `			}` |
|      18 | 2297 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|      18 | 2298 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|       - | 2299 | `		}` |
|     220 | 2300 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|     225 | 2301 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|       - | 2302 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|     ! 0 | 2303 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|       - | 2304 | `				"Abstract property hook cannot have body");` |
|     ! 0 | 2305 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2306 | `				return SXERR_ABORT;` |
|       - | 2307 | `			}` |
|     ! 0 | 2308 | `			return SXERR_CORRUPT;` |
|       - | 2309 | `		}` |
|     225 | 2310 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|       - | 2311 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|     225 | 2312 | `		if( pMeth == 0 ){` |
|     ! 0 | 2313 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2314 | `			return SXERR_ABORT;` |
|       - | 2315 | `		}` |
|     225 | 2316 | `		pMeth->sFunc.nLine = nHLine;` |
|     225 | 2317 | `		if( bGet ){` |
|     137 | 2318 | `			GenStateHookGetReturnType(&pMeth->sFunc,pAttr);` |
|      66 | 2319 | `		}` |
|     225 | 2320 | `		if( !bGet ){` |
|       - | 2321 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|      92 | 2322 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|      24 | 2323 | `				SyToken *pRp = 0;` |
|      24 | 2324 | `				pGen->pIn++;` |
|      24 | 2325 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|      24 | 2326 | `				if( pRp >= pGen->pEnd ){` |
|     ! 0 | 2327 | `					goto HookSyntax;` |
|       - | 2328 | `				}` |
|      24 | 2329 | `				if( pGen->pIn < pRp ){` |
|      24 | 2330 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|      24 | 2331 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 2332 | `						return SXERR_ABORT;` |
|       - | 2333 | `					}` |
|      11 | 2334 | `				}` |
|      24 | 2335 | `				pGen->pIn = &pRp[1];` |
|      11 | 2336 | `			}` |
|      92 | 2337 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|       - | 2338 | `				/* Implicit $value formal */` |
|       - | 2339 | `				ph7_vm_func_arg sVArg;` |
|      70 | 2340 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|      70 | 2341 | `				if( zVName == 0 ){` |
|     ! 0 | 2342 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2343 | `					return SXERR_ABORT;` |
|       - | 2344 | `				}` |
|      70 | 2345 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|      70 | 2346 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|      70 | 2347 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      70 | 2348 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|      70 | 2349 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|      70 | 2350 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|      70 | 2351 | `				GenStateHookSetSignature(&(*pGen),&pMeth->sFunc,pAttr,&sVArg);` |
|      70 | 2352 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|      37 | 2353 | `			}else{` |
|       - | 2354 | ``				/* An EXPLICIT `set(T $v)` keeps its own parameter type; only the`` |
|       - | 2355 | `				 * void return is implicit. */` |
|      24 | 2356 | `				GenStateHookSetSignature(&(*pGen),&pMeth->sFunc,pAttr,0);` |
|       - | 2357 | `			}` |
|      44 | 2358 | `		}` |
|     288 | 2359 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 2360 | `			/* Block body */` |
|     131 | 2361 | `			SyToken *pBodyStart = pGen->pIn;` |
|     131 | 2362 | `			SyToken *pCloser = 0;` |
|     131 | 2363 | `			int bParentCall = 0;` |
|     131 | 2364 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|     131 | 2365 | `			if( pCloser < pGen->pEnd ){` |
|       - | 2366 | `				SyToken *pScan;` |
|    1141 | 2367 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|    1019 | 2368 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|       6 | 2369 | `						bParentCall = 1;` |
|       6 | 2370 | `						break;` |
|       - | 2371 | `					}` |
|     510 | 2372 | `				}` |
|      63 | 2373 | `			}` |
|     131 | 2374 | `			if( bParentCall ){` |
|       - | 2375 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|       - | 2376 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|       - | 2377 | `				 * hook method), then continue past the original body. */` |
|       - | 2378 | `				SySet sBody;` |
|       6 | 2379 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|       6 | 2380 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       6 | 2381 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|       6 | 2382 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 2383 | `					SySetRelease(&sBody);` |
|     ! 0 | 2384 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2385 | `					return SXERR_ABORT;` |
|       - | 2386 | `				}` |
|       6 | 2387 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|       6 | 2388 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|       6 | 2389 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|       6 | 2390 | `				pGen->pIn = &pCloser[1];` |
|       6 | 2391 | `				pGen->pEnd = pSavedEnd;` |
|       6 | 2392 | `				SySetRelease(&sBody);` |
|       6 | 2393 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2394 | `					return SXERR_ABORT;` |
|       - | 2395 | `				}` |
|       6 | 2396 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|       4 | 2397 | `			}else{` |
|     127 | 2398 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|     127 | 2399 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2400 | `					return SXERR_ABORT;` |
|       - | 2401 | `				}` |
|     127 | 2402 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|       - | 2403 | `			}` |
|     131 | 2404 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|      25 | 2405 | `				bRefsSelf = 1;` |
|      16 | 2406 | `			}` |
|     209 | 2407 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|       - | 2408 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|       - | 2409 | `			GenBlock *pBlock;` |
|       - | 2410 | `			SySet *pInstrContainer;` |
|       - | 2411 | `			SyToken *pBodyStart;` |
|       - | 2412 | `			SyToken *pExprEnd;` |
|      99 | 2413 | `			SyToken *pSavedEnd = 0;` |
|       - | 2414 | `			SySet sBody;` |
|      99 | 2415 | `			int bParentCall = 0;` |
|      99 | 2416 | `			pGen->pIn++; /* Jump '=>' */` |
|      99 | 2417 | `			pBodyStart = pGen->pIn;` |
|       - | 2418 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|       - | 2419 | `			 * would end the enclosing hook list) and rewrite any` |
|       - | 2420 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|       - | 2421 | `			 * method on a token copy. */` |
|       - | 2422 | `			{` |
|      99 | 2423 | `				sxi32 iNest = 0;` |
|      99 | 2424 | `				pExprEnd = pBodyStart;` |
|     503 | 2425 | `				while( pExprEnd < pGen->pEnd ){` |
|     503 | 2426 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      19 | 2427 | `						iNest++;` |
|     495 | 2428 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      19 | 2429 | `						if( iNest <= 0 ){` |
|     ! 0 | 2430 | `							break;` |
|       - | 2431 | `						}` |
|      19 | 2432 | `						iNest--;` |
|     479 | 2433 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|      99 | 2434 | `						break;` |
|       - | 2435 | `					}` |
|     409 | 2436 | `					pExprEnd++;` |
|       5 | 2437 | `				}` |
|       - | 2438 | `			}` |
|       - | 2439 | `			{` |
|       - | 2440 | `				SyToken *pScan;` |
|     483 | 2441 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|     391 | 2442 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|       3 | 2443 | `						bParentCall = 1;` |
|       3 | 2444 | `						break;` |
|       - | 2445 | `					}` |
|     197 | 2446 | `				}` |
|       - | 2447 | `			}` |
|      99 | 2448 | `			if( bParentCall ){` |
|       3 | 2449 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       3 | 2450 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|       3 | 2451 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 2452 | `					SySetRelease(&sBody);` |
|     ! 0 | 2453 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2454 | `					return SXERR_ABORT;` |
|       - | 2455 | `				}` |
|       3 | 2456 | `				pSavedEnd = pGen->pEnd;` |
|       3 | 2457 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|       3 | 2458 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|       1 | 2459 | `			}` |
|     146 | 2460 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      94 | 2461 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|      99 | 2462 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 2463 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|     ! 0 | 2464 | `				return SXERR_ABORT;` |
|       - | 2465 | `			}` |
|      99 | 2466 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      99 | 2467 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|      99 | 2468 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      99 | 2469 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      99 | 2470 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      99 | 2471 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      99 | 2472 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      99 | 2473 | `			GenStateLeaveBlock(&(*pGen),0);` |
|      99 | 2474 | `			if( bParentCall ){` |
|       3 | 2475 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|       3 | 2476 | `				pGen->pEnd = pSavedEnd;` |
|       3 | 2477 | `				SySetRelease(&sBody);` |
|       1 | 2478 | `			}` |
|      99 | 2479 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2480 | `				return SXERR_ABORT;` |
|       - | 2481 | `			}` |
|      99 | 2482 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|      99 | 2483 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|      47 | 2484 | `				bRefsSelf = 1;` |
|      22 | 2485 | `			}` |
|      99 | 2486 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      99 | 2487 | `				pGen->pIn++; /* Jump ';' */` |
|      47 | 2488 | `			}` |
|      99 | 2489 | `			if( !bGet ){` |
|       - | 2490 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|       - | 2491 | `				 * the dispatcher consumes the implicit return value — which` |
|       - | 2492 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|       - | 2493 | ``				 * for `$this->NAME = expr`). */`` |
|       8 | 2494 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|       8 | 2495 | `				bRefsSelf = 1;` |
|       3 | 2496 | `			}` |
|      52 | 2497 | `		}else{` |
|     ! 0 | 2498 | `			goto HookSyntax;` |
|       - | 2499 | `		}` |
|     225 | 2500 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|     225 | 2501 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2502 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2503 | `			return SXERR_ABORT;` |
|       - | 2504 | `		}` |
|     225 | 2505 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|       5 | 2506 | `	}` |
|     179 | 2507 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|     ! 0 | 2508 | `		goto HookSyntax;` |
|       - | 2509 | `	}` |
|     179 | 2510 | `	pGen->pIn++; /* Jump '}' */` |
|     179 | 2511 | `	if( !bRefsSelf ){` |
|       - | 2512 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|       - | 2513 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|       - | 2514 | `		 * a default value (compile fatal, php's exact wording). */` |
|     111 | 2515 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|     111 | 2516 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|     ! 0 | 2517 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 2518 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|     ! 0 | 2519 | `				&pClass->sName,&pAttr->sName);` |
|     ! 0 | 2520 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2521 | `				return SXERR_ABORT;` |
|       - | 2522 | `			}` |
|     ! 0 | 2523 | `			return SXERR_CORRUPT;` |
|       - | 2524 | `		}` |
|      53 | 2525 | `	}` |
|     179 | 2526 | `	return SXRET_OK;` |
|     ! 0 | 2527 | `HookSyntax:` |
|     ! 0 | 2528 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 2529 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|     ! 0 | 2530 | `		&pClass->sName,&pAttr->sName);` |
|     ! 0 | 2531 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2532 | `		return SXERR_ABORT;` |
|       - | 2533 | `	}` |
|     ! 0 | 2534 | `	return SXERR_CORRUPT;` |
|      92 | 2535 | `}` |
|       - | 2536 | `/* php's #[\Override] verification, defined with the rest of the class-link` |
|       - | 2537 | ` * checks below; both compilers (class and interface) drive the same pair. */` |
|       - | 2538 | `static sxi32 GenStateCollectOverrides(ph7_gen_state *pGen,ph7_class *pClass,` |
|       - | 2539 | `	SySet *pMeths,SySet *pProps);` |
|       - | 2540 | `static sxi32 GenStateCheckOverrides(ph7_gen_state *pGen,ph7_class *pClass,` |
|       - | 2541 | `	SySet *pMeths,SySet *pProps);` |
|       - | 2542 | `/*` |
|       - | 2543 | ` * Compile an object interface.` |
|       - | 2544 | ` *  According to the PHP language reference manual` |
|       - | 2545 | ` *   Object Interfaces:` |
|       - | 2546 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - | 2547 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - | 2548 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - | 2549 | ` *   class, but without any of the methods having their contents defined.` |
|       - | 2550 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - | 2551 | ` */` |
|     214 | 2552 | `PH7_PRIVATE sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 | 2553 | `{` |
|     219 | 2554 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 2555 | `	ph7_class *pClass,*pBase;` |
|     219 | 2556 | `	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' */` |
|       - | 2557 | `	SyToken *pEnd,*pTmp;` |
|       - | 2558 | `	SyString *pName;` |
|       - | 2559 | `	SySet aOvMeth,aOvProp;   /* the #[\Override] claims this interface DECLARED */` |
|     219 | 2560 | `	sxu32 nErrEntry = pGen->nErr; /* errors already reported when this one started */` |
|       - | 2561 | `	sxi32 nKwrd;` |
|       - | 2562 | `	sxi32 rc;` |
|       - | 2563 | `	{` |
|       - | 2564 | `		/* Deferral gate: parent interfaces may need an autoloader` |
|       - | 2565 | `		 * that has not run yet. */` |
|       - | 2566 | `		sxi32 rcDefer;` |
|     219 | 2567 | `		if( GenStateMaybeDeferClass(pGen,0,PH7_DEFER_KIND_INTERFACE,&rcDefer) ){` |
|       3 | 2568 | `			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 2569 | `		}` |
|       - | 2570 | `	}` |
|       - | 2571 | `	/* Jump the 'interface' keyword */` |
|     217 | 2572 | `	pGen->pIn++;` |
|       - | 2573 | `	/* Extract interface name */` |
|     217 | 2574 | `	pName = &pGen->pIn->sData;` |
|       - | 2575 | `	/* Advance the stream cursor */` |
|     217 | 2576 | `	pGen->pIn++;` |
|       - | 2577 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 2578 | `		SyBlob sFQN;` |
|       - | 2579 | `		SyString sFQNStr;` |
|     217 | 2580 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     217 | 2581 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     217 | 2582 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       - | 2583 | ``		/* php refuses a declaration whose short name a local `use` already took. */`` |
|     217 | 2584 | `		if( GenStateGuardImportRedeclare(pGen,0,pName,&sFQNStr,nLine) == SXERR_ABORT ){` |
|     ! 0 | 2585 | `			SyBlobRelease(&sFQN);` |
|     ! 0 | 2586 | `			return SXERR_ABORT;` |
|       - | 2587 | `		}` |
|     217 | 2588 | `		GenStateRecordDeclaredName(pGen,0,&sFQNStr);` |
|     217 | 2589 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     217 | 2590 | `		SyBlobRelease(&sFQN);` |
|       - | 2591 | `	}` |
|     217 | 2592 | `	if( pClass == 0 ){` |
|     ! 0 | 2593 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2594 | `		return SXERR_ABORT;` |
|       - | 2595 | `	}` |
|     217 | 2596 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     217 | 2597 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|     ! 0 | 2598 | `		return SXERR_ABORT;` |
|       - | 2599 | `	}` |
|       - | 2600 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|     217 | 2601 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|     318 | 2602 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pClass->aAttrs,1,1,` |
|     323 | 2603 | `			&pClass->sName,pClass->iFlags) == SXERR_ABORT ){` |
|     ! 0 | 2604 | `		return SXERR_ABORT;` |
|       - | 2605 | `	}` |
|       - | 2606 | `	/* Assume no base class is given */` |
|     217 | 2607 | `	pBase = 0;` |
|     217 | 2608 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      28 | 2609 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      28 | 2610 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a, c, … */ ){` |
|       - | 2611 | `			/* php lets an interface extend SEVERAL parent interfaces. The first` |
|       - | 2612 | `			 * becomes pBase (single-inheritance chain, hDerived); every extra one` |
|       - | 2613 | `			 * is recorded via PH7_ClassImplement so it lands in aInterface — the` |
|       - | 2614 | `			 * runtime subtype walk (VmInterfaceReaches) follows both. */` |
|      28 | 2615 | `			pGen->pIn++;` |
|      13 | 2616 | `			for(;;){` |
|       - | 2617 | `				SyBlob sResolved;` |
|       - | 2618 | `				SyString sBaseName;` |
|       - | 2619 | `				sxu32 nRefLine;` |
|       - | 2620 | `				ph7_class *pParent;` |
|      30 | 2621 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|      30 | 2622 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      30 | 2623 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 2624 | `					SyBlobRelease(&sResolved);` |
|     ! 0 | 2625 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 2626 | `						"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 | 2627 | `						pName);` |
|     ! 0 | 2628 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 2629 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 2630 | `						return SXERR_ABORT;` |
|       - | 2631 | `					}` |
|     ! 0 | 2632 | `					return SXRET_OK;` |
|       - | 2633 | `				}` |
|      43 | 2634 | `				pParent = PH7_VmExtractClass(pGen->pVm,` |
|      26 | 2635 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      30 | 2636 | `				SyStringInitFromBuf(&sBaseName,` |
|       - | 2637 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - | 2638 | `				/* Only interfaces is allowed */` |
|      30 | 2639 | `				while( pParent && (pParent->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 2640 | `					pParent = pParent->pNextName;` |
|     ! 0 | 2641 | `				}` |
|      30 | 2642 | `				if( pParent == 0 ){` |
|     ! 0 | 2643 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - | 2644 | `						"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 | 2645 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 2646 | `						SyBlobRelease(&sResolved);` |
|     ! 0 | 2647 | `						return SXERR_ABORT;` |
|     ! 0 | 2648 | `					}` |
|      30 | 2649 | `				}else if( pBase == 0 ){` |
|       - | 2650 | `					/* First parent → single-inheritance base */` |
|      28 | 2651 | `					pBase = pParent;` |
|      16 | 2652 | `				}else{` |
|       - | 2653 | `					/* Additional parent → record it in aInterface (+ copy its` |
|       - | 2654 | `					 * constants/method stubs) so instanceof reaches it too. */` |
|       3 | 2655 | `					PH7_ClassImplement(pClass,pParent);` |
|       - | 2656 | `				}` |
|      30 | 2657 | `				SyBlobRelease(&sResolved);` |
|       - | 2658 | `				/* Continue on a comma-separated list */` |
|      30 | 2659 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 2660 | `					pGen->pIn++;` |
|       3 | 2661 | `					continue;` |
|       - | 2662 | `				}` |
|      28 | 2663 | `				break;` |
|     ! 0 | 2664 | `			}` |
|      12 | 2665 | `		}` |
|      12 | 2666 | `	}` |
|     217 | 2667 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 2668 | `		/* Syntax error */` |
|     ! 0 | 2669 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 | 2670 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 2671 | `		if( rc == SXERR_ABORT ){` |
|       - | 2672 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2673 | `			return SXERR_ABORT;` |
|       - | 2674 | `		}` |
|     ! 0 | 2675 | `		return SXRET_OK;` |
|       - | 2676 | `	}` |
|     217 | 2677 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     217 | 2678 | `	pEnd = 0; /* cc warning */` |
|       - | 2679 | `	/* Delimit the interface body */` |
|     217 | 2680 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|     217 | 2681 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 2682 | `		/* Syntax error */` |
|     ! 0 | 2683 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 | 2684 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 2685 | `		if( rc == SXERR_ABORT ){` |
|       - | 2686 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2687 | `			return SXERR_ABORT;` |
|       - | 2688 | `		}` |
|     ! 0 | 2689 | `		return SXRET_OK;` |
|       - | 2690 | `	}` |
|       - | 2691 | `	/* The delimiter token is the interface body's closing brace */` |
|     217 | 2692 | `	pClass->nEndLine = pEnd->nLine;` |
|       - | 2693 | `	/* Swap token stream */` |
|     217 | 2694 | `	pTmp = pGen->pEnd;` |
|     217 | 2695 | `	pGen->pEnd = pEnd;` |
|       - | 2696 | `	/* This interface is now the lexical class for its body (see pCurClass) — a` |
|       - | 2697 | `	 * const default here is not a trait, so __TRAIT__ stays "". */` |
|     217 | 2698 | `	pGen->pCurClass = pClass;` |
|       - | 2699 | `	/* Start the parse process` |
|       - | 2700 | `	 * Note (According to the PHP reference manual):` |
|       - | 2701 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - | 2702 | `	 *  Only 'public' visibility is allowed.` |
|       - | 2703 | `	 */` |
|     158 | 2704 | `	for(;;){` |
|       - | 2705 | `		/* Jump leading/trailing semi-colons */` |
|     429 | 2706 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|     109 | 2707 | `			pGen->pIn++;` |
|       5 | 2708 | `		}` |
|     325 | 2709 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 2710 | `			/* End of interface body */` |
|     213 | 2711 | `			break;` |
|       - | 2712 | `		}` |
|       - | 2713 | `		/* Bind a directly-preceding docblock to this member */` |
|     117 | 2714 | `		GenStateSetPendingDoc(&(*pGen));` |
|     117 | 2715 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 2716 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 2717 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 | 2718 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 2719 | `			if( rc == SXERR_ABORT ){` |
|       - | 2720 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2721 | `				return SXERR_ABORT;` |
|       - | 2722 | `			}` |
|     ! 0 | 2723 | `			goto done;` |
|       - | 2724 | `		}` |
|       - | 2725 | `		/* Extract the current keyword */` |
|     117 | 2726 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     117 | 2727 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 2728 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - | 2729 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 | 2730 | `			const char *zKind = "member";` |
|       3 | 2731 | `			SyString *pMemberName = 0;` |
|       3 | 2732 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 | 2733 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 | 2734 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 | 2735 | `					zKind = "constant";` |
|       3 | 2736 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 | 2737 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 | 2738 | `					}` |
|       1 | 2739 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 2740 | `					zKind = "method";` |
|     ! 0 | 2741 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 | 2742 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 | 2743 | `					}` |
|     ! 0 | 2744 | `				}` |
|       1 | 2745 | `			}` |
|       3 | 2746 | `			if( pMemberName ){` |
|       4 | 2747 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 | 2748 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 | 2749 | `			}else{` |
|     ! 0 | 2750 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 2751 | `					"Access type for interface %s must be public",zKind);` |
|       - | 2752 | `			}` |
|       3 | 2753 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2754 | `				return SXERR_ABORT;` |
|       - | 2755 | `			}` |
|       3 | 2756 | `			goto done;` |
|       - | 2757 | `		}` |
|     115 | 2758 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 2759 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 2760 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 2761 | `			if( rc == SXERR_ABORT ){` |
|       - | 2762 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2763 | `				return SXERR_ABORT;` |
|       - | 2764 | `			}` |
|     ! 0 | 2765 | `			goto done;` |
|       - | 2766 | `		}` |
|     115 | 2767 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - | 2768 | `			/* Advance the stream cursor */` |
|      87 | 2769 | `			pGen->pIn++;` |
|      82 | 2770 | `			if( pGen->pIn < pGen->pEnd` |
|      87 | 2771 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|      82 | 2772 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|       - | 2773 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|       - | 2774 | `				 * requirement. The attribute compiler + hook parser handle it` |
|       - | 2775 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|       - | 2776 | `				 * property without hooks is ITS "Interfaces may only include` |
|       - | 2777 | `				 * hooked properties" error). */` |
|     ! 0 | 2778 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|     ! 0 | 2779 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|     ! 0 | 2780 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 2781 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 2782 | `						return SXERR_ABORT;` |
|       - | 2783 | `					}` |
|     ! 0 | 2784 | `					goto done;` |
|       - | 2785 | `				}` |
|     ! 0 | 2786 | `				continue;` |
|       - | 2787 | `			}` |
|      87 | 2788 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       - | 2789 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|       - | 2790 | `				 * '$' also opens a hooked-property requirement. */` |
|     ! 0 | 2791 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|     ! 0 | 2792 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|     ! 0 | 2793 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|     ! 0 | 2794 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|     ! 0 | 2795 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|     ! 0 | 2796 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 2797 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 2798 | `							return SXERR_ABORT;` |
|       - | 2799 | `						}` |
|     ! 0 | 2800 | `						goto done;` |
|       - | 2801 | `					}` |
|     ! 0 | 2802 | `					continue;` |
|       - | 2803 | `				}` |
|     ! 0 | 2804 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 2805 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 2806 | `				if( rc == SXERR_ABORT ){` |
|       - | 2807 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 2808 | `					return SXERR_ABORT;` |
|       - | 2809 | `				}` |
|     ! 0 | 2810 | `				goto done;` |
|       - | 2811 | `			}` |
|      87 | 2812 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      87 | 2813 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|       - | 2814 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|       - | 2815 | `				 * hooked-property requirement (PHP 8.4). */` |
|       4 | 2816 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|       5 | 2817 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|       7 | 2818 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       2 | 2819 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       5 | 2820 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 2821 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 2822 | `							return SXERR_ABORT;` |
|       - | 2823 | `						}` |
|     ! 0 | 2824 | `						goto done;` |
|       - | 2825 | `					}` |
|       5 | 2826 | `					continue;` |
|       - | 2827 | `				}` |
|     ! 0 | 2828 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 2829 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 2830 | `				if( rc == SXERR_ABORT ){` |
|       - | 2831 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 2832 | `					return SXERR_ABORT;` |
|       - | 2833 | `				}` |
|     ! 0 | 2834 | `				goto done;` |
|       - | 2835 | `			}` |
|      39 | 2836 | `		}` |
|     111 | 2837 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 2838 | `			/* Parse constant */` |
|      28 | 2839 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|      28 | 2840 | `			if( rc != SXRET_OK ){` |
|       3 | 2841 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2842 | `					return SXERR_ABORT;` |
|       - | 2843 | `				}` |
|       3 | 2844 | `				goto done;` |
|       - | 2845 | `			}` |
|      14 | 2846 | `		}else{` |
|      87 | 2847 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|      87 | 2848 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 2849 | `				/* Static method,record that */` |
|     ! 0 | 2850 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - | 2851 | `				/* Advance the stream cursor */` |
|     ! 0 | 2852 | `				pGen->pIn++;` |
|     ! 0 | 2853 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 | 2854 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 2855 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 2856 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 2857 | `						if( rc == SXERR_ABORT ){` |
|       - | 2858 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 2859 | `							return SXERR_ABORT;` |
|       - | 2860 | `						}` |
|     ! 0 | 2861 | `						goto done;` |
|       - | 2862 | `				}` |
|     ! 0 | 2863 | `			}` |
|       - | 2864 | `			/* Process method signature (no body for interface methods) */` |
|      87 | 2865 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|      87 | 2866 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 2867 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2868 | `					return SXERR_ABORT;` |
|       - | 2869 | `				}` |
|     ! 0 | 2870 | `				goto done;` |
|       - | 2871 | `			}` |
|       - | 2872 | `		}` |
|       5 | 2873 | `	}` |
|       - | 2874 | `	/* An interface method may claim #[\Override] too, against the interfaces this` |
|       - | 2875 | `	 * one extends -- collected before the inherit copies theirs in. */` |
|     213 | 2876 | `	GenStateCollectOverrides(&(*pGen),pClass,&aOvMeth,&aOvProp);` |
|       - | 2877 | `	/* Reject a php-fatal redeclaration before hoisting the interface */` |
|     213 | 2878 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|       3 | 2879 | `		SySetRelease(&aOvMeth);` |
|       3 | 2880 | `		SySetRelease(&aOvProp);` |
|       3 | 2881 | `		return SXERR_ABORT;` |
|       - | 2882 | `	}` |
|       - | 2883 | `	/* Install the interface */` |
|     211 | 2884 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     211 | 2885 | `	if( rc == SXRET_OK && pBase ){` |
|       - | 2886 | `		/* Inherit from the base interface */` |
|      28 | 2887 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|      12 | 2888 | `	}` |
|     206 | 2889 | `	if( rc == SXRET_OK && pGen->nErr == nErrEntry` |
|     208 | 2890 | `	 && GenStateCheckOverrides(&(*pGen),pClass,&aOvMeth,&aOvProp) == SXERR_ABORT ){` |
|     ! 0 | 2891 | `		SySetRelease(&aOvMeth);` |
|     ! 0 | 2892 | `		SySetRelease(&aOvProp);` |
|     ! 0 | 2893 | `		return SXERR_ABORT;` |
|       - | 2894 | `	}` |
|     211 | 2895 | `	SySetRelease(&aOvMeth);` |
|     211 | 2896 | `	SySetRelease(&aOvProp);` |
|     211 | 2897 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2898 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2899 | `		return SXERR_ABORT;` |
|       - | 2900 | `	}` |
|     103 | 2901 | `done:` |
|     215 | 2902 | `	pGen->pCurClass = pSavedCurClass;` |
|       - | 2903 | `	/* Point beyond the interface body */` |
|     215 | 2904 | `	pGen->pIn  = &pEnd[1];` |
|     215 | 2905 | `	pGen->pEnd = pTmp;` |
|     215 | 2906 | `	return PH7_OK;` |
|     112 | 2907 | `}` |
|       - | 2908 | `/*` |
|       - | 2909 | ` * Compile a user-defined class.` |
|       - | 2910 | ` * According to the PHP language reference manual` |
|       - | 2911 | ` *  class` |
|       - | 2912 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - | 2913 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - | 2914 | ` *  of the properties and methods belonging to the class.` |
|       - | 2915 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - | 2916 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - | 2917 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - | 2918 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 2919 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - | 2920 | ` *  (called "methods").` |
|       - | 2921 | ` */` |
|       - | 2922 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - | 2923 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - | 2924 | `struct TraitUseEntry {` |
|       - | 2925 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - | 2926 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - | 2927 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - | 2928 | `};` |
|       - | 2929 | `/*` |
|       - | 2930 | ` * Validate that methods implementing interface contracts have compatible` |
|       - | 2931 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - | 2932 | ` */` |
|    4004 | 2933 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 | 2934 | `{` |
|       - | 2935 | `	ph7_class **apIface;` |
|       - | 2936 | `	sxu32 nIface,i;` |
|       - | 2937 | `	sxi32 rc;` |
|    4009 | 2938 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 | 2939 | `		return SXRET_OK;` |
|       - | 2940 | `	}` |
|    4009 | 2941 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    4009 | 2942 | `	nIface = SySetUsed(&pClass->aInterface);` |
|    4795 | 2943 | `	for(i = 0; i < nIface; i++){` |
|     791 | 2944 | `		ph7_class *pIface = apIface[i];` |
|       - | 2945 | `		SyHashEntry *pEntry;` |
|     791 | 2946 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|    2031 | 2947 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|    1245 | 2948 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - | 2949 | `			ph7_class_method *pImplMeth;` |
|    1245 | 2950 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - | 2951 | `			/* Find the implementing method in the class */` |
|    1245 | 2952 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|    1245 | 2953 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      25 | 2954 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - | 2955 | `			}` |
|       - | 2956 | `			/* Check visibility: interface methods must be implemented as public */` |
|    1225 | 2957 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 | 2958 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 2959 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 | 2960 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 | 2961 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2962 | `					return SXERR_ABORT;` |
|       - | 2963 | `				}` |
|       1 | 2964 | `			}` |
|       - | 2965 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - | 2966 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - | 2967 | `			 */` |
|       - | 2968 | `			{` |
|    1225 | 2969 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|    1225 | 2970 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|    1225 | 2971 | `				int sigError = 0;` |
|    1225 | 2972 | `				if( ((pIfaceMeth->sFunc.iFlags \| pImplMeth->sFunc.iFlags) & VM_FUNC_NATIVE) != 0 ){` |
|       - | 2973 | `					/* A NATIVE method's parameters live in its zSig string, not in` |
|       - | 2974 | `					 * compiled aArgs records, so there is nothing to count here --` |
|       - | 2975 | `					 * and an engine-declared signature is compatible by` |
|       - | 2976 | `					 * construction. Counting its empty aArgs as "no parameters" is` |
|       - | 2977 | `					 * what made an enum's own from()/tryFrom() incompatible with` |
|       - | 2978 | `					 * BackedEnum the moment they became native. */` |
|    1171 | 2979 | `					sigError = 0;` |
|     642 | 2980 | `				}else if( nImplArgs < nIfaceArgs ){` |
|       3 | 2981 | `					sigError = 1;` |
|      58 | 2982 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - | 2983 | `					/* Extra parameters must all have default values */` |
|       6 | 2984 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - | 2985 | `					sxu32 k;` |
|       8 | 2986 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       6 | 2987 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 | 2988 | `							sigError = 1;` |
|       3 | 2989 | `							break;` |
|       - | 2990 | `						}` |
|       2 | 2991 | `					}` |
|       2 | 2992 | `				}` |
|    1225 | 2993 | `				if( sigError ){` |
|       - | 2994 | `					SyBlob sImplSig, sIfaceSig;` |
|       - | 2995 | `					ph7_vm_func_arg *aArgs;` |
|       - | 2996 | `					sxu32 j;` |
|       6 | 2997 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       6 | 2998 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - | 2999 | `					/* Build implementing method signature */` |
|       6 | 3000 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      12 | 3001 | `					for(j = 0; j < nImplArgs; j++){` |
|       8 | 3002 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       8 | 3003 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       8 | 3004 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 | 3005 | `					}` |
|       - | 3006 | `					/* Build interface method signature */` |
|       6 | 3007 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      12 | 3008 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       8 | 3009 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       8 | 3010 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       8 | 3011 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 | 3012 | `					}` |
|       8 | 3013 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 3014 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 | 3015 | `						&pClass->sName,pMName,` |
|       4 | 3016 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 | 3017 | `						&pIface->sName,pMName,` |
|       4 | 3018 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       6 | 3019 | `					SyBlobRelease(&sImplSig);` |
|       6 | 3020 | `					SyBlobRelease(&sIfaceSig);` |
|       6 | 3021 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 3022 | `						return SXERR_ABORT;` |
|       - | 3023 | `					}` |
|       2 | 3024 | `				}` |
|       - | 3025 | `			}` |
|       5 | 3026 | `		}` |
|     398 | 3027 | `	}` |
|    4009 | 3028 | `	return SXRET_OK;` |
|    2007 | 3029 | `}` |
|       - | 3030 | `/*` |
|       - | 3031 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|       - | 3032 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|       - | 3033 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|       - | 3034 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|       - | 3035 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|       - | 3036 | ` * means that specific hook is still missing.` |
|       - | 3037 | ` */` |
|      42 | 3038 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|       5 | 3039 | `{` |
|       - | 3040 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|       - | 3041 | `	ph7_class_attr *pProp;` |
|      42 | 3042 | `	if( pMName->nByte <= nPfx` |
|      29 | 3043 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|       4 | 3044 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|      40 | 3045 | `		return 0; /* not a hook stub */` |
|       - | 3046 | `	}` |
|       7 | 3047 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|       7 | 3048 | `	return pProp != 0` |
|       6 | 3049 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|       3 | 3050 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|      26 | 3051 | `}` |
|       - | 3052 | `/*` |
|       - | 3053 | ` * Append an abstract member's display name to the message blob, translating a` |
|       - | 3054 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|       - | 3055 | ` */` |
|      18 | 3056 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|       4 | 3057 | `{` |
|       - | 3058 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|      18 | 3059 | `	if( pMName->nByte > nPfx` |
|      13 | 3060 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|     ! 0 | 3061 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|     ! 0 | 3062 | `		SyBlobAppend(pMsg,"$",1);` |
|     ! 0 | 3063 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|     ! 0 | 3064 | `		SyBlobAppend(pMsg,"::",2);` |
|     ! 0 | 3065 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|     ! 0 | 3066 | `		return;` |
|       - | 3067 | `	}` |
|      22 | 3068 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|      13 | 3069 | `}` |
|       - | 3070 | `/*` |
|       - | 3071 | ` * ---------------------------------------------------------------------------` |
|       - | 3072 | `` * php's `#[\Override]` (8.3, widened to properties in 8.5).`` |
|       - | 3073 | ` *` |
|       - | 3074 | ` * The attribute is a CLAIM the engine checks where the member is written: the` |
|       - | 3075 | ` * name must already exist above, so a typo, a renamed parent method or a base` |
|       - | 3076 | ` * class that dropped one is a fatal at the declaration instead of a method` |
|       - | 3077 | ` * nobody ever calls. php runs it at class LINK time, after inheritance, and its` |
|       - | 3078 | ` * rules are the inheritance rules rather than a name search:` |
|       - | 3079 | ` *` |
|       - | 3080 | ` *   - a PRIVATE parent member is not inherited, so it is not something to` |
|       - | 3081 | ` *     override;` |
|       - | 3082 | ` *   - the CONSTRUCTOR is exempt from php's inheritance signature check unless it` |
|       - | 3083 | ` *     is abstract (or an interface's), and #[\Override] follows that exemption —` |
|       - | 3084 | `` *     a concrete parent `__construct` does NOT satisfy the claim while an`` |
|       - | 3085 | ` *     abstract one does;` |
|       - | 3086 | ` *   - a method matches CASE-INSENSITIVELY and a property case-SENSITIVELY, which` |
|       - | 3087 | ` *     is php's rule for the two namespaces everywhere else;` |
|       - | 3088 | ` *   - an interface counts for a method, at any depth and through any ancestor;` |
|       - | 3089 | ` *   - a TRAIT used by this very class does not: its method is the using class's` |
|       - | 3090 | ` *     own, and php reports the USING class's name when the claim fails.` |
|       - | 3091 | ` * ---------------------------------------------------------------------------` |
|       - | 3092 | ` */` |
|       - | 3093 | `#define GEN_OVERRIDE_ATTR "Override"` |
|       - | 3094 | ``/* Does this member carry `#[\Override]`? The compiler resolves an attribute name`` |
|       - | 3095 | ` * to its fully-qualified spelling and class names are case-insensitive, so one` |
|       - | 3096 | ` * case-folded compare against the whole set is the test. */` |
|    6610 | 3097 | `static int GenStateHasOverrideAttr(SySet *pAttrs)` |
|       5 | 3098 | `{` |
|    6615 | 3099 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|       - | 3100 | `	sxu32 n;` |
|    6745 | 3101 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; ++n ){` |
|     208 | 3102 | `		if( aAttr[n].sName.nByte == sizeof(GEN_OVERRIDE_ATTR)-1` |
|     148 | 3103 | `		 && SyStrnicmp(aAttr[n].sName.zString,GEN_OVERRIDE_ATTR,` |
|      39 | 3104 | `			sizeof(GEN_OVERRIDE_ATTR)-1) == 0 ){` |
|      80 | 3105 | `			return 1;` |
|       - | 3106 | `		}` |
|      70 | 3107 | `	}` |
|    6537 | 3108 | `	return 0;` |
|    3310 | 3109 | `}` |
|       - | 3110 | `/* Does an interface reachable from pClass -- its own, or any ancestor's --` |
|       - | 3111 | ` * declare this method? An interface that extends others already carries their` |
|       - | 3112 | ` * stubs in its own table, so one level of lookup per interface is enough. */` |
|      30 | 3113 | `static int GenStateIfaceDeclaresMethod(ph7_class *pClass,const SyString *pName)` |
|       1 | 3114 | `{` |
|      67 | 3115 | `	for( ; pClass ; pClass = pClass->pBase ){` |
|      45 | 3116 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|       - | 3117 | `		sxu32 n;` |
|      47 | 3118 | `		for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; ++n ){` |
|      10 | 3119 | `			if( apIface[n]` |
|      11 | 3120 | `			 && PH7_ClassExtractMethod(apIface[n],pName->zString,pName->nByte) ){` |
|       9 | 3121 | `				return 1;` |
|       - | 3122 | `			}` |
|       2 | 3123 | `		}` |
|      19 | 3124 | `	}` |
|      23 | 3125 | `	return 0;` |
|      16 | 3126 | `}` |
|       - | 3127 | `/* Is there a parent METHOD this one may claim to override? */` |
|      50 | 3128 | `static int GenStateOverridesMethod(ph7_class *pClass,const SyString *pName)` |
|       1 | 3129 | `{` |
|      79 | 3130 | `	int bCtor = pName->nByte == sizeof("__construct")-1` |
|      50 | 3131 | `		&& SyStrnicmp(pName->zString,"__construct",sizeof("__construct")-1) == 0;` |
|       - | 3132 | `	ph7_class *pWalk;` |
|      65 | 3133 | `	for( pWalk = pClass->pBase ; pWalk ; pWalk = pWalk->pBase ){` |
|      35 | 3134 | `		ph7_class_method *pMeth = PH7_ClassExtractMethod(pWalk,pName->zString,pName->nByte);` |
|      35 | 3135 | `		if( pMeth == 0 \|\| pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|      11 | 3136 | `			continue;` |
|       - | 3137 | `		}` |
|      25 | 3138 | `		if( bCtor && (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       5 | 3139 | `			continue;   /* php exempts a concrete parent constructor */` |
|       - | 3140 | `		}` |
|      21 | 3141 | `		return 1;` |
|     ! 0 | 3142 | `	}` |
|      31 | 3143 | `	return GenStateIfaceDeclaresMethod(pClass,pName);` |
|      26 | 3144 | `}` |
|       - | 3145 | `/* Is there a parent PROPERTY this one may claim to override? Interfaces declare` |
|       - | 3146 | ` * none, so this is the base chain alone. */` |
|      22 | 3147 | `static int GenStateOverridesProp(ph7_class *pClass,const SyString *pName)` |
|       2 | 3148 | `{` |
|       - | 3149 | `	ph7_class *pWalk;` |
|      32 | 3150 | `	for( pWalk = pClass->pBase ; pWalk ; pWalk = pWalk->pBase ){` |
|      22 | 3151 | `		ph7_class_attr *pAttr = PH7_ClassExtractAttribute(pWalk,pName->zString,pName->nByte);` |
|      22 | 3152 | `		if( pAttr && pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|      14 | 3153 | `			return 1;` |
|       - | 3154 | `		}` |
|       5 | 3155 | `	}` |
|      11 | 3156 | `	return 0;` |
|      13 | 3157 | `}` |
|       - | 3158 | `/*` |
|       - | 3159 | `` * Verify every `#[\Override]` the class DECLARED, in php's order: the methods`` |
|       - | 3160 | ` * first and then the properties, each in declaration order, and the first` |
|       - | 3161 | ` * failure is the whole diagnostic (it is a fatal).` |
|       - | 3162 | ` *` |
|       - | 3163 | ` * The two sets are collected BEFORE inheritance runs -- an inherited method` |
|       - | 3164 | ` * keeps the parent's attribute record and php does not re-check it there -- and` |
|       - | 3165 | ` * verified after, which is when the answer exists.` |
|       - | 3166 | ` */` |
|    4218 | 3167 | `static sxi32 GenStateCollectOverrides(ph7_gen_state *pGen,ph7_class *pClass,` |
|       - | 3168 | `	SySet *pMeths,SySet *pProps)` |
|       5 | 3169 | `{` |
|       - | 3170 | `	static const sxu32 nHookPfx = sizeof("__phl_hook_get_")-1;` |
|       - | 3171 | `	SyHashEntry *pEntry;` |
|    4223 | 3172 | `	SySetInit(pMeths,&pGen->pVm->sAllocator,sizeof(ph7_class_method *));` |
|    4223 | 3173 | `	SySetInit(pProps,&pGen->pVm->sAllocator,sizeof(ph7_class_attr *));` |
|    4223 | 3174 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|    8641 | 3175 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|    4423 | 3176 | `		ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    4423 | 3177 | `		SyString *pName = &pMeth->sFunc.sName;` |
|       - | 3178 | `		/* A property hook is compiled to a method here and is a PROPERTY in php,` |
|       - | 3179 | `		 * so the property arm below owns its claim. */` |
|    4418 | 3180 | `		if( pName->nByte > nHookPfx` |
|    2355 | 3181 | `		 && SyMemcmp((const void *)pName->zString,(const void *)"__phl_hook_",` |
|     141 | 3182 | `			sizeof("__phl_hook_")-1) == 0 ){` |
|     241 | 3183 | `			continue;` |
|       - | 3184 | `		}` |
|    4187 | 3185 | `		if( GenStateHasOverrideAttr(&pMeth->sFunc.aAttrs) ){` |
|      55 | 3186 | `			SySetPut(pMeths,(const void *)&pMeth);` |
|      27 | 3187 | `		}` |
|       5 | 3188 | `	}` |
|    4223 | 3189 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    6651 | 3190 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|    2433 | 3191 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|    2433 | 3192 | `		if( GenStateHasOverrideAttr(&pAttr->aAttrs) ){` |
|      26 | 3193 | `			SySetPut(pProps,(const void *)&pAttr);` |
|      12 | 3194 | `		}` |
|       5 | 3195 | `	}` |
|    4223 | 3196 | `	return SXRET_OK;` |
|       5 | 3197 | `}` |
|    4038 | 3198 | `static sxi32 GenStateCheckOverrides(ph7_gen_state *pGen,ph7_class *pClass,` |
|       - | 3199 | `	SySet *pMeths,SySet *pProps)` |
|       5 | 3200 | `{` |
|    4043 | 3201 | `	ph7_class_method **apMeth = (ph7_class_method **)SySetBasePtr(pMeths);` |
|    4043 | 3202 | `	ph7_class_attr **apProp = (ph7_class_attr **)SySetBasePtr(pProps);` |
|       - | 3203 | `	sxu32 n;` |
|       - | 3204 | `	/* hMethod is a LIFO iteration list (SyHashInsert), so the collected order is` |
|       - | 3205 | `	 * the REVERSE of the declaration order; hAttr is a FIFO one` |
|       - | 3206 | `	 * (SyHashInsertTail) and needs no such turn. php reports the first member it` |
|       - | 3207 | `	 * finds in declaration order and stops. */` |
|    4071 | 3208 | `	for( n = SySetUsed(pMeths) ; n > 0 ; --n ){` |
|      51 | 3209 | `		ph7_class_method *pMeth = apMeth[n - 1];` |
|      51 | 3210 | `		if( !GenStateOverridesMethod(pClass,&pMeth->sFunc.sName) ){` |
|      34 | 3211 | `			return PH7_GenCompileError(&(*pGen),E_ERROR,pMeth->sFunc.nLine,` |
|       - | 3212 | `				"%z::%z() has #[\\Override] attribute, but no matching parent method exists",` |
|      11 | 3213 | `				&pClass->sName,&pMeth->sFunc.sName);` |
|       - | 3214 | `		}` |
|      15 | 3215 | `	}` |
|    4033 | 3216 | `	for( n = 0 ; n < SySetUsed(pProps) ; ++n ){` |
|      24 | 3217 | `		if( !GenStateOverridesProp(pClass,&apProp[n]->sName) ){` |
|       - | 3218 | `			/* php reports the CLASS's line for a property, not the property's. */` |
|      16 | 3219 | `			return PH7_GenCompileError(&(*pGen),E_ERROR,pClass->nLine,` |
|       - | 3220 | `				"%z::$%z has #[\\Override] attribute, but no matching parent property exists",` |
|      10 | 3221 | `				&pClass->sName,&apProp[n]->sName);` |
|       - | 3222 | `		}` |
|       8 | 3223 | `	}` |
|    4011 | 3224 | `	return SXRET_OK;` |
|    2024 | 3225 | `}` |
|       - | 3226 | `/*` |
|       - | 3227 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - | 3228 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - | 3229 | ` */` |
|    4004 | 3230 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 | 3231 | `{` |
|       - | 3232 | `	ph7_class_method *pMeth;` |
|       - | 3233 | `	SyHashEntry *pEntry;` |
|       - | 3234 | `	sxu32 nAbstract;` |
|       - | 3235 | `	SyBlob sMsg;` |
|       - | 3236 | `	sxi32 rc;` |
|       - | 3237 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|    4009 | 3238 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     103 | 3239 | `		return SXRET_OK;` |
|       - | 3240 | `	}` |
|       - | 3241 | `	/* Count abstract methods */` |
|    3911 | 3242 | `	nAbstract = 0;` |
|    3911 | 3243 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   17690 | 3244 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   11831 | 3245 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   11831 | 3246 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      29 | 3247 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|       7 | 3248 | `				continue; /* hook requirement met by a plain property (php) */` |
|       - | 3249 | `			}` |
|      22 | 3250 | `			nAbstract++;` |
|       9 | 3251 | `		}` |
|       5 | 3252 | `	}` |
|    3911 | 3253 | `	if( nAbstract == 0 ){` |
|    3895 | 3254 | `		return SXRET_OK;` |
|       - | 3255 | `	}` |
|       - | 3256 | `	/* Build the error message listing all abstract methods with origins */` |
|      20 | 3257 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      20 | 3258 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - | 3259 | `		"be declared abstract or implement the remaining method%s (",` |
|       8 | 3260 | `		&pClass->sName,nAbstract,` |
|       8 | 3261 | `		(nAbstract > 1 ? "s" : ""),` |
|       8 | 3262 | `		(nAbstract > 1 ? "s" : ""));` |
|       - | 3263 | `	/* Second pass: list methods with origins */` |
|       - | 3264 | `	{` |
|      20 | 3265 | `		sxu32 nListed = 0;` |
|      20 | 3266 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      42 | 3267 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      26 | 3268 | `			ph7_class *pOrigin = 0;` |
|       - | 3269 | `			SyString *pMName;` |
|      26 | 3270 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      26 | 3271 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       6 | 3272 | `				continue;` |
|       - | 3273 | `			}` |
|      22 | 3274 | `			pMName = &pMeth->sFunc.sName;` |
|      22 | 3275 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|     ! 0 | 3276 | `				continue; /* hook requirement met by a plain property (php) */` |
|       - | 3277 | `			}` |
|      22 | 3278 | `			if( nListed > 0 ){` |
|       3 | 3279 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 | 3280 | `			}` |
|       - | 3281 | `			/* Find the origin of this abstract method.` |
|       - | 3282 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - | 3283 | `			 * inheritance chains) take precedence for interface-declared` |
|       - | 3284 | `			 * methods. Abstract class methods only win when the class` |
|       - | 3285 | `			 * itself declared the abstract method (not inherited from` |
|       - | 3286 | `			 * an interface). Trait methods are adopted into the using` |
|       - | 3287 | `			 * class's namespace.` |
|       - | 3288 | `			 */` |
|       - | 3289 | `			{` |
|       - | 3290 | `				ph7_class **apIface;` |
|       - | 3291 | `				ph7_class **apTrait;` |
|       - | 3292 | `				ph7_class *pWalk;` |
|       - | 3293 | `				sxu32 i;` |
|       - | 3294 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - | 3295 | `				 * (one that was written in the class body, not inherited from an` |
|       - | 3296 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - | 3297 | `				 */` |
|      22 | 3298 | `				if( pClass->pBase ){` |
|      13 | 3299 | `					pWalk = pClass->pBase;` |
|      21 | 3300 | `					while( pWalk ){` |
|       - | 3301 | `						ph7_class_method *pParentMeth;` |
|      15 | 3302 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      15 | 3303 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - | 3304 | `							/* Exclude methods that came from an interface anywhere` |
|       - | 3305 | `							 * in this class's ancestor chain.` |
|       - | 3306 | `							 */` |
|      15 | 3307 | `							int fromIface = 0;` |
|      15 | 3308 | `							ph7_class *pAnc = pWalk;` |
|      21 | 3309 | `							while( pAnc ){` |
|       - | 3310 | `								ph7_class **apPI;` |
|       - | 3311 | `								sxu32 j;` |
|      17 | 3312 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      17 | 3313 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 | 3314 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 | 3315 | `										fromIface = 1;` |
|      10 | 3316 | `										break;` |
|       - | 3317 | `									}` |
|     ! 0 | 3318 | `								}` |
|      17 | 3319 | `								if( fromIface ) break;` |
|       8 | 3320 | `								pAnc = pAnc->pBase;` |
|       2 | 3321 | `							}` |
|      15 | 3322 | `							if( !fromIface ){` |
|       5 | 3323 | `								pOrigin = pWalk;` |
|       5 | 3324 | `								break;` |
|       - | 3325 | `							}` |
|       4 | 3326 | `						}` |
|      10 | 3327 | `						pWalk = pWalk->pBase;` |
|       2 | 3328 | `					}` |
|       5 | 3329 | `				}` |
|       - | 3330 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - | 3331 | `				 * each interface's own parent chain for the deepest origin.` |
|       - | 3332 | `				 */` |
|      22 | 3333 | `				if( !pOrigin ){` |
|      18 | 3334 | `					pWalk = pClass;` |
|      40 | 3335 | `					while( pWalk && !pOrigin ){` |
|      26 | 3336 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      26 | 3337 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      16 | 3338 | `							ph7_class *pIface = apIface[i];` |
|      16 | 3339 | `							ph7_class *pDeepest = 0;` |
|      28 | 3340 | `							while( pIface ){` |
|      16 | 3341 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      16 | 3342 | `									pDeepest = pIface;` |
|       6 | 3343 | `								}` |
|      16 | 3344 | `								pIface = pIface->pBase;` |
|       4 | 3345 | `							}` |
|      16 | 3346 | `							if( pDeepest ){` |
|      16 | 3347 | `								pOrigin = pDeepest;` |
|      16 | 3348 | `								break;` |
|       - | 3349 | `							}` |
|     ! 0 | 3350 | `						}` |
|      26 | 3351 | `						pWalk = pWalk->pBase;` |
|       4 | 3352 | `					}` |
|       7 | 3353 | `				}` |
|       - | 3354 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      22 | 3355 | `				if( !pOrigin ){` |
|       3 | 3356 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 | 3357 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 | 3358 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 | 3359 | `							pOrigin = pClass;` |
|       3 | 3360 | `							break;` |
|       - | 3361 | `						}` |
|     ! 0 | 3362 | `					}` |
|       1 | 3363 | `				}` |
|       - | 3364 | `			}` |
|      22 | 3365 | `			if( pOrigin ){` |
|      22 | 3366 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|      13 | 3367 | `			}else{` |
|       - | 3368 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 | 3369 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|       - | 3370 | `			}` |
|      22 | 3371 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|      22 | 3372 | `			nListed++;` |
|       4 | 3373 | `		}` |
|       - | 3374 | `	}` |
|      20 | 3375 | `	SyBlobAppend(&sMsg,")",1);` |
|      28 | 3376 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      16 | 3377 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      20 | 3378 | `	SyBlobRelease(&sMsg);` |
|      20 | 3379 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3380 | `		return SXERR_ABORT;` |
|       - | 3381 | `	}` |
|      20 | 3382 | `	return SXRET_OK;` |
|    2007 | 3383 | `}` |
|       - | 3384 | `/*` |
|       - | 3385 | ` * Parse a class/interface name reference from the current token stream.` |
|       - | 3386 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - | 3387 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - | 3388 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - | 3389 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - | 3390 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - | 3391 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - | 3392 | ` */` |
|    7090 | 3393 | `PH7_PRIVATE sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 | 3394 | `{` |
|    7095 | 3395 | `	int isAbsolute = 0;` |
|    7095 | 3396 | `	SyToken *pStart = pGen->pIn;` |
|       - | 3397 | `	SyBlob sName;` |
|    7095 | 3398 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|     963 | 3399 | `		isAbsolute = 1;` |
|     963 | 3400 | `		pGen->pIn++;` |
|     479 | 3401 | `	}` |
|    7095 | 3402 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|       - | 3403 | ``	/* `namespace\X` names the CURRENT namespace and is fully qualified from there. */`` |
|    7095 | 3404 | `	if( !isAbsolute && GenStateNsRelPrefix(pGen,&pGen->pIn,pGen->pEnd,&sName) ){` |
|      17 | 3405 | `		isAbsolute = 1;` |
|       8 | 3406 | `	}` |
|    7095 | 3407 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      11 | 3408 | `		SyBlobRelease(&sName);` |
|      11 | 3409 | `		pGen->pIn = pStart;` |
|      11 | 3410 | `		return SXERR_INVALID;` |
|       - | 3411 | `	}` |
|    7087 | 3412 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|    7087 | 3413 | `	pGen->pIn++;` |
|   10851 | 3414 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    3774 | 3415 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     157 | 3416 | `		SyBlobAppend(&sName,"\\",1);` |
|     157 | 3417 | `		pGen->pIn++;` |
|     157 | 3418 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|     157 | 3419 | `		pGen->pIn++;` |
|       5 | 3420 | `	}` |
|    7087 | 3421 | `	if( isAbsolute ){` |
|     975 | 3422 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     490 | 3423 | `	}else{` |
|       - | 3424 | `		SyString sRaw;` |
|    6117 | 3425 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|    6117 | 3426 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - | 3427 | `	}` |
|    7087 | 3428 | `	SyBlobRelease(&sName);` |
|    7087 | 3429 | `	return SXRET_OK;` |
|    3550 | 3430 | `}` |
|       - | 3431 | `/*` |
|       - | 3432 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - | 3433 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - | 3434 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - | 3435 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - | 3436 | ` * either direction cannot run unbounded.` |
|       - | 3437 | ` */` |
|       - | 3438 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|     342 | 3439 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 | 3440 | `{` |
|       - | 3441 | `	ph7_class **apParent;` |
|       - | 3442 | `	sxu32 n;` |
|     753 | 3443 | `	while( pInterface ){` |
|     421 | 3444 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 | 3445 | `			return FALSE;` |
|       - | 3446 | `		}` |
|     443 | 3447 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|      44 | 3448 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|      14 | 3449 | `			return TRUE;` |
|       - | 3450 | `		}` |
|     411 | 3451 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|     413 | 3452 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|       3 | 3453 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 | 3454 | `				return TRUE;` |
|       - | 3455 | `			}` |
|       2 | 3456 | `		}` |
|     411 | 3457 | `		pInterface = pInterface->pBase;` |
|     411 | 3458 | `		iDepth++;` |
|       5 | 3459 | `	}` |
|     337 | 3460 | `	return FALSE;` |
|     176 | 3461 | `}` |
|     340 | 3462 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 | 3463 | `{` |
|     345 | 3464 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 | 3465 | `}` |
|       - | 3466 | `/*` |
|       - | 3467 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - | 3468 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - | 3469 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - | 3470 | ` */` |
|      10 | 3471 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       4 | 3472 | `{` |
|      18 | 3473 | `	while( pBase ){` |
|      10 | 3474 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 | 3475 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 | 3476 | `			return TRUE;` |
|       - | 3477 | `		}` |
|      10 | 3478 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 | 3479 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 | 3480 | `			return TRUE;` |
|       - | 3481 | `		}` |
|       5 | 3482 | `		pBase = pBase->pBase;` |
|       1 | 3483 | `	}` |
|       9 | 3484 | `	return FALSE;` |
|       9 | 3485 | `}` |
|       - | 3486 | `/*` |
|       - | 3487 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|       - | 3488 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|       - | 3489 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|       - | 3490 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|       - | 3491 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|       - | 3492 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|       - | 3493 | ` * pClass->aEnumCases for cases().` |
|       - | 3494 | ` */` |
|     144 | 3495 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 | 3496 | `{` |
|     149 | 3497 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 3498 | `	SySet *pInstrContainer;` |
|       - | 3499 | `	ph7_class_attr *pCase;` |
|       - | 3500 | `	SyString *pName;` |
|       - | 3501 | `	sxi32 rc;` |
|     149 | 3502 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|     149 | 3503 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 3504 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 3505 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|     ! 0 | 3506 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3507 | `			return SXERR_ABORT;` |
|       - | 3508 | `		}` |
|     ! 0 | 3509 | `		goto Synchronize;` |
|       - | 3510 | `	}` |
|     149 | 3511 | `	pName = &pGen->pIn->sData;` |
|       - | 3512 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|     149 | 3513 | `	if( SyHashGet(&pClass->hConst,(const void *)pName->zString,pName->nByte) != 0 ){` |
|     ! 0 | 3514 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 3515 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|     ! 0 | 3516 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3517 | `			return SXERR_ABORT;` |
|       - | 3518 | `		}` |
|     ! 0 | 3519 | `		goto Synchronize;` |
|       - | 3520 | `	}` |
|     149 | 3521 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|       - | 3522 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|     149 | 3523 | `	if( pCase == 0 ){` |
|     ! 0 | 3524 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3525 | `		return SXERR_ABORT;` |
|       - | 3526 | `	}` |
|     149 | 3527 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|     149 | 3528 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|     ! 0 | 3529 | `		return SXERR_ABORT;` |
|       - | 3530 | `	}` |
|     149 | 3531 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pCase->aAttrs,16,16,0,0) == SXERR_ABORT ){` |
|     ! 0 | 3532 | `		return SXERR_ABORT;` |
|       - | 3533 | `	}` |
|     149 | 3534 | `	pGen->pIn++; /* Jump the case name */` |
|     149 | 3535 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|      97 | 3536 | `		if( pClass->nEnumBacking == 0 ){` |
|       8 | 3537 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       2 | 3538 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|       6 | 3539 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3540 | `				return SXERR_ABORT;` |
|       - | 3541 | `			}` |
|       6 | 3542 | `			goto Synchronize;` |
|       - | 3543 | `		}` |
|      93 | 3544 | `		pGen->pIn++; /* Jump the equal sign */` |
|       - | 3545 | `		/* Compile the backing value expression into the case's own container` |
|       - | 3546 | `		 * (same technique as class constants). */` |
|      93 | 3547 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      93 | 3548 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|      93 | 3549 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      93 | 3550 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 3551 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 3552 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|     ! 0 | 3553 | `		}` |
|      93 | 3554 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      93 | 3555 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      93 | 3556 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3557 | `			return SXERR_ABORT;` |
|       - | 3558 | `		}` |
|      49 | 3559 | `	}else{` |
|      56 | 3560 | `		if( pClass->nEnumBacking != 0 ){` |
|     ! 0 | 3561 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 3562 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|     ! 0 | 3563 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3564 | `				return SXERR_ABORT;` |
|       - | 3565 | `			}` |
|     ! 0 | 3566 | `			goto Synchronize;` |
|       - | 3567 | `		}` |
|       - | 3568 | `	}` |
|     145 | 3569 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|     145 | 3570 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3571 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3572 | `		return SXERR_ABORT;` |
|       - | 3573 | `	}` |
|     145 | 3574 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|     145 | 3575 | `	return SXRET_OK;` |
|       2 | 3576 | `Synchronize:` |
|       - | 3577 | `	/* Synchronize with the first semi-colon */` |
|      14 | 3578 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|      10 | 3579 | `		pGen->pIn++;` |
|       2 | 3580 | `	}` |
|       6 | 3581 | `	return SXERR_CORRUPT;` |
|      77 | 3582 | `}` |
|       - | 3583 | `/*` |
|       - | 3584 | ` * Install the enum interface methods (PHP 8.1): cases() for every enum, plus` |
|       - | 3585 | ` * from()/tryFrom() for backed ones. They are NATIVE methods — the very same C` |
|       - | 3586 | ` * bodies an enum declared from C gets — because php's are internal: it reports` |
|       - | 3587 | `` * them as `<internal, prototype BackedEnum>` with no file and no line, and`` |
|       - | 3588 | `` * declares `from(string\|int $value): static` on the prototype rather than the`` |
|       - | 3589 | ` * enum's own backing type.` |
|       - | 3590 | ` *` |
|       - | 3591 | ` * This used to synthesize PHP source forwarding to three global` |
|       - | 3592 | `` * `__phl_enum_*` thunks, which put those names in php's namespace and reported`` |
|       - | 3593 | `` * every enum's three methods as `<user>` at the enum's own line.`` |
|       - | 3594 | ` */` |
|     114 | 3595 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 | 3596 | `{` |
|     119 | 3597 | `	if( PH7_InstallEnumInterfaceMethods(pGen->pVm,pClass) != SXRET_OK ){` |
|     ! 0 | 3598 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3599 | `		return SXERR_ABORT;` |
|       - | 3600 | `	}` |
|     119 | 3601 | `	return SXRET_OK;` |
|      62 | 3602 | `}` |
|       - | 3603 | `/*` |
|       - | 3604 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|       - | 3605 | ` * __call/__callStatic/__invoke stay allowed).` |
|       - | 3606 | ` */` |
|       - | 3607 | `static const char *azEnumBannedMagic[] = {` |
|       - | 3608 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|       - | 3609 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|       - | 3610 | `};` |
|       - | 3611 | `/*` |
|       - | 3612 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|       - | 3613 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|       - | 3614 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|       - | 3615 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|       - | 3616 | ` * and before the class is installed.` |
|       - | 3617 | ` */` |
|     114 | 3618 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|       5 | 3619 | `{` |
|       - | 3620 | `	SyHashEntry *pEntry;` |
|       - | 3621 | `	sxi32 rc;` |
|       - | 3622 | `	sxu32 n;` |
|       - | 3623 | `	/* php: "Enum %s cannot include properties" */` |
|     119 | 3624 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     119 | 3625 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|       3 | 3626 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       3 | 3627 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       3 | 3628 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|       1 | 3629 | `				"Enum %z cannot include properties",&pClass->sName);` |
|       3 | 3630 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3631 | `				return SXERR_ABORT;` |
|       - | 3632 | `			}` |
|       3 | 3633 | `			break;` |
|       - | 3634 | `		}` |
|     ! 0 | 3635 | `	}` |
|       - | 3636 | `	/* php: "Enum %s cannot include magic method %s" */` |
|    1601 | 3637 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|    2223 | 3638 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|    1487 | 3639 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|     ! 0 | 3640 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 3641 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|     ! 0 | 3642 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3643 | `				return SXERR_ABORT;` |
|       - | 3644 | `			}` |
|     ! 0 | 3645 | `		}` |
|     746 | 3646 | `	}` |
|       - | 3647 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|       - | 3648 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|       - | 3649 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|       - | 3650 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|       - | 3651 | `	{` |
|       - | 3652 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|       - | 3653 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|       - | 3654 | `		ph7_class_attr *pAttr;` |
|     119 | 3655 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|       - | 3656 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|     119 | 3657 | `		if( pAttr == 0 ){` |
|     ! 0 | 3658 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3659 | `			return SXERR_ABORT;` |
|       - | 3660 | `		}` |
|     119 | 3661 | `		pAttr->nType = MEMOBJ_STRING;` |
|     119 | 3662 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|     119 | 3663 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|     119 | 3664 | `		if( pClass->nEnumBacking != 0 ){` |
|      57 | 3665 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|       - | 3666 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      57 | 3667 | `			if( pAttr == 0 ){` |
|     ! 0 | 3668 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3669 | `				return SXERR_ABORT;` |
|       - | 3670 | `			}` |
|      57 | 3671 | `			pAttr->nType = pClass->nEnumBacking;` |
|      57 | 3672 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|      18 | 3673 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|      10 | 3674 | `			}else{` |
|      40 | 3675 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|       - | 3676 | `			}` |
|      57 | 3677 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|      26 | 3678 | `		}` |
|       - | 3679 | `	}` |
|     119 | 3680 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|      62 | 3681 | `}` |
|       - | 3682 | `/*` |
|       - | 3683 | ` * Deferred class declarations (class/anonymous-class extending an` |
|       - | 3684 | ` * autoloaded parent).` |
|       - | 3685 | ` *` |
|       - | 3686 | ` * A class declaration compiles INLINE while its enclosing file compiles, so a` |
|       - | 3687 | ` * parent/interface/trait that an autoloader would provide is unreachable when` |
|       - | 3688 | ` * the autoloader's own spl_autoload_register() statement has not EXECUTED yet` |
|       - | 3689 | `` * (same-file registration, or `new class extends \App\Child {}` anywhere).`` |
|       - | 3690 | ` * php's model has no such problem: a declaration with unresolved dependencies` |
|       - | 3691 | ` * is declared at its EXECUTION point, in statement order, not hoisted.` |
|       - | 3692 | ` *` |
|       - | 3693 | ` * These helpers reproduce that: before compiling a declaration, scan its` |
|       - | 3694 | `` * header (extends/implements) and body (depth-1 trait `use`) for referenced`` |
|       - | 3695 | ` * names and try to resolve each (firing autoload exactly where the normal` |
|       - | 3696 | ` * compile would). If any name is still missing, the WHOLE declaration is` |
|       - | 3697 | `` * captured as re-compilable source — a reconstructed `namespace`/`use`-import/`` |
|       - | 3698 | ` * doc/attribute/modifier prefix plus the declaration's raw text — recorded in` |
|       - | 3699 | ` * a VmDeferredClass, and OP_CLASS_DEFER is emitted at the declaration site.` |
|       - | 3700 | ` * At runtime (VmExecDeferredClass, vm_include.c) the autoloader is live: each` |
|       - | 3701 | `` * recorded name resolves or throws php's catchable `... not found` Error, and`` |
|       - | 3702 | ` * the chunk re-compiles through VmEvalChunk. An anonymous class re-compiles` |
|       - | 3703 | `` * inside `if (false) { new ... }` (installing the class without instantiating`` |
|       - | 3704 | ` * it) under its original synthesized name via pVm->sDeferAnonName; the site's` |
|       - | 3705 | ` * own OP_NEW then instantiates it with the site-compiled arguments.` |
|       - | 3706 | ` *` |
|       - | 3707 | ` * Behavior shifts only for declarations that previously died with the` |
|       - | 3708 | ` * compile-time "Nonexistent base class" fatal: they now follow php — succeed` |
|       - | 3709 | ` * when the autoloader is registered first, or throw php's catchable` |
|       - | 3710 | `` * `Class/Interface/Trait "X" not found` Error at the declaration point.`` |
|       - | 3711 | ` * A deferred declaration's OTHER compile errors (a body syntax error) shift` |
|       - | 3712 | ` * from file-compile time to the declaration's execution — still loud, timing` |
|       - | 3713 | ` * differs from php (recorded).` |
|       - | 3714 | ` */` |
|      84 | 3715 | `static void GenStateDeferEmitUses(SyBlob *pOut,SyHash *pTable,const char *zKind)` |
|       3 | 3716 | `{` |
|       - | 3717 | `	SyHashEntry *pEntry;` |
|      87 | 3718 | `	SyHashResetLoopCursor(pTable);` |
|     129 | 3719 | `	while( (pEntry = SyHashGetNextEntry(pTable)) != 0 ){` |
|     ! 0 | 3720 | `		const char *zFqn = (const char *)pEntry->pUserData;` |
|     ! 0 | 3721 | `		if( zFqn ){` |
|     ! 0 | 3722 | `			SyBlobFormat(pOut,"use %s%s as %.*s;\n",zKind,zFqn,` |
|     ! 0 | 3723 | `				(int)pEntry->nKeyLen,(const char *)pEntry->pKey);` |
|     ! 0 | 3724 | `		}` |
|     ! 0 | 3725 | `	}` |
|      87 | 3726 | `}` |
|       - | 3727 | `/*` |
|       - | 3728 | ` * Parse one class reference at *ppCur (bounded by pEnd) with the SAME` |
|       - | 3729 | ` * namespace/import resolution the real compile uses, and append it to pNames.` |
|       - | 3730 | ` * Advances *ppCur past the reference. Returns SXERR_INVALID on a malformed` |
|       - | 3731 | ` * reference (caller bails out of deferral and lets the normal path report).` |
|       - | 3732 | ` */` |
|    1420 | 3733 | `static sxi32 GenStateDeferRecordRef(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd,` |
|       - | 3734 | `	sxu8 cKind,SySet *pNames)` |
|       5 | 3735 | `{` |
|    1425 | 3736 | `	SyToken *pSavedIn = pGen->pIn;` |
|    1425 | 3737 | `	SyToken *pSavedEnd = pGen->pEnd;` |
|       - | 3738 | `	SyBlob sFqn;` |
|       - | 3739 | `	VmDeferredReq sReq;` |
|       - | 3740 | `	char *zDup;` |
|       - | 3741 | `	sxi32 rc;` |
|    1425 | 3742 | `	SyBlobInit(&sFqn,&pGen->pVm->sAllocator);` |
|    1425 | 3743 | `	pGen->pIn = *ppCur;` |
|    1425 | 3744 | `	pGen->pEnd = pEnd;` |
|    1425 | 3745 | `	rc = GenStateParseClassReference(pGen,&sFqn);` |
|    1425 | 3746 | `	*ppCur = pGen->pIn;` |
|    1425 | 3747 | `	pGen->pIn = pSavedIn;` |
|    1425 | 3748 | `	pGen->pEnd = pSavedEnd;` |
|    1425 | 3749 | `	if( rc != SXRET_OK \|\| SyBlobLength(&sFqn) < 1 ){` |
|       3 | 3750 | `		SyBlobRelease(&sFqn);` |
|       3 | 3751 | `		return SXERR_INVALID;` |
|       - | 3752 | `	}` |
|    2132 | 3753 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    1418 | 3754 | `		(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));` |
|    1423 | 3755 | `	if( zDup == 0 ){` |
|     ! 0 | 3756 | `		SyBlobRelease(&sFqn);` |
|     ! 0 | 3757 | `		return SXERR_INVALID;` |
|       - | 3758 | `	}` |
|    1423 | 3759 | `	SyStringInitFromBuf(&sReq.sName,zDup,SyBlobLength(&sFqn));` |
|    1423 | 3760 | `	sReq.cKind = cKind;` |
|    1423 | 3761 | `	SySetPut(pNames,(const void *)&sReq);` |
|    1423 | 3762 | `	SyBlobRelease(&sFqn);` |
|    1423 | 3763 | `	return SXRET_OK;` |
|     715 | 3764 | `}` |
|       - | 3765 | `/*` |
|       - | 3766 | ` * Scan the declaration whose keyword pGen->pIn sits on (class/enum/interface/` |
|       - | 3767 | `` * trait, or an anonymous `class(args)`) WITHOUT consuming tokens. Collects`` |
|       - | 3768 | ` * every referenced dependency name, locates the body braces, and filters the` |
|       - | 3769 | ` * collected names down to the UNRESOLVABLE ones (each lookup fires autoload,` |
|       - | 3770 | ` * exactly like the compile it replaces). SXRET_OK with an empty pMissing set` |
|       - | 3771 | ` * means "compile normally"; a non-empty set means "defer". Any structural` |
|       - | 3772 | ` * surprise returns SXERR_INVALID so the normal compile reports it.` |
|       - | 3773 | ` */` |
|    4506 | 3774 | `static sxi32 GenStateScanDeferDeps(ph7_gen_state *pGen,int bAnon,int iSelfKind,` |
|       - | 3775 | `	SySet *pMissing,SyToken **ppBody,SyToken **ppBodyEnd,SyBlob *pSelfFqn)` |
|       5 | 3776 | `{` |
|    4511 | 3777 | `	SyToken *pCur = pGen->pIn; /* on the declaration keyword */` |
|    4511 | 3778 | `	SyToken *pEnd = pGen->pEnd;` |
|       - | 3779 | `	SySet aNames;` |
|    4511 | 3780 | `	sxi32 rc = SXRET_OK;` |
|    4511 | 3781 | `	*ppBody = *ppBodyEnd = 0;` |
|    4511 | 3782 | `	SySetInit(&aNames,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|    4511 | 3783 | `	pCur++; /* Jump the keyword */` |
|    4511 | 3784 | `	if( bAnon ){` |
|      85 | 3785 | `		if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|      20 | 3786 | `			SyToken *pClose = 0;` |
|      20 | 3787 | `			pCur++;` |
|      20 | 3788 | `			PH7_DelimitNestedTokens(pCur,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|      20 | 3789 | `			if( pClose == 0 \|\| pClose >= pEnd ){` |
|     ! 0 | 3790 | `				SySetRelease(&aNames);` |
|     ! 0 | 3791 | `				return SXERR_INVALID;` |
|       - | 3792 | `			}` |
|      20 | 3793 | `			pCur = &pClose[1];` |
|       9 | 3794 | `		}` |
|      45 | 3795 | `	}else{` |
|    4431 | 3796 | `		if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 3797 | `			SySetRelease(&aNames);` |
|     ! 0 | 3798 | `			return SXERR_INVALID;` |
|       - | 3799 | `		}` |
|    4431 | 3800 | `		GenStateBuildFQN(pGen,&pCur->sData,pSelfFqn);` |
|    4431 | 3801 | `		pCur++;` |
|       - | 3802 | `	}` |
|       - | 3803 | ``	/* Header: extends/implements lists up to the '{' (an enum's `: int` backing`` |
|       - | 3804 | `	 * and any stray tokens pass through; malformed headers bail to the normal` |
|       - | 3805 | `	 * path's diagnostics). */` |
|    5797 | 3806 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_OCB) == 0 ){` |
|    1293 | 3807 | `		int iKind = -1;` |
|    1293 | 3808 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    1237 | 3809 | `			sxi32 nKw = SX_PTR_TO_INT(pCur->pUserData);` |
|    1237 | 3810 | `			if( nKw == PH7_TKWRD_EXTENDS ){` |
|     855 | 3811 | `				iKind = (iSelfKind == PH7_DEFER_KIND_INTERFACE)` |
|     425 | 3812 | `					? PH7_DEFER_KIND_INTERFACE : PH7_DEFER_KIND_CLASS;` |
|     812 | 3813 | `			}else if( nKw == PH7_TKWRD_IMPLEMENTS ){` |
|     331 | 3814 | `				iKind = PH7_DEFER_KIND_INTERFACE;` |
|     163 | 3815 | `			}` |
|     616 | 3816 | `		}` |
|    1293 | 3817 | `		if( iKind < 0 ){` |
|     117 | 3818 | `			pCur++;` |
|     117 | 3819 | `			continue;` |
|       - | 3820 | `		}` |
|    1181 | 3821 | `		pCur++; /* Jump extends/implements */` |
|     588 | 3822 | `		for(;;){` |
|    1203 | 3823 | `			if( GenStateDeferRecordRef(pGen,&pCur,pEnd,(sxu8)iKind,&aNames) != SXRET_OK ){` |
|       3 | 3824 | `				SySetRelease(&aNames);` |
|       3 | 3825 | `				return SXERR_INVALID;` |
|       - | 3826 | `			}` |
|    1201 | 3827 | `			if( pCur < pEnd && (pCur->nType & PH7_TK_COMMA) ){` |
|      27 | 3828 | `				pCur++;` |
|      27 | 3829 | `				continue;` |
|       - | 3830 | `			}` |
|    1179 | 3831 | `			break;` |
|     ! 0 | 3832 | `		}` |
|       5 | 3833 | `	}` |
|    4509 | 3834 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 3835 | `		SySetRelease(&aNames);` |
|     ! 0 | 3836 | `		return SXERR_INVALID;` |
|       - | 3837 | `	}` |
|    4509 | 3838 | `	*ppBody = pCur;` |
|       - | 3839 | `	{` |
|    4509 | 3840 | `		SyToken *pClose = 0;` |
|    4509 | 3841 | `		PH7_DelimitNestedTokens(&pCur[1],pEnd,PH7_TK_OCB,PH7_TK_CCB,&pClose);` |
|    4509 | 3842 | `		if( pClose == 0 \|\| pClose >= pEnd ){` |
|     ! 0 | 3843 | `			SySetRelease(&aNames);` |
|     ! 0 | 3844 | `			return SXERR_INVALID;` |
|       - | 3845 | `		}` |
|    4509 | 3846 | `		*ppBodyEnd = pClose;` |
|       - | 3847 | `	}` |
|       - | 3848 | ``	/* Body: depth-1 trait `use Name[, Name]` statements. Statement position only`` |
|       - | 3849 | ``	 * (previous token one of '{' '}' ';'), so a closure's `use ($x)` — which`` |
|       - | 3850 | `	 * follows a ')' — never matches. */` |
|       - | 3851 | `	{` |
|    4509 | 3852 | `		SyToken *p = &(*ppBody)[1];` |
|    4509 | 3853 | `		int bStmtPos = 1;` |
|    4509 | 3854 | `		sxi32 iDepth = 1;` |
|  101873 | 3855 | `		while( p < *ppBodyEnd ){` |
|   97369 | 3856 | `			if( p->nType & PH7_TK_OCB ){` |
|    4421 | 3857 | `				iDepth++;` |
|    4421 | 3858 | `				bStmtPos = 1;` |
|    4421 | 3859 | `				p++;` |
|    4421 | 3860 | `				continue;` |
|       - | 3861 | `			}` |
|   92953 | 3862 | `			if( p->nType & PH7_TK_CCB ){` |
|    4421 | 3863 | `				iDepth--;` |
|    4421 | 3864 | `				bStmtPos = 1;` |
|    4421 | 3865 | `				p++;` |
|    4421 | 3866 | `				continue;` |
|       - | 3867 | `			}` |
|   88537 | 3868 | `			if( p->nType & PH7_TK_SEMI ){` |
|    7311 | 3869 | `				bStmtPos = 1;` |
|    7311 | 3870 | `				p++;` |
|    7311 | 3871 | `				continue;` |
|       - | 3872 | `			}` |
|   81226 | 3873 | `			if( iDepth == 1 && bStmtPos && (p->nType & PH7_TK_KEYWORD)` |
|    6923 | 3874 | `			 && SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_USE ){` |
|     209 | 3875 | `				p++;` |
|     102 | 3876 | `				for(;;){` |
|     227 | 3877 | `					if( GenStateDeferRecordRef(pGen,&p,*ppBodyEnd,PH7_DEFER_KIND_TRAIT,&aNames) != SXRET_OK ){` |
|     ! 0 | 3878 | `						SySetRelease(&aNames);` |
|     ! 0 | 3879 | `						return SXERR_INVALID;` |
|       - | 3880 | `					}` |
|     227 | 3881 | `					if( p < *ppBodyEnd && (p->nType & PH7_TK_COMMA) ){` |
|      22 | 3882 | `						p++;` |
|      22 | 3883 | `						continue;` |
|       - | 3884 | `					}` |
|     209 | 3885 | `					break;` |
|     ! 0 | 3886 | `				}` |
|     209 | 3887 | `				continue;` |
|       - | 3888 | `			}` |
|   81027 | 3889 | `			bStmtPos = 0;` |
|   81027 | 3890 | `			p++;` |
|       5 | 3891 | `		}` |
|       - | 3892 | `	}` |
|       - | 3893 | `	/* Filter: keep only the names that do NOT resolve. The lookup fires the` |
|       - | 3894 | `	 * autoloader exactly where the replaced compile would. */` |
|       - | 3895 | `	{` |
|    4509 | 3896 | `		VmDeferredReq *aReq = (VmDeferredReq *)SySetBasePtr(&aNames);` |
|       - | 3897 | `		sxu32 n;` |
|    5927 | 3898 | `		for( n = 0 ; n < SySetUsed(&aNames) ; ++n ){` |
|    1423 | 3899 | `			if( PH7_VmExtractClass(pGen->pVm,aReq[n].sName.zString,aReq[n].sName.nByte,FALSE,0) == 0 ){` |
|      35 | 3900 | `				SySetPut(pMissing,(const void *)&aReq[n]);` |
|      16 | 3901 | `			}` |
|     714 | 3902 | `		}` |
|       - | 3903 | `	}` |
|    4509 | 3904 | `	SySetRelease(&aNames);` |
|    4509 | 3905 | `	return rc;` |
|    2258 | 3906 | `}` |
|       - | 3907 | `/*` |
|       - | 3908 | ` * Capture the declaration as a re-compilable chunk, record it, and emit` |
|       - | 3909 | ` * OP_CLASS_DEFER at the current emission point. On return the statement` |
|       - | 3910 | ` * cursor sits past the declaration's closing '}'. pMissing's entries are` |
|       - | 3911 | ` * COPIED into the record (their name bytes are already allocator-owned).` |
|       - | 3912 | ` */` |
|      28 | 3913 | `static sxi32 GenStateEmitDeferredClass(ph7_gen_state *pGen,sxi32 iFlags,int bAnon,` |
|       - | 3914 | `	SySet *pMissing,SyToken *pBodyEnd,SyBlob *pSelfFqn,const SyString *pAnonName)` |
|       3 | 3915 | `{` |
|      31 | 3916 | `	SyToken *pKw = pGen->pIn; /* the declaration keyword */` |
|       - | 3917 | `	VmDeferredClass *pDefer;` |
|       - | 3918 | `	SyBlob sChunk;` |
|       - | 3919 | `	const char *zFrom;` |
|       - | 3920 | `	const char *zTo;` |
|       - | 3921 | `	char *zDup;` |
|      31 | 3922 | `	SyBlobInit(&sChunk,&pGen->pVm->sAllocator);` |
|       - | 3923 | `	/* The declaration site's compile context is replayed as literal statements:` |
|       - | 3924 | `	 * strict_types first (it must open the chunk), then namespace and the` |
|       - | 3925 | `	 * use-import tables — the runtime re-compile starts in a fresh scope. */` |
|      31 | 3926 | `	if( pGen->bStrictTypes ){` |
|     ! 0 | 3927 | `		SyBlobAppend(&sChunk,"declare(strict_types=1);\n",sizeof("declare(strict_types=1);\n")-1);` |
|     ! 0 | 3928 | `	}` |
|      31 | 3929 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     ! 0 | 3930 | `		SyBlobFormat(&sChunk,"namespace %.*s;\n",` |
|     ! 0 | 3931 | `			(int)SyBlobLength(&pGen->sNamespace),(const char *)SyBlobData(&pGen->sNamespace));` |
|     ! 0 | 3932 | `	}` |
|      31 | 3933 | `	GenStateDeferEmitUses(&sChunk,&pGen->hUseImports,"");` |
|      31 | 3934 | `	GenStateDeferEmitUses(&sChunk,&pGen->hUseFuncImports,"function ");` |
|      31 | 3935 | `	GenStateDeferEmitUses(&sChunk,&pGen->hUseConstImports,"const ");` |
|       - | 3936 | `	/* Doc-comment and attribute groups precede the keyword in the raw source,` |
|       - | 3937 | `	 * outside the captured span — re-emit them from the trivia sidecar. */` |
|      31 | 3938 | `	if( !bAnon && pGen->sPendingDoc.nByte > 0 ){` |
|     ! 0 | 3939 | `		SyBlobAppend(&sChunk,pGen->sPendingDoc.zString,pGen->sPendingDoc.nByte);` |
|     ! 0 | 3940 | `		SyBlobAppend(&sChunk,"\n",1);` |
|     ! 0 | 3941 | `	}` |
|       - | 3942 | `	{` |
|       - | 3943 | `		ph7_trivia *aT;` |
|       - | 3944 | `		sxu32 nT,n;` |
|      31 | 3945 | `		if( bAnon ){` |
|       - | 3946 | ``			/* `new #[A] class` trivia is keyed to the 'class' token */`` |
|       8 | 3947 | `			SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|       8 | 3948 | `			aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|       8 | 3949 | `			nT = SySetUsed(&pGen->aTrivia);` |
|       8 | 3950 | `			if( pGen->pTokenSet && pKw >= pBase && pKw < &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|       8 | 3951 | `				sxu32 nIdx = (sxu32)(pKw - pBase);` |
|       8 | 3952 | `				for( n = 0 ; n < nT ; ++n ){` |
|     ! 0 | 3953 | `					if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|     ! 0 | 3954 | `						SyBlobFormat(&sChunk,"#[%.*s]\n",(int)aT[n].sText.nByte,aT[n].sText.zString);` |
|     ! 0 | 3955 | `					}` |
|     ! 0 | 3956 | `				}` |
|       3 | 3957 | `			}` |
|       5 | 3958 | `		}else{` |
|      24 | 3959 | `			aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|      24 | 3960 | `			nT = SySetUsed(&pGen->aPendingAttrs);` |
|      24 | 3961 | `			for( n = 0 ; n < nT ; ++n ){` |
|     ! 0 | 3962 | `				if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|     ! 0 | 3963 | `					SyBlobFormat(&sChunk,"#[%.*s]\n",(int)aT[n].sText.nByte,aT[n].sText.zString);` |
|     ! 0 | 3964 | `				}` |
|     ! 0 | 3965 | `			}` |
|       - | 3966 | `		}` |
|       - | 3967 | `	}` |
|       - | 3968 | `	/* Pad the prefix with newlines so the declaration keyword sits on its` |
|       - | 3969 | `	 * ORIGINAL line inside the chunk — runtime diagnostics from the deferred` |
|       - | 3970 | `	 * compile then report the source's real line. Best-effort: a prefix` |
|       - | 3971 | `	 * already longer than the declaration line skips the padding. */` |
|       - | 3972 | `	{` |
|      31 | 3973 | `		const char *zScan = (const char *)SyBlobData(&sChunk);` |
|      31 | 3974 | `		sxu32 nHave = 0;` |
|       - | 3975 | `		sxu32 nScan;` |
|      31 | 3976 | `		for( nScan = 0 ; nScan < SyBlobLength(&sChunk) ; ++nScan ){` |
|     ! 0 | 3977 | `			if( zScan[nScan] == '\n' ){` |
|     ! 0 | 3978 | `				nHave++;` |
|     ! 0 | 3979 | `			}` |
|     ! 0 | 3980 | `		}` |
|     305 | 3981 | `		while( nHave + 1 < pKw->nLine ){` |
|     277 | 3982 | `			SyBlobAppend(&sChunk,"\n",1);` |
|     277 | 3983 | `			nHave++;` |
|       3 | 3984 | `		}` |
|       - | 3985 | `	}` |
|      31 | 3986 | `	if( bAnon ){` |
|       - | 3987 | ``		/* `if (false) { new class <header-minus-args> { body } ; }` — installs`` |
|       - | 3988 | `		 * the class at the chunk's compile, never instantiates it. */` |
|       8 | 3989 | `		SyToken *pAfterArgs = &pKw[1];` |
|       8 | 3990 | `		SyBlobAppend(&sChunk,"if (false) { new ",sizeof("if (false) { new ")-1);` |
|       8 | 3991 | `		SyBlobAppend(&sChunk,pKw->sData.zString,pKw->sData.nByte);` |
|       8 | 3992 | `		if( pAfterArgs < pGen->pEnd && (pAfterArgs->nType & PH7_TK_LPAREN) ){` |
|       3 | 3993 | `			SyToken *pClose = 0;` |
|       3 | 3994 | `			PH7_DelimitNestedTokens(&pAfterArgs[1],pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|       3 | 3995 | `			if( pClose == 0 \|\| pClose >= pGen->pEnd ){` |
|     ! 0 | 3996 | `				SyBlobRelease(&sChunk);` |
|     ! 0 | 3997 | `				return SXERR_INVALID;` |
|       - | 3998 | `			}` |
|       3 | 3999 | `			pAfterArgs = &pClose[1];` |
|       1 | 4000 | `		}` |
|       8 | 4001 | `		if( pAfterArgs < pBodyEnd ){` |
|       8 | 4002 | `			SyBlobAppend(&sChunk," ",1);` |
|       8 | 4003 | `			zFrom = pAfterArgs->sData.zString;` |
|       8 | 4004 | `			zTo = pBodyEnd->sData.zString + pBodyEnd->sData.nByte;` |
|       8 | 4005 | `			SyBlobAppend(&sChunk,zFrom,(sxu32)(zTo - zFrom));` |
|       3 | 4006 | `		}` |
|       8 | 4007 | `		SyBlobAppend(&sChunk,"; }",sizeof("; }")-1);` |
|       5 | 4008 | `	}else{` |
|       - | 4009 | `		/* Modifiers were consumed before this compiler ran; reconstruct them` |
|       - | 4010 | ``		 * (an enum's implicit `final` must NOT be spelled out). */`` |
|      22 | 4011 | `		if( (iFlags & PH7_CLASS_ENUM) == 0` |
|      21 | 4012 | `		 && (pKw->nType & PH7_TK_KEYWORD)` |
|      22 | 4013 | `		 && SX_PTR_TO_INT(pKw->pUserData) == PH7_TKWRD_CLASS ){` |
|      16 | 4014 | `			if( iFlags & PH7_CLASS_FINAL ){` |
|       3 | 4015 | `				SyBlobAppend(&sChunk,"final ",sizeof("final ")-1);` |
|       1 | 4016 | `			}` |
|      16 | 4017 | `			if( iFlags & PH7_CLASS_ABSTRACT ){` |
|     ! 0 | 4018 | `				SyBlobAppend(&sChunk,"abstract ",sizeof("abstract ")-1);` |
|     ! 0 | 4019 | `			}` |
|      16 | 4020 | `			if( iFlags & PH7_CLASS_READONLY ){` |
|     ! 0 | 4021 | `				SyBlobAppend(&sChunk,"readonly ",sizeof("readonly ")-1);` |
|     ! 0 | 4022 | `			}` |
|       7 | 4023 | `		}` |
|      24 | 4024 | `		zFrom = pKw->sData.zString;` |
|      24 | 4025 | `		zTo = pBodyEnd->sData.zString + pBodyEnd->sData.nByte;` |
|      24 | 4026 | `		SyBlobAppend(&sChunk,zFrom,(sxu32)(zTo - zFrom));` |
|       - | 4027 | `	}` |
|      31 | 4028 | `	pDefer = (VmDeferredClass *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmDeferredClass));` |
|      31 | 4029 | `	if( pDefer == 0 ){` |
|     ! 0 | 4030 | `		SyBlobRelease(&sChunk);` |
|     ! 0 | 4031 | `		PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4032 | `		return SXERR_ABORT;` |
|       - | 4033 | `	}` |
|      31 | 4034 | `	SyZero(pDefer,sizeof(VmDeferredClass));` |
|      45 | 4035 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      28 | 4036 | `		(const char *)SyBlobData(&sChunk),SyBlobLength(&sChunk));` |
|      31 | 4037 | `	SyBlobRelease(&sChunk);` |
|      31 | 4038 | `	if( zDup == 0 ){` |
|     ! 0 | 4039 | `		PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4040 | `		return SXERR_ABORT;` |
|       - | 4041 | `	}` |
|      31 | 4042 | `	SyStringInitFromBuf(&pDefer->sText,zDup,SyStrlen(zDup));` |
|      31 | 4043 | `	if( bAnon ){` |
|       8 | 4044 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pAnonName->zString,pAnonName->nByte);` |
|       8 | 4045 | `		if( zDup == 0 ){` |
|     ! 0 | 4046 | `			PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4047 | `			return SXERR_ABORT;` |
|       - | 4048 | `		}` |
|       8 | 4049 | `		SyStringInitFromBuf(&pDefer->sAnonName,zDup,pAnonName->nByte);` |
|       8 | 4050 | `		pDefer->sSelfName = pDefer->sAnonName;` |
|       5 | 4051 | `	}else{` |
|      35 | 4052 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      22 | 4053 | `			(const char *)SyBlobData(pSelfFqn),SyBlobLength(pSelfFqn));` |
|      24 | 4054 | `		if( zDup == 0 ){` |
|     ! 0 | 4055 | `			PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4056 | `			return SXERR_ABORT;` |
|       - | 4057 | `		}` |
|      24 | 4058 | `		SyStringInitFromBuf(&pDefer->sSelfName,zDup,SyBlobLength(pSelfFqn));` |
|       - | 4059 | `	}` |
|      31 | 4060 | `	SySetInit(&pDefer->aRequired,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|       - | 4061 | `	{` |
|      31 | 4062 | `		VmDeferredReq *aReq = (VmDeferredReq *)SySetBasePtr(pMissing);` |
|       - | 4063 | `		sxu32 n;` |
|      63 | 4064 | `		for( n = 0 ; n < SySetUsed(pMissing) ; ++n ){` |
|      35 | 4065 | `			SySetPut(&pDefer->aRequired,(const void *)&aReq[n]);` |
|      19 | 4066 | `		}` |
|       - | 4067 | `	}` |
|      31 | 4068 | `	pDefer->nLine = pKw->nLine;` |
|      31 | 4069 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLASS_DEFER,0,0,(void *)pDefer,0);` |
|       - | 4070 | `	/* Skip the declaration: the statement cursor lands past its '}' */` |
|      31 | 4071 | `	pGen->pIn = &pBodyEnd[1];` |
|      31 | 4072 | `	return SXRET_OK;` |
|      17 | 4073 | `}` |
|       - | 4074 | `/*` |
|       - | 4075 | ` * Deferral gate shared by the named-declaration compilers: scan the` |
|       - | 4076 | ` * declaration at pGen->pIn; when a dependency is missing, capture + emit the` |
|       - | 4077 | ` * deferred record and return TRUE (the caller returns immediately — the` |
|       - | 4078 | ` * declaration compiles at execution time). FALSE means compile normally.` |
|       - | 4079 | ` * *pRc carries SXERR_ABORT out of the capture path.` |
|       - | 4080 | ` */` |
|    4426 | 4081 | `static int GenStateMaybeDeferClass(ph7_gen_state *pGen,sxi32 iFlags,int iSelfKind,sxi32 *pRc)` |
|       5 | 4082 | `{` |
|       - | 4083 | `	SySet aMissing;` |
|    4431 | 4084 | `	SyToken *pBody = 0;` |
|    4431 | 4085 | `	SyToken *pBodyEnd = 0;` |
|       - | 4086 | `	SyBlob sSelfFqn;` |
|    4431 | 4087 | `	int bDefer = 0;` |
|    4431 | 4088 | `	*pRc = SXRET_OK;` |
|    4431 | 4089 | `	SySetInit(&aMissing,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|    4431 | 4090 | `	SyBlobInit(&sSelfFqn,&pGen->pVm->sAllocator);` |
|    4426 | 4091 | `	if( GenStateScanDeferDeps(pGen,0,iSelfKind,&aMissing,&pBody,&pBodyEnd,&sSelfFqn) == SXRET_OK` |
|    4430 | 4092 | `	 && SySetUsed(&aMissing) > 0 ){` |
|      24 | 4093 | `		*pRc = GenStateEmitDeferredClass(pGen,iFlags,0,&aMissing,pBodyEnd,&sSelfFqn,0);` |
|      24 | 4094 | `		bDefer = 1;` |
|      11 | 4095 | `	}` |
|    4431 | 4096 | `	SySetRelease(&aMissing);` |
|    4431 | 4097 | `	SyBlobRelease(&sSelfFqn);` |
|    4431 | 4098 | `	return bDefer;` |
|       5 | 4099 | `}` |
|       - | 4100 | `/*` |
|       - | 4101 | ``  * Apply a declaration body's collected `use Trait[, Trait] [{ resolution }]` `` |
|       - | 4102 | ` * entries to pClass — plain application when no resolution block is present,` |
|       - | 4103 | ` * otherwise the two-pass insteadof/as machinery. Shared by the CLASS body and` |
|       - | 4104 | ` * (since the adaptation-block port) the TRAIT body compiler. Returns the last` |
|       - | 4105 | ` * application status (non-OK = out of memory at a copy site).` |
|       - | 4106 | ` */` |
|    4214 | 4107 | `static sxi32 GenStateApplyTraitUses(ph7_gen_state *pGen,ph7_class *pClass,SySet *pUseEntries)` |
|       5 | 4108 | `{` |
|    4219 | 4109 | `	sxi32 rc = SXRET_OK;` |
|       - | 4110 | `	{` |
|       - | 4111 | `		TraitUseEntry *apUse;` |
|       - | 4112 | `		sxu32 nU;` |
|    4219 | 4113 | `		apUse = (TraitUseEntry *)SySetBasePtr(pUseEntries);` |
|    4411 | 4114 | `		for( nU = 0 ; nU < SySetUsed(pUseEntries) ; nU++ ){` |
|     197 | 4115 | `			TraitUseEntry *pUse = &apUse[nU];` |
|     197 | 4116 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|     197 | 4117 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|     197 | 4118 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - | 4119 | `			sxu32 nT;` |
|     197 | 4120 | `			if( !hasResolution ){` |
|       - | 4121 | `				/* No conflict resolution block: use standard trait application */` |
|     321 | 4122 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|     167 | 4123 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|     167 | 4124 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 4125 | `						break;` |
|       - | 4126 | `					}` |
|      86 | 4127 | `				}` |
|      82 | 4128 | `			}else{` |
|       - | 4129 | `				/* With resolution block: copy attributes, record traits,` |
|       - | 4130 | `				 * then use the block to resolve method conflicts.` |
|       - | 4131 | `				 */` |
|       - | 4132 | `				SyToken *pR;` |
|      91 | 4133 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      53 | 4134 | `					ph7_class *pTR = apTrait[nT];` |
|       - | 4135 | `					ph7_class_attr *pAR;` |
|       - | 4136 | `					SyHashEntry *pER;` |
|       - | 4137 | `					SyString *pNR;` |
|      53 | 4138 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      77 | 4139 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 | 4140 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 | 4141 | `						pNR = &pAR->sName;` |
|     ! 0 | 4142 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 | 4143 | `							SyHashInsertTail(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 | 4144 | `						}` |
|     ! 0 | 4145 | `					}` |
|       - | 4146 | `					/* Trait constants (PHP 8.2) live in the separate hConst namespace */` |
|      53 | 4147 | `					SyHashResetLoopCursor(&pTR->hConst);` |
|      77 | 4148 | `					while((pER = SyHashGetNextEntry(&pTR->hConst)) != 0 ){` |
|     ! 0 | 4149 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 | 4150 | `						pNR = &pAR->sName;` |
|     ! 0 | 4151 | `						if( SyHashGet(&pClass->hConst,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 | 4152 | `							SyHashInsertTail(&pClass->hConst,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 | 4153 | `						}` |
|     ! 0 | 4154 | `					}` |
|      53 | 4155 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|      29 | 4156 | `				}` |
|       - | 4157 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      43 | 4158 | `				pR = pUse->pResolvStart;` |
|     109 | 4159 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 4160 | `					SyString sTrait,sMethod;` |
|       - | 4161 | `					ph7_class *pSrcTrait;` |
|       - | 4162 | `					ph7_class_method *pMeth;` |
|       - | 4163 | `					sxi32 nRKwrd;` |
|     175 | 4164 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|     109 | 4165 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      71 | 4166 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      71 | 4167 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      71 | 4168 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      71 | 4169 | `					sMethod = pR->sData;` |
|      71 | 4170 | `					pR++;` |
|      71 | 4171 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|      29 | 4172 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|      29 | 4173 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|      29 | 4174 | `							sTrait = sMethod;` |
|      29 | 4175 | `							pR++;` |
|      29 | 4176 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|      29 | 4177 | `							sMethod = pR->sData;` |
|      29 | 4178 | `							pR++;` |
|      13 | 4179 | `						}` |
|      13 | 4180 | `					}` |
|      71 | 4181 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4182 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 4183 | `						continue;` |
|       - | 4184 | `					}` |
|      71 | 4185 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      71 | 4186 | `					pR++;` |
|      71 | 4187 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|      15 | 4188 | `						pSrcTrait = 0;` |
|      19 | 4189 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      19 | 4190 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      27 | 4191 | `							if( pTN->nByte >= sTrait.nByte &&` |
|      16 | 4192 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|      15 | 4193 | `								pSrcTrait = apTrait[nT];` |
|      15 | 4194 | `								break;` |
|       - | 4195 | `							}` |
|       4 | 4196 | `						}` |
|      15 | 4197 | `						if( pSrcTrait ){` |
|      15 | 4198 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|      15 | 4199 | `							if( pMeth ){` |
|      15 | 4200 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|      15 | 4201 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|      15 | 4202 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       6 | 4203 | `								}` |
|       6 | 4204 | `							}` |
|       6 | 4205 | `						}` |
|       6 | 4206 | `					}` |
|     155 | 4207 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       5 | 4208 | `				}` |
|       - | 4209 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      91 | 4210 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - | 4211 | `					ph7_class_method *pMR;` |
|       - | 4212 | `					SyHashEntry *pER;` |
|       - | 4213 | `					SyString *pNR;` |
|      53 | 4214 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|     163 | 4215 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      91 | 4216 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      91 | 4217 | `						pNR = &pMR->sFunc.sName;` |
|      91 | 4218 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      67 | 4219 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|      31 | 4220 | `						}` |
|       5 | 4221 | `					}` |
|      29 | 4222 | `				}` |
|       - | 4223 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      43 | 4224 | `				pR = pUse->pResolvStart;` |
|     109 | 4225 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 4226 | `					SyString sTrait,sMethod,sAlias;` |
|       - | 4227 | `					ph7_class *pSrcTrait;` |
|       - | 4228 | `					ph7_class_method *pMeth;` |
|     109 | 4229 | `					int hasQual = 0;` |
|       - | 4230 | `					sxi32 nRKwrd;` |
|     175 | 4231 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|     109 | 4232 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      71 | 4233 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      71 | 4234 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      71 | 4235 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      71 | 4236 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      71 | 4237 | `					sMethod = pR->sData;` |
|      71 | 4238 | `					pR++;` |
|      71 | 4239 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|      29 | 4240 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|      29 | 4241 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|      29 | 4242 | `							sTrait = sMethod;` |
|      29 | 4243 | `							hasQual = 1;` |
|      29 | 4244 | `							pR++;` |
|      29 | 4245 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|      29 | 4246 | `							sMethod = pR->sData;` |
|      29 | 4247 | `							pR++;` |
|      13 | 4248 | `						}` |
|      13 | 4249 | `					}` |
|      71 | 4250 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4251 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 4252 | `						continue;` |
|       - | 4253 | `					}` |
|      71 | 4254 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      71 | 4255 | `					pR++;` |
|      71 | 4256 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      59 | 4257 | `						sxi32 iNewVis = -1;` |
|      59 | 4258 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|      23 | 4259 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|      23 | 4260 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|      23 | 4261 | `								iNewVis = nAK;` |
|      23 | 4262 | `								pR++;` |
|      10 | 4263 | `							}` |
|      10 | 4264 | `						}` |
|      59 | 4265 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|      57 | 4266 | `							sAlias = pR->sData;` |
|      57 | 4267 | `							pR++;` |
|      26 | 4268 | `						}` |
|      59 | 4269 | `						pMeth = 0;` |
|      59 | 4270 | `						if( hasQual ){` |
|      17 | 4271 | `							pSrcTrait = 0;` |
|      27 | 4272 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      27 | 4273 | `								SyString *pTN = &apTrait[nT]->sName;` |
|      39 | 4274 | `								if( pTN->nByte >= sTrait.nByte &&` |
|      24 | 4275 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|      17 | 4276 | `									pSrcTrait = apTrait[nT];` |
|      17 | 4277 | `									break;` |
|       - | 4278 | `								}` |
|       8 | 4279 | `							}` |
|      17 | 4280 | `							if( pSrcTrait ){` |
|      17 | 4281 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       7 | 4282 | `							}` |
|      10 | 4283 | `						}else{` |
|      45 | 4284 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - | 4285 | `						}` |
|      59 | 4286 | `						if( pMeth ){` |
|       - | 4287 | `							/* php: a method declared in the class BODY wins over a trait alias` |
|       - | 4288 | ``							 * of the same name (e.g. an explicit __construct over `init as`` |
|       - | 4289 | ``							 * __construct`). If pClass already declares sAlias ITSELF — an own`` |
|       - | 4290 | `							 * method, sFunc.pUserData == pClass — keep it: SyHashInsert is LIFO,` |
|       - | 4291 | `							 * so an unconditional insert would shadow the class method at lookup` |
|       - | 4292 | ``							 * and `new` would run the alias. A name held only by another trait is`` |
|       - | 4293 | `							 * a genuine conflict resolved by the insteadof pass above. */` |
|      59 | 4294 | `							int bClassWins = 0;` |
|      59 | 4295 | `							if( sAlias.nByte > 0 ){` |
|      57 | 4296 | `								ph7_class_method *pOwn = PH7_ClassExtractMethod(pClass,sAlias.zString,sAlias.nByte);` |
|      57 | 4297 | `								bClassWins = (pOwn && pOwn->sFunc.pUserData == pClass);` |
|      26 | 4298 | `							}` |
|      82 | 4299 | `							if( sAlias.nByte > 0 && !bClassWins ){` |
|       - | 4300 | `								/* Create a shallow copy of the method struct for the alias` |
|       - | 4301 | `								 * so it can carry its own visibility without affecting the original.` |
|       - | 4302 | `								 */` |
|       - | 4303 | `								ph7_class_method *pAlias;` |
|       - | 4304 | `								char *zAliasDup;` |
|      51 | 4305 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|      51 | 4306 | `								if( pAlias ){` |
|      51 | 4307 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|      51 | 4308 | `									if( iNewVis >= 0 ){` |
|      21 | 4309 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|      17 | 4310 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       7 | 4311 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       9 | 4312 | `									}` |
|      51 | 4313 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      51 | 4314 | `									if( zAliasDup ){` |
|      51 | 4315 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|      23 | 4316 | `									}` |
|      28 | 4317 | `								}` |
|      33 | 4318 | `							}else if( sAlias.nByte == 0 && iNewVis >= 0 ){` |
|       - | 4319 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - | 4320 | `								ph7_class_method *pCopy;` |
|       3 | 4321 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 | 4322 | `								if( pCopy ){` |
|       3 | 4323 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 | 4324 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 | 4325 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 4326 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 4327 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - | 4328 | `									/* Replace the method in the class hash */` |
|       3 | 4329 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 | 4330 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 | 4331 | `								}` |
|       1 | 4332 | `							}` |
|      27 | 4333 | `						}` |
|      27 | 4334 | `						SXUNUSED(hasQual);` |
|      27 | 4335 | `					}` |
|      83 | 4336 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       5 | 4337 | `				}` |
|       - | 4338 | `			}` |
|     197 | 4339 | `			SySetRelease(&pUse->aTraits);` |
|     101 | 4340 | `		}` |
|       - | 4341 | `	}` |
|    4219 | 4342 | `	return rc;` |
|       5 | 4343 | `}` |
|       - | 4344 | `/*` |
|       - | 4345 | ` * Compile a class declaration, named or anonymous.` |
|       - | 4346 | ` *` |
|       - | 4347 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|       - | 4348 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|       - | 4349 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|       - | 4350 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|       - | 4351 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|       - | 4352 | ` * implements, body, install) is shared by both paths.` |
|       - | 4353 | ` */` |
|    4078 | 4354 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|       - | 4355 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|       5 | 4356 | `{` |
|    4083 | 4357 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4358 | `	ph7_class *pClass,*pBase;` |
|    4083 | 4359 | `	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' (enclosing class for a nested anon) */` |
|       - | 4360 | `	SyToken *pEnd,*pTmp;` |
|       - | 4361 | `	sxi32 iProtection;` |
|       - | 4362 | `	SySet aInterfaces;` |
|       - | 4363 | `	SySet aUseEntries;` |
|       - | 4364 | `	SySet aOvMeth,aOvProp;   /* the #[\Override] claims this class DECLARED */` |
|    4083 | 4365 | `	sxu32 nErrEntry = pGen->nErr; /* errors already reported when this class started */` |
|       - | 4366 | `	sxi32 iAttrflags;` |
|       - | 4367 | `	SyString *pName;` |
|       - | 4368 | `	sxi32 nKwrd;` |
|       - | 4369 | `	sxi32 rc;` |
|    4083 | 4370 | `	if( pAnonName == 0 ){` |
|       - | 4371 | `		/* Deferral gate: an unresolvable parent/interface/trait —` |
|       - | 4372 | `		 * its autoloader has not RUN yet — re-compiles this declaration at its` |
|       - | 4373 | `		 * execution point instead of dying on "Nonexistent base class". */` |
|       - | 4374 | `		sxi32 rcDefer;` |
|    4009 | 4375 | `		if( GenStateMaybeDeferClass(pGen,iFlags,PH7_DEFER_KIND_CLASS,&rcDefer) ){` |
|      18 | 4376 | `			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 4377 | `		}` |
|    1994 | 4378 | `	}` |
|       - | 4379 | `	/* Jump the 'class' keyword */` |
|    4067 | 4380 | `	pGen->pIn++;` |
|    4067 | 4381 | `	if( pAnonName ){` |
|       - | 4382 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|       - | 4383 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|       - | 4384 | `		 * then use the synthesized name. */` |
|      79 | 4385 | `		*ppArgStart = *ppArgEnd = 0;` |
|      79 | 4386 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|      17 | 4387 | `			pGen->pIn++; /* Jump '(' */` |
|      17 | 4388 | `			*ppArgStart = pGen->pIn;` |
|      25 | 4389 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|       8 | 4390 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|      17 | 4391 | `			pGen->pIn = *ppArgEnd;` |
|      17 | 4392 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|       8 | 4393 | `		}` |
|      79 | 4394 | `		pName = pAnonName;` |
|      79 | 4395 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|      42 | 4396 | `	}else{` |
|    3993 | 4397 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4398 | `			/* Syntax error */` |
|     ! 0 | 4399 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 | 4400 | `			if( rc == SXERR_ABORT ){` |
|       - | 4401 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4402 | `				return SXERR_ABORT;` |
|       - | 4403 | `			}` |
|       - | 4404 | `			/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 | 4405 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 | 4406 | `				pGen->pIn++;` |
|     ! 0 | 4407 | `			}` |
|     ! 0 | 4408 | `			return SXRET_OK;` |
|       - | 4409 | `		}` |
|       - | 4410 | `		/* Extract class name */` |
|    3993 | 4411 | `		pName = &pGen->pIn->sData;` |
|       - | 4412 | `		/* Advance the stream cursor */` |
|    3993 | 4413 | `		pGen->pIn++;` |
|       - | 4414 | `		/* Build FQN and obtain a raw class */ {` |
|       - | 4415 | `			SyBlob sFQN;` |
|       - | 4416 | `			SyString sFQNStr;` |
|    3993 | 4417 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    3993 | 4418 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|    3993 | 4419 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       - | 4420 | ``			/* php refuses a declaration whose short name a local `use` already took. */`` |
|    3993 | 4421 | `			if( GenStateGuardImportRedeclare(pGen,0,pName,&sFQNStr,nLine) == SXERR_ABORT ){` |
|     ! 0 | 4422 | `				SyBlobRelease(&sFQN);` |
|     ! 0 | 4423 | `				return SXERR_ABORT;` |
|       - | 4424 | `			}` |
|    3993 | 4425 | `			GenStateRecordDeclaredName(pGen,0,&sFQNStr);` |
|    3993 | 4426 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    3993 | 4427 | `			SyBlobRelease(&sFQN);` |
|       - | 4428 | `		}` |
|       - | 4429 | `	}` |
|    4067 | 4430 | `	if( pClass == 0 ){` |
|     ! 0 | 4431 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4432 | `		return SXERR_ABORT;` |
|       - | 4433 | `	}` |
|    4062 | 4434 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|     123 | 4435 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|       - | 4436 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|      59 | 4437 | `		pGen->pIn++; /* Jump ':' */` |
|      54 | 4438 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      59 | 4439 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|      18 | 4440 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|      18 | 4441 | `			pGen->pIn++;` |
|      48 | 4442 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      43 | 4443 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|      40 | 4444 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|      40 | 4445 | `			pGen->pIn++;` |
|      22 | 4446 | `		}else{` |
|       3 | 4447 | `			SyToken *pTok = pGen->pIn;` |
|       3 | 4448 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|       4 | 4449 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|       1 | 4450 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|       3 | 4451 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4452 | `				return SXERR_ABORT;` |
|       - | 4453 | `			}` |
|       3 | 4454 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       3 | 4455 | `				pGen->pIn++; /* Skip the bogus type token */` |
|       1 | 4456 | `			}` |
|       - | 4457 | `		}` |
|      27 | 4458 | `	}` |
|    4067 | 4459 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    4067 | 4460 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|     ! 0 | 4461 | `		return SXERR_ABORT;` |
|       - | 4462 | `	}` |
|       - | 4463 | `	/* implemented interfaces and per-use-statement trait containers */` |
|    4067 | 4464 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    4067 | 4465 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - | 4466 | `	/* Assume a standalone class */` |
|    4067 | 4467 | `	pBase = 0;` |
|    4067 | 4468 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    1107 | 4469 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    1107 | 4470 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - | 4471 | `			SyBlob sResolved;` |
|       - | 4472 | `			SyString sBaseName;` |
|       - | 4473 | `			sxu32 nRefLine;` |
|     817 | 4474 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|       - | 4475 | `				/* php parse-fatals here (enums have no inheritance) */` |
|     ! 0 | 4476 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4477 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|     ! 0 | 4478 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4479 | `					return SXERR_ABORT;` |
|       - | 4480 | `				}` |
|     ! 0 | 4481 | `			}` |
|     817 | 4482 | `			pGen->pIn++; /* Advance past 'extends' */` |
|     817 | 4483 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     817 | 4484 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     817 | 4485 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 | 4486 | `				SyBlobRelease(&sResolved);` |
|       4 | 4487 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4488 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 | 4489 | `					pName);` |
|       3 | 4490 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 | 4491 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4492 | `					return SXERR_ABORT;` |
|       - | 4493 | `				}` |
|       3 | 4494 | `				return SXRET_OK;` |
|       - | 4495 | `			}` |
|    1220 | 4496 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|     810 | 4497 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     815 | 4498 | `			SyStringInitFromBuf(&sBaseName,` |
|       - | 4499 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - | 4500 | `			/* Interfaces are not allowed */` |
|     815 | 4501 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 | 4502 | `				pBase = pBase->pNextName;` |
|     ! 0 | 4503 | `			}` |
|     815 | 4504 | `			if( pBase == 0 ){` |
|     ! 0 | 4505 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - | 4506 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 | 4507 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4508 | `					SyBlobRelease(&sResolved);` |
|     ! 0 | 4509 | `					return SXERR_ABORT;` |
|       - | 4510 | `				}` |
|     ! 0 | 4511 | `			}else{` |
|     815 | 4512 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|       4 | 4513 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 | 4514 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|       3 | 4515 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4516 | `						SyBlobRelease(&sResolved);` |
|     ! 0 | 4517 | `						return SXERR_ABORT;` |
|       - | 4518 | `					}` |
|       3 | 4519 | `					pBase = 0; /* Never inherit from an enum */` |
|     814 | 4520 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|       7 | 4521 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4522 | `						/* php's wording, unquoted: "Class B cannot extend final class A". */` |
|       2 | 4523 | `						"Class %z cannot extend final class %z",pName,&pBase->sName);` |
|       5 | 4524 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4525 | `						SyBlobRelease(&sResolved);` |
|     ! 0 | 4526 | `						return SXERR_ABORT;` |
|       - | 4527 | `					}` |
|       2 | 4528 | `				}` |
|       - | 4529 | `			}` |
|     815 | 4530 | `			SyBlobRelease(&sResolved);` |
|     815 | 4531 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|     ! 0 | 4532 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|     ! 0 | 4533 | `			}` |
|     405 | 4534 | `		}` |
|    1105 | 4535 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - | 4536 | `			ph7_class *pInterface;` |
|       - | 4537 | `			/* Interface implementation */` |
|     325 | 4538 | `			pGen->pIn++; /* Advance the stream cursor */` |
|     180 | 4539 | `			for(;;){` |
|       - | 4540 | `				SyBlob sResolved;` |
|       - | 4541 | `				SyString sIntName;` |
|       - | 4542 | `				sxu32 nRefLine;` |
|     345 | 4543 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     345 | 4544 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     345 | 4545 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 4546 | `					SyBlobRelease(&sResolved);` |
|     ! 0 | 4547 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4548 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 | 4549 | `						pName);` |
|     ! 0 | 4550 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4551 | `						return SXERR_ABORT;` |
|       - | 4552 | `					}` |
|     ! 0 | 4553 | `					break;` |
|       - | 4554 | `				}` |
|     685 | 4555 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|     340 | 4556 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     345 | 4557 | `				SyStringInitFromBuf(&sIntName,` |
|       - | 4558 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - | 4559 | `				/* Only interfaces are allowed */` |
|     345 | 4560 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 4561 | `					pInterface = pInterface->pNextName;` |
|     ! 0 | 4562 | `				}` |
|     345 | 4563 | `				if( pInterface == 0 ){` |
|     ! 0 | 4564 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - | 4565 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 | 4566 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4567 | `						SyBlobRelease(&sResolved);` |
|     ! 0 | 4568 | `						return SXERR_ABORT;` |
|       - | 4569 | `					}` |
|     ! 0 | 4570 | `				}else{` |
|       - | 4571 | `					/* Reject user classes that try to implement Throwable` |
|       - | 4572 | `					 * directly (or via an interface that extends Throwable)` |
|       - | 4573 | `					 * unless they already extend Exception or Error.` |
|       - | 4574 | `					 * Exception and Error themselves are compiled from the` |
|       - | 4575 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - | 4576 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|     345 | 4577 | `					SyString *pFqn = &pClass->sName;` |
|     345 | 4578 | `					int bIsExceptionOrError =` |
|     177 | 4579 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|     517 | 4580 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|     348 | 4581 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|      16 | 4582 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|     345 | 4583 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|      18 | 4584 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|       3 | 4585 | `						!bIsExceptionOrError ){` |
|      12 | 4586 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4587 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 | 4588 | `							&pClass->sName);` |
|       9 | 4589 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 4590 | `							SyBlobRelease(&sResolved);` |
|     ! 0 | 4591 | `							return SXERR_ABORT;` |
|       - | 4592 | `						}` |
|       - | 4593 | `						/* Skip registration so the follow-up abstract-method` |
|       - | 4594 | `						 * check does not produce a duplicate fatal. */` |
|       6 | 4595 | `					}else{` |
|     339 | 4596 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - | 4597 | `					}` |
|       - | 4598 | `				}` |
|     345 | 4599 | `				SyBlobRelease(&sResolved);` |
|     345 | 4600 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     165 | 4601 | `					break;` |
|       - | 4602 | `				}` |
|      25 | 4603 | `				pGen->pIn++;/* Jump the comma */` |
|       5 | 4604 | `			}` |
|     160 | 4605 | `		}` |
|     550 | 4606 | `	}` |
|    4065 | 4607 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 4608 | `		/* Syntax error */` |
|     ! 0 | 4609 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 | 4610 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4611 | `		if( rc == SXERR_ABORT ){` |
|       - | 4612 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4613 | `			return SXERR_ABORT;` |
|       - | 4614 | `		}` |
|     ! 0 | 4615 | `		return SXRET_OK;` |
|       - | 4616 | `	}` |
|    4065 | 4617 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    4065 | 4618 | `	pEnd = 0; /* cc warning */` |
|       - | 4619 | `	/* Delimit the class body */` |
|    4065 | 4620 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    4065 | 4621 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4622 | `		/* Syntax error */` |
|     ! 0 | 4623 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 | 4624 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4625 | `		if( rc == SXERR_ABORT ){` |
|       - | 4626 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4627 | `			return SXERR_ABORT;` |
|       - | 4628 | `		}` |
|     ! 0 | 4629 | `		return SXRET_OK;` |
|       - | 4630 | `	}` |
|       - | 4631 | `	/* The delimiter token is the class body's closing brace */` |
|    4065 | 4632 | `	pClass->nEndLine = pEnd->nLine;` |
|       - | 4633 | `	/* Swap token stream */` |
|    4065 | 4634 | `	pTmp = pGen->pEnd;` |
|    4065 | 4635 | `	pGen->pEnd = pEnd;` |
|       - | 4636 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|    4065 | 4637 | `	pClass->iFlags \|= iFlags;` |
|       - | 4638 | ``	/* ...which is what php's own attribute validators judge: `#[\Attribute]` on an`` |
|       - | 4639 | ``	 * abstract class, `#[\AllowDynamicProperties]` on a readonly one or an enum. */`` |
|    6090 | 4640 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pClass->aAttrs,1,1,` |
|    6095 | 4641 | `			&pClass->sName,pClass->iFlags) == SXERR_ABORT ){` |
|     ! 0 | 4642 | `		return SXERR_ABORT;` |
|       - | 4643 | `	}` |
|       - | 4644 | `	/* This class/enum is now the lexical class for its body — see pCurClass. */` |
|    4065 | 4645 | `	pGen->pCurClass = pClass;` |
|       - | 4646 | `	/* Start the parse process */` |
|    4033 | 4647 | `	for(;;){` |
|       - | 4648 | `		/* Jump leading/trailing semi-colons */` |
|   13229 | 4649 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    2723 | 4650 | `			pGen->pIn++;` |
|       5 | 4651 | `		}` |
|   10511 | 4652 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4653 | `			/* End of class body */` |
|    4015 | 4654 | `			break;` |
|       - | 4655 | `		}` |
|       - | 4656 | `		/* Bind a directly-preceding docblock to this member */` |
|    6501 | 4657 | `		GenStateSetPendingDoc(&(*pGen));` |
|    6496 | 4658 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|    3253 | 4659 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|     ! 0 | 4660 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4661 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 4662 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 4663 | `			if( rc == SXERR_ABORT ){` |
|       - | 4664 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4665 | `				return SXERR_ABORT;` |
|       - | 4666 | `			}` |
|     ! 0 | 4667 | `			goto done;` |
|       - | 4668 | `		}` |
|       - | 4669 | `		/* Assume public visibility */` |
|    6501 | 4670 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|    6501 | 4671 | `		iAttrflags = 0;` |
|       - | 4672 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|       - | 4673 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|       - | 4674 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|       - | 4675 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|    6501 | 4676 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 | 4677 | `			int bMod = 0;` |
|     ! 0 | 4678 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 | 4679 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       - | 4680 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|       - | 4681 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|       - | 4682 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|       - | 4683 | `			 * that the generic keyword dispatch would misread as a method. */` |
|     ! 0 | 4684 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     ! 0 | 4685 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 | 4686 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|     ! 0 | 4687 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|     ! 0 | 4688 | `			}` |
|     ! 0 | 4689 | `			if( !bMod ){` |
|     ! 0 | 4690 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 4691 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 4692 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4693 | `						return SXERR_ABORT;` |
|       - | 4694 | `					}` |
|     ! 0 | 4695 | `					goto done;` |
|       - | 4696 | `				}` |
|     ! 0 | 4697 | `				continue;` |
|       - | 4698 | `			}` |
|     ! 0 | 4699 | `		}` |
|    6501 | 4700 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 4701 | `			/* Extract the current keyword */` |
|    6501 | 4702 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    6501 | 4703 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|       - | 4704 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|     149 | 4705 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|     149 | 4706 | `				if( rc != SXRET_OK ){` |
|       6 | 4707 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4708 | `						return SXERR_ABORT;` |
|       - | 4709 | `					}` |
|       6 | 4710 | `					goto done;` |
|       - | 4711 | `				}` |
|     145 | 4712 | `				continue;` |
|       - | 4713 | `			}` |
|    6357 | 4714 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 4715 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - | 4716 | `				TraitUseEntry sUse;` |
|     183 | 4717 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|     183 | 4718 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|     183 | 4719 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|     103 | 4720 | `				for(;;){` |
|       - | 4721 | `					ph7_class *pTrait;` |
|       - | 4722 | `					SyBlob sResolved;` |
|       - | 4723 | `					SyString sTraitName;` |
|     197 | 4724 | `					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|       - | 4725 | `					/* A trait name is a full class reference: it may be qualified or` |
|       - | 4726 | ``					 * fully-qualified (`use Foo\Bar\T;`, `use \Foo\Bar\T;`) — the generated`` |
|       - | 4727 | `					 * PHPUnit mocks name their traits absolutely. Parse it with the shared` |
|       - | 4728 | `					 * class-reference reader (handles the leading '\', every '\'-segment and` |
|       - | 4729 | `					 * namespace/import resolution) instead of a single-identifier read, which` |
|       - | 4730 | `					 * choked on the first '\'. */` |
|     197 | 4731 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     197 | 4732 | `					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 4733 | `						SyBlobRelease(&sResolved);` |
|     ! 0 | 4734 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|     ! 0 | 4735 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 | 4736 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 4737 | `							return SXERR_ABORT;` |
|       - | 4738 | `						}` |
|     ! 0 | 4739 | `						break;` |
|       - | 4740 | `					}` |
|     389 | 4741 | `					pTrait = PH7_VmExtractClass(pGen->pVm,` |
|     192 | 4742 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     197 | 4743 | `					SyStringInitFromBuf(&sTraitName,` |
|       - | 4744 | `						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - | 4745 | `					/* Only traits are allowed */` |
|     197 | 4746 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 4747 | `						pTrait = pTrait->pNextName;` |
|     ! 0 | 4748 | `					}` |
|     197 | 4749 | `					if( pTrait == 0 ){` |
|     ! 0 | 4750 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|       - | 4751 | `							"'%z' is not a trait",&sTraitName);` |
|     ! 0 | 4752 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 4753 | `							SyBlobRelease(&sResolved);` |
|     ! 0 | 4754 | `							return SXERR_ABORT;` |
|       - | 4755 | `						}` |
|     ! 0 | 4756 | `					}else{` |
|     197 | 4757 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - | 4758 | `					}` |
|     197 | 4759 | `					SyBlobRelease(&sResolved);` |
|       - | 4760 | `					/* GenStateParseClassReference already advanced past the whole name —` |
|       - | 4761 | `					 * continue only across a comma-separated trait list. */` |
|     197 | 4762 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      94 | 4763 | `						break;` |
|       - | 4764 | `					}` |
|      18 | 4765 | `					pGen->pIn++; /* Jump the comma */` |
|       4 | 4766 | `				}` |
|       - | 4767 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|     183 | 4768 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 4769 | `					SyToken *pBlock;` |
|      39 | 4770 | `					pGen->pIn++; /* Jump '{' */` |
|      39 | 4771 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      39 | 4772 | `					sUse.pResolvStart = pGen->pIn;` |
|      39 | 4773 | `					sUse.pResolvEnd = pBlock;` |
|      39 | 4774 | `					if( pBlock < pGen->pEnd ){` |
|      39 | 4775 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|      22 | 4776 | `					}else{` |
|     ! 0 | 4777 | `						pGen->pIn = pGen->pEnd;` |
|       - | 4778 | `					}` |
|      17 | 4779 | `				}` |
|     183 | 4780 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - | 4781 | `				/* The semicolon will be consumed by the outer loop */` |
|     183 | 4782 | `				continue;` |
|       - | 4783 | `			}` |
|    6179 | 4784 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 4785 | `				int nSetTok;` |
|    5257 | 4786 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|    5257 | 4787 | `				if( nSetVis ){` |
|       - | 4788 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|       - | 4789 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|       3 | 4790 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|       3 | 4791 | `					pGen->pIn += nSetTok;` |
|       2 | 4792 | `				}else{` |
|    5255 | 4793 | `					iProtection = nKwrd;` |
|    5255 | 4794 | `					pGen->pIn++; /* Jump the visibility token */` |
|       - | 4795 | `					/* Optional asymmetric set-visibility after the read` |
|       - | 4796 | ``					 * visibility: `public private(set) int $x`. */`` |
|    5255 | 4797 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|    5255 | 4798 | `					if( nSetVis ){` |
|      29 | 4799 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|      29 | 4800 | `						pGen->pIn += nSetTok;` |
|      14 | 4801 | `					}` |
|       - | 4802 | `				}` |
|       - | 4803 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|       - | 4804 | ``				 * `public private(set) readonly int $x`. */`` |
|    5257 | 4805 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      61 | 4806 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      61 | 4807 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|      28 | 4808 | `				}` |
|    5252 | 4809 | `				if( pGen->pIn >= pGen->pEnd` |
|    5257 | 4810 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 | 4811 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4812 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 4813 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 4814 | `					if( rc == SXERR_ABORT ){` |
|       - | 4815 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 4816 | `						return SXERR_ABORT;` |
|       - | 4817 | `					}` |
|     ! 0 | 4818 | `					goto done;` |
|       - | 4819 | `				}` |
|    5257 | 4820 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 4821 | `					/* Attribute declaration (untyped) */` |
|    1273 | 4822 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    1273 | 4823 | `					if( rc != SXRET_OK ){` |
|      11 | 4824 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 4825 | `							return SXERR_ABORT;` |
|       - | 4826 | `						}` |
|      11 | 4827 | `						goto done;` |
|       - | 4828 | `					}` |
|    1557 | 4829 | `					continue;` |
|       - | 4830 | `				}` |
|    3989 | 4831 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - | 4832 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     595 | 4833 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     595 | 4834 | `					if( rc != SXRET_OK ){` |
|       9 | 4835 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 4836 | `							return SXERR_ABORT;` |
|       - | 4837 | `						}` |
|       9 | 4838 | `						goto done;` |
|       - | 4839 | `					}` |
|     589 | 4840 | `					continue;` |
|       - | 4841 | `				}` |
|       - | 4842 | `				/* Extract the keyword */` |
|    3399 | 4843 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    1697 | 4844 | `			}` |
|    4321 | 4845 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 4846 | `				/* Process constant declaration */` |
|     455 | 4847 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|     455 | 4848 | `				if( rc != SXRET_OK ){` |
|      15 | 4849 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4850 | `						return SXERR_ABORT;` |
|       - | 4851 | `					}` |
|      15 | 4852 | `					goto done;` |
|       - | 4853 | `				}` |
|     224 | 4854 | `			}else{` |
|    3871 | 4855 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 4856 | `					/* Static method or attribute,record that */` |
|     629 | 4857 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     629 | 4858 | `					pGen->pIn++; /* Jump the static keyword */` |
|     629 | 4859 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 4860 | `						int nSetTok;` |
|     441 | 4861 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|     441 | 4862 | `						if( nSetVis ){` |
|       - | 4863 | ``							/* `static private(set) int $x` — read side stays public */`` |
|       3 | 4864 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|       3 | 4865 | `							pGen->pIn += nSetTok;` |
|       2 | 4866 | `						}else{` |
|       - | 4867 | `							/* Extract the keyword */` |
|     439 | 4868 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     439 | 4869 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 4870 | `								iProtection = nKwrd;` |
|     ! 0 | 4871 | `								pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 | 4872 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|     ! 0 | 4873 | `								if( nSetVis ){` |
|     ! 0 | 4874 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|     ! 0 | 4875 | `									pGen->pIn += nSetTok;` |
|     ! 0 | 4876 | `								}` |
|     ! 0 | 4877 | `							}` |
|       - | 4878 | `						}` |
|     218 | 4879 | `					}` |
|       - | 4880 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|       - | 4881 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|       - | 4882 | `					 * than a generic "expecting method" parse error. */` |
|     629 | 4883 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 | 4884 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 | 4885 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|     ! 0 | 4886 | `					}` |
|     624 | 4887 | `					if( pGen->pIn >= pGen->pEnd` |
|     629 | 4888 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 | 4889 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4890 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 | 4891 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 4892 | `						if( rc == SXERR_ABORT ){` |
|       - | 4893 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 4894 | `							return SXERR_ABORT;` |
|       - | 4895 | `						}` |
|     ! 0 | 4896 | `						goto done;` |
|       - | 4897 | `					}` |
|     629 | 4898 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 4899 | `						/* Attribute declaration */` |
|     185 | 4900 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     185 | 4901 | `						if( rc != SXRET_OK ){` |
|       3 | 4902 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 4903 | `								return SXERR_ABORT;` |
|       - | 4904 | `							}` |
|       3 | 4905 | `							goto done;` |
|       - | 4906 | `						}` |
|     183 | 4907 | `						continue;` |
|       - | 4908 | `					}` |
|     449 | 4909 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - | 4910 | `						/* Typed static attribute declaration */` |
|      81 | 4911 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      81 | 4912 | `						if( rc != SXRET_OK ){` |
|       3 | 4913 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 4914 | `								return SXERR_ABORT;` |
|       - | 4915 | `							}` |
|       3 | 4916 | `							goto done;` |
|       - | 4917 | `						}` |
|      79 | 4918 | `						continue;` |
|       - | 4919 | `					}` |
|       - | 4920 | `					/* Extract the keyword */` |
|     373 | 4921 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    3431 | 4922 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - | 4923 | `					/* Abstract method,record that.` |
|       - | 4924 | `					 * PHL used to also mark the whole CLASS abstract here, silently` |
|       - | 4925 | ``					 * promoting `class C{abstract function m();}` -- which php rejects`` |
|       - | 4926 | `					 * outright -- into a valid abstract class. That promotion is why` |
|       - | 4927 | `					 * GenStateCheckAbstractMethods never fired for it: by the time the` |
|       - | 4928 | `					 * check ran, the class looked declared-abstract. The declaration is` |
|       - | 4929 | `					 * now diagnosed where the method name is known (see the install` |
|       - | 4930 | `					 * site), so the class flag stays what the SOURCE said. */` |
|      51 | 4931 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - | 4932 | `					/* Advance the stream cursor */` |
|      51 | 4933 | `					pGen->pIn++;` |
|      51 | 4934 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      51 | 4935 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      51 | 4936 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      47 | 4937 | `							iProtection = nKwrd;` |
|      47 | 4938 | `							pGen->pIn++; /* Jump the visibility token */` |
|      21 | 4939 | `						}` |
|      23 | 4940 | `					}` |
|      51 | 4941 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      46 | 4942 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 4943 | `							/* Static method */` |
|     ! 0 | 4944 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 4945 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 4946 | `					}` |
|      51 | 4947 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      46 | 4948 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       - | 4949 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|       - | 4950 | `							 * HOOKED property declaration. Route anything that is not a` |
|       - | 4951 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|       - | 4952 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|       - | 4953 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|       8 | 4954 | `							if( pGen->pIn < pGen->pEnd` |
|      10 | 4955 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|       4 | 4956 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|      10 | 4957 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      10 | 4958 | `								if( rc != SXRET_OK ){` |
|     ! 0 | 4959 | `									if( rc == SXERR_ABORT ){` |
|     ! 0 | 4960 | `										return SXERR_ABORT;` |
|       - | 4961 | `									}` |
|     ! 0 | 4962 | `									goto done;` |
|       - | 4963 | `								}` |
|      10 | 4964 | `								continue;` |
|       - | 4965 | `							}` |
|     ! 0 | 4966 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4967 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 | 4968 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 4969 | `							if( rc == SXERR_ABORT ){` |
|       - | 4970 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 4971 | `								return SXERR_ABORT;` |
|       - | 4972 | `							}` |
|     ! 0 | 4973 | `							goto done;` |
|       - | 4974 | `					}` |
|      43 | 4975 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|    3220 | 4976 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - | 4977 | `					/* final method ,record that */` |
|      34 | 4978 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|      34 | 4979 | `					pGen->pIn++; /* Jump the final keyword */` |
|      34 | 4980 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 4981 | `						/* Extract the keyword */` |
|      34 | 4982 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      34 | 4983 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      22 | 4984 | `							iProtection = nKwrd;` |
|      22 | 4985 | `							pGen->pIn++; /* Jump the visibility token */` |
|       9 | 4986 | `						}` |
|      15 | 4987 | `					}` |
|      34 | 4988 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      30 | 4989 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|       - | 4990 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|       - | 4991 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|       - | 4992 | `							 * child class is compiled (PH7_ClassInherit). */` |
|      20 | 4993 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      20 | 4994 | `							if( rc != SXRET_OK ){` |
|     ! 0 | 4995 | `								if( rc == SXERR_ABORT ){` |
|     ! 0 | 4996 | `									return SXERR_ABORT;` |
|       - | 4997 | `								}` |
|     ! 0 | 4998 | `								goto done;` |
|       - | 4999 | `							}` |
|      20 | 5000 | `							continue;` |
|       - | 5001 | `					}` |
|      16 | 5002 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      12 | 5003 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 5004 | `							/* Static method */` |
|       3 | 5005 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       3 | 5006 | `							pGen->pIn++; /* Jump the static keyword */` |
|       1 | 5007 | `					}` |
|      16 | 5008 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      12 | 5009 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5010 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5011 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 | 5012 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 5013 | `							if( rc == SXERR_ABORT ){` |
|       - | 5014 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 5015 | `								return SXERR_ABORT;` |
|       - | 5016 | `							}` |
|     ! 0 | 5017 | `							goto done;` |
|       - | 5018 | `					}` |
|      16 | 5019 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       6 | 5020 | `				}` |
|    3589 | 5021 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 5022 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5023 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 | 5024 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5025 | `						if( rc == SXERR_ABORT ){` |
|       - | 5026 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5027 | `							return SXERR_ABORT;` |
|       - | 5028 | `						}` |
|     ! 0 | 5029 | `						goto done;` |
|       - | 5030 | `				}` |
|    3589 | 5031 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 | 5032 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 | 5033 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 | 5034 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5035 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 5036 | `						if( rc == SXERR_ABORT ){` |
|       - | 5037 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5038 | `							return SXERR_ABORT;` |
|       - | 5039 | `						}` |
|     ! 0 | 5040 | `						goto done;` |
|       - | 5041 | `					}` |
|       - | 5042 | `					/* Attribute declaration */` |
|       7 | 5043 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 | 5044 | `				}else{` |
|       - | 5045 | `					/* Process method declaration */` |
|    3583 | 5046 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 5047 | `				}` |
|    3589 | 5048 | `				if( rc != SXRET_OK ){` |
|      20 | 5049 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5050 | `						return SXERR_ABORT;` |
|       - | 5051 | `					}` |
|      20 | 5052 | `					goto done;` |
|       - | 5053 | `				}` |
|       - | 5054 | `			}` |
|    2008 | 5055 | `		}else{` |
|       - | 5056 | `			/* Attribute declaration */` |
|     ! 0 | 5057 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 5058 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5059 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5060 | `					return SXERR_ABORT;` |
|       - | 5061 | `				}` |
|     ! 0 | 5062 | `				goto done;` |
|       - | 5063 | `			}` |
|       - | 5064 | `		}` |
|       5 | 5065 | `	}` |
|       - | 5066 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - | 5067 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - | 5068 | `	 */` |
|    4015 | 5069 | `	rc = GenStateApplyTraitUses(&(*pGen),pClass,&aUseEntries);` |
|    4015 | 5070 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5071 | `		SySetRelease(&aUseEntries);` |
|     ! 0 | 5072 | `		SySetRelease(&aInterfaces);` |
|     ! 0 | 5073 | `		return SXERR_ABORT;` |
|       - | 5074 | `	}` |
|    4015 | 5075 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|       - | 5076 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|       - | 5077 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|     119 | 5078 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|     119 | 5079 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5080 | `			SySetRelease(&aUseEntries);` |
|     ! 0 | 5081 | `			SySetRelease(&aInterfaces);` |
|     ! 0 | 5082 | `			return SXERR_ABORT;` |
|       - | 5083 | `		}` |
|      57 | 5084 | `	}` |
|       - | 5085 | `	/* The members this class DECLARES that claim #[\Override] -- recorded here,` |
|       - | 5086 | `	 * before inheritance copies the base's records in beside them, and verified` |
|       - | 5087 | `	 * once the answer exists. */` |
|    4015 | 5088 | `	GenStateCollectOverrides(&(*pGen),pClass,&aOvMeth,&aOvProp);` |
|       - | 5089 | `	/* Reject a php-fatal redeclaration before hoisting the class */` |
|    4015 | 5090 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|       9 | 5091 | `		SySetRelease(&aOvMeth);` |
|       9 | 5092 | `		SySetRelease(&aOvProp);` |
|       9 | 5093 | `		return SXERR_ABORT;` |
|       - | 5094 | `	}` |
|       - | 5095 | `	/* Install the class */` |
|    4009 | 5096 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    4009 | 5097 | `	if( rc == SXRET_OK ){` |
|       - | 5098 | `		ph7_class **apInterface;` |
|       - | 5099 | `		sxu32 n;` |
|    4009 | 5100 | `		if( pBase ){` |
|       - | 5101 | `			/* Inherit from base class and mark as a subclass */` |
|     813 | 5102 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|     404 | 5103 | `		}` |
|    4009 | 5104 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|    4343 | 5105 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 5106 | `			/* Implements one or more interface */` |
|     339 | 5107 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|     339 | 5108 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5109 | `				break;` |
|       - | 5110 | `			}` |
|     172 | 5111 | `		}` |
|       - | 5112 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|       - | 5113 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|    4009 | 5114 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|     117 | 5115 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|     117 | 5116 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5117 | `				pIntf = pIntf->pNextName;` |
|     ! 0 | 5118 | `			}` |
|     117 | 5119 | `			if( pIntf ){` |
|     117 | 5120 | `				PH7_ClassImplement(pClass,pIntf);` |
|      56 | 5121 | `			}` |
|     117 | 5122 | `			if( pClass->nEnumBacking != 0 ){` |
|      57 | 5123 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|      57 | 5124 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5125 | `					pIntf = pIntf->pNextName;` |
|     ! 0 | 5126 | `				}` |
|      57 | 5127 | `				if( pIntf ){` |
|      57 | 5128 | `					PH7_ClassImplement(pClass,pIntf);` |
|      26 | 5129 | `				}` |
|      26 | 5130 | `			}` |
|      56 | 5131 | `		}` |
|       - | 5132 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - | 5133 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|    4004 | 5134 | `		if( rc == SXRET_OK` |
|    4004 | 5135 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|    4009 | 5136 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|     295 | 5137 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - | 5138 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|     295 | 5139 | `			if( pStringable ){` |
|     295 | 5140 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|     295 | 5141 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - | 5142 | `				sxu32 i;` |
|     295 | 5143 | `				int bAlready = 0;` |
|     299 | 5144 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|       8 | 5145 | `					if( apImpl[i] == pStringable ){` |
|       3 | 5146 | `						bAlready = 1;` |
|       3 | 5147 | `						break;` |
|       - | 5148 | `					}` |
|       3 | 5149 | `				}` |
|     295 | 5150 | `				if( !bAlready ){` |
|     293 | 5151 | `					PH7_ClassImplement(pClass,pStringable);` |
|     144 | 5152 | `				}` |
|     145 | 5153 | `			}` |
|     145 | 5154 | `		}` |
|       - | 5155 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|    4009 | 5156 | `		if( rc == SXRET_OK ){` |
|    4009 | 5157 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|    4009 | 5158 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 5159 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 5160 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 5161 | `				SySetRelease(&aOvMeth);` |
|     ! 0 | 5162 | `				SySetRelease(&aOvProp);` |
|     ! 0 | 5163 | `				return SXERR_ABORT;` |
|       - | 5164 | `			}` |
|    2002 | 5165 | `		}` |
|       - | 5166 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|    4009 | 5167 | `		if( rc == SXRET_OK ){` |
|    4009 | 5168 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|    4009 | 5169 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 5170 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 5171 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 5172 | `				SySetRelease(&aOvMeth);` |
|     ! 0 | 5173 | `				SySetRelease(&aOvProp);` |
|     ! 0 | 5174 | `				return SXERR_ABORT;` |
|       - | 5175 | `			}` |
|    2002 | 5176 | `		}` |
|       - | 5177 | `		/* ...and the #[\Override] claims LAST: php reports an unimplemented` |
|       - | 5178 | `		 * abstract method and an inheritance visibility clash before this one,` |
|       - | 5179 | `		 * and stops there — php's E_COMPILE_ERROR does not return, so a` |
|       - | 5180 | `		 * declaration that already failed says nothing more. */` |
|    4009 | 5181 | `		if( rc == SXRET_OK && pGen->nErr == nErrEntry ){` |
|    3843 | 5182 | `			sxi32 rcCheck = GenStateCheckOverrides(&(*pGen),pClass,&aOvMeth,&aOvProp);` |
|    3843 | 5183 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 5184 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 5185 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 5186 | `				SySetRelease(&aOvMeth);` |
|     ! 0 | 5187 | `				SySetRelease(&aOvProp);` |
|     ! 0 | 5188 | `				return SXERR_ABORT;` |
|       - | 5189 | `			}` |
|    1919 | 5190 | `		}` |
|    2002 | 5191 | `	}` |
|    4009 | 5192 | `	SySetRelease(&aUseEntries);` |
|    4009 | 5193 | `	SySetRelease(&aInterfaces);` |
|    4009 | 5194 | `	SySetRelease(&aOvMeth);` |
|    4009 | 5195 | `	SySetRelease(&aOvProp);` |
|    4009 | 5196 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5197 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5198 | `		return SXERR_ABORT;` |
|       - | 5199 | `	}` |
|    2002 | 5200 | `done:` |
|    4059 | 5201 | `	pGen->pCurClass = pSavedCurClass;` |
|       - | 5202 | `	/* Point beyond the class body */` |
|    4059 | 5203 | `	pGen->pIn = &pEnd[1];` |
|    4059 | 5204 | `	pGen->pEnd = pTmp;` |
|    4059 | 5205 | `	return PH7_OK;` |
|    2044 | 5206 | `}` |
|       - | 5207 | `/* Compile a named class declaration (the common case). */` |
|    4004 | 5208 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 | 5209 | `{` |
|    4009 | 5210 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|       5 | 5211 | `}` |
|       - | 5212 | `/*` |
|       - | 5213 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|       - | 5214 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|       - | 5215 | ` * compile + install the class body once (at compile time, like every other` |
|       - | 5216 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|       - | 5217 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|       - | 5218 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|       - | 5219 | ` */` |
|      80 | 5220 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 5221 | `{` |
|       - | 5222 | `	char zName[128];         /* Synthesized class name */` |
|       - | 5223 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|       - | 5224 | `	SyString sName;` |
|       - | 5225 | `	SyToken *pArgStart,*pArgEnd;` |
|      85 | 5226 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|       - | 5227 | `	                              * is keyed to this 'class' token */` |
|       - | 5228 | `	ph7_value *pObj;` |
|      85 | 5229 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5230 | `	sxu32 nIdx,nLen;` |
|       - | 5231 | `	sxi32 nArg,rc;` |
|      40 | 5232 | `	SXUNUSED(iCompileFlag);` |
|      85 | 5233 | `	if( pGen->pVm->sDeferAnonName.nByte > 0 ){` |
|       - | 5234 | `		/* Deferred re-compile (VmExecDeferredClass): install under the SAME` |
|       - | 5235 | `		 * synthesized name the original site's OP_NEW loads. One-shot. */` |
|       5 | 5236 | `		sName = pGen->pVm->sDeferAnonName;` |
|       5 | 5237 | `		nLen = sName.nByte;` |
|       5 | 5238 | `		pGen->pVm->sDeferAnonName.zString = 0;` |
|       5 | 5239 | `		pGen->pVm->sDeferAnonName.nByte = 0;` |
|       3 | 5240 | `	}else{` |
|       - | 5241 | `		/* Generate a unique anonymous-class name (collision-checked) */` |
|      81 | 5242 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      81 | 5243 | `		while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 | 5244 | `			nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|     ! 0 | 5245 | `		}` |
|      81 | 5246 | `		SyStringInitFromBuf(&sName,zName,nLen);` |
|       - | 5247 | `	}` |
|       - | 5248 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|       - | 5249 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|       - | 5250 | `	 * delimited construct; GenStateCompileClassEx restores both on success.` |
|       - | 5251 | ``	 * Deferral gate: `new class extends \App\Child {}` where the`` |
|       - | 5252 | `	 * parent's autoloader has not RUN yet — capture the class for a runtime` |
|       - | 5253 | `	 * re-compile and keep only the site's argument/OP_NEW emission here. */` |
|      85 | 5254 | `	pArgStart = pArgEnd = 0;` |
|       - | 5255 | `	{` |
|       - | 5256 | `		SySet aMissing;` |
|      85 | 5257 | `		SyToken *pBody = 0;` |
|      85 | 5258 | `		SyToken *pBodyEnd = 0;` |
|       - | 5259 | `		SyBlob sSelfFqn;` |
|      85 | 5260 | `		int bDeferred = 0;` |
|      85 | 5261 | `		SySetInit(&aMissing,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|      85 | 5262 | `		SyBlobInit(&sSelfFqn,&pGen->pVm->sAllocator);` |
|      80 | 5263 | `		if( GenStateScanDeferDeps(pGen,1,PH7_DEFER_KIND_CLASS,&aMissing,&pBody,&pBodyEnd,&sSelfFqn) == SXRET_OK` |
|      85 | 5264 | `		 && SySetUsed(&aMissing) > 0 ){` |
|       8 | 5265 | `			if( &pTokKw[1] < pGen->pEnd && (pTokKw[1].nType & PH7_TK_LPAREN) ){` |
|       3 | 5266 | `				SyToken *pClose = 0;` |
|       3 | 5267 | `				PH7_DelimitNestedTokens(&pTokKw[2],pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|       3 | 5268 | `				if( pClose && pClose < pGen->pEnd ){` |
|       3 | 5269 | `					pArgStart = &pTokKw[2];` |
|       3 | 5270 | `					pArgEnd = pClose;` |
|       1 | 5271 | `				}` |
|       1 | 5272 | `			}` |
|       8 | 5273 | `			rc = GenStateEmitDeferredClass(pGen,0,1,&aMissing,pBodyEnd,0,&sName);` |
|       8 | 5274 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5275 | `				SySetRelease(&aMissing);` |
|     ! 0 | 5276 | `				SyBlobRelease(&sSelfFqn);` |
|     ! 0 | 5277 | `				return SXERR_ABORT;` |
|       - | 5278 | `			}` |
|       8 | 5279 | `			bDeferred = ( rc == SXRET_OK );` |
|       3 | 5280 | `		}` |
|      85 | 5281 | `		SySetRelease(&aMissing);` |
|      85 | 5282 | `		SyBlobRelease(&sSelfFqn);` |
|      85 | 5283 | `		if( !bDeferred ){` |
|      79 | 5284 | `			rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|      79 | 5285 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5286 | `				return rc;` |
|       - | 5287 | `			}` |
|       - | 5288 | `			{` |
|       - | 5289 | ``				/* Expression-position attributes (`new #[A] class {…}`) */`` |
|      79 | 5290 | `				ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,sName.zString,nLen,FALSE,0);` |
|      74 | 5291 | `				if( pAnonClass` |
|      79 | 5292 | `				 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|     ! 0 | 5293 | `					return SXERR_ABORT;` |
|       - | 5294 | `				}` |
|       - | 5295 | `			}` |
|      37 | 5296 | `		}` |
|       - | 5297 | `	}` |
|       - | 5298 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|       - | 5299 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|      85 | 5300 | `	nArg = 0;` |
|      85 | 5301 | `	if( pArgStart < pArgEnd ){` |
|      18 | 5302 | `		SyToken *pSavedIn = pGen->pIn;` |
|      18 | 5303 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|       - | 5304 | `		SyToken *pArgNext;` |
|      18 | 5305 | `		pGen->pIn = pArgStart;` |
|      18 | 5306 | `		pGen->pEnd = pArgEnd;` |
|      34 | 5307 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|      18 | 5308 | `			if( pGen->pIn < pArgNext ){` |
|      18 | 5309 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|      18 | 5310 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5311 | `					pGen->pIn = pSavedIn;` |
|     ! 0 | 5312 | `					pGen->pEnd = pSavedEnd;` |
|     ! 0 | 5313 | `					return SXERR_ABORT;` |
|       - | 5314 | `				}` |
|      18 | 5315 | `				nArg++;` |
|       8 | 5316 | `			}` |
|      18 | 5317 | `			pGen->pIn = &pArgNext[1];` |
|       2 | 5318 | `		}` |
|      18 | 5319 | `		pGen->pIn = pSavedIn;` |
|      18 | 5320 | `		pGen->pEnd = pSavedEnd;` |
|       8 | 5321 | `	}` |
|       - | 5322 | `	/* Load the synthesized class name */` |
|      85 | 5323 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      85 | 5324 | `	if( pObj == 0 ){` |
|     ! 0 | 5325 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 5326 | `		return SXERR_ABORT;` |
|       - | 5327 | `	}` |
|      85 | 5328 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      85 | 5329 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 5330 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|      85 | 5331 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      85 | 5332 | `	return SXRET_OK;` |
|      45 | 5333 | `}` |
|       - | 5334 | `/*` |
|       - | 5335 | ` * Compile a user-defined abstract class.` |
|       - | 5336 | ` *  According to the PHP language reference manual` |
|       - | 5337 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 5338 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 5339 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 5340 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 5341 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 5342 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 5343 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 5344 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 5345 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 5346 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 5347 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 5348 | ` *   could differ.` |
|       - | 5349 | ` */` |
|       - | 5350 | `/*` |
|       - | 5351 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|       - | 5352 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|       - | 5353 | ` * receives the corresponding PH7_CLASS_* bit.` |
|       - | 5354 | ` */` |
| 2059492 | 5355 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|       5 | 5356 | `{` |
| 2059497 | 5357 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
| 1150651 | 5358 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
| 1150651 | 5359 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
| 1150579 | 5360 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  575185 | 5361 | `	}` |
| 2059221 | 5362 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
| 2059135 | 5363 | `	return FALSE;` |
| 1029751 | 5364 | `}` |
|       - | 5365 | `/*` |
|       - | 5366 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|       - | 5367 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|       - | 5368 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|       - | 5369 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|       - | 5370 | ` */` |
| 2059130 | 5371 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|       5 | 5372 | `{` |
| 2059135 | 5373 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
| 2059135 | 5374 | `	sxi32 iFlags = 0,iFlag;` |
| 2059497 | 5375 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|     367 | 5376 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|       5 | 5377 | `			pDup = pIn;` |
|       2 | 5378 | `		}` |
|     367 | 5379 | `		iFlags \|= iFlag;` |
|     367 | 5380 | `		pIn++;` |
|       5 | 5381 | `	}` |
| 2059135 | 5382 | `	*ppIn = pIn;` |
| 2059135 | 5383 | `	if( ppDup ){ *ppDup = pDup; }` |
| 2059135 | 5384 | `	return iFlags;` |
|       5 | 5385 | `}` |
|       - | 5386 | `/*` |
|       - | 5387 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|       - | 5388 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|       - | 5389 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|       - | 5390 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|       - | 5391 | `` * `readonly`) to their existing handlers.`` |
|       - | 5392 | ` */` |
| 2058966 | 5393 | `PH7_PRIVATE int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|       5 | 5394 | `{` |
| 2058971 | 5395 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
| 1029664 | 5396 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
| 2059055 | 5397 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|       5 | 5398 | `}` |
|       - | 5399 | `/*` |
|       - | 5400 | ` * Compile a class declaration carrying one or more leading modifiers` |
|       - | 5401 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|       - | 5402 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|       - | 5403 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|       - | 5404 | `` * `abstract`+`final` pair, like PHP.`` |
|       - | 5405 | ` */` |
|     164 | 5406 | `PH7_PRIVATE sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|       5 | 5407 | `{` |
|       - | 5408 | `	SyToken *pDup;` |
|     169 | 5409 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|       - | 5410 | `	sxi32 rc;` |
|     169 | 5411 | `	if( pDup ){` |
|       4 | 5412 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|       2 | 5413 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|       3 | 5414 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5415 | `			return SXERR_ABORT;` |
|       - | 5416 | `		}` |
|       1 | 5417 | `	}` |
|     164 | 5418 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|      87 | 5419 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|       3 | 5420 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5421 | `			"Cannot use the final modifier on an abstract class");` |
|       3 | 5422 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5423 | `			return SXERR_ABORT;` |
|       - | 5424 | `		}` |
|       1 | 5425 | `	}` |
|     169 | 5426 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|      87 | 5427 | `}` |
|       - | 5428 | `/*` |
|       - | 5429 | ` * Compile a user-defined trait.` |
|       - | 5430 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - | 5431 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - | 5432 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - | 5433 | ` */` |
|     208 | 5434 | `PH7_PRIVATE sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 | 5435 | `{` |
|     213 | 5436 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5437 | `	ph7_class *pClass;` |
|     213 | 5438 | `	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' */` |
|       - | 5439 | `	SyToken *pEnd,*pTmp;` |
|       - | 5440 | `	sxi32 iProtection;` |
|       - | 5441 | `	sxi32 iAttrflags;` |
|       - | 5442 | ``	SySet aUseEntries; /* trait-body `use` statements (incl. adaptation blocks) */`` |
|       - | 5443 | `	SyString *pName;` |
|       - | 5444 | `	sxi32 nKwrd;` |
|       - | 5445 | `	sxi32 rc;` |
|       - | 5446 | `	{` |
|       - | 5447 | `		/* Deferral gate: a used trait may need an autoloader that` |
|       - | 5448 | `		 * has not run yet. */` |
|       - | 5449 | `		sxi32 rcDefer;` |
|     213 | 5450 | `		if( GenStateMaybeDeferClass(pGen,0,PH7_DEFER_KIND_TRAIT,&rcDefer) ){` |
|       6 | 5451 | `			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 5452 | `		}` |
|       - | 5453 | `	}` |
|     209 | 5454 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - | 5455 | `	/* Jump the 'trait' keyword */` |
|     209 | 5456 | `	pGen->pIn++;` |
|     209 | 5457 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 5458 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 | 5459 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5460 | `			return SXERR_ABORT;` |
|       - | 5461 | `		}` |
|     ! 0 | 5462 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 5463 | `			pGen->pIn++;` |
|     ! 0 | 5464 | `		}` |
|     ! 0 | 5465 | `		return SXRET_OK;` |
|       - | 5466 | `	}` |
|       - | 5467 | `	/* Extract trait name */` |
|     209 | 5468 | `	pName = &pGen->pIn->sData;` |
|     209 | 5469 | `	pGen->pIn++;` |
|       - | 5470 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5471 | `		SyBlob sFQN;` |
|       - | 5472 | `		SyString sFQNStr;` |
|     209 | 5473 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     209 | 5474 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     209 | 5475 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       - | 5476 | ``		/* php refuses a declaration whose short name a local `use` already took. */`` |
|     209 | 5477 | `		if( GenStateGuardImportRedeclare(pGen,0,pName,&sFQNStr,nLine) == SXERR_ABORT ){` |
|     ! 0 | 5478 | `			SyBlobRelease(&sFQN);` |
|     ! 0 | 5479 | `			return SXERR_ABORT;` |
|       - | 5480 | `		}` |
|     209 | 5481 | `		GenStateRecordDeclaredName(pGen,0,&sFQNStr);` |
|     209 | 5482 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     209 | 5483 | `		SyBlobRelease(&sFQN);` |
|       - | 5484 | `	}` |
|     209 | 5485 | `	if( pClass == 0 ){` |
|     ! 0 | 5486 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5487 | `		return SXERR_ABORT;` |
|       - | 5488 | `	}` |
|     209 | 5489 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     209 | 5490 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|     ! 0 | 5491 | `		return SXERR_ABORT;` |
|       - | 5492 | `	}` |
|       - | 5493 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|     209 | 5494 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 5495 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 | 5496 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5497 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5498 | `			return SXERR_ABORT;` |
|       - | 5499 | `		}` |
|     ! 0 | 5500 | `		return SXRET_OK;` |
|       - | 5501 | `	}` |
|     209 | 5502 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     209 | 5503 | `	pEnd = 0;` |
|     209 | 5504 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|     209 | 5505 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 5506 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 5507 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5508 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5509 | `			return SXERR_ABORT;` |
|       - | 5510 | `		}` |
|     ! 0 | 5511 | `		return SXRET_OK;` |
|       - | 5512 | `	}` |
|       - | 5513 | `	/* The delimiter token is the trait body's closing brace */` |
|     209 | 5514 | `	pClass->nEndLine = pEnd->nLine;` |
|       - | 5515 | `	/* Swap token stream */` |
|     209 | 5516 | `	pTmp = pGen->pEnd;` |
|     209 | 5517 | `	pGen->pEnd = pEnd;` |
|       - | 5518 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|     209 | 5519 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|     306 | 5520 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pClass->aAttrs,1,1,` |
|     311 | 5521 | `			&pClass->sName,pClass->iFlags) == SXERR_ABORT ){` |
|     ! 0 | 5522 | `		return SXERR_ABORT;` |
|       - | 5523 | `	}` |
|       - | 5524 | `	/* This trait is now the lexical class for its body, so a property/parameter` |
|       - | 5525 | `	 * default here resolves __TRAIT__ to it (see pCurClass). */` |
|     209 | 5526 | `	pGen->pCurClass = pClass;` |
|       - | 5527 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|     223 | 5528 | `	for(;;){` |
|     537 | 5529 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      49 | 5530 | `			pGen->pIn++;` |
|       5 | 5531 | `		}` |
|     493 | 5532 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     209 | 5533 | `			break;` |
|       - | 5534 | `		}` |
|       - | 5535 | `		/* Bind a directly-preceding docblock to this member */` |
|     289 | 5536 | `		GenStateSetPendingDoc(&(*pGen));` |
|     289 | 5537 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5538 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5539 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 5540 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5541 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5542 | `				return SXERR_ABORT;` |
|       - | 5543 | `			}` |
|     ! 0 | 5544 | `			goto done;` |
|       - | 5545 | `		}` |
|     289 | 5546 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|     289 | 5547 | `		iAttrflags = 0;` |
|     289 | 5548 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|     289 | 5549 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     289 | 5550 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 5551 | `				/* Trait uses another trait: use T[, T2] [{ resolution }]; A trait` |
|       - | 5552 | `				 * name is a full class reference — qualified or fully-qualified` |
|       - | 5553 | ``				 * (`use Foo\T;`, `use \Foo\T;`) — so parse it with the shared`` |
|       - | 5554 | `				 * class-reference reader like the CLASS body's trait-use does` |
|       - | 5555 | `				 * (the old single-identifier read choked on the leading '\'),` |
|       - | 5556 | `				 * and collect a TraitUseEntry so an adaptation block` |
|       - | 5557 | `				 * (insteadof/as) applies through the same shared machinery. */` |
|       - | 5558 | `				TraitUseEntry sUse;` |
|      19 | 5559 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      19 | 5560 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      19 | 5561 | `				pGen->pIn++; /* Jump 'use' */` |
|      11 | 5562 | `				for(;;){` |
|       - | 5563 | `					ph7_class *pUsedTrait;` |
|       - | 5564 | `					SyBlob sResolved;` |
|       - | 5565 | `					SyString sUsedName;` |
|      23 | 5566 | `					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|      23 | 5567 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      23 | 5568 | `					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 5569 | `						SyBlobRelease(&sResolved);` |
|     ! 0 | 5570 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|     ! 0 | 5571 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 5572 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5573 | `							return SXERR_ABORT;` |
|       - | 5574 | `						}` |
|     ! 0 | 5575 | `						break;` |
|       - | 5576 | `					}` |
|      41 | 5577 | `					pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|      18 | 5578 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      23 | 5579 | `					SyStringInitFromBuf(&sUsedName,` |
|       - | 5580 | `						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|      23 | 5581 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 5582 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 5583 | `					}` |
|      23 | 5584 | `					if( pUsedTrait == 0 ){` |
|     ! 0 | 5585 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|       - | 5586 | `							"'%z' is not a trait",&sUsedName);` |
|     ! 0 | 5587 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5588 | `							SyBlobRelease(&sResolved);` |
|     ! 0 | 5589 | `							return SXERR_ABORT;` |
|       - | 5590 | `						}` |
|     ! 0 | 5591 | `					}else{` |
|      23 | 5592 | `						SySetPut(&sUse.aTraits,(const void *)&pUsedTrait);` |
|       - | 5593 | `					}` |
|      23 | 5594 | `					SyBlobRelease(&sResolved);` |
|      23 | 5595 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      12 | 5596 | `						break;` |
|       - | 5597 | `					}` |
|       6 | 5598 | `					pGen->pIn++;` |
|       2 | 5599 | `				}` |
|       - | 5600 | `				/* Optional adaptation block (conflict resolution) */` |
|      19 | 5601 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 5602 | `					SyToken *pBlock;` |
|       5 | 5603 | `					pGen->pIn++; /* Jump '{' */` |
|       5 | 5604 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       5 | 5605 | `					sUse.pResolvStart = pGen->pIn;` |
|       5 | 5606 | `					sUse.pResolvEnd = pBlock;` |
|       5 | 5607 | `					if( pBlock < pGen->pEnd ){` |
|       5 | 5608 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       3 | 5609 | `					}else{` |
|     ! 0 | 5610 | `						pGen->pIn = pGen->pEnd;` |
|       - | 5611 | `					}` |
|       2 | 5612 | `				}` |
|      19 | 5613 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|      19 | 5614 | `				continue;` |
|       - | 5615 | `			}` |
|     275 | 5616 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     253 | 5617 | `				iProtection = nKwrd;` |
|     253 | 5618 | `				pGen->pIn++;` |
|       - | 5619 | ``				/* Optional `readonly` after the visibility (PHP 8.1): `private readonly T`` |
|       - | 5620 | ``				 * $x` — the generated PHPUnit runtime traits (StubApi, …) declare their`` |
|       - | 5621 | `				 * readonly state this way. Mirrors the class-body attribute parser. */` |
|     253 | 5622 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       5 | 5623 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       5 | 5624 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       2 | 5625 | `				}` |
|     248 | 5626 | `				if( pGen->pIn >= pGen->pEnd` |
|     253 | 5627 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 | 5628 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5629 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 5630 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 5631 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5632 | `						return SXERR_ABORT;` |
|       - | 5633 | `					}` |
|     ! 0 | 5634 | `					goto done;` |
|       - | 5635 | `				}` |
|     253 | 5636 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      23 | 5637 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      23 | 5638 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 5639 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5640 | `							return SXERR_ABORT;` |
|       - | 5641 | `						}` |
|     ! 0 | 5642 | `						goto done;` |
|       - | 5643 | `					}` |
|      23 | 5644 | `					continue;` |
|       - | 5645 | `				}` |
|     235 | 5646 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       9 | 5647 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       9 | 5648 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 5649 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5650 | `							return SXERR_ABORT;` |
|       - | 5651 | `						}` |
|     ! 0 | 5652 | `						goto done;` |
|       - | 5653 | `					}` |
|       9 | 5654 | `					continue;` |
|       - | 5655 | `				}` |
|     227 | 5656 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     111 | 5657 | `			}` |
|     249 | 5658 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 5659 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5660 | `					"Traits cannot have constants");` |
|     ! 0 | 5661 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5662 | `					return SXERR_ABORT;` |
|       - | 5663 | `				}` |
|     ! 0 | 5664 | `				goto done;` |
|     ! 0 | 5665 | `			}else{` |
|     249 | 5666 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|      19 | 5667 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      19 | 5668 | `					pGen->pIn++;` |
|      19 | 5669 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      17 | 5670 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      17 | 5671 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 5672 | `							iProtection = nKwrd;` |
|     ! 0 | 5673 | `							pGen->pIn++;` |
|     ! 0 | 5674 | `						}` |
|       7 | 5675 | `					}` |
|      16 | 5676 | `					if( pGen->pIn >= pGen->pEnd` |
|      19 | 5677 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 | 5678 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5679 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 5680 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5681 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5682 | `							return SXERR_ABORT;` |
|       - | 5683 | `						}` |
|     ! 0 | 5684 | `						goto done;` |
|       - | 5685 | `					}` |
|      19 | 5686 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 5687 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 5688 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 5689 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 5690 | `								return SXERR_ABORT;` |
|       - | 5691 | `							}` |
|     ! 0 | 5692 | `							goto done;` |
|       - | 5693 | `						}` |
|       3 | 5694 | `						continue;` |
|       - | 5695 | `					}` |
|      17 | 5696 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 | 5697 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 5698 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 5699 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 5700 | `								return SXERR_ABORT;` |
|       - | 5701 | `							}` |
|     ! 0 | 5702 | `							goto done;` |
|       - | 5703 | `						}` |
|     ! 0 | 5704 | `						continue;` |
|       - | 5705 | `					}` |
|      17 | 5706 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     240 | 5707 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       9 | 5708 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       9 | 5709 | `					pGen->pIn++;` |
|       9 | 5710 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       9 | 5711 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       9 | 5712 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       9 | 5713 | `							iProtection = nKwrd;` |
|       9 | 5714 | `							pGen->pIn++;` |
|       3 | 5715 | `						}` |
|       3 | 5716 | `					}` |
|       9 | 5717 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       6 | 5718 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5719 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5720 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 5721 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5722 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5723 | `							return SXERR_ABORT;` |
|       - | 5724 | `						}` |
|     ! 0 | 5725 | `						goto done;` |
|       - | 5726 | `					}` |
|       9 | 5727 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       3 | 5728 | `				}` |
|     247 | 5729 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 5730 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5731 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 5732 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 5733 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5734 | `						return SXERR_ABORT;` |
|       - | 5735 | `					}` |
|     ! 0 | 5736 | `					goto done;` |
|       - | 5737 | `				}` |
|     247 | 5738 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 5739 | `					pGen->pIn++;` |
|     ! 0 | 5740 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 5741 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5742 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 5743 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5744 | `							return SXERR_ABORT;` |
|       - | 5745 | `						}` |
|     ! 0 | 5746 | `						goto done;` |
|       - | 5747 | `					}` |
|     ! 0 | 5748 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 5749 | `				}else{` |
|     247 | 5750 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 5751 | `				}` |
|     247 | 5752 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 5753 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5754 | `						return SXERR_ABORT;` |
|       - | 5755 | `					}` |
|     ! 0 | 5756 | `					goto done;` |
|       - | 5757 | `				}` |
|       - | 5758 | `			}` |
|     126 | 5759 | `		}else{` |
|     ! 0 | 5760 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 5761 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5762 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5763 | `					return SXERR_ABORT;` |
|       - | 5764 | `				}` |
|     ! 0 | 5765 | `				goto done;` |
|       - | 5766 | `			}` |
|       - | 5767 | `		}` |
|       5 | 5768 | `	}` |
|       - | 5769 | ``	/* Apply the collected `use` entries (incl. adaptation blocks) through the`` |
|       - | 5770 | `	 * machinery shared with the class-body compiler. */` |
|     209 | 5771 | `	rc = GenStateApplyTraitUses(&(*pGen),pClass,&aUseEntries);` |
|     209 | 5772 | `	SySetRelease(&aUseEntries);` |
|     209 | 5773 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5774 | `		return SXERR_ABORT;` |
|       - | 5775 | `	}` |
|       - | 5776 | `	/* Reject a php-fatal redeclaration before hoisting the trait */` |
|     209 | 5777 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|       3 | 5778 | `		return SXERR_ABORT;` |
|       - | 5779 | `	}` |
|       - | 5780 | `	/* Install the trait */` |
|     207 | 5781 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     207 | 5782 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5783 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5784 | `		return SXERR_ABORT;` |
|       - | 5785 | `	}` |
|     101 | 5786 | `done:` |
|     207 | 5787 | `	pGen->pCurClass = pSavedCurClass;` |
|       - | 5788 | `	/* Point beyond the trait body */` |
|     207 | 5789 | `	pGen->pIn = &pEnd[1];` |
|     207 | 5790 | `	pGen->pEnd = pTmp;` |
|     207 | 5791 | `	return PH7_OK;` |
|     109 | 5792 | `}` |
|       - | 5793 | `/*` |
|       - | 5794 | ` * Compile a user-defined class.` |
|       - | 5795 | ` *  According to the PHP language reference manual` |
|       - | 5796 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 5797 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 5798 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 5799 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 5800 | ` *   and functions (called "methods").` |
|       - | 5801 | ` */` |
|    3720 | 5802 | `PH7_PRIVATE sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 | 5803 | `{` |
|       - | 5804 | `	sxi32 rc;` |
|    3725 | 5805 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|    3725 | 5806 | `	return rc;` |
|       5 | 5807 | `}` |
|       - | 5808 | `/*` |
|       - | 5809 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|       - | 5810 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|       - | 5811 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|       - | 5812 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|       - | 5813 | `` * meaning; `enum Name` can never start a valid expression.`` |
|       - | 5814 | ` */` |
| 2058792 | 5815 | `PH7_PRIVATE int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|       5 | 5816 | `{` |
| 2109665 | 5817 | `	return (pIn->nType & PH7_TK_ID)` |
| 1080264 | 5818 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|   62835 | 5819 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
| 2109660 | 5820 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|       5 | 5821 | `}` |
|       - | 5822 | `/*` |
|       - | 5823 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|       - | 5824 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|       - | 5825 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|       - | 5826 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|       - | 5827 | ` */` |
|     120 | 5828 | `PH7_PRIVATE sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|       5 | 5829 | `{` |
|     125 | 5830 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|       5 | 5831 | `}` |
|       - | 5832 |  |
