# src/ph7/compile_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2642/3426 lines (77.12%)

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
|    3640 |   31 | `static sxi32 GenStateGuardClassRedeclaration(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |   32 | `{` |
|       - |   33 | `	SyHashEntry *pEntry;` |
|    3645 |   34 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|      60 |   35 | `		return SXRET_OK; /* conditional/nested: keep hoisting */` |
|       - |   36 | `	}` |
|    3589 |   37 | `	pClass->iFlags \|= PH7_CLASS_BOUND;` |
|    3589 |   38 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|     ! 0 |   39 | `		return SXRET_OK; /* the prelude installs each builtin exactly once */` |
|       - |   40 | `	}` |
|    3589 |   41 | `	pEntry = SyHashGet(&pGen->pVm->hClass,(const void *)pClass->sName.zString,pClass->sName.nByte);` |
|    3589 |   42 | `	if( pEntry ){` |
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
|    3579 |   62 | `	return SXRET_OK;` |
|    1825 |   63 | `}` |
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
|    5468 |   75 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |   76 | `{` |
|    5473 |   77 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|     443 |   78 | `		return PH7_CLASS_PROT_PRIVATE;` |
|    5035 |   79 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|     160 |   80 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |   81 | `	}` |
|       - |   82 | `	/* Assume public by default */` |
|    4879 |   83 | `	return PH7_CLASS_PROT_PUBLIC;` |
|    2739 |   84 | `}` |
|       - |   85 | `/*` |
|       - |   86 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|       - |   87 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|       - |   88 | ` * token immediately followed by '='. Anything else with a leading type token` |
|       - |   89 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|       - |   90 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|       - |   91 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|       - |   92 | ` */` |
|     458 |   93 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|       5 |   94 | `{` |
|       - |   95 | `	SyToken *p0, *p1;` |
|     463 |   96 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |   97 | `		return 0;` |
|       - |   98 | `	}` |
|     463 |   99 | `	p0 = pGen->pIn;` |
|       - |  100 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|     463 |  101 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|     ! 0 |  102 | `		return 1;` |
|       - |  103 | `	}` |
|     463 |  104 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|       5 |  105 | `		return 1;` |
|       - |  106 | `	}` |
|       - |  107 | `	/* A name-like first token begins a type only when followed by another` |
|       - |  108 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|       - |  109 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|     459 |  110 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     459 |  111 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|     459 |  112 | `		if( p1 ){` |
|     459 |  113 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|      46 |  114 | `				return 1;` |
|       - |  115 | `			}` |
|     417 |  116 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|       5 |  117 | `				return 1;` |
|       - |  118 | `			}` |
|     204 |  119 | `		}` |
|     204 |  120 | `	}` |
|     413 |  121 | `	return 0;` |
|     234 |  122 | `}` |
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
|      34 |  140 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
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
|     182 |  151 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|       5 |  152 | `{` |
|       - |  153 | `	sxi32 iOp;` |
|     187 |  154 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|      33 |  155 | `		return 0;` |
|       - |  156 | `	}` |
|     157 |  157 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|     157 |  158 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|      96 |  159 | `}` |
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
|    1712 |  177 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|       5 |  178 | `{` |
|    1717 |  179 | `	SyToken *p = pGen->pIn;` |
|    1717 |  180 | `	int iDepth = 0;` |
|   17417 |  181 | `	while( p < pGen->pEnd ){` |
|   17417 |  182 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    1663 |  183 | `			break; /* end of this initializer */` |
|       - |  184 | `		}` |
|   15754 |  185 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    7914 |  186 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      59 |  187 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|       - |  188 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|       - |  189 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|       - |  190 | `			 * expression. */` |
|      13 |  191 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|      13 |  192 | `			p++;` |
|      13 |  193 | `			if( bArrow ){` |
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
|      13 |  215 | `				int iLocal = 0;` |
|      33 |  216 | `				while( p < pGen->pEnd ){` |
|      33 |  217 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|      13 |  218 | `						break; /* body brace */` |
|       - |  219 | `					}` |
|      23 |  220 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      13 |  221 | `						iLocal++;` |
|      18 |  222 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      13 |  223 | `						if( iLocal > 0 ){` |
|      13 |  224 | `							iLocal--;` |
|       5 |  225 | `						}` |
|       5 |  226 | `					}` |
|      23 |  227 | `					p++;` |
|       3 |  228 | `				}` |
|      13 |  229 | `				if( p < pGen->pEnd ){` |
|      13 |  230 | `					int iBrace = 0; /* p is on the body '{' */` |
|      95 |  231 | `					while( p < pGen->pEnd ){` |
|      95 |  232 | `						if( p->nType & PH7_TK_OCB ){` |
|      15 |  233 | `							iBrace++;` |
|      89 |  234 | `						}else if( p->nType & PH7_TK_CCB ){` |
|      15 |  235 | `							iBrace--;` |
|      15 |  236 | `							if( iBrace == 0 ){` |
|      13 |  237 | `								p++;` |
|      13 |  238 | `								break;` |
|       - |  239 | `							}` |
|       1 |  240 | `						}` |
|      85 |  241 | `						p++;` |
|       3 |  242 | `					}` |
|       5 |  243 | `				}` |
|       - |  244 | `			}` |
|      13 |  245 | `			continue;` |
|       - |  246 | `		}` |
|   15749 |  247 | `		if( p->nType & PH7_TK_OCB ){` |
|      47 |  248 | `			if( iDepth == 0 ){` |
|       - |  249 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|       - |  250 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|       - |  251 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|       - |  252 | `				 * is legal — don't scan into it. */` |
|      47 |  253 | `				break;` |
|       - |  254 | `			}` |
|     ! 0 |  255 | `			iDepth++;` |
|   15703 |  256 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     165 |  257 | `			iDepth++;` |
|   15623 |  258 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     163 |  259 | `			if( iDepth > 0 ){` |
|     163 |  260 | `				iDepth--;` |
|      79 |  261 | `			}` |
|   15464 |  262 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    7419 |  263 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|       - |  264 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|       - |  265 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|       - |  266 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|      11 |  267 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|      11 |  268 | `				return 1;` |
|       - |  269 | `			}` |
|     ! 0 |  270 | `		}` |
|   15695 |  271 | `		p++;` |
|       5 |  272 | `	}` |
|    1709 |  273 | `	return 0;` |
|     861 |  274 | `}` |
|       - |  275 | `/*` |
|       - |  276 | ` * php keeps class CONSTANTS and PROPERTIES in separate namespaces: a class may` |
|       - |  277 | `` * declare `const C` and `public $C` together, and `$obj->C` never resolves to the`` |
|       - |  278 | ` * constant. PHL stores each in its own table (constants in hConst, properties in` |
|       - |  279 | ` * hAttr), so these two lookups target the right namespace and never collide.` |
|       - |  280 | ` */` |
|     450 |  281 | `static ph7_class_attr * GenStateExtractConstant(ph7_class *pClass,SyString *pName)` |
|       5 |  282 | `{` |
|     455 |  283 | `	return PH7_ClassExtractConstant(pClass,pName->zString,pName->nByte);` |
|       5 |  284 | `}` |
|    1866 |  285 | `static ph7_class_attr * GenStateExtractProperty(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  286 | `{` |
|    1871 |  287 | `	return PH7_ClassExtractAttribute(pClass,zName,nByte);` |
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
|    1850 |  310 | `PH7_PRIVATE int PH7_GenStateInitHasCallExpr(ph7_gen_state *pGen)` |
|       5 |  311 | `{` |
|    1855 |  312 | `	SyToken *p = pGen->pIn;` |
|    1855 |  313 | `	int iDepth = 0;` |
|       - |  314 | `	/* Conservative ternary bail-out (see the note above). */` |
|       - |  315 | `	{` |
|    1855 |  316 | `		SyToken *q = pGen->pIn;` |
|    1855 |  317 | `		int iQd = 0;` |
|   19115 |  318 | `		while( q < pGen->pEnd ){` |
|   19075 |  319 | `			if( iQd == 0 && (q->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    1809 |  320 | `				break;` |
|       - |  321 | `			}` |
|   17271 |  322 | `			if( q->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     337 |  323 | `				iQd++;` |
|   17105 |  324 | `			}else if( q->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     337 |  325 | `				if( iQd > 0 ){ iQd--; }` |
|   16773 |  326 | `			}else if( (q->nType & PH7_TK_OP) && q->pUserData` |
|    7661 |  327 | `				&& ((const ph7_expr_op *)q->pUserData)->iOp == EXPR_OP_QUESTY ){` |
|       8 |  328 | `				return 0;` |
|       - |  329 | `			}` |
|   17265 |  330 | `			q++;` |
|       5 |  331 | `		}` |
|       - |  332 | `	}` |
|   17805 |  333 | `	while( p < pGen->pEnd ){` |
|   17805 |  334 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    1801 |  335 | `			break; /* end of this initializer */` |
|       - |  336 | `		}` |
|   16004 |  337 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    8043 |  338 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      65 |  339 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|       - |  340 | `			/* A call inside a closure/arrow-fn is deferred to call time: skip the` |
|       - |  341 | `			 * whole construct. Delegating to the sibling scanner is not possible` |
|       - |  342 | ``			 * (it reports `new`), so mirror its bracket walk. */`` |
|      17 |  343 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|      17 |  344 | `			int iBase = iDepth;` |
|      17 |  345 | `			p++;` |
|      17 |  346 | `			if( bArrow ){` |
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
|      17 |  361 | `				int iLocal = 0;` |
|      45 |  362 | `				while( p < pGen->pEnd ){` |
|      45 |  363 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|      17 |  364 | `						break;` |
|       - |  365 | `					}` |
|      31 |  366 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      17 |  367 | `						iLocal++;` |
|      24 |  368 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      17 |  369 | `						if( iLocal > 0 ){ iLocal--; }` |
|       7 |  370 | `					}` |
|      31 |  371 | `					p++;` |
|       3 |  372 | `				}` |
|      17 |  373 | `				if( p < pGen->pEnd ){` |
|      17 |  374 | `					int iBrace = 0;` |
|     121 |  375 | `					while( p < pGen->pEnd ){` |
|     121 |  376 | `						if( p->nType & PH7_TK_OCB ){` |
|      19 |  377 | `							iBrace++;` |
|     113 |  378 | `						}else if( p->nType & PH7_TK_CCB ){` |
|      19 |  379 | `							iBrace--;` |
|      19 |  380 | `							if( iBrace == 0 ){` |
|      17 |  381 | `								p++;` |
|      17 |  382 | `								break;` |
|       - |  383 | `							}` |
|       1 |  384 | `						}` |
|     107 |  385 | `						p++;` |
|       3 |  386 | `					}` |
|       7 |  387 | `				}` |
|       - |  388 | `			}` |
|      17 |  389 | `			continue;` |
|       - |  390 | `		}` |
|   15995 |  391 | `		if( p->nType & PH7_TK_OCB ){` |
|      45 |  392 | `			if( iDepth == 0 ){` |
|      45 |  393 | `				break; /* property-hook list: the default expression ends here */` |
|       - |  394 | `			}` |
|     ! 0 |  395 | `			iDepth++;` |
|   15951 |  396 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|       - |  397 | `			/* A '(' directly after a NAME is a call. A name here is an identifier` |
|       - |  398 | ``			 * token; `new X(` is excluded by looking for the `new` operator, and`` |
|       - |  399 | ``			 * `f(...)` (first-class callable) by peeking for a lone ellipsis. */`` |
|     184 |  400 | `			if( (p->nType & PH7_TK_LPAREN) && p > pGen->pIn` |
|      32 |  401 | `				&& (p[-1].nType & PH7_TK_ID)` |
|      29 |  402 | `				&& !((p[-1].nType & PH7_TK_OP) && p[-1].pUserData` |
|     ! 0 |  403 | `					&& ((const ph7_expr_op *)p[-1].pUserData)->iOp == EXPR_OP_NEW) ){` |
|      19 |  404 | `				int bNewCtor = 0;` |
|      19 |  405 | `				SyToken *q = &p[-1];` |
|       - |  406 | ``				/* Walk back over a qualified name (A\B, A::b, $o->m) to a `new`. */`` |
|      19 |  407 | `				while( q > pGen->pIn && (q[-1].nType & (PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) ){` |
|      10 |  408 | `					if( (q[-1].nType & PH7_TK_OP) && q[-1].pUserData` |
|      13 |  409 | `						&& ((const ph7_expr_op *)q[-1].pUserData)->iOp == EXPR_OP_NEW ){` |
|      13 |  410 | `						bNewCtor = 1;` |
|      13 |  411 | `						break;` |
|       - |  412 | `					}` |
|     ! 0 |  413 | `					if( !GenStateTokenIsMemberOp(&q[-1]) && (q[-1].nType & PH7_TK_NSSEP) == 0 ){` |
|     ! 0 |  414 | `						break;` |
|       - |  415 | `					}` |
|     ! 0 |  416 | `					q--;` |
|     ! 0 |  417 | `				}` |
|      16 |  418 | `				if( !bNewCtor` |
|      14 |  419 | `					&& !(&p[1] < pGen->pEnd && (p[1].nType & PH7_TK_ELLIPSIS)) ){` |
|       6 |  420 | `					return 1;` |
|       - |  421 | `				}` |
|       6 |  422 | `			}` |
|     185 |  423 | `			iDepth++;` |
|   15857 |  424 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     185 |  425 | `			if( iDepth > 0 ){` |
|     185 |  426 | `				iDepth--;` |
|      90 |  427 | `			}` |
|      90 |  428 | `		}` |
|   15947 |  429 | `		p++;` |
|       5 |  430 | `	}` |
|    1845 |  431 | `	return 0;` |
|     930 |  432 | `}` |
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
|    1856 |  450 | `PH7_PRIVATE int PH7_GenStateInitClosureError(ph7_gen_state *pGen)` |
|       5 |  451 | `{` |
|    1861 |  452 | `	SyToken *p = pGen->pIn;` |
|    1861 |  453 | `	int iDepth = 0;` |
|   17855 |  454 | `	while( p < pGen->pEnd ){` |
|   17855 |  455 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    1809 |  456 | `			break; /* end of this initializer */` |
|       - |  457 | `		}` |
|   16046 |  458 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    8068 |  459 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      71 |  460 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|      24 |  461 | `			if( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ){` |
|       - |  462 | `				/* An arrow function is never a valid constant expression. */` |
|       3 |  463 | `				return 1;` |
|       - |  464 | `			}` |
|       - |  465 | ``			/* A closure literal must be `static function`: the modifier sits in the`` |
|       - |  466 | ``			 * token immediately before `function`. `static::X` never reaches here`` |
|       - |  467 | ``			 * (its next token is `::`, not the keyword). */`` |
|      22 |  468 | `			if( !(p > pGen->pIn && (p[-1].nType & PH7_TK_KEYWORD)` |
|      14 |  469 | `				&& SX_PTR_TO_INT(p[-1].pUserData) == PH7_TKWRD_STATIC) ){` |
|       6 |  470 | `				return 2;` |
|       - |  471 | `			}` |
|       - |  472 | ``			/* `static function(){...}`: accepted -- skip the whole construct`` |
|       - |  473 | `			 * (parameter parens then the brace-balanced body) exactly like the call` |
|       - |  474 | `			 * scanner, then keep looking for a sibling closure in the initializer. */` |
|      17 |  475 | `			p++;` |
|       - |  476 | `			{` |
|      17 |  477 | `				int iLocal = 0;` |
|      45 |  478 | `				while( p < pGen->pEnd ){` |
|      45 |  479 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|      17 |  480 | `						break;` |
|       - |  481 | `					}` |
|      31 |  482 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      17 |  483 | `						iLocal++;` |
|      24 |  484 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      17 |  485 | `						if( iLocal > 0 ){ iLocal--; }` |
|       7 |  486 | `					}` |
|      31 |  487 | `					p++;` |
|       3 |  488 | `				}` |
|      17 |  489 | `				if( p < pGen->pEnd ){` |
|      17 |  490 | `					int iBrace = 0;` |
|     121 |  491 | `					while( p < pGen->pEnd ){` |
|     121 |  492 | `						if( p->nType & PH7_TK_OCB ){` |
|      19 |  493 | `							iBrace++;` |
|     113 |  494 | `						}else if( p->nType & PH7_TK_CCB ){` |
|      19 |  495 | `							iBrace--;` |
|      19 |  496 | `							if( iBrace == 0 ){` |
|      17 |  497 | `								p++;` |
|      17 |  498 | `								break;` |
|       - |  499 | `							}` |
|       1 |  500 | `						}` |
|     107 |  501 | `						p++;` |
|       3 |  502 | `					}` |
|       7 |  503 | `				}` |
|       - |  504 | `			}` |
|      17 |  505 | `			continue;` |
|       - |  506 | `		}` |
|   16031 |  507 | `		if( p->nType & PH7_TK_OCB ){` |
|      47 |  508 | `			if( iDepth == 0 ){` |
|      47 |  509 | `				break; /* property-hook list: the default expression ends here */` |
|       - |  510 | `			}` |
|     ! 0 |  511 | `			iDepth++;` |
|   15985 |  512 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     189 |  513 | `			iDepth++;` |
|   15893 |  514 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     189 |  515 | `			if( iDepth > 0 ){` |
|     189 |  516 | `				iDepth--;` |
|      92 |  517 | `			}` |
|      92 |  518 | `		}` |
|   15985 |  519 | `		p++;` |
|       5 |  520 | `	}` |
|    1855 |  521 | `	return 0;` |
|     933 |  522 | `}` |
|       - |  523 | `/*` |
|       - |  524 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|       - |  525 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|       - |  526 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|       - |  527 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|       - |  528 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|       - |  529 | ` * share the same backing.` |
|       - |  530 | ` */` |
|     618 |  531 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|       - |  532 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|       5 |  533 | `{` |
|     623 |  534 | `	pAttr->nType = nType;` |
|     623 |  535 | `	pAttr->sClass = *pClass;` |
|     623 |  536 | `	pAttr->sTypeName = *pTypeName;` |
|     623 |  537 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  538 | `		sxu32 i;` |
|     121 |  539 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      83 |  540 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|      83 |  541 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      44 |  542 | `		}` |
|      19 |  543 | `	}` |
|     623 |  544 | `}` |
|     458 |  545 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  546 | `{` |
|     463 |  547 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  548 | `	SySet *pInstrContainer;` |
|       - |  549 | `	ph7_class_attr *pCons;` |
|       - |  550 | `	SyString *pName;` |
|       - |  551 | `	sxi32 rc;` |
|     463 |  552 | `	sxu32 nType = 0;` |
|       - |  553 | `	SyString sTypeClass;` |
|       - |  554 | `	SyString sTypeText;` |
|       - |  555 | `	SySet aUnionAlts;` |
|     463 |  556 | `	sxi32 iTypeFlags = 0;` |
|     463 |  557 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|     463 |  558 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|     463 |  559 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  560 | `	/* Extract visibility level */` |
|     463 |  561 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  562 | `	/* Mark as constant */` |
|     463 |  563 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|     463 |  564 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |  565 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|       - |  566 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|     488 |  567 | `	if( GenStateClassConstHasType(pGen) ){` |
|      79 |  568 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|      50 |  569 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|       - |  570 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|       - |  571 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|       - |  572 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|       - |  573 | `		 * and success paths release. */` |
|      54 |  574 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  575 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  576 | `			goto Synchronize;` |
|      54 |  577 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  578 | `			return SXERR_ABORT;` |
|      54 |  579 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  580 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  581 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|     ! 0 |  582 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  583 | `				return SXERR_ABORT;` |
|       - |  584 | `			}` |
|     ! 0 |  585 | `			goto Synchronize;` |
|       - |  586 | `		}` |
|      54 |  587 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      25 |  588 | `	}` |
|     229 |  589 | `loop:` |
|       - |  590 | ``	/* php 8 accepts EVERY reserved word as a class-constant name — `const list = 5`,`` |
|       - |  591 | ``	 * `const match`, `const function`, even `const true` — because a class constant is`` |
|       - |  592 | ``	 * addressed only through `C::name`, where no keyword can be ambiguous. The single`` |
|       - |  593 | ``	 * exception is `class`, reserved for `C::class`, and it gets its own message.`` |
|       - |  594 | `	 * (Method names already accept the whole set; this is the member-name side of the` |
|       - |  595 | ``	 * same rule. Global `const` is NOT the same rule: php rejects a reserved word there.)`` |
|       - |  596 | `	 * A keyword arrives as PH7_TK_KEYWORD, which this ID-only test rejected — so PHL` |
|       - |  597 | ``	 * accepted only the alpha-OPERATOR keywords (`const new`, `const and`), which the`` |
|       - |  598 | `	 * lexer marks PH7_TK_ID\|PH7_TK_OP, and that partial allow-list looked like a design. */` |
|     469 |  599 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  600 | `		/* Invalid constant name */` |
|     ! 0 |  601 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  602 | `		if( rc == SXERR_ABORT ){` |
|       - |  603 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  604 | `			return SXERR_ABORT;` |
|       - |  605 | `		}` |
|     ! 0 |  606 | `		goto Synchronize;` |
|       - |  607 | `	}` |
|       - |  608 | `	/* Peek constant name */` |
|     469 |  609 | `	pName = &pGen->pIn->sData;` |
|     464 |  610 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     272 |  611 | `		&& (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CLASS ){` |
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
|     467 |  624 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      79 |  625 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|      50 |  626 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      25 |  627 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|      54 |  628 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  629 | `			return SXERR_ABORT;` |
|      54 |  630 | `		}else if( rc != SXRET_OK ){` |
|       3 |  631 | `			goto Synchronize;` |
|       - |  632 | `		}` |
|      24 |  633 | `	}` |
|       - |  634 | `	/* Advance the stream cursor */` |
|     465 |  635 | `	pGen->pIn++;` |
|     465 |  636 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  637 | `		/* Invalid declaration */` |
|     ! 0 |  638 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  639 | `		if( rc == SXERR_ABORT ){` |
|       - |  640 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  641 | `			return SXERR_ABORT;` |
|       - |  642 | `		}` |
|     ! 0 |  643 | `		goto Synchronize;` |
|       - |  644 | `	}` |
|     465 |  645 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  646 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|       - |  647 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|       - |  648 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|       - |  649 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|     460 |  650 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
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
|     461 |  663 | `		int iClo = PH7_GenStateInitClosureError(pGen);` |
|     461 |  664 | `		if( iClo ){` |
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
|     459 |  676 | `	if( PH7_GenStateInitHasCallExpr(pGen) ){` |
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
|     459 |  687 | `	if( GenStateInitHasNewExpr(pGen) ){` |
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
|     455 |  701 | `	if( GenStateExtractConstant(pClass,pName) != 0 ){` |
|     ! 0 |  702 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  703 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|     ! 0 |  704 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  705 | `			return SXERR_ABORT;` |
|       - |  706 | `		}` |
|     ! 0 |  707 | `		goto Synchronize;` |
|       - |  708 | `	}` |
|       - |  709 | `	/* Allocate a new class attribute */` |
|     455 |  710 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|     455 |  711 | `	if( pCons ){` |
|     455 |  712 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|     455 |  713 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|     ! 0 |  714 | `			return SXERR_ABORT;` |
|       - |  715 | `		}` |
|     225 |  716 | `	}` |
|     455 |  717 | `	if( pCons == 0 ){` |
|     ! 0 |  718 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  719 | `		return SXERR_ABORT;` |
|       - |  720 | `	}` |
|     455 |  721 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      48 |  722 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      22 |  723 | `	}` |
|       - |  724 | `	/* Swap bytecode container */` |
|     455 |  725 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     455 |  726 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  727 | `	/* Compile constant value.` |
|       - |  728 | `	 */` |
|     455 |  729 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     455 |  730 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  731 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  732 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  733 | `			return SXERR_ABORT;` |
|       - |  734 | `		}` |
|       1 |  735 | `	}` |
|       - |  736 | `	/* Emit the done instruction */` |
|     455 |  737 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|     455 |  738 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     455 |  739 | `	if( rc == SXERR_ABORT ){` |
|       - |  740 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  741 | `		return SXERR_ABORT;` |
|       - |  742 | `	}` |
|       - |  743 | `	/* All done,install the constant */` |
|     455 |  744 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|     455 |  745 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  746 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  747 | `		return SXERR_ABORT;` |
|       - |  748 | `	}` |
|     455 |  749 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  750 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|       7 |  751 | `		pGen->pIn++; /* Jump the comma */` |
|       - |  752 | `		/* A reserved word is a valid name for EVERY constant in the declaration, not` |
|       - |  753 | ``		 * just the first (`const list = 1, match = 2`) — same allow-list as the head. */`` |
|       7 |  754 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  755 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  756 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  757 | `				pTok--;` |
|     ! 0 |  758 | `			}` |
|     ! 0 |  759 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  760 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  761 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  762 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  763 | `				return SXERR_ABORT;` |
|       - |  764 | `			}` |
|     ! 0 |  765 | `		}else{` |
|       7 |  766 | `			goto loop;` |
|       - |  767 | `		}` |
|     ! 0 |  768 | `	}` |
|     449 |  769 | `	SySetRelease(&aUnionAlts);` |
|     449 |  770 | `	return SXRET_OK;` |
|       7 |  771 | `Synchronize:` |
|      17 |  772 | `	SySetRelease(&aUnionAlts);` |
|       - |  773 | `	/* Synchronize with the first semi-colon */` |
|      67 |  774 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      53 |  775 | `		pGen->pIn++;` |
|       3 |  776 | `	}` |
|      17 |  777 | `	return SXERR_CORRUPT;` |
|     234 |  778 | `}` |
|       - |  779 | `/*` |
|       - |  780 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  781 | ` * According to the PHP language reference manual` |
|       - |  782 | ` *  Properties` |
|       - |  783 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  784 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  785 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  786 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  787 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  788 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  789 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  790 | ` * Symisc eXtension.` |
|       - |  791 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  792 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  793 | ` *  Example:` |
|       - |  794 | ` *   class Test{` |
|       - |  795 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  796 | ` *   };` |
|       - |  797 | ` *   var_dump(TEST::myVar);` |
|       - |  798 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  799 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  800 | ` */` |
|       - |  801 | `/*` |
|       - |  802 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  803 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  804 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  805 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  806 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  807 | ` */` |
|    3856 |  808 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  809 | `{` |
|    3861 |  810 | `	SyToken *p = pStart;` |
|    3861 |  811 | `	int bFirst = 1;` |
|    3861 |  812 | `	if( p >= pEnd ) return 0;` |
|       - |  813 | ``	/* Optional nullable `?` shorthand. */`` |
|    3861 |  814 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      61 |  815 | `		p++;` |
|      61 |  816 | `		if( p >= pEnd ) return 0;` |
|      28 |  817 | `	}` |
|       - |  818 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|       - |  819 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|       - |  820 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|       - |  821 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|    1928 |  822 | `	for(;;){` |
|    3897 |  823 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|       - |  824 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|       3 |  825 | `			p++;` |
|       9 |  826 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|       3 |  827 | `			if( p >= pEnd ) return 0;` |
|       3 |  828 | `			p++; /* skip ')' */` |
|       2 |  829 | `		}else{` |
|       - |  830 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|       - |  831 | ``			 * then any `&`-joined intersection members. */`` |
|    3895 |  832 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|    3895 |  833 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  834 | `				return 0;` |
|       - |  835 | `			}` |
|       - |  836 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|       - |  837 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|       - |  838 | `			 * may still appear at the initial dispatch site). */` |
|    3895 |  839 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|    3815 |  840 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|    3810 |  841 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    1051 |  842 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|    3295 |  843 | `					return 0;` |
|       - |  844 | `				}` |
|     260 |  845 | `			}` |
|     605 |  846 | `			p++;` |
|     607 |  847 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  848 | `				p += 2;` |
|       1 |  849 | `			}` |
|     903 |  850 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|     608 |  851 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  852 | `				p++; /* skip '&' */` |
|       3 |  853 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|       3 |  854 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|       3 |  855 | `				p++;` |
|       3 |  856 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  857 | `					p += 2;` |
|     ! 0 |  858 | `				}` |
|       1 |  859 | `			}` |
|       - |  860 | `		}` |
|     607 |  861 | `		bFirst = 0;` |
|     602 |  862 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      41 |  863 | `			&& p->sData.zString[0] == '\|' ){` |
|      41 |  864 | ``			p++; /* next `\|`-separated part */`` |
|      41 |  865 | `			continue;` |
|       - |  866 | `		}` |
|     571 |  867 | `		break;` |
|     ! 0 |  868 | `	}` |
|     571 |  869 | `	if( p >= pEnd ) return 0;` |
|     571 |  870 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|    1933 |  871 | `}` |
|       - |  872 |  |
|       - |  873 | `/*` |
|       - |  874 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  875 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  876 | ` * if not). Recognized forms:` |
|       - |  877 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  878 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  879 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  880 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  881 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  882 | ` * on unrecoverable error.` |
|       - |  883 | ` *` |
|       - |  884 | ` * When a type is parsed:` |
|       - |  885 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  886 | ` *   *pClass is set to the class name (for class types)` |
|       - |  887 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  888 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  889 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  890 | ` */` |
|     578 |  891 | `static sxi32 GenStateParsePropertyType(` |
|       - |  892 | `	ph7_gen_state *pGen,` |
|       - |  893 | `	sxu32 *pnType,` |
|       - |  894 | `	SyString *pClass,` |
|       - |  895 | `	sxi32 *piTypeFlags,` |
|       - |  896 | `	SyString *pTypeText,` |
|       - |  897 | `	SySet *pAlts` |
|       5 |  898 | `){` |
|     583 |  899 | `	sxi32 iFlags = 0;` |
|       - |  900 | `	sxi32 rc;` |
|     583 |  901 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  902 | `		return SXRET_OK;` |
|       - |  903 | `	}` |
|       - |  904 | `	/* If the first token is '$', there's no type */` |
|     583 |  905 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  906 | `		return SXRET_OK;` |
|       - |  907 | `	}` |
|     583 |  908 | `	rc = GenStateParseUnionTypeDecl(` |
|     289 |  909 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  910 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  911 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  912 | `		/* bAllowVoid */ 0,` |
|     578 |  913 | `		pGen->pIn->nLine);` |
|     583 |  914 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  915 | `		return rc;` |
|       - |  916 | `	}` |
|       - |  917 | `	/* Verify next token is '$' (start of property name) */` |
|     583 |  918 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  919 | `		return SXERR_SYNTAX;` |
|       - |  920 | `	}` |
|     583 |  921 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     583 |  922 | `	return SXRET_OK;` |
|     294 |  923 | `}` |
|       - |  924 |  |
|       - |  925 | `/*` |
|       - |  926 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  927 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  928 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  929 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  930 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  931 | ` * by the type parser itself before reaching here.` |
|       - |  932 | ` *` |
|       - |  933 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  934 | ` * use in the error message.` |
|       - |  935 | ` */` |
|     816 |  936 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  937 | `	sxu32 nType,` |
|       - |  938 | `	const SyString *pClass,` |
|       - |  939 | `	const char **pzName,` |
|       - |  940 | `	sxu32 *pnName)` |
|       5 |  941 | `{` |
|       - |  942 | `	const char *z;` |
|       - |  943 | `	sxu32 n;` |
|     821 |  944 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     727 |  945 | `		return 0;` |
|       - |  946 | `	}` |
|      99 |  947 | `	z = pClass->zString;` |
|      99 |  948 | `	n = pClass->nByte;` |
|      99 |  949 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       9 |  950 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  951 | `	}` |
|       - |  952 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - |  953 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - |  954 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      93 |  955 | `	return 0;` |
|     413 |  956 | `}` |
|       - |  957 |  |
|       - |  958 | `/*` |
|       - |  959 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|       - |  960 | ` * constant) — the main atom plus any union alternatives — against the` |
|       - |  961 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|       - |  962 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|       - |  963 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|       - |  964 | ` * type T" vs "Class constant C::X cannot have type T").` |
|       - |  965 | ` *` |
|       - |  966 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  967 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  968 | ` */` |
|     722 |  969 | `PH7_PRIVATE sxi32 GenStateValidateMemberType(` |
|       - |  970 | `	ph7_gen_state *pGen,` |
|       - |  971 | `	ph7_class *pClass,` |
|       - |  972 | `	const SyString *pMemberName,` |
|       - |  973 | `	sxu32 nType,` |
|       - |  974 | `	const SyString *pTypeClass,` |
|       - |  975 | `	const SyString *pTypeText,` |
|       - |  976 | `	SySet *pUnionAlts,` |
|       - |  977 | `	const char *zErrFmt,` |
|       - |  978 | `	sxu32 nLine)` |
|       5 |  979 | `{` |
|     727 |  980 | `	const char *zBad = 0;` |
|     727 |  981 | `	sxu32 nBad = 0;` |
|       - |  982 | `	SyString sFallback;` |
|       - |  983 | `	const SyString *pBad;` |
|       - |  984 | `	sxi32 rc;` |
|     727 |  985 | `	int bDisallowed = 0;` |
|     727 |  986 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       6 |  987 | `		bDisallowed = 1;` |
|     725 |  988 | `	}else if( pUnionAlts ){` |
|       - |  989 | `		sxu32 i;` |
|     143 |  990 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      99 |  991 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      99 |  992 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  993 | `				bDisallowed = 1;` |
|       3 |  994 | `				break;` |
|       - |  995 | `			}` |
|      51 |  996 | `		}` |
|      23 |  997 | `	}` |
|     727 |  998 | `	if( !bDisallowed ){` |
|     721 |  999 | `		return SXRET_OK;` |
|       - | 1000 | `	}` |
|       - | 1001 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - | 1002 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - | 1003 | `	 * canonical spelling if the type text is unavailable. */` |
|       9 | 1004 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       9 | 1005 | `		pBad = pTypeText;` |
|       6 | 1006 | `	}else{` |
|     ! 0 | 1007 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 | 1008 | `		pBad = &sFallback;` |
|       - | 1009 | `	}` |
|      12 | 1010 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 | 1011 | `		zErrFmt,` |
|       3 | 1012 | `		&pClass->sName,pMemberName,pBad);` |
|       9 | 1013 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1014 | `		return SXERR_ABORT;` |
|       - | 1015 | `	}` |
|       9 | 1016 | `	return SXERR_SYNTAX;` |
|     366 | 1017 | `}` |
|       - | 1018 | `/*` |
|       - | 1019 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|       - | 1020 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|       - | 1021 | ` * matched as a plain identifier in the class-member modifier position rather` |
|       - | 1022 | ` * than promoted to a lexer keyword.` |
|       - | 1023 | ` */` |
| 1859418 | 1024 | `PH7_PRIVATE int GenStateIsReadonly(SyToken *pTok)` |
|       5 | 1025 | `{` |
| 1890192 | 1026 | `	return (pTok->nType & PH7_TK_ID)` |
|  960478 | 1027 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 1890187 | 1028 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|       5 | 1029 | `}` |
|       - | 1030 | `/*` |
|       - | 1031 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|       - | 1032 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|       - | 1033 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|       - | 1034 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|       - | 1035 | ` */` |
|  275538 | 1036 | `PH7_PRIVATE sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|       5 | 1037 | `{` |
|  275543 | 1038 | `	*pnTok = 0;` |
|  275538 | 1039 | `	if( &pTok[3] < pEnd` |
|  234318 | 1040 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|  114670 | 1041 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|   18132 | 1042 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      22 | 1043 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|      22 | 1044 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|      27 | 1045 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|      23 | 1046 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|      23 | 1047 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|      23 | 1048 | `			*pnTok = 4;` |
|      23 | 1049 | `			return nKw;` |
|       - | 1050 | `		}` |
|     ! 0 | 1051 | `	}` |
|  275521 | 1052 | `	return 0;` |
|  137774 | 1053 | `}` |
|       - | 1054 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|      22 | 1055 | `PH7_PRIVATE sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|       1 | 1056 | `{` |
|      23 | 1057 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|      17 | 1058 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|       - | 1059 | `	}` |
|       7 | 1060 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|       5 | 1061 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|       - | 1062 | `	}` |
|       3 | 1063 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|      12 | 1064 | `}` |
|    1776 | 1065 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 | 1066 | `{` |
|    1781 | 1067 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 1068 | `	ph7_class_attr *pAttr;` |
|       - | 1069 | `	SyString *pName;` |
|       - | 1070 | `	sxi32 rc;` |
|    1781 | 1071 | `	sxu32 nType = 0;` |
|       - | 1072 | `	SyString sTypeClass;` |
|       - | 1073 | `	SyString sTypeText;` |
|       - | 1074 | `	SySet aUnionAlts;` |
|    1781 | 1075 | `	sxi32 iTypeFlags = 0;` |
|    1781 | 1076 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    1781 | 1077 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    1781 | 1078 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - | 1079 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|       - | 1080 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|       - | 1081 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|    1781 | 1082 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      23 | 1083 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|      10 | 1084 | `	}` |
|       - | 1085 | `	/* Extract visibility level */` |
|    1781 | 1086 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - | 1087 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|    2070 | 1088 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     583 | 1089 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     583 | 1090 | `		if( rc == SXERR_CORRUPT ){` |
|       - | 1091 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 | 1092 | `			goto Synchronize;` |
|     583 | 1093 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 | 1094 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1095 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 | 1096 | `				&pGen->pIn->sData);` |
|     ! 0 | 1097 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1098 | `				return SXERR_ABORT;` |
|       - | 1099 | `			}` |
|     ! 0 | 1100 | `			goto Synchronize;` |
|     583 | 1101 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 | 1102 | `			return SXERR_ABORT;` |
|       - | 1103 | `		}` |
|     289 | 1104 | `	}` |
|     ! 0 | 1105 | `loop:` |
|    1785 | 1106 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 1107 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 | 1108 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1109 | `			return SXERR_ABORT;` |
|       - | 1110 | `		}` |
|     ! 0 | 1111 | `		goto Synchronize;` |
|       - | 1112 | `	}` |
|    1785 | 1113 | `	pGen->pIn++; /* Jump the dollar sign */` |
|    1785 | 1114 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - | 1115 | `		/* Invalid attribute name */` |
|     ! 0 | 1116 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 | 1117 | `		if( rc == SXERR_ABORT ){` |
|       - | 1118 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1119 | `			return SXERR_ABORT;` |
|       - | 1120 | `		}` |
|     ! 0 | 1121 | `		goto Synchronize;` |
|       - | 1122 | `	}` |
|       - | 1123 | `	/* Peek attribute name */` |
|    1785 | 1124 | `	pName = &pGen->pIn->sData;` |
|       - | 1125 | `	/* Advance the stream cursor */` |
|    1785 | 1126 | `	pGen->pIn++;` |
|    1785 | 1127 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|       - | 1128 | `		/* Invalid declaration */` |
|       - | 1129 | `		/* php reports the offending token here, expecting "," or ";". */` |
|       3 | 1130 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\",\" or \";\"");` |
|       3 | 1131 | `		if( rc == SXERR_ABORT ){` |
|       - | 1132 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1133 | `			return SXERR_ABORT;` |
|       - | 1134 | `		}` |
|       3 | 1135 | `		goto Synchronize;` |
|       - | 1136 | `	}` |
|       - | 1137 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|       - | 1138 | `	 * the read visibility must not be narrower than the set visibility. */` |
|    1783 | 1139 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|      19 | 1140 | `		const char *zAvErr = 0;` |
|      28 | 1141 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|      15 | 1142 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|       3 | 1143 | `			: PH7_CLASS_PROT_PUBLIC;` |
|      19 | 1144 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|     ! 0 | 1145 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|      19 | 1146 | `		}else if( iProtection > iSetLevel ){` |
|     ! 0 | 1147 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|     ! 0 | 1148 | `		}` |
|      19 | 1149 | `		if( zAvErr ){` |
|     ! 0 | 1150 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|     ! 0 | 1151 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1152 | `				return SXERR_ABORT;` |
|       - | 1153 | `			}` |
|     ! 0 | 1154 | `			goto Synchronize;` |
|       - | 1155 | `		}` |
|       9 | 1156 | `	}` |
|       - | 1157 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|       - | 1158 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|    1783 | 1159 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      63 | 1160 | `		const char *zRoErr = 0;` |
|      63 | 1161 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       3 | 1162 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|      62 | 1163 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       6 | 1164 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|      59 | 1165 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|       6 | 1166 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|       2 | 1167 | `		}` |
|      63 | 1168 | `		if( zRoErr ){` |
|      13 | 1169 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|      13 | 1170 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1171 | `				return SXERR_ABORT;` |
|       - | 1172 | `			}` |
|      13 | 1173 | `			goto Synchronize;` |
|       - | 1174 | `		}` |
|      24 | 1175 | `	}` |
|       - | 1176 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - | 1177 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - | 1178 | `	 * by the type parser. */` |
|    1773 | 1179 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     869 | 1180 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - | 1181 | `			&sTypeText,` |
|     576 | 1182 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|     288 | 1183 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     581 | 1184 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1185 | `			return SXERR_ABORT;` |
|     581 | 1186 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 | 1187 | `			goto Synchronize;` |
|       - | 1188 | `		}` |
|     288 | 1189 | `	}` |
|       - | 1190 | `	/* Reject redeclaration (catches clash with an earlier promoted property).` |
|       - | 1191 | `	 * A same-name class CONSTANT is NOT a clash — php's separate namespaces let` |
|       - | 1192 | ``	 * `const C` and `public $C` coexist (stored in disjoint hConst / hAttr tables). */`` |
|    1773 | 1193 | `	if( GenStateExtractProperty(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 | 1194 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 | 1195 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 | 1196 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1197 | `			return SXERR_ABORT;` |
|       - | 1198 | `		}` |
|       3 | 1199 | `		goto Synchronize;` |
|       - | 1200 | `	}` |
|       - | 1201 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|       - | 1202 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|       - | 1203 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|       - | 1204 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|       - | 1205 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|       - | 1206 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|       - | 1207 | ``	/* php: a property default holding a closure must use `static function` too. */`` |
|    1771 | 1208 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|    1263 | 1209 | `		int iClo = PH7_GenStateInitClosureError(pGen);` |
|    1263 | 1210 | `		if( iClo ){` |
|     ! 0 | 1211 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 1212 | `				iClo == 2 ? "Closures in constant expressions must be static"` |
|       - | 1213 | `				          : "Constant expression contains invalid operations");` |
|     ! 0 | 1214 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1215 | `				return SXERR_ABORT;` |
|       - | 1216 | `			}` |
|     ! 0 | 1217 | `			goto Synchronize;` |
|       - | 1218 | `		}` |
|     629 | 1219 | `	}` |
|       - | 1220 | `	/* php: a property default may not CALL anything either. */` |
|    1771 | 1221 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && PH7_GenStateInitHasCallExpr(pGen) ){` |
|     ! 0 | 1222 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1223 | `			"Constant expression contains invalid operations");` |
|     ! 0 | 1224 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1225 | `			return SXERR_ABORT;` |
|       - | 1226 | `		}` |
|     ! 0 | 1227 | `		goto Synchronize;` |
|       - | 1228 | `	}` |
|    1771 | 1229 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|       6 | 1230 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1231 | `			"New expressions are not supported in this context");` |
|       6 | 1232 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1233 | `			return SXERR_ABORT;` |
|       - | 1234 | `		}` |
|       6 | 1235 | `		goto Synchronize;` |
|       - | 1236 | `	}` |
|       - | 1237 | `	/* Allocate a new class attribute */` |
|    1767 | 1238 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    1767 | 1239 | `	if( pAttr ){` |
|    1767 | 1240 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|    1767 | 1241 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|     ! 0 | 1242 | `			return SXERR_ABORT;` |
|       - | 1243 | `		}` |
|     881 | 1244 | `	}` |
|    1767 | 1245 | `	if( pAttr == 0 ){` |
|     ! 0 | 1246 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1247 | `		return SXERR_ABORT;` |
|       - | 1248 | `	}` |
|    1767 | 1249 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     579 | 1250 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|     287 | 1251 | `	}` |
|    1767 | 1252 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - | 1253 | `		SySet *pInstrContainer;` |
|    1259 | 1254 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|    1259 | 1255 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - | 1256 | `		{` |
|       - | 1257 | `			/* Delimit the default expression: it ends at the declaration's` |
|       - | 1258 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|       - | 1259 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|       - | 1260 | `			 * compiler would otherwise run into the hook tokens. */` |
|    1259 | 1261 | `			SyToken *pScan = pGen->pIn;` |
|    1259 | 1262 | `			sxi32 iNest = 0;` |
|   11055 | 1263 | `			while( pScan < pGen->pEnd ){` |
|   11055 | 1264 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     141 | 1265 | `					iNest++;` |
|   10987 | 1266 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|     141 | 1267 | `					iNest--;` |
|   10851 | 1268 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|    1259 | 1269 | `					break;` |
|       - | 1270 | `				}` |
|    9801 | 1271 | `				pScan++;` |
|       5 | 1272 | `			}` |
|    1259 | 1273 | `			pGen->pEnd = pScan;` |
|       - | 1274 | `		}` |
|       - | 1275 | `		/* Swap bytecode container */` |
|    1259 | 1276 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    1259 | 1277 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - | 1278 | `		/* Compile attribute value. The default is a const-expression belonging to` |
|       - | 1279 | `		 * pClass (see iInMemberDefault) — __TRAIT__ in it reads pCurClass. */` |
|    1259 | 1280 | `		pGen->iInMemberDefault++;` |
|    1259 | 1281 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    1259 | 1282 | `		pGen->iInMemberDefault--;` |
|    1259 | 1283 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 1284 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 | 1285 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1286 | `				return SXERR_ABORT;` |
|       - | 1287 | `			}` |
|     ! 0 | 1288 | `		}` |
|       - | 1289 | `		/* Emit the done instruction */` |
|    1259 | 1290 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    1259 | 1291 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    1259 | 1292 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|    1259 | 1293 | `		pGen->pEnd = pSavedDefEnd;` |
|     627 | 1294 | `	}` |
|       - | 1295 | `	/* All done,install the attribute */` |
|    1767 | 1296 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|    1767 | 1297 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1298 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 1299 | `		return SXERR_ABORT;` |
|       - | 1300 | `	}` |
|    1767 | 1301 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|       - | 1302 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|       - | 1303 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|     159 | 1304 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|     159 | 1305 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1306 | `			return SXERR_ABORT;` |
|       - | 1307 | `		}` |
|     159 | 1308 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1309 | `			goto Synchronize;` |
|       - | 1310 | `		}` |
|     159 | 1311 | `		SySetRelease(&aUnionAlts);` |
|     159 | 1312 | `		return SXRET_OK;` |
|       - | 1313 | `	}` |
|    1613 | 1314 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 1315 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|       - | 1316 | `		 * wording differs per declaration site) */` |
|     ! 0 | 1317 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 1318 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|       - | 1319 | `				? "Interfaces may only include hooked properties"` |
|       - | 1320 | `				: "Only hooked properties may be declared abstract");` |
|     ! 0 | 1321 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1322 | `			return SXERR_ABORT;` |
|       - | 1323 | `		}` |
|     ! 0 | 1324 | `		goto Synchronize;` |
|       - | 1325 | `	}` |
|    1613 | 1326 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 1327 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 | 1328 | `		pGen->pIn++; /* Jump the comma */` |
|       5 | 1329 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 1330 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 1331 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 1332 | `				pTok--;` |
|     ! 0 | 1333 | `			}` |
|     ! 0 | 1334 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 1335 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 | 1336 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 1337 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1338 | `				return SXERR_ABORT;` |
|       - | 1339 | `			}` |
|     ! 0 | 1340 | `		}else{` |
|       5 | 1341 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 | 1342 | `				goto loop;` |
|       - | 1343 | `			}` |
|       - | 1344 | `		}` |
|     ! 0 | 1345 | `	}` |
|    1609 | 1346 | `	SySetRelease(&aUnionAlts);` |
|    1609 | 1347 | `	return SXRET_OK;` |
|       9 | 1348 | `Synchronize:` |
|       - | 1349 | `	/* Synchronize with the first semi-colon */` |
|      56 | 1350 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      38 | 1351 | `		pGen->pIn++;` |
|       4 | 1352 | `	}` |
|      22 | 1353 | `	SySetRelease(&aUnionAlts);` |
|      22 | 1354 | `	return SXERR_CORRUPT;` |
|     893 | 1355 | `}` |
|       - | 1356 | `/*` |
|       - | 1357 | ` * php validates a magic method's DECLARATION at compile time` |
|       - | 1358 | ` * (zend_check_magic_method_implementation): the ENGINE builds the arguments and` |
|       - | 1359 | ` * calls these methods on its own, so a wrong shape is rejected where it is` |
|       - | 1360 | ` * written rather than discovered — or silently tolerated — at the dispatch.` |
|       - | 1361 | ` *` |
|       - | 1362 | ` * One row per magic name; its fields are the checks php makes for that name.` |
|       - | 1363 | ` * Arity is the first of them, in php's order — which is what decides the message` |
|       - | 1364 | ` * when a declaration breaks more than one of php's rules at once.` |
|       - | 1365 | ` */` |
|       - | 1366 | `typedef struct MagicMethodRule MagicMethodRule;` |
|       - | 1367 | `struct MagicMethodRule` |
|       - | 1368 | `{` |
|       - | 1369 | `	const char *zName; /* Magic method name */` |
|       - | 1370 | `	sxu32 nName;       /* Its length */` |
|       - | 1371 | `	int nArgs;         /* Declared arguments php requires, -1 when it does not check */` |
|       - | 1372 | `	int bStatic;       /* TRUE: must be static · FALSE: must NOT be static */` |
|       - | 1373 | `	int bPublic;       /* TRUE: must be public — a WARNING, and dispatched anyway */` |
|       - | 1374 | `	int bNoReturnType; /* TRUE: declaring ANY return type is a fatal */` |
|       - | 1375 | `};` |
|       - | 1376 | `#define MAGIC_METHOD_ROW(N,A,S,P,R) { N, sizeof(N)-1, A, S, P, R }` |
|       - | 1377 | `static const MagicMethodRule aMagicMethod[] = {` |
|       - | 1378 | `	MAGIC_METHOD_ROW("__construct",  -1, FALSE, FALSE, TRUE),` |
|       - | 1379 | `	MAGIC_METHOD_ROW("__destruct",    0, FALSE, FALSE, TRUE),` |
|       - | 1380 | `	MAGIC_METHOD_ROW("__clone",       0, FALSE, FALSE, FALSE),` |
|       - | 1381 | `	MAGIC_METHOD_ROW("__get",         1, FALSE, TRUE,  FALSE),` |
|       - | 1382 | `	MAGIC_METHOD_ROW("__set",         2, FALSE, TRUE,  FALSE),` |
|       - | 1383 | `	MAGIC_METHOD_ROW("__isset",       1, FALSE, TRUE,  FALSE),` |
|       - | 1384 | `	MAGIC_METHOD_ROW("__unset",       1, FALSE, TRUE,  FALSE),` |
|       - | 1385 | `	MAGIC_METHOD_ROW("__call",        2, FALSE, TRUE,  FALSE),` |
|       - | 1386 | `	MAGIC_METHOD_ROW("__callStatic",  2, TRUE,  TRUE,  FALSE),` |
|       - | 1387 | `	MAGIC_METHOD_ROW("__toString",    0, FALSE, TRUE,  FALSE),` |
|       - | 1388 | `	MAGIC_METHOD_ROW("__invoke",     -1, FALSE, TRUE,  FALSE),` |
|       - | 1389 | `	MAGIC_METHOD_ROW("__debugInfo",   0, FALSE, TRUE,  FALSE),` |
|       - | 1390 | `	MAGIC_METHOD_ROW("__serialize",   0, FALSE, TRUE,  FALSE),` |
|       - | 1391 | `	MAGIC_METHOD_ROW("__unserialize", 1, FALSE, TRUE,  FALSE),` |
|       - | 1392 | `	MAGIC_METHOD_ROW("__sleep",       0, FALSE, TRUE,  FALSE),` |
|       - | 1393 | `	MAGIC_METHOD_ROW("__wakeup",      0, FALSE, TRUE,  FALSE),` |
|       - | 1394 | `	MAGIC_METHOD_ROW("__set_state",   1, TRUE,  TRUE,  FALSE)` |
|       - | 1395 | `};` |
|       - | 1396 | `#undef MAGIC_METHOD_ROW` |
|       - | 1397 | `/*` |
|       - | 1398 | ` * Find the rule for a declared method name, or 0 when the name is not magic.` |
|       - | 1399 | `` * php matches method names case-insensitively everywhere, so `__GET` is `__get`;`` |
|       - | 1400 | ` * the two-underscore prefix test is php's own cheap reject.` |
|       - | 1401 | ` */` |
|  104366 | 1402 | `static const MagicMethodRule * GenStateMagicMethodRule(const SyString *pName)` |
|       5 | 1403 | `{` |
|       - | 1404 | `	sxu32 n;` |
|  104371 | 1405 | `	if( pName->nByte < sizeof("__x")-1 \|\| pName->zString[0] != '_' \|\| pName->zString[1] != '_' ){` |
|    2397 | 1406 | `		return 0;` |
|       - | 1407 | `	}` |
| 1113489 | 1408 | `	for( n = 0 ; n < SX_ARRAYSIZE(aMagicMethod) ; ++n ){` |
| 1113476 | 1409 | `		if( pName->nByte == aMagicMethod[n].nName` |
|  608053 | 1410 | `		 && SyStrnicmp(pName->zString,aMagicMethod[n].zName,aMagicMethod[n].nName) == 0 ){` |
|  101971 | 1411 | `			return &aMagicMethod[n];` |
|       - | 1412 | `		}` |
|  505760 | 1413 | `	}` |
|      10 | 1414 | `	return 0;` |
|   52188 | 1415 | `}` |
|       - | 1416 | `/*` |
|       - | 1417 | ` * TRUE when php requires this magic method to be PUBLIC: the rows it merely` |
|       - | 1418 | ` * WARNS about at the declaration and then dispatches regardless of what the` |
|       - | 1419 | ` * declaration said. The runtime's visibility gate reads this to let an` |
|       - | 1420 | ` * engine-built call through — a call the user WROTE stays denied.` |
|       - | 1421 | ` *` |
|       - | 1422 | `` * `__construct`/`__destruct`/`__clone` are deliberately not in the set: a`` |
|       - | 1423 | ` * private constructor is the singleton idiom, and php enforces those three at` |
|       - | 1424 | ` * the call like any other method.` |
|       - | 1425 | ` */` |
|  101134 | 1426 | `PH7_PRIVATE int PH7_MagicMethodMustBePublic(const SyString *pName)` |
|       5 | 1427 | `{` |
|  101139 | 1428 | `	const MagicMethodRule *pRule = GenStateMagicMethodRule(pName);` |
|  101139 | 1429 | `	return pRule != 0 && pRule->bPublic;` |
|       5 | 1430 | `}` |
|       - | 1431 | `/*` |
|       - | 1432 | ` * Enforce the rules of pRule against the declaration just parsed. pName is the` |
|       - | 1433 | ` * name AS WRITTEN — php quotes that spelling, not the canonical one.` |
|       - | 1434 | ` *` |
|       - | 1435 | ` * The diagnostic is FORMATTED, not reported: php decides these rules while the` |
|       - | 1436 | ` * signature is in hand (so the arity beats __toString's return-type rule) but` |
|       - | 1437 | ` * raises them only once the declaration has cleared the checks php makes` |
|       - | 1438 | ` * first — the redeclaration and abstract-placement rules, and any parse error` |
|       - | 1439 | ` * in the body php has already read. The caller reports the buffer at that` |
|       - | 1440 | ` * point.` |
|       - | 1441 | ` *` |
|       - | 1442 | ` * Returns the severity it wrote: E_ERROR, E_WARNING, or 0 for a clean` |
|       - | 1443 | ` * declaration.` |
|       - | 1444 | ` */` |
|    3232 | 1445 | `static sxi32 GenStateCheckMagicMethod(` |
|       - | 1446 | `	ph7_class *pClass,` |
|       - | 1447 | `	const SyString *pName,` |
|       - | 1448 | `	ph7_class_method *pMeth,` |
|       - | 1449 | `	char *zErr,` |
|       - | 1450 | `	int nErrBuf` |
|       - | 1451 | `	)` |
|       5 | 1452 | `{` |
|    3237 | 1453 | `	const MagicMethodRule *pRule = GenStateMagicMethodRule(pName);` |
|    3237 | 1454 | `	if( pRule == 0 ){` |
|    2405 | 1455 | `		return 0;` |
|       - | 1456 | `	}` |
|     837 | 1457 | `	if( pRule->nArgs >= 0 ){` |
|       - | 1458 | `		/* php counts DECLARED parameters — an optional one counts` |
|       - | 1459 | ``		 * (`__destruct($a = null)` is rejected) and the variadic tail does not`` |
|       - | 1460 | ``		 * (`__clone(...$a)` declares zero and passes, `__get(...$a)` declares`` |
|       - | 1461 | `		 * zero where one is required and does not). */` |
|     445 | 1462 | `		sxu32 nDecl = SySetUsed(&pMeth->sFunc.aArgs);` |
|     445 | 1463 | `		sxu32 nGiven = 0;` |
|       - | 1464 | `		sxu32 n;` |
|     817 | 1465 | `		for( n = 0 ; n < nDecl ; ++n ){` |
|     377 | 1466 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,n);` |
|     377 | 1467 | `			if( pArg && (pArg->iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|     375 | 1468 | `				nGiven++;` |
|     185 | 1469 | `			}` |
|     191 | 1470 | `		}` |
|     445 | 1471 | `		if( nGiven != (sxu32)pRule->nArgs ){` |
|       9 | 1472 | `			if( pRule->nArgs == 0 ){` |
|       4 | 1473 | `				SyBufferFormat(zErr,nErrBuf,"Method %z::%z() cannot take arguments",` |
|       1 | 1474 | `					&pClass->sName,pName);` |
|       2 | 1475 | `			}else{` |
|       8 | 1476 | `				SyBufferFormat(zErr,nErrBuf,"Method %z::%z() must take exactly %d argument%s",` |
|       6 | 1477 | `					&pClass->sName,pName,pRule->nArgs,pRule->nArgs == 1 ? "" : "s");` |
|       - | 1478 | `			}` |
|       9 | 1479 | `			return E_ERROR;` |
|       - | 1480 | `		}` |
|       - | 1481 | `		/* None of the arguments the engine builds may be by-reference — there is` |
|       - | 1482 | `		 * no caller variable to write back to. php checks as many arguments as` |
|       - | 1483 | `		 * the rule counts, and the count above already skipped variadics, so` |
|       - | 1484 | `		 * this walk skips them the same way. */` |
|     801 | 1485 | `		for( n = 0 ; n < nDecl ; ++n ){` |
|     369 | 1486 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,n);` |
|     369 | 1487 | `			if( pArg == 0 \|\| (pArg->iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|       3 | 1488 | `				continue;` |
|       - | 1489 | `			}` |
|     367 | 1490 | `			if( pArg->iFlags & VM_FUNC_ARG_BY_REF ){` |
|       4 | 1491 | `				SyBufferFormat(zErr,nErrBuf,"Method %z::%z() cannot take arguments by reference",` |
|       1 | 1492 | `					&pClass->sName,pName);` |
|       3 | 1493 | `				return E_ERROR;` |
|       - | 1494 | `			}` |
|     185 | 1495 | `		}` |
|     216 | 1496 | `	}` |
|       - | 1497 | `	/* Static-ness. Whether the engine has a receiver for a magic method is not` |
|       - | 1498 | ``	 * the declaration's to choose: `__callStatic` and `__set_state` are reached`` |
|       - | 1499 | `	 * with a class and nothing else, every other row is reached through an` |
|       - | 1500 | `	 * object. PHL took the declaration at its word and then dispatched the` |
|       - | 1501 | ``	 * method anyway — a `static function __get()` ran with no `$this` at all,`` |
|       - | 1502 | `	 * so a hook property table or a lazy-loading accessor read whatever the` |
|       - | 1503 | `	 * unbound scope happened to hold. php checks this after the arity, which is` |
|       - | 1504 | ``	 * why `static function __get($a,$b)` reports the count first. */`` |
|     829 | 1505 | `	if( pRule->bStatic != ((pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0) ){` |
|       8 | 1506 | `		SyBufferFormat(zErr,nErrBuf,"Method %z::%z() %s be static",` |
|       4 | 1507 | `			&pClass->sName,pName,pRule->bStatic ? "must" : "cannot");` |
|       6 | 1508 | `		return E_ERROR;` |
|       - | 1509 | `	}` |
|       - | 1510 | `	/* Visibility. This one is a WARNING: php names the declaration and then` |
|       - | 1511 | ``	 * dispatches the method anyway, because the engine calling `__get` is not`` |
|       - | 1512 | `	 * the outside world reaching for a private member. PHL was silent at the` |
|       - | 1513 | ``	 * declaration and threw `Call to private method C::__get()` at the ACCESS —`` |
|       - | 1514 | `	 * the one rule of this family that changed what a program php RUNS does,` |
|       - | 1515 | `	 * and it killed the script. The dispatch half is` |
|       - | 1516 | `	 * PH7_MagicMethodMustBePublic, read by the runtime visibility gate. */` |
|     825 | 1517 | `	if( pRule->bPublic && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|      51 | 1518 | `		SyBufferFormat(zErr,nErrBuf,"The magic method %z::%z() must have public visibility",` |
|      16 | 1519 | `			&pClass->sName,pName);` |
|      35 | 1520 | `		return E_WARNING;` |
|       - | 1521 | `	}` |
|       - | 1522 | ``	/* A return type on the two methods that have no return VALUE. `new C` is the`` |
|       - | 1523 | `	 * instance, never whatever __construct returned, and __destruct is called by` |
|       - | 1524 | `	 * the engine at a point with nowhere to put an answer — so php rejects any` |
|       - | 1525 | ``	 * declared type on either, `void` and `never` included, rather than let a`` |
|       - | 1526 | ``	 * declaration promise something no caller can read. (`__clone` is NOT in`` |
|       - | 1527 | ``	 * this row: `: void` on it is valid php.) PHL enforced the declared type at`` |
|       - | 1528 | ``	 * runtime instead, so `__construct(): int` raised a TypeError at every`` |
|       - | 1529 | `	 * instantiation — a diagnostic on the CALL for a mistake in the` |
|       - | 1530 | `	 * declaration. */` |
|     788 | 1531 | `	if( pRule->bNoReturnType` |
|     731 | 1532 | `	 && (pMeth->sFunc.nReturnType > 0` |
|     332 | 1533 | `	  \|\| SyStringLength(&pMeth->sFunc.sReturnClass) > 0` |
|     330 | 1534 | `	  \|\| SySetUsed(&pMeth->sFunc.aReturnUnion) > 0) ){` |
|       8 | 1535 | `		SyBufferFormat(zErr,nErrBuf,"Method %z::%z() cannot declare a return type",` |
|       2 | 1536 | `			&pClass->sName,pName);` |
|       6 | 1537 | `		return E_ERROR;` |
|       - | 1538 | `	}` |
|     789 | 1539 | `	return 0;` |
|    1621 | 1540 | `}` |
|       - | 1541 | `/*` |
|       - | 1542 | ` * Raise the declaration diagnostic parked above (GenStateCheckMagicMethod's magic-method` |
|       - | 1543 | ` * rules, or the final-private one beside its call), once, and disarm it.` |
|       - | 1544 | ` * Suppressed when this declaration has already reported a fatal php decides` |
|       - | 1545 | ` * FIRST — a redeclaration, an abstract method in a non-abstract class, a parse` |
|       - | 1546 | ` * error in the body — since php stops at its own first fatal.` |
|       - | 1547 | ` */` |
|    3216 | 1548 | `static sxi32 GenStateRaiseMagicDiag(` |
|       - | 1549 | `	ph7_gen_state *pGen,` |
|       - | 1550 | `	sxi32 *pnSeverity,   /* IN/OUT: the parked severity, zeroed here */` |
|       - | 1551 | `	const char *zErr,` |
|       - | 1552 | `	sxu32 nLine,` |
|       - | 1553 | `	sxu32 nErrEntry      /* pGen->nErr when this declaration started */` |
|       - | 1554 | `	)` |
|       5 | 1555 | `{` |
|    3221 | 1556 | `	sxi32 rc = SXRET_OK;` |
|    3221 | 1557 | `	if( *pnSeverity != 0 ){` |
|      54 | 1558 | `		if( pGen->nErr == nErrEntry ){` |
|      54 | 1559 | `			rc = PH7_GenCompileError(pGen,*pnSeverity,nLine,"%s",zErr);` |
|      25 | 1560 | `		}` |
|      54 | 1561 | `		*pnSeverity = 0;` |
|      25 | 1562 | `	}` |
|    3221 | 1563 | `	return rc;` |
|       5 | 1564 | `}` |
|       - | 1565 | `/*` |
|       - | 1566 | ` * Compile a class method.` |
|       - | 1567 | ` *` |
|       - | 1568 | ` * Refer to the official documentation for more information` |
|       - | 1569 | ` * on the powerful extension introduced by the PH7 engine` |
|       - | 1570 | ` * to the OO subsystem such as full type hinting,method` |
|       - | 1571 | ` * overloading and many more.` |
|       - | 1572 | ` */` |
|    3234 | 1573 | `static sxi32 GenStateCompileClassMethod(` |
|       - | 1574 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 1575 | `	sxi32 iProtection,   /* Visibility level */` |
|       - | 1576 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - | 1577 | `	int doBody,          /* TRUE to process method body */` |
|       - | 1578 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - | 1579 | `	)` |
|       5 | 1580 | `{` |
|    3239 | 1581 | `	sxu32 nLine = pGen->pIn->nLine;` |
|    3239 | 1582 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    3239 | 1583 | `	sxu32 nErrEntry = pGen->nErr; /* Errors already reported when this declaration started */` |
|       - | 1584 | `	char zMagicErr[256];          /* Pending magic-method rule violation, reported at the end */` |
|    3239 | 1585 | `	sxi32 nMagicSeverity = 0;     /* E_ERROR / E_WARNING while zMagicErr is still unreported */` |
|    3239 | 1586 | `	int bMagicFatal = FALSE;      /* The parked diagnostic was a fatal: do not install the method */` |
|       - | 1587 | `	ph7_class_method *pMeth;` |
|       - | 1588 | `	sxi32 iFuncFlags;` |
|       - | 1589 | `	SyString *pName;` |
|       - | 1590 | `	SyToken *pEnd;` |
|       - | 1591 | `	sxi32 rc;` |
|       - | 1592 | `	/* Extract visibility level */` |
|    3239 | 1593 | `	iProtection = GetProtectionLevel(iProtection);` |
|    3239 | 1594 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|    3239 | 1595 | `	iFuncFlags = 0;` |
|    3239 | 1596 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 1597 | `		/* Invalid method name */` |
|     ! 0 | 1598 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 1599 | `		if( rc == SXERR_ABORT ){` |
|       - | 1600 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1601 | `			return SXERR_ABORT;` |
|       - | 1602 | `		}` |
|     ! 0 | 1603 | `		goto Synchronize;` |
|       - | 1604 | `	}` |
|    3239 | 1605 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 1606 | `		/* Return by reference,remember that */` |
|       6 | 1607 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 1608 | `		/* Jump the '&' token */` |
|       6 | 1609 | `		pGen->pIn++;` |
|       2 | 1610 | `	}` |
|    3239 | 1611 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 1612 | `		/* Invalid method name */` |
|     ! 0 | 1613 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 1614 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1615 | `			return SXERR_ABORT;` |
|       - | 1616 | `		}` |
|     ! 0 | 1617 | `		goto Synchronize;` |
|       - | 1618 | `	}` |
|       - | 1619 | `	/* Peek method name */` |
|    3239 | 1620 | `	pName = &pGen->pIn->sData;` |
|    3239 | 1621 | `	nLine = pGen->pIn->nLine;` |
|       - | 1622 | `	/* Jump the method name */` |
|    3239 | 1623 | `	pGen->pIn++;` |
|    3239 | 1624 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 1625 | `		/* Abstract method */` |
|     107 | 1626 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 1627 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1628 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 | 1629 | `				&pClass->sName,pName);` |
|     ! 0 | 1630 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1631 | `				return SXERR_ABORT;` |
|       - | 1632 | `			}` |
|     ! 0 | 1633 | `		}` |
|       - | 1634 | `		/* Assemble method signature only */` |
|     107 | 1635 | `		doBody = FALSE;` |
|      51 | 1636 | `	}` |
|    3239 | 1637 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 1638 | `		/* Syntax error */` |
|     ! 0 | 1639 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 | 1640 | `		if( rc == SXERR_ABORT ){` |
|       - | 1641 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1642 | `			return SXERR_ABORT;` |
|       - | 1643 | `		}` |
|     ! 0 | 1644 | `		goto Synchronize;` |
|       - | 1645 | `	}` |
|       - | 1646 | `	/* Allocate a new class_method instance */` |
|    3239 | 1647 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|    3239 | 1648 | `	if( pMeth == 0 ){` |
|     ! 0 | 1649 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 1650 | `		return SXERR_ABORT;` |
|       - | 1651 | `	}` |
|    3239 | 1652 | `	pMeth->sFunc.nLine = nKwLine;` |
|    3239 | 1653 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|    3239 | 1654 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|     ! 0 | 1655 | `		return SXERR_ABORT;` |
|       - | 1656 | `	}` |
|       - | 1657 | `	/* Jump the left parenthesis '(' */` |
|    3239 | 1658 | `	pGen->pIn++;` |
|    3239 | 1659 | `	pEnd = 0; /* cc warning */` |
|       - | 1660 | `	/* Delimit the method signature */` |
|    3239 | 1661 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3239 | 1662 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 1663 | `		/* Syntax error */` |
|       3 | 1664 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 | 1665 | `		if( rc == SXERR_ABORT ){` |
|       - | 1666 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1667 | `			return SXERR_ABORT;` |
|       - | 1668 | `		}` |
|       3 | 1669 | `		goto Synchronize;` |
|       - | 1670 | `	}` |
|       - | 1671 | `	{` |
|    3237 | 1672 | `		int bIsCtor = 0;` |
|    3237 | 1673 | `		int bAbstractCtor = 0;` |
|       - | 1674 | `		/* Only __construct is the constructor (PHP-4 class-name constructors removed` |
|       - | 1675 | `		 * in 8.0): a method named like the class is a plain method, so promoted` |
|       - | 1676 | `		 * properties in it are rejected exactly as php does elsewhere. */` |
|    3232 | 1677 | `		if( pName->nByte == sizeof("__construct") - 1` |
|    1868 | 1678 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0 ){` |
|     323 | 1679 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 | 1680 | `				bAbstractCtor = 1;` |
|       2 | 1681 | `			}else{` |
|     321 | 1682 | `				bIsCtor = 1;` |
|       - | 1683 | `			}` |
|     159 | 1684 | `		}` |
|    3237 | 1685 | `		if( pGen->pIn < pEnd ){` |
|       - | 1686 | `			/* Collect method arguments */` |
|    1223 | 1687 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|    1223 | 1688 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1689 | `				return SXERR_ABORT;` |
|       - | 1690 | `			}` |
|     609 | 1691 | `		}` |
|       - | 1692 | `	}` |
|       - | 1693 | `	/* Point past ')' and parse optional return type ': type' */` |
|    3237 | 1694 | `	pGen->pIn = &pEnd[1];` |
|       - | 1695 | `	{` |
|    3237 | 1696 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|    3237 | 1697 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 | 1698 | `			return SXERR_ABORT;` |
|    3237 | 1699 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 | 1700 | `			goto Synchronize;` |
|       - | 1701 | `		}` |
|       - | 1702 | `	}` |
|       - | 1703 | `	/* php's compile-time magic-method declaration rules, DECIDED here — with the` |
|       - | 1704 | `	 * signature in hand and before the __toString return-type rule below, which` |
|       - | 1705 | ``	 * is php's own order (`static function __toString($a): int` reports the`` |
|       - | 1706 | `	 * arity). Reported at the end of this function; see zMagicErr there. */` |
|    3237 | 1707 | `	nMagicSeverity = GenStateCheckMagicMethod(pClass,pName,pMeth,zMagicErr,(int)sizeof(zMagicErr));` |
|    3232 | 1708 | `	if( nMagicSeverity == 0` |
|    3208 | 1709 | `	 && (pMeth->iFlags & PH7_CLASS_ATTR_FINAL)` |
|    1603 | 1710 | `	 && pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       - | 1711 | `		/* Not a magic rule, but the same KIND of rule and the same parking: php` |
|       - | 1712 | `		 * checks a declaration php itself decides the meaning of. A private method` |
|       - | 1713 | ``		 * is never overridden, so `final` on one says nothing — php WARNS here (8.0+)`` |
|       - | 1714 | `		 * and compiles the class. PHL was silent at the declaration and then fataled` |
|       - | 1715 | `		 * at the SUBCLASS that reused the name ("Cannot override final method"), a` |
|       - | 1716 | `		 * class php accepts; the inheritance half is in PH7_ClassInherit. */` |
|       3 | 1717 | `		SyBufferFormat(zMagicErr,sizeof(zMagicErr),` |
|       - | 1718 | `			"Private methods cannot be final as they are never overridden by other classes");` |
|       3 | 1719 | `		nMagicSeverity = E_WARNING;` |
|       1 | 1720 | `	}` |
|    3237 | 1721 | `	if( nMagicSeverity == E_ERROR ){` |
|       - | 1722 | `		/* Suppress the __toString rule below: php never reaches it on a` |
|       - | 1723 | `		 * declaration the magic rules already rejected. */` |
|      20 | 1724 | `		bMagicFatal = TRUE;` |
|      20 | 1725 | `		goto SkipToStringType;` |
|       - | 1726 | `	}` |
|       - | 1727 | `	/*` |
|       - | 1728 | ``	 * php gives __toString() an IMPLICIT `string` return type. That is what makes`` |
|       - | 1729 | ``	 * `return 42` coerce to "42" and `return null` / an array / an object / falling`` |
|       - | 1730 | `	 * off the end raise` |
|       - | 1731 | `	 *   C::__toString(): Return value must be of type string, X returned` |
|       - | 1732 | `	 * PHL enforced DECLARED return types only, so an undeclared __toString could` |
|       - | 1733 | `	 * answer anything and MemObjStringValue fell back to the "Object" placeholder` |
|       - | 1734 | `	 * for whatever was not a non-empty string. Installing the type here reuses the` |
|       - | 1735 | `	 * enforcement that already matches php byte for byte.` |
|       - | 1736 | `	 *` |
|       - | 1737 | `	 * sReturnTypeName is filled in as well, for two reasons: reflection reports the` |
|       - | 1738 | `	 * implicit type exactly as php does (hasReturnType() TRUE, getReturnType()` |
|       - | 1739 | `	 * "string" for an undeclared __toString), and the generator-return-type fatal` |
|       - | 1740 | ``	 * renders from it — so a __toString with a `yield` in it now reports php's`` |
|       - | 1741 | `	 * "Generator return type must be a supertype of Generator, string given".` |
|       - | 1742 | `	 *` |
|       - | 1743 | ``	 * Declaring any OTHER return type is php's own compile fatal, `?string`, a`` |
|       - | 1744 | ``	 * union, `mixed`, `static` and `void` included. (php checks`` |
|       - | 1745 | ``	 * "A void method must not return a value" FIRST when a `: void` __toString also`` |
|       - | 1746 | `	 * returns a value; PHL has no such check yet, so it reports this one instead —` |
|       - | 1747 | `	 * both reject, on doubly-invalid input only.)` |
|       - | 1748 | `	 */` |
|    3216 | 1749 | `	if( pName->nByte == sizeof("__toString")-1` |
|    1767 | 1750 | `	 && SyStrnicmp(pName->zString,"__toString",sizeof("__toString")-1) == 0 ){` |
|     121 | 1751 | `		ph7_vm_func *pTsFunc = &pMeth->sFunc;` |
|     121 | 1752 | `		int bTsDeclared = pTsFunc->nReturnType > 0 \|\| SySetUsed(&pTsFunc->aReturnUnion) > 0;` |
|     121 | 1753 | `		if( bTsDeclared ){` |
|      78 | 1754 | `			if( pTsFunc->nReturnType != MEMOBJ_STRING` |
|      77 | 1755 | `			 \|\| SySetUsed(&pTsFunc->aReturnUnion) > 0` |
|      81 | 1756 | `			 \|\| (pTsFunc->iFlags & VM_FUNC_RETURN_NULLABLE) ){` |
|       - | 1757 | `				/* php raises this one AFTER the magic rules, so a parked` |
|       - | 1758 | `				 * visibility warning is php's first line here rather than a` |
|       - | 1759 | `				 * casualty of the fatal about to be counted. */` |
|       6 | 1760 | `				if( GenStateRaiseMagicDiag(pGen,&nMagicSeverity,zMagicErr,` |
|       6 | 1761 | `						nKwLine,nErrEntry) == SXERR_ABORT ){` |
|     ! 0 | 1762 | `					return SXERR_ABORT;` |
|       - | 1763 | `				}` |
|       8 | 1764 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1765 | `					"%z::%z(): Return type must be string when declared",` |
|       2 | 1766 | `					&pClass->sName,pName);` |
|       6 | 1767 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1768 | `					return SXERR_ABORT;` |
|       - | 1769 | `				}` |
|       6 | 1770 | `				goto Synchronize;` |
|       - | 1771 | `			}` |
|      42 | 1772 | `		}else{` |
|      43 | 1773 | `			char *zTsType = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       - | 1774 | `				"string",sizeof("string")-1);` |
|      43 | 1775 | `			pTsFunc->nReturnType = MEMOBJ_STRING;` |
|      43 | 1776 | `			if( zTsType ){` |
|      43 | 1777 | `				SyStringInitFromBuf(&pTsFunc->sReturnTypeName,zTsType,sizeof("string")-1);` |
|      19 | 1778 | `			}` |
|       - | 1779 | `		}` |
|      56 | 1780 | `	}` |
|     ! 0 | 1781 | `SkipToStringType:` |
|       - | 1782 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - | 1783 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - | 1784 | `	 * since we mint real ph7_class_attr entries. */` |
|       - | 1785 | `	{` |
|    3233 | 1786 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - | 1787 | `		sxu32 i;` |
|    4829 | 1788 | `		for( i = 0; i < nArg; i++ ){` |
|    1611 | 1789 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - | 1790 | `			ph7_class_attr *pAttr;` |
|    1611 | 1791 | `			sxi32 iAttrFlags = 0;` |
|       - | 1792 | `			int bArgTyped;` |
|    1611 | 1793 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|    1507 | 1794 | `				continue;` |
|       - | 1795 | `			}` |
|       - | 1796 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|       - | 1797 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|       - | 1798 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|      71 | 1799 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|     111 | 1800 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|     109 | 1801 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 | 1802 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1803 | `					"Cannot declare variadic promoted property");` |
|       3 | 1804 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1805 | `					return SXERR_ABORT;` |
|       - | 1806 | `				}` |
|       3 | 1807 | `				goto Synchronize;` |
|       - | 1808 | `			}` |
|       - | 1809 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - | 1810 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - | 1811 | `			 * appear as an alternative of a union type. */` |
|     107 | 1812 | `			if( bArgTyped ){` |
|     149 | 1813 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|      96 | 1814 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      96 | 1815 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      48 | 1816 | `					"Property %z::$%z cannot have type %z",nLine);` |
|     101 | 1817 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1818 | `					return SXERR_ABORT;` |
|     101 | 1819 | `				}else if( rc != SXRET_OK ){` |
|       6 | 1820 | `					goto Synchronize;` |
|       - | 1821 | `				}` |
|      46 | 1822 | `			}` |
|       - | 1823 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|     103 | 1824 | `			if( GenStateExtractProperty(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 | 1825 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 | 1826 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 | 1827 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1828 | `					return SXERR_ABORT;` |
|       - | 1829 | `				}` |
|       3 | 1830 | `				goto Synchronize;` |
|       - | 1831 | `			}` |
|     101 | 1832 | `			if( bArgTyped ){` |
|      95 | 1833 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      45 | 1834 | `			}` |
|     101 | 1835 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 | 1836 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 | 1837 | `			}` |
|     101 | 1838 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       8 | 1839 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       3 | 1840 | `			}` |
|     101 | 1841 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|       - | 1842 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|       - | 1843 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|      30 | 1844 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       4 | 1845 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 | 1846 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|       3 | 1847 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 1848 | `						return SXERR_ABORT;` |
|       - | 1849 | `					}` |
|       3 | 1850 | `					goto Synchronize;` |
|       - | 1851 | `				}` |
|      28 | 1852 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|      12 | 1853 | `			}` |
|      99 | 1854 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|       - | 1855 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|       5 | 1856 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|     ! 0 | 1857 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1858 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|     ! 0 | 1859 | `						&pClass->sName,&pArg->sName);` |
|     ! 0 | 1860 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 1861 | `						return SXERR_ABORT;` |
|       - | 1862 | `					}` |
|     ! 0 | 1863 | `					goto Synchronize;` |
|       - | 1864 | `				}` |
|       5 | 1865 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|       2 | 1866 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|       2 | 1867 | `			}` |
|      99 | 1868 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      99 | 1869 | `			if( pAttr == 0 ){` |
|     ! 0 | 1870 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 1871 | `				return SXERR_ABORT;` |
|       - | 1872 | `			}` |
|      99 | 1873 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      95 | 1874 | `				pAttr->nType = pArg->nType;` |
|      95 | 1875 | `				pAttr->sClass = pArg->sClass;` |
|      95 | 1876 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      95 | 1877 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - | 1878 | `					sxu32 k;` |
|      20 | 1879 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|      14 | 1880 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|      14 | 1881 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|       8 | 1882 | `					}` |
|       3 | 1883 | `				}` |
|      45 | 1884 | `			}` |
|      99 | 1885 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      99 | 1886 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 1887 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 1888 | `				return SXERR_ABORT;` |
|       - | 1889 | `			}` |
|      52 | 1890 | `		}` |
|       - | 1891 | `	}` |
|    3223 | 1892 | `	if( doBody ){` |
|       - | 1893 | `		/* Compile method body */` |
|    3121 | 1894 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|    3121 | 1895 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1896 | `			return SXERR_ABORT;` |
|       - | 1897 | `		}` |
|       - | 1898 | `		/* The cursor sits just past the body's closing brace */` |
|    3121 | 1899 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|    1563 | 1900 | `	}else{` |
|       - | 1901 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|     107 | 1902 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|     107 | 1903 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|      51 | 1904 | `		}` |
|       - | 1905 | `		/* Only method signature is allowed */` |
|     107 | 1906 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 | 1907 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 1908 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 | 1909 | `				if( rc == SXERR_ABORT ){` |
|       - | 1910 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 1911 | `					return SXERR_ABORT;` |
|       - | 1912 | `				}` |
|     ! 0 | 1913 | `				return SXERR_CORRUPT;` |
|       - | 1914 | `			}` |
|       - | 1915 | `	}` |
|       - | 1916 | `	/* php: an abstract method in a class that was not DECLARED abstract is a fatal,` |
|       - | 1917 | `	 * and it names the method -- distinct from the "contains N abstract methods"` |
|       - | 1918 | `	 * wording php uses for an unimplemented INHERITED one, which` |
|       - | 1919 | `	 * GenStateCheckAbstractMethods already handles. Interfaces and traits may carry` |
|       - | 1920 | `	 * abstract methods freely. */` |
|    3218 | 1921 | `	if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT)` |
|    1665 | 1922 | `	 && (pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|       4 | 1923 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1924 | `			"Class %z declares abstract method %z() and must therefore be declared abstract",` |
|       1 | 1925 | `			&pClass->sName,pName);` |
|       3 | 1926 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1927 | `			return SXERR_ABORT;` |
|       - | 1928 | `		}` |
|       3 | 1929 | `		return SXRET_OK;` |
|       - | 1930 | `	}` |
|       - | 1931 | `	/* php: two methods of the same name in one class body is a fatal, and the` |
|       - | 1932 | ``	 * comparison is case-INSENSITIVE (hMethod now matches that way), so `f` and`` |
|       - | 1933 | ``	 * `F` collide. php names the offending declaration with the spelling used at`` |
|       - | 1934 | `	 * the SECOND site. */` |
|    3221 | 1935 | `	if( PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte) != 0 ){` |
|       8 | 1936 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       2 | 1937 | `			"Cannot redeclare %z::%z()",&pClass->sName,pName);` |
|       6 | 1938 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1939 | `			return SXERR_ABORT;` |
|       - | 1940 | `		}` |
|       6 | 1941 | `		return SXRET_OK;` |
|       - | 1942 | `	}` |
|       - | 1943 | `	/* The magic-method rule this declaration broke (decided above, with the` |
|       - | 1944 | `	 * signature in hand). It is raised HERE because php raises it last of the` |
|       - | 1945 | `	 * declaration's fatals: a redeclaration, an abstract method in a class that` |
|       - | 1946 | `	 * is not abstract, and any parse error inside the body php has already read` |
|       - | 1947 | `	 * all win — and each of them has, by now, either returned or bumped nErr.` |
|       - | 1948 | `	 * php stops at its first fatal, so one is all this declaration reports. The` |
|       - | 1949 | ``	 * line is the `function` KEYWORD's, which is where php points once a`` |
|       - | 1950 | `	 * signature wraps across lines. */` |
|    3217 | 1951 | `	if( GenStateRaiseMagicDiag(pGen,&nMagicSeverity,zMagicErr,nKwLine,nErrEntry) == SXERR_ABORT ){` |
|     ! 0 | 1952 | `		return SXERR_ABORT;` |
|       - | 1953 | `	}` |
|    3217 | 1954 | `	if( bMagicFatal ){` |
|       - | 1955 | `		/* Never install a method php refused to declare. A WARNING falls through:` |
|       - | 1956 | `		 * php keeps the method and calls it. */` |
|      20 | 1957 | `		return SXRET_OK;` |
|       - | 1958 | `	}` |
|       - | 1959 | `	/* All done,install the method */` |
|    3201 | 1960 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|    3201 | 1961 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1962 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 1963 | `		return SXERR_ABORT;` |
|       - | 1964 | `	}` |
|    3201 | 1965 | `	return SXRET_OK;` |
|       8 | 1966 | `Synchronize:` |
|       - | 1967 | `	/* Synchronize with the first semi-colon */` |
|      56 | 1968 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      40 | 1969 | `		pGen->pIn++;` |
|       4 | 1970 | `	}` |
|      20 | 1971 | `	return SXERR_CORRUPT;` |
|    1622 | 1972 | `}` |
|       - | 1973 | `/*` |
|       - | 1974 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|       - | 1975 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|       - | 1976 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|       - | 1977 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|       - | 1978 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|       - | 1979 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|       - | 1980 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|       - | 1981 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|       - | 1982 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|       - | 1983 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|       - | 1984 | `` * implicit `$value` formal.`` |
|       - | 1985 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|       - | 1986 | ` */` |
|       - | 1987 | `/*` |
|       - | 1988 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|       - | 1989 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|       - | 1990 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|       - | 1991 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|       - | 1992 | ` * allowed, excluded from the raw object surfaces.` |
|       - | 1993 | ` */` |
|     158 | 1994 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|       5 | 1995 | `{` |
|       - | 1996 | `	SyToken *p;` |
|     771 | 1997 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|     671 | 1998 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|     547 | 1999 | `			continue;` |
|       - | 2000 | `		}` |
|       - | 2001 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|     124 | 2002 | `		if( p + 3 < pEnd` |
|     124 | 2003 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|     124 | 2004 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|     107 | 2005 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|      90 | 2006 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|      90 | 2007 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|      90 | 2008 | `		 && p[3].sData.nByte == pName->nByte` |
|      82 | 2009 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      56 | 2010 | `			return 1;` |
|       - | 2011 | `		}` |
|       - | 2012 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|       - | 2013 | `		 * hook operates on the shared per-instance backing store, so the` |
|       - | 2014 | `		 * property is backed (php compiles a default alongside it). */` |
|      70 | 2015 | `		if( p > pStart` |
|      66 | 2016 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|      33 | 2017 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       4 | 2018 | `		 && p[1].sData.nByte == pName->nByte` |
|       8 | 2019 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       6 | 2020 | `			return 1;` |
|       - | 2021 | `		}` |
|      36 | 2022 | `	}` |
|     105 | 2023 | `	return 0;` |
|      84 | 2024 | `}` |
|       - | 2025 | `/*` |
|       - | 2026 | ` * True when p opens php 8.4's parent-hook call form` |
|       - | 2027 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|       - | 2028 | ` */` |
|    1358 | 2029 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|       5 | 2030 | `{` |
|    1581 | 2031 | `	return p + 6 < pEnd` |
|     897 | 2032 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|     312 | 2033 | `	 && p->sData.nByte == sizeof("parent")-1` |
|     107 | 2034 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|      19 | 2035 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|      12 | 2036 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|      12 | 2037 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|      12 | 2038 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|      12 | 2039 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|      12 | 2040 | `	 && p[5].sData.nByte == 3` |
|      12 | 2041 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|       8 | 2042 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|    1576 | 2043 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|       5 | 2044 | `}` |
|       - | 2045 | `/*` |
|       - | 2046 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|       - | 2047 | ` * hook body into calls of the parent class's synthesized hook method` |
|       - | 2048 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|       - | 2049 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|       - | 2050 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|       - | 2051 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|       - | 2052 | ` * or SXERR_MEM.` |
|       - | 2053 | ` */` |
|       6 | 2054 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|       - | 2055 | `	SyToken *pStart,SyToken *pEnd)` |
|       2 | 2056 | `{` |
|       8 | 2057 | `	SyToken *p = pStart;` |
|      56 | 2058 | `	while( p < pEnd ){` |
|      50 | 2059 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|       - | 2060 | `			SyToken sTok;` |
|       - | 2061 | `			char zName[384];` |
|       - | 2062 | `			sxu32 nName;` |
|       - | 2063 | `			char *zDup;` |
|       - | 2064 | ``			/* `parent` `::` */`` |
|       8 | 2065 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|       8 | 2066 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|      11 | 2067 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|       6 | 2068 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|       8 | 2069 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|       8 | 2070 | `			if( zDup == 0 ){` |
|     ! 0 | 2071 | `				return SXERR_MEM;` |
|       - | 2072 | `			}` |
|       8 | 2073 | `			sTok = p[3]; /* keep the line info of the property name */` |
|       8 | 2074 | `			sTok.nType = PH7_TK_ID;` |
|       8 | 2075 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|       8 | 2076 | `			sTok.pUserData = 0;` |
|       8 | 2077 | `			SySetPut(pCopy,(const void *)&sTok);` |
|       8 | 2078 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|       8 | 2079 | `			continue;` |
|       - | 2080 | `		}` |
|      44 | 2081 | `		SySetPut(pCopy,(const void *)p);` |
|      44 | 2082 | `		p++;` |
|       2 | 2083 | `	}` |
|       8 | 2084 | `	return SXRET_OK;` |
|       5 | 2085 | `}` |
|       - | 2086 | `/*` |
|       - | 2087 | `` * A `get` hook's return type IS the property's declared type — php never lets a`` |
|       - | 2088 | ` * hook declare one, so there is nothing else it could be, and that is what makes` |
|       - | 2089 | `` * `public int $p { get { return "5"; } }` answer int(5) and a `get` returning`` |
|       - | 2090 | `` * "x" raise `C::$p::get(): Return value must be of type int, string returned`.`` |
|       - | 2091 | ` * Installing it on the synthesized method reuses the return enforcement that` |
|       - | 2092 | ` * already matches php byte for byte (the same move the __toString implicit` |
|       - | 2093 | `` * `string` type made), and lets the compile-time bare-`return;` check see the`` |
|       - | 2094 | ` * hook as the typed function php treats it as.` |
|       - | 2095 | ` *` |
|       - | 2096 | ` * The union alternatives are SHARED, not copied: their class-name SyStrings are` |
|       - | 2097 | ` * VM-allocator owned and outlive both records, which is the same contract` |
|       - | 2098 | ` * GenStateCopyTypeToAttr relies on.` |
|       - | 2099 | ` */` |
|     122 | 2100 | `static void GenStateHookGetReturnType(ph7_vm_func *pFunc,ph7_class_attr *pAttr)` |
|       5 | 2101 | `{` |
|     127 | 2102 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      13 | 2103 | `		return; /* untyped property: the hook is untyped too */` |
|       - | 2104 | `	}` |
|     115 | 2105 | `	pFunc->nReturnType = pAttr->nType;` |
|     115 | 2106 | `	pFunc->sReturnClass = pAttr->sClass;` |
|     115 | 2107 | `	pFunc->sReturnTypeName = pAttr->sTypeName;` |
|     115 | 2108 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|      14 | 2109 | `		pFunc->iFlags \|= VM_FUNC_RETURN_NULLABLE;` |
|       6 | 2110 | `	}` |
|     115 | 2111 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       - | 2112 | `		sxu32 i;` |
|     ! 0 | 2113 | `		for( i = 0 ; i < SySetUsed(&pAttr->aUnionAlts) ; i++ ){` |
|     ! 0 | 2114 | `			SySetPut(&pFunc->aReturnUnion,SySetAt(&pAttr->aUnionAlts,i));` |
|     ! 0 | 2115 | `		}` |
|     ! 0 | 2116 | `	}` |
|      66 | 2117 | `}` |
|       - | 2118 | `/*` |
|       - | 2119 | `` * The mirror for a `set` hook. php gives it two implicit pieces of signature:`` |
|       - | 2120 | `` * the implicit `$value` formal carries the PROPERTY's declared type (so`` |
|       - | 2121 | `` * `public int $p { set { ... } }` coerces `$o->p = "7"` to int(7) and rejects`` |
|       - | 2122 | `` * "abc" with `C::$p::set(): Argument #1 ($value) must be of type int, string`` |
|       - | 2123 | `` * given`), and the hook itself returns `void` — a set hook that returns a value`` |
|       - | 2124 | `` * is php's `A void method must not return a value`, on an untyped property too.`` |
|       - | 2125 | `` * An EXPLICIT `set(T $v)` keeps its own declared type; only the implicit formal`` |
|       - | 2126 | ` * is typed from the property, which is why the caller passes pValueArg only` |
|       - | 2127 | ` * when it synthesized one.` |
|       - | 2128 | ` */` |
|      92 | 2129 | `static void GenStateHookSetSignature(ph7_gen_state *pGen,ph7_vm_func *pFunc,` |
|       - | 2130 | `	ph7_class_attr *pAttr,ph7_vm_func_arg *pValueArg)` |
|       3 | 2131 | `{` |
|       - | 2132 | `	char *zVoid;` |
|      95 | 2133 | `	if( pValueArg && (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|      69 | 2134 | `		pValueArg->nType = pAttr->nType;` |
|      69 | 2135 | `		pValueArg->sClass = pAttr->sClass;` |
|      69 | 2136 | `		pValueArg->sTypeName = pAttr->sTypeName;` |
|      69 | 2137 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|      14 | 2138 | `			pValueArg->iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|       6 | 2139 | `		}` |
|      69 | 2140 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       - | 2141 | `			sxu32 i;` |
|       3 | 2142 | `			pValueArg->iFlags \|= VM_FUNC_ARG_UNION;` |
|       7 | 2143 | `			for( i = 0 ; i < SySetUsed(&pAttr->aUnionAlts) ; i++ ){` |
|       5 | 2144 | `				SySetPut(&pValueArg->aUnionAlts,SySetAt(&pAttr->aUnionAlts,i));` |
|       3 | 2145 | `			}` |
|       1 | 2146 | `		}` |
|      33 | 2147 | `	}` |
|      95 | 2148 | `	pFunc->nReturnType = MEMOBJ_VOID;` |
|      95 | 2149 | `	zVoid = SyMemBackendStrDup(&pGen->pVm->sAllocator,"void",sizeof("void")-1);` |
|      95 | 2150 | `	if( zVoid ){` |
|      95 | 2151 | `		SyStringInitFromBuf(&pFunc->sReturnTypeName,zVoid,sizeof("void")-1);` |
|      46 | 2152 | `	}` |
|      95 | 2153 | `}` |
|     154 | 2154 | `PH7_PRIVATE sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|       5 | 2155 | `{` |
|     159 | 2156 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 2157 | `	sxi32 rc;` |
|     159 | 2158 | `	int bRefsSelf = 0;` |
|     159 | 2159 | `	pGen->pIn++; /* Jump '{' */` |
|     389 | 2160 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|       - | 2161 | `		char zHook[384];` |
|       - | 2162 | `		SyString sHookName;` |
|       - | 2163 | `		ph7_class_method *pMeth;` |
|       - | 2164 | `		int bGet;` |
|     235 | 2165 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|     235 | 2166 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|      18 | 2167 | `			pGen->pIn++; /* stray ';' between hooks */` |
|      26 | 2168 | `			continue;` |
|       - | 2169 | `		}` |
|     219 | 2170 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - | 2171 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|     ! 0 | 2172 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|       - | 2173 | `				"By-reference property hooks are not supported for %z::$%z",` |
|     ! 0 | 2174 | `				&pClass->sName,&pAttr->sName);` |
|     ! 0 | 2175 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2176 | `				return SXERR_ABORT;` |
|       - | 2177 | `			}` |
|     ! 0 | 2178 | `			return SXERR_CORRUPT;` |
|       - | 2179 | `		}` |
|     219 | 2180 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 2181 | `			goto HookSyntax;` |
|       - | 2182 | `		}` |
|     214 | 2183 | `		if( pGen->pIn->sData.nByte == 3` |
|     219 | 2184 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|     127 | 2185 | `			bGet = 1;` |
|     158 | 2186 | `		}else if( pGen->pIn->sData.nByte == 3` |
|      95 | 2187 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|      95 | 2188 | `			bGet = 0;` |
|      49 | 2189 | `		}else{` |
|     ! 0 | 2190 | `			goto HookSyntax;` |
|       - | 2191 | `		}` |
|     219 | 2192 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|     219 | 2193 | `		sHookName.zString = zHook;` |
|     326 | 2194 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|     107 | 2195 | `			bGet ? "get" : "set",&pAttr->sName);` |
|     219 | 2196 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|       - | 2197 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|       - | 2198 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|       - | 2199 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|       - | 2200 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|       - | 2201 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|      16 | 2202 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|      10 | 2203 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 2204 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|       - | 2205 | `					"Non-abstract property hook must have a body");` |
|     ! 0 | 2206 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2207 | `					return SXERR_ABORT;` |
|       - | 2208 | `				}` |
|     ! 0 | 2209 | `				return SXERR_CORRUPT;` |
|       - | 2210 | `			}` |
|      18 | 2211 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|       - | 2212 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|      18 | 2213 | `			if( pMeth == 0 ){` |
|     ! 0 | 2214 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2215 | `				return SXERR_ABORT;` |
|       - | 2216 | `			}` |
|      18 | 2217 | `			pMeth->sFunc.nLine = nHLine;` |
|      18 | 2218 | `			if( bGet ){` |
|      12 | 2219 | `				GenStateHookGetReturnType(&pMeth->sFunc,pAttr);` |
|       5 | 2220 | `			}` |
|      18 | 2221 | `			if( !bGet ){` |
|       - | 2222 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|       - | 2223 | `				 * compatible with concrete set-hook implementations (which` |
|       - | 2224 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|       - | 2225 | `				 * type (php: the abstract set's parameter type IS the property` |
|       - | 2226 | `				 * type), so the override contravariance check accepts a typed` |
|       - | 2227 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|       - | 2228 | `				ph7_vm_func_arg sVArg;` |
|       7 | 2229 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|       7 | 2230 | `				if( zVName == 0 ){` |
|     ! 0 | 2231 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2232 | `					return SXERR_ABORT;` |
|       - | 2233 | `				}` |
|       7 | 2234 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|       7 | 2235 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|       7 | 2236 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       7 | 2237 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       7 | 2238 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|       7 | 2239 | `				GenStateHookSetSignature(&(*pGen),&pMeth->sFunc,pAttr,&sVArg);` |
|       7 | 2240 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|       3 | 2241 | `			}` |
|      18 | 2242 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|      18 | 2243 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 2244 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2245 | `				return SXERR_ABORT;` |
|       - | 2246 | `			}` |
|      18 | 2247 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|      18 | 2248 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|       - | 2249 | `		}` |
|     198 | 2250 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|     203 | 2251 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|       - | 2252 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|     ! 0 | 2253 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|       - | 2254 | `				"Abstract property hook cannot have body");` |
|     ! 0 | 2255 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2256 | `				return SXERR_ABORT;` |
|       - | 2257 | `			}` |
|     ! 0 | 2258 | `			return SXERR_CORRUPT;` |
|       - | 2259 | `		}` |
|     203 | 2260 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|       - | 2261 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|     203 | 2262 | `		if( pMeth == 0 ){` |
|     ! 0 | 2263 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2264 | `			return SXERR_ABORT;` |
|       - | 2265 | `		}` |
|     203 | 2266 | `		pMeth->sFunc.nLine = nHLine;` |
|     203 | 2267 | `		if( bGet ){` |
|     117 | 2268 | `			GenStateHookGetReturnType(&pMeth->sFunc,pAttr);` |
|      56 | 2269 | `		}` |
|     203 | 2270 | `		if( !bGet ){` |
|       - | 2271 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|      89 | 2272 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|      22 | 2273 | `				SyToken *pRp = 0;` |
|      22 | 2274 | `				pGen->pIn++;` |
|      22 | 2275 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|      22 | 2276 | `				if( pRp >= pGen->pEnd ){` |
|     ! 0 | 2277 | `					goto HookSyntax;` |
|       - | 2278 | `				}` |
|      22 | 2279 | `				if( pGen->pIn < pRp ){` |
|      22 | 2280 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|      22 | 2281 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 2282 | `						return SXERR_ABORT;` |
|       - | 2283 | `					}` |
|      10 | 2284 | `				}` |
|      22 | 2285 | `				pGen->pIn = &pRp[1];` |
|      10 | 2286 | `			}` |
|      89 | 2287 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|       - | 2288 | `				/* Implicit $value formal */` |
|       - | 2289 | `				ph7_vm_func_arg sVArg;` |
|      69 | 2290 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|      69 | 2291 | `				if( zVName == 0 ){` |
|     ! 0 | 2292 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2293 | `					return SXERR_ABORT;` |
|       - | 2294 | `				}` |
|      69 | 2295 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|      69 | 2296 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|      69 | 2297 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      69 | 2298 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|      69 | 2299 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|      69 | 2300 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|      69 | 2301 | `				GenStateHookSetSignature(&(*pGen),&pMeth->sFunc,pAttr,&sVArg);` |
|      69 | 2302 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|      36 | 2303 | `			}else{` |
|       - | 2304 | ``				/* An EXPLICIT `set(T $v)` keeps its own parameter type; only the`` |
|       - | 2305 | `				 * void return is implicit. */` |
|      22 | 2306 | `				GenStateHookSetSignature(&(*pGen),&pMeth->sFunc,pAttr,0);` |
|       - | 2307 | `			}` |
|      43 | 2308 | `		}` |
|     264 | 2309 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 2310 | `			/* Block body */` |
|     127 | 2311 | `			SyToken *pBodyStart = pGen->pIn;` |
|     127 | 2312 | `			SyToken *pCloser = 0;` |
|     127 | 2313 | `			int bParentCall = 0;` |
|     127 | 2314 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|     127 | 2315 | `			if( pCloser < pGen->pEnd ){` |
|       - | 2316 | `				SyToken *pScan;` |
|    1121 | 2317 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|    1003 | 2318 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|       6 | 2319 | `						bParentCall = 1;` |
|       6 | 2320 | `						break;` |
|       - | 2321 | `					}` |
|     502 | 2322 | `				}` |
|      61 | 2323 | `			}` |
|     127 | 2324 | `			if( bParentCall ){` |
|       - | 2325 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|       - | 2326 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|       - | 2327 | `				 * hook method), then continue past the original body. */` |
|       - | 2328 | `				SySet sBody;` |
|       6 | 2329 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|       6 | 2330 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       6 | 2331 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|       6 | 2332 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 2333 | `					SySetRelease(&sBody);` |
|     ! 0 | 2334 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2335 | `					return SXERR_ABORT;` |
|       - | 2336 | `				}` |
|       6 | 2337 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|       6 | 2338 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|       6 | 2339 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|       6 | 2340 | `				pGen->pIn = &pCloser[1];` |
|       6 | 2341 | `				pGen->pEnd = pSavedEnd;` |
|       6 | 2342 | `				SySetRelease(&sBody);` |
|       6 | 2343 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2344 | `					return SXERR_ABORT;` |
|       - | 2345 | `				}` |
|       6 | 2346 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|       4 | 2347 | `			}else{` |
|     123 | 2348 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|     123 | 2349 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2350 | `					return SXERR_ABORT;` |
|       - | 2351 | `				}` |
|     123 | 2352 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|       - | 2353 | `			}` |
|     127 | 2354 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|      23 | 2355 | `				bRefsSelf = 1;` |
|      15 | 2356 | `			}` |
|     178 | 2357 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|       - | 2358 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|       - | 2359 | `			GenBlock *pBlock;` |
|       - | 2360 | `			SySet *pInstrContainer;` |
|       - | 2361 | `			SyToken *pBodyStart;` |
|       - | 2362 | `			SyToken *pExprEnd;` |
|      79 | 2363 | `			SyToken *pSavedEnd = 0;` |
|       - | 2364 | `			SySet sBody;` |
|      79 | 2365 | `			int bParentCall = 0;` |
|      79 | 2366 | `			pGen->pIn++; /* Jump '=>' */` |
|      79 | 2367 | `			pBodyStart = pGen->pIn;` |
|       - | 2368 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|       - | 2369 | `			 * would end the enclosing hook list) and rewrite any` |
|       - | 2370 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|       - | 2371 | `			 * method on a token copy. */` |
|       - | 2372 | `			{` |
|      79 | 2373 | `				sxi32 iNest = 0;` |
|      79 | 2374 | `				pExprEnd = pBodyStart;` |
|     409 | 2375 | `				while( pExprEnd < pGen->pEnd ){` |
|     409 | 2376 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       9 | 2377 | `						iNest++;` |
|     405 | 2378 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       9 | 2379 | `						if( iNest <= 0 ){` |
|     ! 0 | 2380 | `							break;` |
|       - | 2381 | `						}` |
|       9 | 2382 | `						iNest--;` |
|     397 | 2383 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|      79 | 2384 | `						break;` |
|       - | 2385 | `					}` |
|     333 | 2386 | `					pExprEnd++;` |
|       3 | 2387 | `				}` |
|       - | 2388 | `			}` |
|       - | 2389 | `			{` |
|       - | 2390 | `				SyToken *pScan;` |
|     389 | 2391 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|     315 | 2392 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|       3 | 2393 | `						bParentCall = 1;` |
|       3 | 2394 | `						break;` |
|       - | 2395 | `					}` |
|     158 | 2396 | `				}` |
|       - | 2397 | `			}` |
|      79 | 2398 | `			if( bParentCall ){` |
|       3 | 2399 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       3 | 2400 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|       3 | 2401 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 2402 | `					SySetRelease(&sBody);` |
|     ! 0 | 2403 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2404 | `					return SXERR_ABORT;` |
|       - | 2405 | `				}` |
|       3 | 2406 | `				pSavedEnd = pGen->pEnd;` |
|       3 | 2407 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|       3 | 2408 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|       1 | 2409 | `			}` |
|     117 | 2410 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      76 | 2411 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|      79 | 2412 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 2413 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|     ! 0 | 2414 | `				return SXERR_ABORT;` |
|       - | 2415 | `			}` |
|      79 | 2416 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      79 | 2417 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|      79 | 2418 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      79 | 2419 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      79 | 2420 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      79 | 2421 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      79 | 2422 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      79 | 2423 | `			GenStateLeaveBlock(&(*pGen),0);` |
|      79 | 2424 | `			if( bParentCall ){` |
|       3 | 2425 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|       3 | 2426 | `				pGen->pEnd = pSavedEnd;` |
|       3 | 2427 | `				SySetRelease(&sBody);` |
|       1 | 2428 | `			}` |
|      79 | 2429 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2430 | `				return SXERR_ABORT;` |
|       - | 2431 | `			}` |
|      79 | 2432 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|      79 | 2433 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|      39 | 2434 | `				bRefsSelf = 1;` |
|      19 | 2435 | `			}` |
|      79 | 2436 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      79 | 2437 | `				pGen->pIn++; /* Jump ';' */` |
|      38 | 2438 | `			}` |
|      79 | 2439 | `			if( !bGet ){` |
|       - | 2440 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|       - | 2441 | `				 * the dispatcher consumes the implicit return value — which` |
|       - | 2442 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|       - | 2443 | ``				 * for `$this->NAME = expr`). */`` |
|       8 | 2444 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|       8 | 2445 | `				bRefsSelf = 1;` |
|       3 | 2446 | `			}` |
|      41 | 2447 | `		}else{` |
|     ! 0 | 2448 | `			goto HookSyntax;` |
|       - | 2449 | `		}` |
|     203 | 2450 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|     203 | 2451 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2452 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2453 | `			return SXERR_ABORT;` |
|       - | 2454 | `		}` |
|     203 | 2455 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|       5 | 2456 | `	}` |
|     159 | 2457 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|     ! 0 | 2458 | `		goto HookSyntax;` |
|       - | 2459 | `	}` |
|     159 | 2460 | `	pGen->pIn++; /* Jump '}' */` |
|     159 | 2461 | `	if( !bRefsSelf ){` |
|       - | 2462 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|       - | 2463 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|       - | 2464 | `		 * a default value (compile fatal, php's exact wording). */` |
|      99 | 2465 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|      99 | 2466 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|     ! 0 | 2467 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 2468 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|     ! 0 | 2469 | `				&pClass->sName,&pAttr->sName);` |
|     ! 0 | 2470 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2471 | `				return SXERR_ABORT;` |
|       - | 2472 | `			}` |
|     ! 0 | 2473 | `			return SXERR_CORRUPT;` |
|       - | 2474 | `		}` |
|      47 | 2475 | `	}` |
|     159 | 2476 | `	return SXRET_OK;` |
|     ! 0 | 2477 | `HookSyntax:` |
|     ! 0 | 2478 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 2479 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|     ! 0 | 2480 | `		&pClass->sName,&pAttr->sName);` |
|     ! 0 | 2481 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2482 | `		return SXERR_ABORT;` |
|       - | 2483 | `	}` |
|     ! 0 | 2484 | `	return SXERR_CORRUPT;` |
|      82 | 2485 | `}` |
|       - | 2486 | `/*` |
|       - | 2487 | ` * Compile an object interface.` |
|       - | 2488 | ` *  According to the PHP language reference manual` |
|       - | 2489 | ` *   Object Interfaces:` |
|       - | 2490 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - | 2491 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - | 2492 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - | 2493 | ` *   class, but without any of the methods having their contents defined.` |
|       - | 2494 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - | 2495 | ` */` |
|     176 | 2496 | `PH7_PRIVATE sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 | 2497 | `{` |
|     181 | 2498 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 2499 | `	ph7_class *pClass,*pBase;` |
|     181 | 2500 | `	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' */` |
|       - | 2501 | `	SyToken *pEnd,*pTmp;` |
|       - | 2502 | `	SyString *pName;` |
|       - | 2503 | `	sxi32 nKwrd;` |
|       - | 2504 | `	sxi32 rc;` |
|       - | 2505 | `	{` |
|       - | 2506 | `		/* Deferral gate: parent interfaces may need an autoloader` |
|       - | 2507 | `		 * that has not run yet. */` |
|       - | 2508 | `		sxi32 rcDefer;` |
|     181 | 2509 | `		if( GenStateMaybeDeferClass(pGen,0,PH7_DEFER_KIND_INTERFACE,&rcDefer) ){` |
|       3 | 2510 | `			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 2511 | `		}` |
|       - | 2512 | `	}` |
|       - | 2513 | `	/* Jump the 'interface' keyword */` |
|     179 | 2514 | `	pGen->pIn++;` |
|       - | 2515 | `	/* Extract interface name */` |
|     179 | 2516 | `	pName = &pGen->pIn->sData;` |
|       - | 2517 | `	/* Advance the stream cursor */` |
|     179 | 2518 | `	pGen->pIn++;` |
|       - | 2519 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 2520 | `		SyBlob sFQN;` |
|       - | 2521 | `		SyString sFQNStr;` |
|     179 | 2522 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     179 | 2523 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     179 | 2524 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       - | 2525 | ``		/* php refuses a declaration whose short name a local `use` already took. */`` |
|     179 | 2526 | `		if( GenStateGuardImportRedeclare(pGen,0,pName,&sFQNStr,nLine) == SXERR_ABORT ){` |
|     ! 0 | 2527 | `			SyBlobRelease(&sFQN);` |
|     ! 0 | 2528 | `			return SXERR_ABORT;` |
|       - | 2529 | `		}` |
|     179 | 2530 | `		GenStateRecordDeclaredName(pGen,0,&sFQNStr);` |
|     179 | 2531 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     179 | 2532 | `		SyBlobRelease(&sFQN);` |
|       - | 2533 | `	}` |
|     179 | 2534 | `	if( pClass == 0 ){` |
|     ! 0 | 2535 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2536 | `		return SXERR_ABORT;` |
|       - | 2537 | `	}` |
|     179 | 2538 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     179 | 2539 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|     ! 0 | 2540 | `		return SXERR_ABORT;` |
|       - | 2541 | `	}` |
|       - | 2542 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|     179 | 2543 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|       - | 2544 | `	/* Assume no base class is given */` |
|     179 | 2545 | `	pBase = 0;` |
|     179 | 2546 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      22 | 2547 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      22 | 2548 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a, c, … */ ){` |
|       - | 2549 | `			/* php lets an interface extend SEVERAL parent interfaces. The first` |
|       - | 2550 | `			 * becomes pBase (single-inheritance chain, hDerived); every extra one` |
|       - | 2551 | `			 * is recorded via PH7_ClassImplement so it lands in aInterface — the` |
|       - | 2552 | `			 * runtime subtype walk (VmInterfaceReaches) follows both. */` |
|      22 | 2553 | `			pGen->pIn++;` |
|      10 | 2554 | `			for(;;){` |
|       - | 2555 | `				SyBlob sResolved;` |
|       - | 2556 | `				SyString sBaseName;` |
|       - | 2557 | `				sxu32 nRefLine;` |
|       - | 2558 | `				ph7_class *pParent;` |
|      24 | 2559 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|      24 | 2560 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      24 | 2561 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 2562 | `					SyBlobRelease(&sResolved);` |
|     ! 0 | 2563 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 2564 | `						"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 | 2565 | `						pName);` |
|     ! 0 | 2566 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 2567 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 2568 | `						return SXERR_ABORT;` |
|       - | 2569 | `					}` |
|     ! 0 | 2570 | `					return SXRET_OK;` |
|       - | 2571 | `				}` |
|      34 | 2572 | `				pParent = PH7_VmExtractClass(pGen->pVm,` |
|      20 | 2573 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      24 | 2574 | `				SyStringInitFromBuf(&sBaseName,` |
|       - | 2575 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - | 2576 | `				/* Only interfaces is allowed */` |
|      24 | 2577 | `				while( pParent && (pParent->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 2578 | `					pParent = pParent->pNextName;` |
|     ! 0 | 2579 | `				}` |
|      24 | 2580 | `				if( pParent == 0 ){` |
|     ! 0 | 2581 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - | 2582 | `						"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 | 2583 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 2584 | `						SyBlobRelease(&sResolved);` |
|     ! 0 | 2585 | `						return SXERR_ABORT;` |
|     ! 0 | 2586 | `					}` |
|      24 | 2587 | `				}else if( pBase == 0 ){` |
|       - | 2588 | `					/* First parent → single-inheritance base */` |
|      22 | 2589 | `					pBase = pParent;` |
|      13 | 2590 | `				}else{` |
|       - | 2591 | `					/* Additional parent → record it in aInterface (+ copy its` |
|       - | 2592 | `					 * constants/method stubs) so instanceof reaches it too. */` |
|       3 | 2593 | `					PH7_ClassImplement(pClass,pParent);` |
|       - | 2594 | `				}` |
|      24 | 2595 | `				SyBlobRelease(&sResolved);` |
|       - | 2596 | `				/* Continue on a comma-separated list */` |
|      24 | 2597 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 2598 | `					pGen->pIn++;` |
|       3 | 2599 | `					continue;` |
|       - | 2600 | `				}` |
|      22 | 2601 | `				break;` |
|     ! 0 | 2602 | `			}` |
|       9 | 2603 | `		}` |
|       9 | 2604 | `	}` |
|     179 | 2605 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 2606 | `		/* Syntax error */` |
|     ! 0 | 2607 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 | 2608 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 2609 | `		if( rc == SXERR_ABORT ){` |
|       - | 2610 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2611 | `			return SXERR_ABORT;` |
|       - | 2612 | `		}` |
|     ! 0 | 2613 | `		return SXRET_OK;` |
|       - | 2614 | `	}` |
|     179 | 2615 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     179 | 2616 | `	pEnd = 0; /* cc warning */` |
|       - | 2617 | `	/* Delimit the interface body */` |
|     179 | 2618 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|     179 | 2619 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 2620 | `		/* Syntax error */` |
|     ! 0 | 2621 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 | 2622 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 2623 | `		if( rc == SXERR_ABORT ){` |
|       - | 2624 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2625 | `			return SXERR_ABORT;` |
|       - | 2626 | `		}` |
|     ! 0 | 2627 | `		return SXRET_OK;` |
|       - | 2628 | `	}` |
|       - | 2629 | `	/* The delimiter token is the interface body's closing brace */` |
|     179 | 2630 | `	pClass->nEndLine = pEnd->nLine;` |
|       - | 2631 | `	/* Swap token stream */` |
|     179 | 2632 | `	pTmp = pGen->pEnd;` |
|     179 | 2633 | `	pGen->pEnd = pEnd;` |
|       - | 2634 | `	/* This interface is now the lexical class for its body (see pCurClass) — a` |
|       - | 2635 | `	 * const default here is not a trait, so __TRAIT__ stays "". */` |
|     179 | 2636 | `	pGen->pCurClass = pClass;` |
|       - | 2637 | `	/* Start the parse process` |
|       - | 2638 | `	 * Note (According to the PHP reference manual):` |
|       - | 2639 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - | 2640 | `	 *  Only 'public' visibility is allowed.` |
|       - | 2641 | `	 */` |
|     129 | 2642 | `	for(;;){` |
|       - | 2643 | `		/* Jump leading/trailing semi-colons */` |
|     351 | 2644 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|      89 | 2645 | `			pGen->pIn++;` |
|       5 | 2646 | `		}` |
|     267 | 2647 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 2648 | `			/* End of interface body */` |
|     175 | 2649 | `			break;` |
|       - | 2650 | `		}` |
|       - | 2651 | `		/* Bind a directly-preceding docblock to this member */` |
|      97 | 2652 | `		GenStateSetPendingDoc(&(*pGen));` |
|      97 | 2653 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 2654 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 2655 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 | 2656 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 2657 | `			if( rc == SXERR_ABORT ){` |
|       - | 2658 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2659 | `				return SXERR_ABORT;` |
|       - | 2660 | `			}` |
|     ! 0 | 2661 | `			goto done;` |
|       - | 2662 | `		}` |
|       - | 2663 | `		/* Extract the current keyword */` |
|      97 | 2664 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      97 | 2665 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 2666 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - | 2667 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 | 2668 | `			const char *zKind = "member";` |
|       3 | 2669 | `			SyString *pMemberName = 0;` |
|       3 | 2670 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 | 2671 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 | 2672 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 | 2673 | `					zKind = "constant";` |
|       3 | 2674 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 | 2675 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 | 2676 | `					}` |
|       1 | 2677 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 2678 | `					zKind = "method";` |
|     ! 0 | 2679 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 | 2680 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 | 2681 | `					}` |
|     ! 0 | 2682 | `				}` |
|       1 | 2683 | `			}` |
|       3 | 2684 | `			if( pMemberName ){` |
|       4 | 2685 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 | 2686 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 | 2687 | `			}else{` |
|     ! 0 | 2688 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 2689 | `					"Access type for interface %s must be public",zKind);` |
|       - | 2690 | `			}` |
|       3 | 2691 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2692 | `				return SXERR_ABORT;` |
|       - | 2693 | `			}` |
|       3 | 2694 | `			goto done;` |
|       - | 2695 | `		}` |
|      95 | 2696 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 2697 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 2698 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 2699 | `			if( rc == SXERR_ABORT ){` |
|       - | 2700 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2701 | `				return SXERR_ABORT;` |
|       - | 2702 | `			}` |
|     ! 0 | 2703 | `			goto done;` |
|       - | 2704 | `		}` |
|      95 | 2705 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - | 2706 | `			/* Advance the stream cursor */` |
|      67 | 2707 | `			pGen->pIn++;` |
|      62 | 2708 | `			if( pGen->pIn < pGen->pEnd` |
|      67 | 2709 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|      62 | 2710 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|       - | 2711 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|       - | 2712 | `				 * requirement. The attribute compiler + hook parser handle it` |
|       - | 2713 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|       - | 2714 | `				 * property without hooks is ITS "Interfaces may only include` |
|       - | 2715 | `				 * hooked properties" error). */` |
|     ! 0 | 2716 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|     ! 0 | 2717 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|     ! 0 | 2718 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 2719 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 2720 | `						return SXERR_ABORT;` |
|       - | 2721 | `					}` |
|     ! 0 | 2722 | `					goto done;` |
|       - | 2723 | `				}` |
|     ! 0 | 2724 | `				continue;` |
|       - | 2725 | `			}` |
|      67 | 2726 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       - | 2727 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|       - | 2728 | `				 * '$' also opens a hooked-property requirement. */` |
|     ! 0 | 2729 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|     ! 0 | 2730 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|     ! 0 | 2731 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|     ! 0 | 2732 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|     ! 0 | 2733 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|     ! 0 | 2734 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 2735 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 2736 | `							return SXERR_ABORT;` |
|       - | 2737 | `						}` |
|     ! 0 | 2738 | `						goto done;` |
|       - | 2739 | `					}` |
|     ! 0 | 2740 | `					continue;` |
|       - | 2741 | `				}` |
|     ! 0 | 2742 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 2743 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 2744 | `				if( rc == SXERR_ABORT ){` |
|       - | 2745 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 2746 | `					return SXERR_ABORT;` |
|       - | 2747 | `				}` |
|     ! 0 | 2748 | `				goto done;` |
|       - | 2749 | `			}` |
|      67 | 2750 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      67 | 2751 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|       - | 2752 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|       - | 2753 | `				 * hooked-property requirement (PHP 8.4). */` |
|       4 | 2754 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|       5 | 2755 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|       7 | 2756 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       2 | 2757 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       5 | 2758 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 2759 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 2760 | `							return SXERR_ABORT;` |
|       - | 2761 | `						}` |
|     ! 0 | 2762 | `						goto done;` |
|       - | 2763 | `					}` |
|       5 | 2764 | `					continue;` |
|       - | 2765 | `				}` |
|     ! 0 | 2766 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 2767 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 2768 | `				if( rc == SXERR_ABORT ){` |
|       - | 2769 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 2770 | `					return SXERR_ABORT;` |
|       - | 2771 | `				}` |
|     ! 0 | 2772 | `				goto done;` |
|       - | 2773 | `			}` |
|      29 | 2774 | `		}` |
|      91 | 2775 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 2776 | `			/* Parse constant */` |
|      26 | 2777 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|      26 | 2778 | `			if( rc != SXRET_OK ){` |
|       3 | 2779 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2780 | `					return SXERR_ABORT;` |
|       - | 2781 | `				}` |
|       3 | 2782 | `				goto done;` |
|       - | 2783 | `			}` |
|      13 | 2784 | `		}else{` |
|      67 | 2785 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|      67 | 2786 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 2787 | `				/* Static method,record that */` |
|     ! 0 | 2788 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - | 2789 | `				/* Advance the stream cursor */` |
|     ! 0 | 2790 | `				pGen->pIn++;` |
|     ! 0 | 2791 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 | 2792 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 2793 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 2794 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 2795 | `						if( rc == SXERR_ABORT ){` |
|       - | 2796 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 2797 | `							return SXERR_ABORT;` |
|       - | 2798 | `						}` |
|     ! 0 | 2799 | `						goto done;` |
|       - | 2800 | `				}` |
|     ! 0 | 2801 | `			}` |
|       - | 2802 | `			/* Process method signature (no body for interface methods) */` |
|      67 | 2803 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|      67 | 2804 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 2805 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2806 | `					return SXERR_ABORT;` |
|       - | 2807 | `				}` |
|     ! 0 | 2808 | `				goto done;` |
|       - | 2809 | `			}` |
|       - | 2810 | `		}` |
|       5 | 2811 | `	}` |
|       - | 2812 | `	/* Reject a php-fatal redeclaration before hoisting the interface */` |
|     175 | 2813 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|       3 | 2814 | `		return SXERR_ABORT;` |
|       - | 2815 | `	}` |
|       - | 2816 | `	/* Install the interface */` |
|     173 | 2817 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     173 | 2818 | `	if( rc == SXRET_OK && pBase ){` |
|       - | 2819 | `		/* Inherit from the base interface */` |
|      22 | 2820 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       9 | 2821 | `	}` |
|     173 | 2822 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2823 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2824 | `		return SXERR_ABORT;` |
|       - | 2825 | `	}` |
|      84 | 2826 | `done:` |
|     177 | 2827 | `	pGen->pCurClass = pSavedCurClass;` |
|       - | 2828 | `	/* Point beyond the interface body */` |
|     177 | 2829 | `	pGen->pIn  = &pEnd[1];` |
|     177 | 2830 | `	pGen->pEnd = pTmp;` |
|     177 | 2831 | `	return PH7_OK;` |
|      93 | 2832 | `}` |
|       - | 2833 | `/*` |
|       - | 2834 | ` * Compile a user-defined class.` |
|       - | 2835 | ` * According to the PHP language reference manual` |
|       - | 2836 | ` *  class` |
|       - | 2837 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - | 2838 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - | 2839 | ` *  of the properties and methods belonging to the class.` |
|       - | 2840 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - | 2841 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - | 2842 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - | 2843 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 2844 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - | 2845 | ` *  (called "methods").` |
|       - | 2846 | ` */` |
|       - | 2847 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - | 2848 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - | 2849 | `struct TraitUseEntry {` |
|       - | 2850 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - | 2851 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - | 2852 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - | 2853 | `};` |
|       - | 2854 | `/*` |
|       - | 2855 | ` * Validate that methods implementing interface contracts have compatible` |
|       - | 2856 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - | 2857 | ` */` |
|    3284 | 2858 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 | 2859 | `{` |
|       - | 2860 | `	ph7_class **apIface;` |
|       - | 2861 | `	sxu32 nIface,i;` |
|       - | 2862 | `	sxi32 rc;` |
|    3289 | 2863 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 | 2864 | `		return SXRET_OK;` |
|       - | 2865 | `	}` |
|    3289 | 2866 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    3289 | 2867 | `	nIface = SySetUsed(&pClass->aInterface);` |
|    3913 | 2868 | `	for(i = 0; i < nIface; i++){` |
|     629 | 2869 | `		ph7_class *pIface = apIface[i];` |
|       - | 2870 | `		SyHashEntry *pEntry;` |
|     629 | 2871 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|    1625 | 2872 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|    1001 | 2873 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - | 2874 | `			ph7_class_method *pImplMeth;` |
|    1001 | 2875 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - | 2876 | `			/* Find the implementing method in the class */` |
|    1001 | 2877 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|    1001 | 2878 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      23 | 2879 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - | 2880 | `			}` |
|       - | 2881 | `			/* Check visibility: interface methods must be implemented as public */` |
|     983 | 2882 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 | 2883 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 2884 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 | 2885 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 | 2886 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2887 | `					return SXERR_ABORT;` |
|       - | 2888 | `				}` |
|       1 | 2889 | `			}` |
|       - | 2890 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - | 2891 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - | 2892 | `			 */` |
|       - | 2893 | `			{` |
|     983 | 2894 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|     983 | 2895 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|     983 | 2896 | `				int sigError = 0;` |
|     983 | 2897 | `				if( ((pIfaceMeth->sFunc.iFlags \| pImplMeth->sFunc.iFlags) & VM_FUNC_NATIVE) != 0 ){` |
|       - | 2898 | `					/* A NATIVE method's parameters live in its zSig string, not in` |
|       - | 2899 | `					 * compiled aArgs records, so there is nothing to count here --` |
|       - | 2900 | `					 * and an engine-declared signature is compatible by` |
|       - | 2901 | `					 * construction. Counting its empty aArgs as "no parameters" is` |
|       - | 2902 | `					 * what made an enum's own from()/tryFrom() incompatible with` |
|       - | 2903 | `					 * BackedEnum the moment they became native. */` |
|     937 | 2904 | `					sigError = 0;` |
|     517 | 2905 | `				}else if( nImplArgs < nIfaceArgs ){` |
|       3 | 2906 | `					sigError = 1;` |
|      50 | 2907 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - | 2908 | `					/* Extra parameters must all have default values */` |
|       6 | 2909 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - | 2910 | `					sxu32 k;` |
|       8 | 2911 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       6 | 2912 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 | 2913 | `							sigError = 1;` |
|       3 | 2914 | `							break;` |
|       - | 2915 | `						}` |
|       2 | 2916 | `					}` |
|       2 | 2917 | `				}` |
|     983 | 2918 | `				if( sigError ){` |
|       - | 2919 | `					SyBlob sImplSig, sIfaceSig;` |
|       - | 2920 | `					ph7_vm_func_arg *aArgs;` |
|       - | 2921 | `					sxu32 j;` |
|       6 | 2922 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       6 | 2923 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - | 2924 | `					/* Build implementing method signature */` |
|       6 | 2925 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      12 | 2926 | `					for(j = 0; j < nImplArgs; j++){` |
|       8 | 2927 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       8 | 2928 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       8 | 2929 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 | 2930 | `					}` |
|       - | 2931 | `					/* Build interface method signature */` |
|       6 | 2932 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      12 | 2933 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       8 | 2934 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       8 | 2935 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       8 | 2936 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 | 2937 | `					}` |
|       8 | 2938 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 2939 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 | 2940 | `						&pClass->sName,pMName,` |
|       4 | 2941 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 | 2942 | `						&pIface->sName,pMName,` |
|       4 | 2943 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       6 | 2944 | `					SyBlobRelease(&sImplSig);` |
|       6 | 2945 | `					SyBlobRelease(&sIfaceSig);` |
|       6 | 2946 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 2947 | `						return SXERR_ABORT;` |
|       - | 2948 | `					}` |
|       2 | 2949 | `				}` |
|       - | 2950 | `			}` |
|       5 | 2951 | `		}` |
|     317 | 2952 | `	}` |
|    3289 | 2953 | `	return SXRET_OK;` |
|    1647 | 2954 | `}` |
|       - | 2955 | `/*` |
|       - | 2956 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|       - | 2957 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|       - | 2958 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|       - | 2959 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|       - | 2960 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|       - | 2961 | ` * means that specific hook is still missing.` |
|       - | 2962 | ` */` |
|      38 | 2963 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|       5 | 2964 | `{` |
|       - | 2965 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|       - | 2966 | `	ph7_class_attr *pProp;` |
|      38 | 2967 | `	if( pMName->nByte <= nPfx` |
|      27 | 2968 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|       4 | 2969 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|      36 | 2970 | `		return 0; /* not a hook stub */` |
|       - | 2971 | `	}` |
|       7 | 2972 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|       7 | 2973 | `	return pProp != 0` |
|       6 | 2974 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|       3 | 2975 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|      24 | 2976 | `}` |
|       - | 2977 | `/*` |
|       - | 2978 | ` * Append an abstract member's display name to the message blob, translating a` |
|       - | 2979 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|       - | 2980 | ` */` |
|      16 | 2981 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|       4 | 2982 | `{` |
|       - | 2983 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|      16 | 2984 | `	if( pMName->nByte > nPfx` |
|      12 | 2985 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|     ! 0 | 2986 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|     ! 0 | 2987 | `		SyBlobAppend(pMsg,"$",1);` |
|     ! 0 | 2988 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|     ! 0 | 2989 | `		SyBlobAppend(pMsg,"::",2);` |
|     ! 0 | 2990 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|     ! 0 | 2991 | `		return;` |
|       - | 2992 | `	}` |
|      20 | 2993 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|      12 | 2994 | `}` |
|       - | 2995 | `/*` |
|       - | 2996 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - | 2997 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - | 2998 | ` */` |
|    3284 | 2999 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 | 3000 | `{` |
|       - | 3001 | `	ph7_class_method *pMeth;` |
|       - | 3002 | `	SyHashEntry *pEntry;` |
|       - | 3003 | `	sxu32 nAbstract;` |
|       - | 3004 | `	SyBlob sMsg;` |
|       - | 3005 | `	sxi32 rc;` |
|       - | 3006 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|    3289 | 3007 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      87 | 3008 | `		return SXRET_OK;` |
|       - | 3009 | `	}` |
|       - | 3010 | `	/* Count abstract methods */` |
|    3207 | 3011 | `	nAbstract = 0;` |
|    3207 | 3012 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   11676 | 3013 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|    6873 | 3014 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    6873 | 3015 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      27 | 3016 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|       7 | 3017 | `				continue; /* hook requirement met by a plain property (php) */` |
|       - | 3018 | `			}` |
|      20 | 3019 | `			nAbstract++;` |
|       8 | 3020 | `		}` |
|       5 | 3021 | `	}` |
|    3207 | 3022 | `	if( nAbstract == 0 ){` |
|    3193 | 3023 | `		return SXRET_OK;` |
|       - | 3024 | `	}` |
|       - | 3025 | `	/* Build the error message listing all abstract methods with origins */` |
|      18 | 3026 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      18 | 3027 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - | 3028 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 | 3029 | `		&pClass->sName,nAbstract,` |
|       7 | 3030 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 | 3031 | `		(nAbstract > 1 ? "s" : ""));` |
|       - | 3032 | `	/* Second pass: list methods with origins */` |
|       - | 3033 | `	{` |
|      18 | 3034 | `		sxu32 nListed = 0;` |
|      18 | 3035 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 | 3036 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      22 | 3037 | `			ph7_class *pOrigin = 0;` |
|       - | 3038 | `			SyString *pMName;` |
|      22 | 3039 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      22 | 3040 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 | 3041 | `				continue;` |
|       - | 3042 | `			}` |
|      20 | 3043 | `			pMName = &pMeth->sFunc.sName;` |
|      20 | 3044 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|     ! 0 | 3045 | `				continue; /* hook requirement met by a plain property (php) */` |
|       - | 3046 | `			}` |
|      20 | 3047 | `			if( nListed > 0 ){` |
|       3 | 3048 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 | 3049 | `			}` |
|       - | 3050 | `			/* Find the origin of this abstract method.` |
|       - | 3051 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - | 3052 | `			 * inheritance chains) take precedence for interface-declared` |
|       - | 3053 | `			 * methods. Abstract class methods only win when the class` |
|       - | 3054 | `			 * itself declared the abstract method (not inherited from` |
|       - | 3055 | `			 * an interface). Trait methods are adopted into the using` |
|       - | 3056 | `			 * class's namespace.` |
|       - | 3057 | `			 */` |
|       - | 3058 | `			{` |
|       - | 3059 | `				ph7_class **apIface;` |
|       - | 3060 | `				ph7_class **apTrait;` |
|       - | 3061 | `				ph7_class *pWalk;` |
|       - | 3062 | `				sxu32 i;` |
|       - | 3063 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - | 3064 | `				 * (one that was written in the class body, not inherited from an` |
|       - | 3065 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - | 3066 | `				 */` |
|      20 | 3067 | `				if( pClass->pBase ){` |
|      11 | 3068 | `					pWalk = pClass->pBase;` |
|      19 | 3069 | `					while( pWalk ){` |
|       - | 3070 | `						ph7_class_method *pParentMeth;` |
|      13 | 3071 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      13 | 3072 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - | 3073 | `							/* Exclude methods that came from an interface anywhere` |
|       - | 3074 | `							 * in this class's ancestor chain.` |
|       - | 3075 | `							 */` |
|      13 | 3076 | `							int fromIface = 0;` |
|      13 | 3077 | `							ph7_class *pAnc = pWalk;` |
|      17 | 3078 | `							while( pAnc ){` |
|       - | 3079 | `								ph7_class **apPI;` |
|       - | 3080 | `								sxu32 j;` |
|      15 | 3081 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      15 | 3082 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 | 3083 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 | 3084 | `										fromIface = 1;` |
|      10 | 3085 | `										break;` |
|       - | 3086 | `									}` |
|     ! 0 | 3087 | `								}` |
|      15 | 3088 | `								if( fromIface ) break;` |
|       6 | 3089 | `								pAnc = pAnc->pBase;` |
|       2 | 3090 | `							}` |
|      13 | 3091 | `							if( !fromIface ){` |
|       3 | 3092 | `								pOrigin = pWalk;` |
|       3 | 3093 | `								break;` |
|       - | 3094 | `							}` |
|       4 | 3095 | `						}` |
|      10 | 3096 | `						pWalk = pWalk->pBase;` |
|       2 | 3097 | `					}` |
|       4 | 3098 | `				}` |
|       - | 3099 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - | 3100 | `				 * each interface's own parent chain for the deepest origin.` |
|       - | 3101 | `				 */` |
|      20 | 3102 | `				if( !pOrigin ){` |
|      18 | 3103 | `					pWalk = pClass;` |
|      40 | 3104 | `					while( pWalk && !pOrigin ){` |
|      26 | 3105 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      26 | 3106 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      16 | 3107 | `							ph7_class *pIface = apIface[i];` |
|      16 | 3108 | `							ph7_class *pDeepest = 0;` |
|      28 | 3109 | `							while( pIface ){` |
|      16 | 3110 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      16 | 3111 | `									pDeepest = pIface;` |
|       6 | 3112 | `								}` |
|      16 | 3113 | `								pIface = pIface->pBase;` |
|       4 | 3114 | `							}` |
|      16 | 3115 | `							if( pDeepest ){` |
|      16 | 3116 | `								pOrigin = pDeepest;` |
|      16 | 3117 | `								break;` |
|       - | 3118 | `							}` |
|     ! 0 | 3119 | `						}` |
|      26 | 3120 | `						pWalk = pWalk->pBase;` |
|       4 | 3121 | `					}` |
|       7 | 3122 | `				}` |
|       - | 3123 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      20 | 3124 | `				if( !pOrigin ){` |
|       3 | 3125 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 | 3126 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 | 3127 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 | 3128 | `							pOrigin = pClass;` |
|       3 | 3129 | `							break;` |
|       - | 3130 | `						}` |
|     ! 0 | 3131 | `					}` |
|       1 | 3132 | `				}` |
|       - | 3133 | `			}` |
|      20 | 3134 | `			if( pOrigin ){` |
|      20 | 3135 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|      12 | 3136 | `			}else{` |
|       - | 3137 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 | 3138 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|       - | 3139 | `			}` |
|      20 | 3140 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|      20 | 3141 | `			nListed++;` |
|       4 | 3142 | `		}` |
|       - | 3143 | `	}` |
|      18 | 3144 | `	SyBlobAppend(&sMsg,")",1);` |
|      25 | 3145 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 | 3146 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      18 | 3147 | `	SyBlobRelease(&sMsg);` |
|      18 | 3148 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3149 | `		return SXERR_ABORT;` |
|       - | 3150 | `	}` |
|      18 | 3151 | `	return SXRET_OK;` |
|    1647 | 3152 | `}` |
|       - | 3153 | `/*` |
|       - | 3154 | ` * Parse a class/interface name reference from the current token stream.` |
|       - | 3155 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - | 3156 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - | 3157 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - | 3158 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - | 3159 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - | 3160 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - | 3161 | ` */` |
|    5268 | 3162 | `PH7_PRIVATE sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 | 3163 | `{` |
|    5273 | 3164 | `	int isAbsolute = 0;` |
|    5273 | 3165 | `	SyToken *pStart = pGen->pIn;` |
|       - | 3166 | `	SyBlob sName;` |
|    5273 | 3167 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|     913 | 3168 | `		isAbsolute = 1;` |
|     913 | 3169 | `		pGen->pIn++;` |
|     454 | 3170 | `	}` |
|    5273 | 3171 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|       - | 3172 | ``	/* `namespace\X` names the CURRENT namespace and is fully qualified from there. */`` |
|    5273 | 3173 | `	if( !isAbsolute && GenStateNsRelPrefix(pGen,&pGen->pIn,pGen->pEnd,&sName) ){` |
|      17 | 3174 | `		isAbsolute = 1;` |
|       8 | 3175 | `	}` |
|    5273 | 3176 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      11 | 3177 | `		SyBlobRelease(&sName);` |
|      11 | 3178 | `		pGen->pIn = pStart;` |
|      11 | 3179 | `		return SXERR_INVALID;` |
|       - | 3180 | `	}` |
|    5265 | 3181 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|    5265 | 3182 | `	pGen->pIn++;` |
|    8052 | 3183 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    2797 | 3184 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     112 | 3185 | `		SyBlobAppend(&sName,"\\",1);` |
|     112 | 3186 | `		pGen->pIn++;` |
|     112 | 3187 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|     112 | 3188 | `		pGen->pIn++;` |
|       4 | 3189 | `	}` |
|    5265 | 3190 | `	if( isAbsolute ){` |
|     925 | 3191 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     465 | 3192 | `	}else{` |
|       - | 3193 | `		SyString sRaw;` |
|    4345 | 3194 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|    4345 | 3195 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - | 3196 | `	}` |
|    5265 | 3197 | `	SyBlobRelease(&sName);` |
|    5265 | 3198 | `	return SXRET_OK;` |
|    2639 | 3199 | `}` |
|       - | 3200 | `/*` |
|       - | 3201 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - | 3202 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - | 3203 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - | 3204 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - | 3205 | ` * either direction cannot run unbounded.` |
|       - | 3206 | ` */` |
|       - | 3207 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|     286 | 3208 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 | 3209 | `{` |
|       - | 3210 | `	ph7_class **apParent;` |
|       - | 3211 | `	sxu32 n;` |
|     635 | 3212 | `	while( pInterface ){` |
|     359 | 3213 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 | 3214 | `			return FALSE;` |
|       - | 3215 | `		}` |
|     381 | 3216 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|      44 | 3217 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|      14 | 3218 | `			return TRUE;` |
|       - | 3219 | `		}` |
|     349 | 3220 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|     351 | 3221 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|       3 | 3222 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 | 3223 | `				return TRUE;` |
|       - | 3224 | `			}` |
|       2 | 3225 | `		}` |
|     349 | 3226 | `		pInterface = pInterface->pBase;` |
|     349 | 3227 | `		iDepth++;` |
|       5 | 3228 | `	}` |
|     281 | 3229 | `	return FALSE;` |
|     148 | 3230 | `}` |
|     284 | 3231 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 | 3232 | `{` |
|     289 | 3233 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 | 3234 | `}` |
|       - | 3235 | `/*` |
|       - | 3236 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - | 3237 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - | 3238 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - | 3239 | ` */` |
|      10 | 3240 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       4 | 3241 | `{` |
|      18 | 3242 | `	while( pBase ){` |
|      10 | 3243 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 | 3244 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 | 3245 | `			return TRUE;` |
|       - | 3246 | `		}` |
|      10 | 3247 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 | 3248 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 | 3249 | `			return TRUE;` |
|       - | 3250 | `		}` |
|       5 | 3251 | `		pBase = pBase->pBase;` |
|       1 | 3252 | `	}` |
|       9 | 3253 | `	return FALSE;` |
|       9 | 3254 | `}` |
|       - | 3255 | `/*` |
|       - | 3256 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|       - | 3257 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|       - | 3258 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|       - | 3259 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|       - | 3260 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|       - | 3261 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|       - | 3262 | ` * pClass->aEnumCases for cases().` |
|       - | 3263 | ` */` |
|     128 | 3264 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 | 3265 | `{` |
|     133 | 3266 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 3267 | `	SySet *pInstrContainer;` |
|       - | 3268 | `	ph7_class_attr *pCase;` |
|       - | 3269 | `	SyString *pName;` |
|       - | 3270 | `	sxi32 rc;` |
|     133 | 3271 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|     133 | 3272 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 3273 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 3274 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|     ! 0 | 3275 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3276 | `			return SXERR_ABORT;` |
|       - | 3277 | `		}` |
|     ! 0 | 3278 | `		goto Synchronize;` |
|       - | 3279 | `	}` |
|     133 | 3280 | `	pName = &pGen->pIn->sData;` |
|       - | 3281 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|     133 | 3282 | `	if( SyHashGet(&pClass->hConst,(const void *)pName->zString,pName->nByte) != 0 ){` |
|     ! 0 | 3283 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 3284 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|     ! 0 | 3285 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3286 | `			return SXERR_ABORT;` |
|       - | 3287 | `		}` |
|     ! 0 | 3288 | `		goto Synchronize;` |
|       - | 3289 | `	}` |
|     133 | 3290 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|       - | 3291 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|     133 | 3292 | `	if( pCase == 0 ){` |
|     ! 0 | 3293 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3294 | `		return SXERR_ABORT;` |
|       - | 3295 | `	}` |
|     133 | 3296 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|     133 | 3297 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|     ! 0 | 3298 | `		return SXERR_ABORT;` |
|       - | 3299 | `	}` |
|     133 | 3300 | `	pGen->pIn++; /* Jump the case name */` |
|     133 | 3301 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|      93 | 3302 | `		if( pClass->nEnumBacking == 0 ){` |
|       8 | 3303 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       2 | 3304 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|       6 | 3305 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3306 | `				return SXERR_ABORT;` |
|       - | 3307 | `			}` |
|       6 | 3308 | `			goto Synchronize;` |
|       - | 3309 | `		}` |
|      88 | 3310 | `		pGen->pIn++; /* Jump the equal sign */` |
|       - | 3311 | `		/* Compile the backing value expression into the case's own container` |
|       - | 3312 | `		 * (same technique as class constants). */` |
|      88 | 3313 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      88 | 3314 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|      88 | 3315 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      88 | 3316 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 3317 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 3318 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|     ! 0 | 3319 | `		}` |
|      88 | 3320 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      88 | 3321 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      88 | 3322 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3323 | `			return SXERR_ABORT;` |
|       - | 3324 | `		}` |
|      46 | 3325 | `	}else{` |
|      44 | 3326 | `		if( pClass->nEnumBacking != 0 ){` |
|     ! 0 | 3327 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 3328 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|     ! 0 | 3329 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3330 | `				return SXERR_ABORT;` |
|       - | 3331 | `			}` |
|     ! 0 | 3332 | `			goto Synchronize;` |
|       - | 3333 | `		}` |
|       - | 3334 | `	}` |
|     128 | 3335 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|     128 | 3336 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3337 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3338 | `		return SXERR_ABORT;` |
|       - | 3339 | `	}` |
|     128 | 3340 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|     128 | 3341 | `	return SXRET_OK;` |
|       2 | 3342 | `Synchronize:` |
|       - | 3343 | `	/* Synchronize with the first semi-colon */` |
|      14 | 3344 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|      10 | 3345 | `		pGen->pIn++;` |
|       2 | 3346 | `	}` |
|       6 | 3347 | `	return SXERR_CORRUPT;` |
|      69 | 3348 | `}` |
|       - | 3349 | `/*` |
|       - | 3350 | ` * Install the enum interface methods (PHP 8.1): cases() for every enum, plus` |
|       - | 3351 | ` * from()/tryFrom() for backed ones. They are NATIVE methods — the very same C` |
|       - | 3352 | ` * bodies an enum declared from C gets — because php's are internal: it reports` |
|       - | 3353 | `` * them as `<internal, prototype BackedEnum>` with no file and no line, and`` |
|       - | 3354 | `` * declares `from(string\|int $value): static` on the prototype rather than the`` |
|       - | 3355 | ` * enum's own backing type.` |
|       - | 3356 | ` *` |
|       - | 3357 | ` * This used to synthesize PHP source forwarding to three global` |
|       - | 3358 | `` * `__phl_enum_*` thunks, which put those names in php's namespace and reported`` |
|       - | 3359 | `` * every enum's three methods as `<user>` at the enum's own line.`` |
|       - | 3360 | ` */` |
|      92 | 3361 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 | 3362 | `{` |
|      97 | 3363 | `	if( PH7_InstallEnumInterfaceMethods(pGen->pVm,pClass) != SXRET_OK ){` |
|     ! 0 | 3364 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3365 | `		return SXERR_ABORT;` |
|       - | 3366 | `	}` |
|      97 | 3367 | `	return SXRET_OK;` |
|      51 | 3368 | `}` |
|       - | 3369 | `/*` |
|       - | 3370 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|       - | 3371 | ` * __call/__callStatic/__invoke stay allowed).` |
|       - | 3372 | ` */` |
|       - | 3373 | `static const char *azEnumBannedMagic[] = {` |
|       - | 3374 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|       - | 3375 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|       - | 3376 | `};` |
|       - | 3377 | `/*` |
|       - | 3378 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|       - | 3379 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|       - | 3380 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|       - | 3381 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|       - | 3382 | ` * and before the class is installed.` |
|       - | 3383 | ` */` |
|      92 | 3384 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|       5 | 3385 | `{` |
|       - | 3386 | `	SyHashEntry *pEntry;` |
|       - | 3387 | `	sxi32 rc;` |
|       - | 3388 | `	sxu32 n;` |
|       - | 3389 | `	/* php: "Enum %s cannot include properties" */` |
|      97 | 3390 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|      97 | 3391 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|       3 | 3392 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       3 | 3393 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       3 | 3394 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|       1 | 3395 | `				"Enum %z cannot include properties",&pClass->sName);` |
|       3 | 3396 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3397 | `				return SXERR_ABORT;` |
|       - | 3398 | `			}` |
|       3 | 3399 | `			break;` |
|       - | 3400 | `		}` |
|     ! 0 | 3401 | `	}` |
|       - | 3402 | `	/* php: "Enum %s cannot include magic method %s" */` |
|    1293 | 3403 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|    1794 | 3404 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|    1201 | 3405 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|     ! 0 | 3406 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 3407 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|     ! 0 | 3408 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3409 | `				return SXERR_ABORT;` |
|       - | 3410 | `			}` |
|     ! 0 | 3411 | `		}` |
|     603 | 3412 | `	}` |
|       - | 3413 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|       - | 3414 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|       - | 3415 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|       - | 3416 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|       - | 3417 | `	{` |
|       - | 3418 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|       - | 3419 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|       - | 3420 | `		ph7_class_attr *pAttr;` |
|      97 | 3421 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|       - | 3422 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      97 | 3423 | `		if( pAttr == 0 ){` |
|     ! 0 | 3424 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3425 | `			return SXERR_ABORT;` |
|       - | 3426 | `		}` |
|      97 | 3427 | `		pAttr->nType = MEMOBJ_STRING;` |
|      97 | 3428 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|      97 | 3429 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|      97 | 3430 | `		if( pClass->nEnumBacking != 0 ){` |
|      52 | 3431 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|       - | 3432 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      52 | 3433 | `			if( pAttr == 0 ){` |
|     ! 0 | 3434 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3435 | `				return SXERR_ABORT;` |
|       - | 3436 | `			}` |
|      52 | 3437 | `			pAttr->nType = pClass->nEnumBacking;` |
|      52 | 3438 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|      18 | 3439 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|      10 | 3440 | `			}else{` |
|      36 | 3441 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|       - | 3442 | `			}` |
|      52 | 3443 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|      24 | 3444 | `		}` |
|       - | 3445 | `	}` |
|      97 | 3446 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|      51 | 3447 | `}` |
|       - | 3448 | `/*` |
|       - | 3449 | ` * Deferred class declarations (class/anonymous-class extending an` |
|       - | 3450 | ` * autoloaded parent).` |
|       - | 3451 | ` *` |
|       - | 3452 | ` * A class declaration compiles INLINE while its enclosing file compiles, so a` |
|       - | 3453 | ` * parent/interface/trait that an autoloader would provide is unreachable when` |
|       - | 3454 | ` * the autoloader's own spl_autoload_register() statement has not EXECUTED yet` |
|       - | 3455 | `` * (same-file registration, or `new class extends \App\Child {}` anywhere).`` |
|       - | 3456 | ` * php's model has no such problem: a declaration with unresolved dependencies` |
|       - | 3457 | ` * is declared at its EXECUTION point, in statement order, not hoisted.` |
|       - | 3458 | ` *` |
|       - | 3459 | ` * These helpers reproduce that: before compiling a declaration, scan its` |
|       - | 3460 | `` * header (extends/implements) and body (depth-1 trait `use`) for referenced`` |
|       - | 3461 | ` * names and try to resolve each (firing autoload exactly where the normal` |
|       - | 3462 | ` * compile would). If any name is still missing, the WHOLE declaration is` |
|       - | 3463 | `` * captured as re-compilable source — a reconstructed `namespace`/`use`-import/`` |
|       - | 3464 | ` * doc/attribute/modifier prefix plus the declaration's raw text — recorded in` |
|       - | 3465 | ` * a VmDeferredClass, and OP_CLASS_DEFER is emitted at the declaration site.` |
|       - | 3466 | ` * At runtime (VmExecDeferredClass, vm_include.c) the autoloader is live: each` |
|       - | 3467 | `` * recorded name resolves or throws php's catchable `... not found` Error, and`` |
|       - | 3468 | ` * the chunk re-compiles through VmEvalChunk. An anonymous class re-compiles` |
|       - | 3469 | `` * inside `if (false) { new ... }` (installing the class without instantiating`` |
|       - | 3470 | ` * it) under its original synthesized name via pVm->sDeferAnonName; the site's` |
|       - | 3471 | ` * own OP_NEW then instantiates it with the site-compiled arguments.` |
|       - | 3472 | ` *` |
|       - | 3473 | ` * Behavior shifts only for declarations that previously died with the` |
|       - | 3474 | ` * compile-time "Nonexistent base class" fatal: they now follow php — succeed` |
|       - | 3475 | ` * when the autoloader is registered first, or throw php's catchable` |
|       - | 3476 | `` * `Class/Interface/Trait "X" not found` Error at the declaration point.`` |
|       - | 3477 | ` * A deferred declaration's OTHER compile errors (a body syntax error) shift` |
|       - | 3478 | ` * from file-compile time to the declaration's execution — still loud, timing` |
|       - | 3479 | ` * differs from php (recorded).` |
|       - | 3480 | ` */` |
|      84 | 3481 | `static void GenStateDeferEmitUses(SyBlob *pOut,SyHash *pTable,const char *zKind)` |
|       2 | 3482 | `{` |
|       - | 3483 | `	SyHashEntry *pEntry;` |
|      86 | 3484 | `	SyHashResetLoopCursor(pTable);` |
|     128 | 3485 | `	while( (pEntry = SyHashGetNextEntry(pTable)) != 0 ){` |
|     ! 0 | 3486 | `		const char *zFqn = (const char *)pEntry->pUserData;` |
|     ! 0 | 3487 | `		if( zFqn ){` |
|     ! 0 | 3488 | `			SyBlobFormat(pOut,"use %s%s as %.*s;\n",zKind,zFqn,` |
|     ! 0 | 3489 | `				(int)pEntry->nKeyLen,(const char *)pEntry->pKey);` |
|     ! 0 | 3490 | `		}` |
|     ! 0 | 3491 | `	}` |
|      86 | 3492 | `}` |
|       - | 3493 | `/*` |
|       - | 3494 | ` * Parse one class reference at *ppCur (bounded by pEnd) with the SAME` |
|       - | 3495 | ` * namespace/import resolution the real compile uses, and append it to pNames.` |
|       - | 3496 | ` * Advances *ppCur past the reference. Returns SXERR_INVALID on a malformed` |
|       - | 3497 | ` * reference (caller bails out of deferral and lets the normal path report).` |
|       - | 3498 | ` */` |
|    1094 | 3499 | `static sxi32 GenStateDeferRecordRef(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd,` |
|       - | 3500 | `	sxu8 cKind,SySet *pNames)` |
|       5 | 3501 | `{` |
|    1099 | 3502 | `	SyToken *pSavedIn = pGen->pIn;` |
|    1099 | 3503 | `	SyToken *pSavedEnd = pGen->pEnd;` |
|       - | 3504 | `	SyBlob sFqn;` |
|       - | 3505 | `	VmDeferredReq sReq;` |
|       - | 3506 | `	char *zDup;` |
|       - | 3507 | `	sxi32 rc;` |
|    1099 | 3508 | `	SyBlobInit(&sFqn,&pGen->pVm->sAllocator);` |
|    1099 | 3509 | `	pGen->pIn = *ppCur;` |
|    1099 | 3510 | `	pGen->pEnd = pEnd;` |
|    1099 | 3511 | `	rc = GenStateParseClassReference(pGen,&sFqn);` |
|    1099 | 3512 | `	*ppCur = pGen->pIn;` |
|    1099 | 3513 | `	pGen->pIn = pSavedIn;` |
|    1099 | 3514 | `	pGen->pEnd = pSavedEnd;` |
|    1099 | 3515 | `	if( rc != SXRET_OK \|\| SyBlobLength(&sFqn) < 1 ){` |
|       3 | 3516 | `		SyBlobRelease(&sFqn);` |
|       3 | 3517 | `		return SXERR_INVALID;` |
|       - | 3518 | `	}` |
|    1643 | 3519 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    1092 | 3520 | `		(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));` |
|    1097 | 3521 | `	if( zDup == 0 ){` |
|     ! 0 | 3522 | `		SyBlobRelease(&sFqn);` |
|     ! 0 | 3523 | `		return SXERR_INVALID;` |
|       - | 3524 | `	}` |
|    1097 | 3525 | `	SyStringInitFromBuf(&sReq.sName,zDup,SyBlobLength(&sFqn));` |
|    1097 | 3526 | `	sReq.cKind = cKind;` |
|    1097 | 3527 | `	SySetPut(pNames,(const void *)&sReq);` |
|    1097 | 3528 | `	SyBlobRelease(&sFqn);` |
|    1097 | 3529 | `	return SXRET_OK;` |
|     552 | 3530 | `}` |
|       - | 3531 | `/*` |
|       - | 3532 | ` * Scan the declaration whose keyword pGen->pIn sits on (class/enum/interface/` |
|       - | 3533 | `` * trait, or an anonymous `class(args)`) WITHOUT consuming tokens. Collects`` |
|       - | 3534 | ` * every referenced dependency name, locates the body braces, and filters the` |
|       - | 3535 | ` * collected names down to the UNRESOLVABLE ones (each lookup fires autoload,` |
|       - | 3536 | ` * exactly like the compile it replaces). SXRET_OK with an empty pMissing set` |
|       - | 3537 | ` * means "compile normally"; a non-empty set means "defer". Any structural` |
|       - | 3538 | ` * surprise returns SXERR_INVALID so the normal compile reports it.` |
|       - | 3539 | ` */` |
|    3724 | 3540 | `static sxi32 GenStateScanDeferDeps(ph7_gen_state *pGen,int bAnon,int iSelfKind,` |
|       - | 3541 | `	SySet *pMissing,SyToken **ppBody,SyToken **ppBodyEnd,SyBlob *pSelfFqn)` |
|       5 | 3542 | `{` |
|    3729 | 3543 | `	SyToken *pCur = pGen->pIn; /* on the declaration keyword */` |
|    3729 | 3544 | `	SyToken *pEnd = pGen->pEnd;` |
|       - | 3545 | `	SySet aNames;` |
|    3729 | 3546 | `	sxi32 rc = SXRET_OK;` |
|    3729 | 3547 | `	*ppBody = *ppBodyEnd = 0;` |
|    3729 | 3548 | `	SySetInit(&aNames,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|    3729 | 3549 | `	pCur++; /* Jump the keyword */` |
|    3729 | 3550 | `	if( bAnon ){` |
|      79 | 3551 | `		if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|      16 | 3552 | `			SyToken *pClose = 0;` |
|      16 | 3553 | `			pCur++;` |
|      16 | 3554 | `			PH7_DelimitNestedTokens(pCur,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|      16 | 3555 | `			if( pClose == 0 \|\| pClose >= pEnd ){` |
|     ! 0 | 3556 | `				SySetRelease(&aNames);` |
|     ! 0 | 3557 | `				return SXERR_INVALID;` |
|       - | 3558 | `			}` |
|      16 | 3559 | `			pCur = &pClose[1];` |
|       7 | 3560 | `		}` |
|      42 | 3561 | `	}else{` |
|    3655 | 3562 | `		if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 3563 | `			SySetRelease(&aNames);` |
|     ! 0 | 3564 | `			return SXERR_INVALID;` |
|       - | 3565 | `		}` |
|    3655 | 3566 | `		GenStateBuildFQN(pGen,&pCur->sData,pSelfFqn);` |
|    3655 | 3567 | `		pCur++;` |
|       - | 3568 | `	}` |
|       - | 3569 | ``	/* Header: extends/implements lists up to the '{' (an enum's `: int` backing`` |
|       - | 3570 | `	 * and any stray tokens pass through; malformed headers bail to the normal` |
|       - | 3571 | `	 * path's diagnostics). */` |
|    4699 | 3572 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_OCB) == 0 ){` |
|     977 | 3573 | `		int iKind = -1;` |
|     977 | 3574 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|     925 | 3575 | `			sxi32 nKw = SX_PTR_TO_INT(pCur->pUserData);` |
|     925 | 3576 | `			if( nKw == PH7_TKWRD_EXTENDS ){` |
|     597 | 3577 | `				iKind = (iSelfKind == PH7_DEFER_KIND_INTERFACE)` |
|     296 | 3578 | `					? PH7_DEFER_KIND_INTERFACE : PH7_DEFER_KIND_CLASS;` |
|     629 | 3579 | `			}else if( nKw == PH7_TKWRD_IMPLEMENTS ){` |
|     281 | 3580 | `				iKind = PH7_DEFER_KIND_INTERFACE;` |
|     138 | 3581 | `			}` |
|     460 | 3582 | `		}` |
|     977 | 3583 | `		if( iKind < 0 ){` |
|     108 | 3584 | `			pCur++;` |
|     108 | 3585 | `			continue;` |
|       - | 3586 | `		}` |
|     873 | 3587 | `		pCur++; /* Jump extends/implements */` |
|     434 | 3588 | `		for(;;){` |
|     889 | 3589 | `			if( GenStateDeferRecordRef(pGen,&pCur,pEnd,(sxu8)iKind,&aNames) != SXRET_OK ){` |
|       3 | 3590 | `				SySetRelease(&aNames);` |
|       3 | 3591 | `				return SXERR_INVALID;` |
|       - | 3592 | `			}` |
|     887 | 3593 | `			if( pCur < pEnd && (pCur->nType & PH7_TK_COMMA) ){` |
|      20 | 3594 | `				pCur++;` |
|      20 | 3595 | `				continue;` |
|       - | 3596 | `			}` |
|     871 | 3597 | `			break;` |
|     ! 0 | 3598 | `		}` |
|       5 | 3599 | `	}` |
|    3727 | 3600 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 3601 | `		SySetRelease(&aNames);` |
|     ! 0 | 3602 | `		return SXERR_INVALID;` |
|       - | 3603 | `	}` |
|    3727 | 3604 | `	*ppBody = pCur;` |
|       - | 3605 | `	{` |
|    3727 | 3606 | `		SyToken *pClose = 0;` |
|    3727 | 3607 | `		PH7_DelimitNestedTokens(&pCur[1],pEnd,PH7_TK_OCB,PH7_TK_CCB,&pClose);` |
|    3727 | 3608 | `		if( pClose == 0 \|\| pClose >= pEnd ){` |
|     ! 0 | 3609 | `			SySetRelease(&aNames);` |
|     ! 0 | 3610 | `			return SXERR_INVALID;` |
|       - | 3611 | `		}` |
|    3727 | 3612 | `		*ppBodyEnd = pClose;` |
|       - | 3613 | `	}` |
|       - | 3614 | ``	/* Body: depth-1 trait `use Name[, Name]` statements. Statement position only`` |
|       - | 3615 | ``	 * (previous token one of '{' '}' ';'), so a closure's `use ($x)` — which`` |
|       - | 3616 | `	 * follows a ')' — never matches. */` |
|       - | 3617 | `	{` |
|    3727 | 3618 | `		SyToken *p = &(*ppBody)[1];` |
|    3727 | 3619 | `		int bStmtPos = 1;` |
|    3727 | 3620 | `		sxi32 iDepth = 1;` |
|   86495 | 3621 | `		while( p < *ppBodyEnd ){` |
|   82773 | 3622 | `			if( p->nType & PH7_TK_OCB ){` |
|    3709 | 3623 | `				iDepth++;` |
|    3709 | 3624 | `				bStmtPos = 1;` |
|    3709 | 3625 | `				p++;` |
|    3709 | 3626 | `				continue;` |
|       - | 3627 | `			}` |
|   79069 | 3628 | `			if( p->nType & PH7_TK_CCB ){` |
|    3709 | 3629 | `				iDepth--;` |
|    3709 | 3630 | `				bStmtPos = 1;` |
|    3709 | 3631 | `				p++;` |
|    3709 | 3632 | `				continue;` |
|       - | 3633 | `			}` |
|   75365 | 3634 | `			if( p->nType & PH7_TK_SEMI ){` |
|    6081 | 3635 | `				bStmtPos = 1;` |
|    6081 | 3636 | `				p++;` |
|    6081 | 3637 | `				continue;` |
|       - | 3638 | `			}` |
|   69284 | 3639 | `			if( iDepth == 1 && bStmtPos && (p->nType & PH7_TK_KEYWORD)` |
|    5808 | 3640 | `			 && SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_USE ){` |
|     197 | 3641 | `				p++;` |
|      96 | 3642 | `				for(;;){` |
|     215 | 3643 | `					if( GenStateDeferRecordRef(pGen,&p,*ppBodyEnd,PH7_DEFER_KIND_TRAIT,&aNames) != SXRET_OK ){` |
|     ! 0 | 3644 | `						SySetRelease(&aNames);` |
|     ! 0 | 3645 | `						return SXERR_INVALID;` |
|       - | 3646 | `					}` |
|     215 | 3647 | `					if( p < *ppBodyEnd && (p->nType & PH7_TK_COMMA) ){` |
|      22 | 3648 | `						p++;` |
|      22 | 3649 | `						continue;` |
|       - | 3650 | `					}` |
|     197 | 3651 | `					break;` |
|     ! 0 | 3652 | `				}` |
|     197 | 3653 | `				continue;` |
|       - | 3654 | `			}` |
|   69097 | 3655 | `			bStmtPos = 0;` |
|   69097 | 3656 | `			p++;` |
|       5 | 3657 | `		}` |
|       - | 3658 | `	}` |
|       - | 3659 | `	/* Filter: keep only the names that do NOT resolve. The lookup fires the` |
|       - | 3660 | `	 * autoloader exactly where the replaced compile would. */` |
|       - | 3661 | `	{` |
|    3727 | 3662 | `		VmDeferredReq *aReq = (VmDeferredReq *)SySetBasePtr(&aNames);` |
|       - | 3663 | `		sxu32 n;` |
|    4819 | 3664 | `		for( n = 0 ; n < SySetUsed(&aNames) ; ++n ){` |
|    1097 | 3665 | `			if( PH7_VmExtractClass(pGen->pVm,aReq[n].sName.zString,aReq[n].sName.nByte,FALSE,0) == 0 ){` |
|      34 | 3666 | `				SySetPut(pMissing,(const void *)&aReq[n]);` |
|      16 | 3667 | `			}` |
|     551 | 3668 | `		}` |
|       - | 3669 | `	}` |
|    3727 | 3670 | `	SySetRelease(&aNames);` |
|    3727 | 3671 | `	return rc;` |
|    1867 | 3672 | `}` |
|       - | 3673 | `/*` |
|       - | 3674 | ` * Capture the declaration as a re-compilable chunk, record it, and emit` |
|       - | 3675 | ` * OP_CLASS_DEFER at the current emission point. On return the statement` |
|       - | 3676 | ` * cursor sits past the declaration's closing '}'. pMissing's entries are` |
|       - | 3677 | ` * COPIED into the record (their name bytes are already allocator-owned).` |
|       - | 3678 | ` */` |
|      28 | 3679 | `static sxi32 GenStateEmitDeferredClass(ph7_gen_state *pGen,sxi32 iFlags,int bAnon,` |
|       - | 3680 | `	SySet *pMissing,SyToken *pBodyEnd,SyBlob *pSelfFqn,const SyString *pAnonName)` |
|       2 | 3681 | `{` |
|      30 | 3682 | `	SyToken *pKw = pGen->pIn; /* the declaration keyword */` |
|       - | 3683 | `	VmDeferredClass *pDefer;` |
|       - | 3684 | `	SyBlob sChunk;` |
|       - | 3685 | `	const char *zFrom;` |
|       - | 3686 | `	const char *zTo;` |
|       - | 3687 | `	char *zDup;` |
|      30 | 3688 | `	SyBlobInit(&sChunk,&pGen->pVm->sAllocator);` |
|       - | 3689 | `	/* The declaration site's compile context is replayed as literal statements:` |
|       - | 3690 | `	 * strict_types first (it must open the chunk), then namespace and the` |
|       - | 3691 | `	 * use-import tables — the runtime re-compile starts in a fresh scope. */` |
|      30 | 3692 | `	if( pGen->bStrictTypes ){` |
|     ! 0 | 3693 | `		SyBlobAppend(&sChunk,"declare(strict_types=1);\n",sizeof("declare(strict_types=1);\n")-1);` |
|     ! 0 | 3694 | `	}` |
|      30 | 3695 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     ! 0 | 3696 | `		SyBlobFormat(&sChunk,"namespace %.*s;\n",` |
|     ! 0 | 3697 | `			(int)SyBlobLength(&pGen->sNamespace),(const char *)SyBlobData(&pGen->sNamespace));` |
|     ! 0 | 3698 | `	}` |
|      30 | 3699 | `	GenStateDeferEmitUses(&sChunk,&pGen->hUseImports,"");` |
|      30 | 3700 | `	GenStateDeferEmitUses(&sChunk,&pGen->hUseFuncImports,"function ");` |
|      30 | 3701 | `	GenStateDeferEmitUses(&sChunk,&pGen->hUseConstImports,"const ");` |
|       - | 3702 | `	/* Doc-comment and attribute groups precede the keyword in the raw source,` |
|       - | 3703 | `	 * outside the captured span — re-emit them from the trivia sidecar. */` |
|      30 | 3704 | `	if( !bAnon && pGen->sPendingDoc.nByte > 0 ){` |
|     ! 0 | 3705 | `		SyBlobAppend(&sChunk,pGen->sPendingDoc.zString,pGen->sPendingDoc.nByte);` |
|     ! 0 | 3706 | `		SyBlobAppend(&sChunk,"\n",1);` |
|     ! 0 | 3707 | `	}` |
|       - | 3708 | `	{` |
|       - | 3709 | `		ph7_trivia *aT;` |
|       - | 3710 | `		sxu32 nT,n;` |
|      30 | 3711 | `		if( bAnon ){` |
|       - | 3712 | ``			/* `new #[A] class` trivia is keyed to the 'class' token */`` |
|       8 | 3713 | `			SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|       8 | 3714 | `			aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|       8 | 3715 | `			nT = SySetUsed(&pGen->aTrivia);` |
|       8 | 3716 | `			if( pGen->pTokenSet && pKw >= pBase && pKw < &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|       8 | 3717 | `				sxu32 nIdx = (sxu32)(pKw - pBase);` |
|       8 | 3718 | `				for( n = 0 ; n < nT ; ++n ){` |
|     ! 0 | 3719 | `					if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|     ! 0 | 3720 | `						SyBlobFormat(&sChunk,"#[%.*s]\n",(int)aT[n].sText.nByte,aT[n].sText.zString);` |
|     ! 0 | 3721 | `					}` |
|     ! 0 | 3722 | `				}` |
|       3 | 3723 | `			}` |
|       5 | 3724 | `		}else{` |
|      24 | 3725 | `			aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|      24 | 3726 | `			nT = SySetUsed(&pGen->aPendingAttrs);` |
|      24 | 3727 | `			for( n = 0 ; n < nT ; ++n ){` |
|     ! 0 | 3728 | `				if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|     ! 0 | 3729 | `					SyBlobFormat(&sChunk,"#[%.*s]\n",(int)aT[n].sText.nByte,aT[n].sText.zString);` |
|     ! 0 | 3730 | `				}` |
|     ! 0 | 3731 | `			}` |
|       - | 3732 | `		}` |
|       - | 3733 | `	}` |
|       - | 3734 | `	/* Pad the prefix with newlines so the declaration keyword sits on its` |
|       - | 3735 | `	 * ORIGINAL line inside the chunk — runtime diagnostics from the deferred` |
|       - | 3736 | `	 * compile then report the source's real line. Best-effort: a prefix` |
|       - | 3737 | `	 * already longer than the declaration line skips the padding. */` |
|       - | 3738 | `	{` |
|      30 | 3739 | `		const char *zScan = (const char *)SyBlobData(&sChunk);` |
|      30 | 3740 | `		sxu32 nHave = 0;` |
|       - | 3741 | `		sxu32 nScan;` |
|      30 | 3742 | `		for( nScan = 0 ; nScan < SyBlobLength(&sChunk) ; ++nScan ){` |
|     ! 0 | 3743 | `			if( zScan[nScan] == '\n' ){` |
|     ! 0 | 3744 | `				nHave++;` |
|     ! 0 | 3745 | `			}` |
|     ! 0 | 3746 | `		}` |
|     304 | 3747 | `		while( nHave + 1 < pKw->nLine ){` |
|     276 | 3748 | `			SyBlobAppend(&sChunk,"\n",1);` |
|     276 | 3749 | `			nHave++;` |
|       2 | 3750 | `		}` |
|       - | 3751 | `	}` |
|      30 | 3752 | `	if( bAnon ){` |
|       - | 3753 | ``		/* `if (false) { new class <header-minus-args> { body } ; }` — installs`` |
|       - | 3754 | `		 * the class at the chunk's compile, never instantiates it. */` |
|       8 | 3755 | `		SyToken *pAfterArgs = &pKw[1];` |
|       8 | 3756 | `		SyBlobAppend(&sChunk,"if (false) { new ",sizeof("if (false) { new ")-1);` |
|       8 | 3757 | `		SyBlobAppend(&sChunk,pKw->sData.zString,pKw->sData.nByte);` |
|       8 | 3758 | `		if( pAfterArgs < pGen->pEnd && (pAfterArgs->nType & PH7_TK_LPAREN) ){` |
|       3 | 3759 | `			SyToken *pClose = 0;` |
|       3 | 3760 | `			PH7_DelimitNestedTokens(&pAfterArgs[1],pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|       3 | 3761 | `			if( pClose == 0 \|\| pClose >= pGen->pEnd ){` |
|     ! 0 | 3762 | `				SyBlobRelease(&sChunk);` |
|     ! 0 | 3763 | `				return SXERR_INVALID;` |
|       - | 3764 | `			}` |
|       3 | 3765 | `			pAfterArgs = &pClose[1];` |
|       1 | 3766 | `		}` |
|       8 | 3767 | `		if( pAfterArgs < pBodyEnd ){` |
|       8 | 3768 | `			SyBlobAppend(&sChunk," ",1);` |
|       8 | 3769 | `			zFrom = pAfterArgs->sData.zString;` |
|       8 | 3770 | `			zTo = pBodyEnd->sData.zString + pBodyEnd->sData.nByte;` |
|       8 | 3771 | `			SyBlobAppend(&sChunk,zFrom,(sxu32)(zTo - zFrom));` |
|       3 | 3772 | `		}` |
|       8 | 3773 | `		SyBlobAppend(&sChunk,"; }",sizeof("; }")-1);` |
|       5 | 3774 | `	}else{` |
|       - | 3775 | `		/* Modifiers were consumed before this compiler ran; reconstruct them` |
|       - | 3776 | ``		 * (an enum's implicit `final` must NOT be spelled out). */`` |
|      22 | 3777 | `		if( (iFlags & PH7_CLASS_ENUM) == 0` |
|      21 | 3778 | `		 && (pKw->nType & PH7_TK_KEYWORD)` |
|      22 | 3779 | `		 && SX_PTR_TO_INT(pKw->pUserData) == PH7_TKWRD_CLASS ){` |
|      16 | 3780 | `			if( iFlags & PH7_CLASS_FINAL ){` |
|       3 | 3781 | `				SyBlobAppend(&sChunk,"final ",sizeof("final ")-1);` |
|       1 | 3782 | `			}` |
|      16 | 3783 | `			if( iFlags & PH7_CLASS_ABSTRACT ){` |
|     ! 0 | 3784 | `				SyBlobAppend(&sChunk,"abstract ",sizeof("abstract ")-1);` |
|     ! 0 | 3785 | `			}` |
|      16 | 3786 | `			if( iFlags & PH7_CLASS_READONLY ){` |
|     ! 0 | 3787 | `				SyBlobAppend(&sChunk,"readonly ",sizeof("readonly ")-1);` |
|     ! 0 | 3788 | `			}` |
|       7 | 3789 | `		}` |
|      24 | 3790 | `		zFrom = pKw->sData.zString;` |
|      24 | 3791 | `		zTo = pBodyEnd->sData.zString + pBodyEnd->sData.nByte;` |
|      24 | 3792 | `		SyBlobAppend(&sChunk,zFrom,(sxu32)(zTo - zFrom));` |
|       - | 3793 | `	}` |
|      30 | 3794 | `	pDefer = (VmDeferredClass *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmDeferredClass));` |
|      30 | 3795 | `	if( pDefer == 0 ){` |
|     ! 0 | 3796 | `		SyBlobRelease(&sChunk);` |
|     ! 0 | 3797 | `		PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3798 | `		return SXERR_ABORT;` |
|       - | 3799 | `	}` |
|      30 | 3800 | `	SyZero(pDefer,sizeof(VmDeferredClass));` |
|      44 | 3801 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      28 | 3802 | `		(const char *)SyBlobData(&sChunk),SyBlobLength(&sChunk));` |
|      30 | 3803 | `	SyBlobRelease(&sChunk);` |
|      30 | 3804 | `	if( zDup == 0 ){` |
|     ! 0 | 3805 | `		PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3806 | `		return SXERR_ABORT;` |
|       - | 3807 | `	}` |
|      30 | 3808 | `	SyStringInitFromBuf(&pDefer->sText,zDup,SyStrlen(zDup));` |
|      30 | 3809 | `	if( bAnon ){` |
|       8 | 3810 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pAnonName->zString,pAnonName->nByte);` |
|       8 | 3811 | `		if( zDup == 0 ){` |
|     ! 0 | 3812 | `			PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3813 | `			return SXERR_ABORT;` |
|       - | 3814 | `		}` |
|       8 | 3815 | `		SyStringInitFromBuf(&pDefer->sAnonName,zDup,pAnonName->nByte);` |
|       8 | 3816 | `		pDefer->sSelfName = pDefer->sAnonName;` |
|       5 | 3817 | `	}else{` |
|      35 | 3818 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      22 | 3819 | `			(const char *)SyBlobData(pSelfFqn),SyBlobLength(pSelfFqn));` |
|      24 | 3820 | `		if( zDup == 0 ){` |
|     ! 0 | 3821 | `			PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3822 | `			return SXERR_ABORT;` |
|       - | 3823 | `		}` |
|      24 | 3824 | `		SyStringInitFromBuf(&pDefer->sSelfName,zDup,SyBlobLength(pSelfFqn));` |
|       - | 3825 | `	}` |
|      30 | 3826 | `	SySetInit(&pDefer->aRequired,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|       - | 3827 | `	{` |
|      30 | 3828 | `		VmDeferredReq *aReq = (VmDeferredReq *)SySetBasePtr(pMissing);` |
|       - | 3829 | `		sxu32 n;` |
|      62 | 3830 | `		for( n = 0 ; n < SySetUsed(pMissing) ; ++n ){` |
|      34 | 3831 | `			SySetPut(&pDefer->aRequired,(const void *)&aReq[n]);` |
|      18 | 3832 | `		}` |
|       - | 3833 | `	}` |
|      30 | 3834 | `	pDefer->nLine = pKw->nLine;` |
|      30 | 3835 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLASS_DEFER,0,0,(void *)pDefer,0);` |
|       - | 3836 | `	/* Skip the declaration: the statement cursor lands past its '}' */` |
|      30 | 3837 | `	pGen->pIn = &pBodyEnd[1];` |
|      30 | 3838 | `	return SXRET_OK;` |
|      16 | 3839 | `}` |
|       - | 3840 | `/*` |
|       - | 3841 | ` * Deferral gate shared by the named-declaration compilers: scan the` |
|       - | 3842 | ` * declaration at pGen->pIn; when a dependency is missing, capture + emit the` |
|       - | 3843 | ` * deferred record and return TRUE (the caller returns immediately — the` |
|       - | 3844 | ` * declaration compiles at execution time). FALSE means compile normally.` |
|       - | 3845 | ` * *pRc carries SXERR_ABORT out of the capture path.` |
|       - | 3846 | ` */` |
|    3650 | 3847 | `static int GenStateMaybeDeferClass(ph7_gen_state *pGen,sxi32 iFlags,int iSelfKind,sxi32 *pRc)` |
|       5 | 3848 | `{` |
|       - | 3849 | `	SySet aMissing;` |
|    3655 | 3850 | `	SyToken *pBody = 0;` |
|    3655 | 3851 | `	SyToken *pBodyEnd = 0;` |
|       - | 3852 | `	SyBlob sSelfFqn;` |
|    3655 | 3853 | `	int bDefer = 0;` |
|    3655 | 3854 | `	*pRc = SXRET_OK;` |
|    3655 | 3855 | `	SySetInit(&aMissing,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|    3655 | 3856 | `	SyBlobInit(&sSelfFqn,&pGen->pVm->sAllocator);` |
|    3650 | 3857 | `	if( GenStateScanDeferDeps(pGen,0,iSelfKind,&aMissing,&pBody,&pBodyEnd,&sSelfFqn) == SXRET_OK` |
|    3654 | 3858 | `	 && SySetUsed(&aMissing) > 0 ){` |
|      24 | 3859 | `		*pRc = GenStateEmitDeferredClass(pGen,iFlags,0,&aMissing,pBodyEnd,&sSelfFqn,0);` |
|      24 | 3860 | `		bDefer = 1;` |
|      11 | 3861 | `	}` |
|    3655 | 3862 | `	SySetRelease(&aMissing);` |
|    3655 | 3863 | `	SyBlobRelease(&sSelfFqn);` |
|    3655 | 3864 | `	return bDefer;` |
|       5 | 3865 | `}` |
|       - | 3866 | `/*` |
|       - | 3867 | ``  * Apply a declaration body's collected `use Trait[, Trait] [{ resolution }]` `` |
|       - | 3868 | ` * entries to pClass — plain application when no resolution block is present,` |
|       - | 3869 | ` * otherwise the two-pass insteadof/as machinery. Shared by the CLASS body and` |
|       - | 3870 | ` * (since the adaptation-block port) the TRAIT body compiler. Returns the last` |
|       - | 3871 | ` * application status (non-OK = out of memory at a copy site).` |
|       - | 3872 | ` */` |
|    3470 | 3873 | `static sxi32 GenStateApplyTraitUses(ph7_gen_state *pGen,ph7_class *pClass,SySet *pUseEntries)` |
|       5 | 3874 | `{` |
|    3475 | 3875 | `	sxi32 rc = SXRET_OK;` |
|       - | 3876 | `	{` |
|       - | 3877 | `		TraitUseEntry *apUse;` |
|       - | 3878 | `		sxu32 nU;` |
|    3475 | 3879 | `		apUse = (TraitUseEntry *)SySetBasePtr(pUseEntries);` |
|    3655 | 3880 | `		for( nU = 0 ; nU < SySetUsed(pUseEntries) ; nU++ ){` |
|     185 | 3881 | `			TraitUseEntry *pUse = &apUse[nU];` |
|     185 | 3882 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|     185 | 3883 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|     185 | 3884 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - | 3885 | `			sxu32 nT;` |
|     185 | 3886 | `			if( !hasResolution ){` |
|       - | 3887 | `				/* No conflict resolution block: use standard trait application */` |
|     297 | 3888 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|     155 | 3889 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|     155 | 3890 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 3891 | `						break;` |
|       - | 3892 | `					}` |
|      80 | 3893 | `				}` |
|      76 | 3894 | `			}else{` |
|       - | 3895 | `				/* With resolution block: copy attributes, record traits,` |
|       - | 3896 | `				 * then use the block to resolve method conflicts.` |
|       - | 3897 | `				 */` |
|       - | 3898 | `				SyToken *pR;` |
|      90 | 3899 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      52 | 3900 | `					ph7_class *pTR = apTrait[nT];` |
|       - | 3901 | `					ph7_class_attr *pAR;` |
|       - | 3902 | `					SyHashEntry *pER;` |
|       - | 3903 | `					SyString *pNR;` |
|      52 | 3904 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      76 | 3905 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 | 3906 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 | 3907 | `						pNR = &pAR->sName;` |
|     ! 0 | 3908 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 | 3909 | `							SyHashInsertTail(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 | 3910 | `						}` |
|     ! 0 | 3911 | `					}` |
|       - | 3912 | `					/* Trait constants (PHP 8.2) live in the separate hConst namespace */` |
|      52 | 3913 | `					SyHashResetLoopCursor(&pTR->hConst);` |
|      76 | 3914 | `					while((pER = SyHashGetNextEntry(&pTR->hConst)) != 0 ){` |
|     ! 0 | 3915 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 | 3916 | `						pNR = &pAR->sName;` |
|     ! 0 | 3917 | `						if( SyHashGet(&pClass->hConst,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 | 3918 | `							SyHashInsertTail(&pClass->hConst,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 | 3919 | `						}` |
|     ! 0 | 3920 | `					}` |
|      52 | 3921 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|      28 | 3922 | `				}` |
|       - | 3923 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      42 | 3924 | `				pR = pUse->pResolvStart;` |
|     108 | 3925 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 3926 | `					SyString sTrait,sMethod;` |
|       - | 3927 | `					ph7_class *pSrcTrait;` |
|       - | 3928 | `					ph7_class_method *pMeth;` |
|       - | 3929 | `					sxi32 nRKwrd;` |
|     174 | 3930 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|     108 | 3931 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      70 | 3932 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      70 | 3933 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      70 | 3934 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      70 | 3935 | `					sMethod = pR->sData;` |
|      70 | 3936 | `					pR++;` |
|      70 | 3937 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|      29 | 3938 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|      29 | 3939 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|      29 | 3940 | `							sTrait = sMethod;` |
|      29 | 3941 | `							pR++;` |
|      29 | 3942 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|      29 | 3943 | `							sMethod = pR->sData;` |
|      29 | 3944 | `							pR++;` |
|      13 | 3945 | `						}` |
|      13 | 3946 | `					}` |
|      70 | 3947 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 3948 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 3949 | `						continue;` |
|       - | 3950 | `					}` |
|      70 | 3951 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      70 | 3952 | `					pR++;` |
|      70 | 3953 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|      15 | 3954 | `						pSrcTrait = 0;` |
|      19 | 3955 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      19 | 3956 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      27 | 3957 | `							if( pTN->nByte >= sTrait.nByte &&` |
|      16 | 3958 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|      15 | 3959 | `								pSrcTrait = apTrait[nT];` |
|      15 | 3960 | `								break;` |
|       - | 3961 | `							}` |
|       4 | 3962 | `						}` |
|      15 | 3963 | `						if( pSrcTrait ){` |
|      15 | 3964 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|      15 | 3965 | `							if( pMeth ){` |
|      15 | 3966 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|      15 | 3967 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|      15 | 3968 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       6 | 3969 | `								}` |
|       6 | 3970 | `							}` |
|       6 | 3971 | `						}` |
|       6 | 3972 | `					}` |
|     154 | 3973 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       4 | 3974 | `				}` |
|       - | 3975 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      90 | 3976 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - | 3977 | `					ph7_class_method *pMR;` |
|       - | 3978 | `					SyHashEntry *pER;` |
|       - | 3979 | `					SyString *pNR;` |
|      52 | 3980 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|     162 | 3981 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      90 | 3982 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      90 | 3983 | `						pNR = &pMR->sFunc.sName;` |
|      90 | 3984 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      66 | 3985 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|      31 | 3986 | `						}` |
|       4 | 3987 | `					}` |
|      28 | 3988 | `				}` |
|       - | 3989 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      42 | 3990 | `				pR = pUse->pResolvStart;` |
|     108 | 3991 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 3992 | `					SyString sTrait,sMethod,sAlias;` |
|       - | 3993 | `					ph7_class *pSrcTrait;` |
|       - | 3994 | `					ph7_class_method *pMeth;` |
|     108 | 3995 | `					int hasQual = 0;` |
|       - | 3996 | `					sxi32 nRKwrd;` |
|     174 | 3997 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|     108 | 3998 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      70 | 3999 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      70 | 4000 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      70 | 4001 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      70 | 4002 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      70 | 4003 | `					sMethod = pR->sData;` |
|      70 | 4004 | `					pR++;` |
|      70 | 4005 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|      29 | 4006 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|      29 | 4007 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|      29 | 4008 | `							sTrait = sMethod;` |
|      29 | 4009 | `							hasQual = 1;` |
|      29 | 4010 | `							pR++;` |
|      29 | 4011 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|      29 | 4012 | `							sMethod = pR->sData;` |
|      29 | 4013 | `							pR++;` |
|      13 | 4014 | `						}` |
|      13 | 4015 | `					}` |
|      70 | 4016 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4017 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 4018 | `						continue;` |
|       - | 4019 | `					}` |
|      70 | 4020 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      70 | 4021 | `					pR++;` |
|      70 | 4022 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      58 | 4023 | `						sxi32 iNewVis = -1;` |
|      58 | 4024 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|      23 | 4025 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|      23 | 4026 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|      23 | 4027 | `								iNewVis = nAK;` |
|      23 | 4028 | `								pR++;` |
|      10 | 4029 | `							}` |
|      10 | 4030 | `						}` |
|      58 | 4031 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|      56 | 4032 | `							sAlias = pR->sData;` |
|      56 | 4033 | `							pR++;` |
|      26 | 4034 | `						}` |
|      58 | 4035 | `						pMeth = 0;` |
|      58 | 4036 | `						if( hasQual ){` |
|      17 | 4037 | `							pSrcTrait = 0;` |
|      27 | 4038 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      27 | 4039 | `								SyString *pTN = &apTrait[nT]->sName;` |
|      39 | 4040 | `								if( pTN->nByte >= sTrait.nByte &&` |
|      24 | 4041 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|      17 | 4042 | `									pSrcTrait = apTrait[nT];` |
|      17 | 4043 | `									break;` |
|       - | 4044 | `								}` |
|       8 | 4045 | `							}` |
|      17 | 4046 | `							if( pSrcTrait ){` |
|      17 | 4047 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       7 | 4048 | `							}` |
|      10 | 4049 | `						}else{` |
|      44 | 4050 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - | 4051 | `						}` |
|      58 | 4052 | `						if( pMeth ){` |
|       - | 4053 | `							/* php: a method declared in the class BODY wins over a trait alias` |
|       - | 4054 | ``							 * of the same name (e.g. an explicit __construct over `init as`` |
|       - | 4055 | ``							 * __construct`). If pClass already declares sAlias ITSELF — an own`` |
|       - | 4056 | `							 * method, sFunc.pUserData == pClass — keep it: SyHashInsert is LIFO,` |
|       - | 4057 | `							 * so an unconditional insert would shadow the class method at lookup` |
|       - | 4058 | ``							 * and `new` would run the alias. A name held only by another trait is`` |
|       - | 4059 | `							 * a genuine conflict resolved by the insteadof pass above. */` |
|      58 | 4060 | `							int bClassWins = 0;` |
|      58 | 4061 | `							if( sAlias.nByte > 0 ){` |
|      56 | 4062 | `								ph7_class_method *pOwn = PH7_ClassExtractMethod(pClass,sAlias.zString,sAlias.nByte);` |
|      56 | 4063 | `								bClassWins = (pOwn && pOwn->sFunc.pUserData == pClass);` |
|      26 | 4064 | `							}` |
|      81 | 4065 | `							if( sAlias.nByte > 0 && !bClassWins ){` |
|       - | 4066 | `								/* Create a shallow copy of the method struct for the alias` |
|       - | 4067 | `								 * so it can carry its own visibility without affecting the original.` |
|       - | 4068 | `								 */` |
|       - | 4069 | `								ph7_class_method *pAlias;` |
|       - | 4070 | `								char *zAliasDup;` |
|      50 | 4071 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|      50 | 4072 | `								if( pAlias ){` |
|      50 | 4073 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|      50 | 4074 | `									if( iNewVis >= 0 ){` |
|      21 | 4075 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|      17 | 4076 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       7 | 4077 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       9 | 4078 | `									}` |
|      50 | 4079 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      50 | 4080 | `									if( zAliasDup ){` |
|      50 | 4081 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|      23 | 4082 | `									}` |
|      27 | 4083 | `								}` |
|      33 | 4084 | `							}else if( sAlias.nByte == 0 && iNewVis >= 0 ){` |
|       - | 4085 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - | 4086 | `								ph7_class_method *pCopy;` |
|       3 | 4087 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 | 4088 | `								if( pCopy ){` |
|       3 | 4089 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 | 4090 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 | 4091 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 4092 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 4093 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - | 4094 | `									/* Replace the method in the class hash */` |
|       3 | 4095 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 | 4096 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 | 4097 | `								}` |
|       1 | 4098 | `							}` |
|      27 | 4099 | `						}` |
|      27 | 4100 | `						SXUNUSED(hasQual);` |
|      27 | 4101 | `					}` |
|      82 | 4102 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       4 | 4103 | `				}` |
|       - | 4104 | `			}` |
|     185 | 4105 | `			SySetRelease(&pUse->aTraits);` |
|      95 | 4106 | `		}` |
|       - | 4107 | `	}` |
|    3475 | 4108 | `	return rc;` |
|       5 | 4109 | `}` |
|       - | 4110 | `/*` |
|       - | 4111 | ` * Compile a class declaration, named or anonymous.` |
|       - | 4112 | ` *` |
|       - | 4113 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|       - | 4114 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|       - | 4115 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|       - | 4116 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|       - | 4117 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|       - | 4118 | ` * implements, body, install) is shared by both paths.` |
|       - | 4119 | ` */` |
|    3358 | 4120 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|       - | 4121 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|       5 | 4122 | `{` |
|    3363 | 4123 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4124 | `	ph7_class *pClass,*pBase;` |
|    3363 | 4125 | `	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' (enclosing class for a nested anon) */` |
|       - | 4126 | `	SyToken *pEnd,*pTmp;` |
|       - | 4127 | `	sxi32 iProtection;` |
|       - | 4128 | `	SySet aInterfaces;` |
|       - | 4129 | `	SySet aUseEntries;` |
|       - | 4130 | `	sxi32 iAttrflags;` |
|       - | 4131 | `	SyString *pName;` |
|       - | 4132 | `	sxi32 nKwrd;` |
|       - | 4133 | `	sxi32 rc;` |
|    3363 | 4134 | `	if( pAnonName == 0 ){` |
|       - | 4135 | `		/* Deferral gate: an unresolvable parent/interface/trait —` |
|       - | 4136 | `		 * its autoloader has not RUN yet — re-compiles this declaration at its` |
|       - | 4137 | `		 * execution point instead of dying on "Nonexistent base class". */` |
|       - | 4138 | `		sxi32 rcDefer;` |
|    3295 | 4139 | `		if( GenStateMaybeDeferClass(pGen,iFlags,PH7_DEFER_KIND_CLASS,&rcDefer) ){` |
|      18 | 4140 | `			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 4141 | `		}` |
|    1637 | 4142 | `	}` |
|       - | 4143 | `	/* Jump the 'class' keyword */` |
|    3347 | 4144 | `	pGen->pIn++;` |
|    3347 | 4145 | `	if( pAnonName ){` |
|       - | 4146 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|       - | 4147 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|       - | 4148 | `		 * then use the synthesized name. */` |
|      73 | 4149 | `		*ppArgStart = *ppArgEnd = 0;` |
|      73 | 4150 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|      13 | 4151 | `			pGen->pIn++; /* Jump '(' */` |
|      13 | 4152 | `			*ppArgStart = pGen->pIn;` |
|      19 | 4153 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|       6 | 4154 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|      13 | 4155 | `			pGen->pIn = *ppArgEnd;` |
|      13 | 4156 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|       6 | 4157 | `		}` |
|      73 | 4158 | `		pName = pAnonName;` |
|      73 | 4159 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|      39 | 4160 | `	}else{` |
|    3279 | 4161 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4162 | `			/* Syntax error */` |
|     ! 0 | 4163 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 | 4164 | `			if( rc == SXERR_ABORT ){` |
|       - | 4165 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4166 | `				return SXERR_ABORT;` |
|       - | 4167 | `			}` |
|       - | 4168 | `			/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 | 4169 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 | 4170 | `				pGen->pIn++;` |
|     ! 0 | 4171 | `			}` |
|     ! 0 | 4172 | `			return SXRET_OK;` |
|       - | 4173 | `		}` |
|       - | 4174 | `		/* Extract class name */` |
|    3279 | 4175 | `		pName = &pGen->pIn->sData;` |
|       - | 4176 | `		/* Advance the stream cursor */` |
|    3279 | 4177 | `		pGen->pIn++;` |
|       - | 4178 | `		/* Build FQN and obtain a raw class */ {` |
|       - | 4179 | `			SyBlob sFQN;` |
|       - | 4180 | `			SyString sFQNStr;` |
|    3279 | 4181 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    3279 | 4182 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|    3279 | 4183 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       - | 4184 | ``			/* php refuses a declaration whose short name a local `use` already took. */`` |
|    3279 | 4185 | `			if( GenStateGuardImportRedeclare(pGen,0,pName,&sFQNStr,nLine) == SXERR_ABORT ){` |
|     ! 0 | 4186 | `				SyBlobRelease(&sFQN);` |
|     ! 0 | 4187 | `				return SXERR_ABORT;` |
|       - | 4188 | `			}` |
|    3279 | 4189 | `			GenStateRecordDeclaredName(pGen,0,&sFQNStr);` |
|    3279 | 4190 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    3279 | 4191 | `			SyBlobRelease(&sFQN);` |
|       - | 4192 | `		}` |
|       - | 4193 | `	}` |
|    3347 | 4194 | `	if( pClass == 0 ){` |
|     ! 0 | 4195 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4196 | `		return SXERR_ABORT;` |
|       - | 4197 | `	}` |
|    3342 | 4198 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|     101 | 4199 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|       - | 4200 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|      54 | 4201 | `		pGen->pIn++; /* Jump ':' */` |
|      50 | 4202 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      54 | 4203 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|      18 | 4204 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|      18 | 4205 | `			pGen->pIn++;` |
|      44 | 4206 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      38 | 4207 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|      36 | 4208 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|      36 | 4209 | `			pGen->pIn++;` |
|      20 | 4210 | `		}else{` |
|       3 | 4211 | `			SyToken *pTok = pGen->pIn;` |
|       3 | 4212 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|       4 | 4213 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|       1 | 4214 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|       3 | 4215 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4216 | `				return SXERR_ABORT;` |
|       - | 4217 | `			}` |
|       3 | 4218 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       3 | 4219 | `				pGen->pIn++; /* Skip the bogus type token */` |
|       1 | 4220 | `			}` |
|       - | 4221 | `		}` |
|      25 | 4222 | `	}` |
|    3347 | 4223 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    3347 | 4224 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|     ! 0 | 4225 | `		return SXERR_ABORT;` |
|       - | 4226 | `	}` |
|       - | 4227 | `	/* implemented interfaces and per-use-statement trait containers */` |
|    3347 | 4228 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    3347 | 4229 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - | 4230 | `	/* Assume a standalone class */` |
|    3347 | 4231 | `	pBase = 0;` |
|    3347 | 4232 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     807 | 4233 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     807 | 4234 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - | 4235 | `			SyBlob sResolved;` |
|       - | 4236 | `			SyString sBaseName;` |
|       - | 4237 | `			sxu32 nRefLine;` |
|     565 | 4238 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|       - | 4239 | `				/* php parse-fatals here (enums have no inheritance) */` |
|     ! 0 | 4240 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4241 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|     ! 0 | 4242 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4243 | `					return SXERR_ABORT;` |
|       - | 4244 | `				}` |
|     ! 0 | 4245 | `			}` |
|     565 | 4246 | `			pGen->pIn++; /* Advance past 'extends' */` |
|     565 | 4247 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     565 | 4248 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     565 | 4249 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 | 4250 | `				SyBlobRelease(&sResolved);` |
|       4 | 4251 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4252 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 | 4253 | `					pName);` |
|       3 | 4254 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 | 4255 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4256 | `					return SXERR_ABORT;` |
|       - | 4257 | `				}` |
|       3 | 4258 | `				return SXRET_OK;` |
|       - | 4259 | `			}` |
|     842 | 4260 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|     558 | 4261 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     563 | 4262 | `			SyStringInitFromBuf(&sBaseName,` |
|       - | 4263 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - | 4264 | `			/* Interfaces are not allowed */` |
|     563 | 4265 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 | 4266 | `				pBase = pBase->pNextName;` |
|     ! 0 | 4267 | `			}` |
|     563 | 4268 | `			if( pBase == 0 ){` |
|     ! 0 | 4269 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - | 4270 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 | 4271 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4272 | `					SyBlobRelease(&sResolved);` |
|     ! 0 | 4273 | `					return SXERR_ABORT;` |
|       - | 4274 | `				}` |
|     ! 0 | 4275 | `			}else{` |
|     563 | 4276 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|       4 | 4277 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 | 4278 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|       3 | 4279 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4280 | `						SyBlobRelease(&sResolved);` |
|     ! 0 | 4281 | `						return SXERR_ABORT;` |
|       - | 4282 | `					}` |
|       3 | 4283 | `					pBase = 0; /* Never inherit from an enum */` |
|     562 | 4284 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 | 4285 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4286 | `						/* php's wording, unquoted: "Class B cannot extend final class A". */` |
|     ! 0 | 4287 | `						"Class %z cannot extend final class %z",pName,&pBase->sName);` |
|     ! 0 | 4288 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4289 | `						SyBlobRelease(&sResolved);` |
|     ! 0 | 4290 | `						return SXERR_ABORT;` |
|       - | 4291 | `					}` |
|     ! 0 | 4292 | `				}` |
|       - | 4293 | `			}` |
|     563 | 4294 | `			SyBlobRelease(&sResolved);` |
|     563 | 4295 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|     ! 0 | 4296 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|     ! 0 | 4297 | `			}` |
|     279 | 4298 | `		}` |
|     805 | 4299 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - | 4300 | `			ph7_class *pInterface;` |
|       - | 4301 | `			/* Interface implementation */` |
|     275 | 4302 | `			pGen->pIn++; /* Advance the stream cursor */` |
|     149 | 4303 | `			for(;;){` |
|       - | 4304 | `				SyBlob sResolved;` |
|       - | 4305 | `				SyString sIntName;` |
|       - | 4306 | `				sxu32 nRefLine;` |
|     289 | 4307 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     289 | 4308 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     289 | 4309 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 4310 | `					SyBlobRelease(&sResolved);` |
|     ! 0 | 4311 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4312 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 | 4313 | `						pName);` |
|     ! 0 | 4314 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4315 | `						return SXERR_ABORT;` |
|       - | 4316 | `					}` |
|     ! 0 | 4317 | `					break;` |
|       - | 4318 | `				}` |
|     573 | 4319 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|     284 | 4320 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     289 | 4321 | `				SyStringInitFromBuf(&sIntName,` |
|       - | 4322 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - | 4323 | `				/* Only interfaces are allowed */` |
|     289 | 4324 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 4325 | `					pInterface = pInterface->pNextName;` |
|     ! 0 | 4326 | `				}` |
|     289 | 4327 | `				if( pInterface == 0 ){` |
|     ! 0 | 4328 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - | 4329 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 | 4330 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4331 | `						SyBlobRelease(&sResolved);` |
|     ! 0 | 4332 | `						return SXERR_ABORT;` |
|       - | 4333 | `					}` |
|     ! 0 | 4334 | `				}else{` |
|       - | 4335 | `					/* Reject user classes that try to implement Throwable` |
|       - | 4336 | `					 * directly (or via an interface that extends Throwable)` |
|       - | 4337 | `					 * unless they already extend Exception or Error.` |
|       - | 4338 | `					 * Exception and Error themselves are compiled from the` |
|       - | 4339 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - | 4340 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|     289 | 4341 | `					SyString *pFqn = &pClass->sName;` |
|     289 | 4342 | `					int bIsExceptionOrError =` |
|     149 | 4343 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|     433 | 4344 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|     292 | 4345 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|      16 | 4346 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|     289 | 4347 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|      18 | 4348 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|       3 | 4349 | `						!bIsExceptionOrError ){` |
|      12 | 4350 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4351 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 | 4352 | `							&pClass->sName);` |
|       9 | 4353 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 4354 | `							SyBlobRelease(&sResolved);` |
|     ! 0 | 4355 | `							return SXERR_ABORT;` |
|       - | 4356 | `						}` |
|       - | 4357 | `						/* Skip registration so the follow-up abstract-method` |
|       - | 4358 | `						 * check does not produce a duplicate fatal. */` |
|       6 | 4359 | `					}else{` |
|     283 | 4360 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - | 4361 | `					}` |
|       - | 4362 | `				}` |
|     289 | 4363 | `				SyBlobRelease(&sResolved);` |
|     289 | 4364 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     140 | 4365 | `					break;` |
|       - | 4366 | `				}` |
|      18 | 4367 | `				pGen->pIn++;/* Jump the comma */` |
|       4 | 4368 | `			}` |
|     135 | 4369 | `		}` |
|     400 | 4370 | `	}` |
|    3345 | 4371 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 4372 | `		/* Syntax error */` |
|     ! 0 | 4373 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 | 4374 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4375 | `		if( rc == SXERR_ABORT ){` |
|       - | 4376 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4377 | `			return SXERR_ABORT;` |
|       - | 4378 | `		}` |
|     ! 0 | 4379 | `		return SXRET_OK;` |
|       - | 4380 | `	}` |
|    3345 | 4381 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    3345 | 4382 | `	pEnd = 0; /* cc warning */` |
|       - | 4383 | `	/* Delimit the class body */` |
|    3345 | 4384 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    3345 | 4385 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4386 | `		/* Syntax error */` |
|     ! 0 | 4387 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 | 4388 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4389 | `		if( rc == SXERR_ABORT ){` |
|       - | 4390 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4391 | `			return SXERR_ABORT;` |
|       - | 4392 | `		}` |
|     ! 0 | 4393 | `		return SXRET_OK;` |
|       - | 4394 | `	}` |
|       - | 4395 | `	/* The delimiter token is the class body's closing brace */` |
|    3345 | 4396 | `	pClass->nEndLine = pEnd->nLine;` |
|       - | 4397 | `	/* Swap token stream */` |
|    3345 | 4398 | `	pTmp = pGen->pEnd;` |
|    3345 | 4399 | `	pGen->pEnd = pEnd;` |
|       - | 4400 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|    3345 | 4401 | `	pClass->iFlags \|= iFlags;` |
|       - | 4402 | `	/* This class/enum is now the lexical class for its body — see pCurClass. */` |
|    3345 | 4403 | `	pGen->pCurClass = pClass;` |
|       - | 4404 | `	/* Start the parse process */` |
|    3338 | 4405 | `	for(;;){` |
|       - | 4406 | `		/* Jump leading/trailing semi-colons */` |
|   10997 | 4407 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    2293 | 4408 | `			pGen->pIn++;` |
|       5 | 4409 | `		}` |
|    8709 | 4410 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4411 | `			/* End of class body */` |
|    3295 | 4412 | `			break;` |
|       - | 4413 | `		}` |
|       - | 4414 | `		/* Bind a directly-preceding docblock to this member */` |
|    5419 | 4415 | `		GenStateSetPendingDoc(&(*pGen));` |
|    5414 | 4416 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|    2712 | 4417 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|     ! 0 | 4418 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4419 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 4420 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 4421 | `			if( rc == SXERR_ABORT ){` |
|       - | 4422 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4423 | `				return SXERR_ABORT;` |
|       - | 4424 | `			}` |
|     ! 0 | 4425 | `			goto done;` |
|       - | 4426 | `		}` |
|       - | 4427 | `		/* Assume public visibility */` |
|    5419 | 4428 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|    5419 | 4429 | `		iAttrflags = 0;` |
|       - | 4430 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|       - | 4431 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|       - | 4432 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|       - | 4433 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|    5419 | 4434 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 | 4435 | `			int bMod = 0;` |
|     ! 0 | 4436 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 | 4437 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       - | 4438 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|       - | 4439 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|       - | 4440 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|       - | 4441 | `			 * that the generic keyword dispatch would misread as a method. */` |
|     ! 0 | 4442 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     ! 0 | 4443 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 | 4444 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|     ! 0 | 4445 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|     ! 0 | 4446 | `			}` |
|     ! 0 | 4447 | `			if( !bMod ){` |
|     ! 0 | 4448 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 4449 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 4450 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4451 | `						return SXERR_ABORT;` |
|       - | 4452 | `					}` |
|     ! 0 | 4453 | `					goto done;` |
|       - | 4454 | `				}` |
|     ! 0 | 4455 | `				continue;` |
|       - | 4456 | `			}` |
|     ! 0 | 4457 | `		}` |
|    5419 | 4458 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 4459 | `			/* Extract the current keyword */` |
|    5419 | 4460 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    5419 | 4461 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|       - | 4462 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|     133 | 4463 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|     133 | 4464 | `				if( rc != SXRET_OK ){` |
|       6 | 4465 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4466 | `						return SXERR_ABORT;` |
|       - | 4467 | `					}` |
|       6 | 4468 | `					goto done;` |
|       - | 4469 | `				}` |
|     128 | 4470 | `				continue;` |
|       - | 4471 | `			}` |
|    5291 | 4472 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 4473 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - | 4474 | `				TraitUseEntry sUse;` |
|     171 | 4475 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|     171 | 4476 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|     171 | 4477 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      97 | 4478 | `				for(;;){` |
|       - | 4479 | `					ph7_class *pTrait;` |
|       - | 4480 | `					SyBlob sResolved;` |
|       - | 4481 | `					SyString sTraitName;` |
|     185 | 4482 | `					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|       - | 4483 | `					/* A trait name is a full class reference: it may be qualified or` |
|       - | 4484 | ``					 * fully-qualified (`use Foo\Bar\T;`, `use \Foo\Bar\T;`) — the generated`` |
|       - | 4485 | `					 * PHPUnit mocks name their traits absolutely. Parse it with the shared` |
|       - | 4486 | `					 * class-reference reader (handles the leading '\', every '\'-segment and` |
|       - | 4487 | `					 * namespace/import resolution) instead of a single-identifier read, which` |
|       - | 4488 | `					 * choked on the first '\'. */` |
|     185 | 4489 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     185 | 4490 | `					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 4491 | `						SyBlobRelease(&sResolved);` |
|     ! 0 | 4492 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|     ! 0 | 4493 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 | 4494 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 4495 | `							return SXERR_ABORT;` |
|       - | 4496 | `						}` |
|     ! 0 | 4497 | `						break;` |
|       - | 4498 | `					}` |
|     365 | 4499 | `					pTrait = PH7_VmExtractClass(pGen->pVm,` |
|     180 | 4500 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     185 | 4501 | `					SyStringInitFromBuf(&sTraitName,` |
|       - | 4502 | `						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - | 4503 | `					/* Only traits are allowed */` |
|     185 | 4504 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 4505 | `						pTrait = pTrait->pNextName;` |
|     ! 0 | 4506 | `					}` |
|     185 | 4507 | `					if( pTrait == 0 ){` |
|     ! 0 | 4508 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|       - | 4509 | `							"'%z' is not a trait",&sTraitName);` |
|     ! 0 | 4510 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 4511 | `							SyBlobRelease(&sResolved);` |
|     ! 0 | 4512 | `							return SXERR_ABORT;` |
|       - | 4513 | `						}` |
|     ! 0 | 4514 | `					}else{` |
|     185 | 4515 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - | 4516 | `					}` |
|     185 | 4517 | `					SyBlobRelease(&sResolved);` |
|       - | 4518 | `					/* GenStateParseClassReference already advanced past the whole name —` |
|       - | 4519 | `					 * continue only across a comma-separated trait list. */` |
|     185 | 4520 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      88 | 4521 | `						break;` |
|       - | 4522 | `					}` |
|      18 | 4523 | `					pGen->pIn++; /* Jump the comma */` |
|       4 | 4524 | `				}` |
|       - | 4525 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|     171 | 4526 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 4527 | `					SyToken *pBlock;` |
|      38 | 4528 | `					pGen->pIn++; /* Jump '{' */` |
|      38 | 4529 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      38 | 4530 | `					sUse.pResolvStart = pGen->pIn;` |
|      38 | 4531 | `					sUse.pResolvEnd = pBlock;` |
|      38 | 4532 | `					if( pBlock < pGen->pEnd ){` |
|      38 | 4533 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|      21 | 4534 | `					}else{` |
|     ! 0 | 4535 | `						pGen->pIn = pGen->pEnd;` |
|       - | 4536 | `					}` |
|      17 | 4537 | `				}` |
|     171 | 4538 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - | 4539 | `				/* The semicolon will be consumed by the outer loop */` |
|     171 | 4540 | `				continue;` |
|       - | 4541 | `			}` |
|    5125 | 4542 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 4543 | `				int nSetTok;` |
|    4261 | 4544 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|    4261 | 4545 | `				if( nSetVis ){` |
|       - | 4546 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|       - | 4547 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|       3 | 4548 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|       3 | 4549 | `					pGen->pIn += nSetTok;` |
|       2 | 4550 | `				}else{` |
|    4259 | 4551 | `					iProtection = nKwrd;` |
|    4259 | 4552 | `					pGen->pIn++; /* Jump the visibility token */` |
|       - | 4553 | `					/* Optional asymmetric set-visibility after the read` |
|       - | 4554 | ``					 * visibility: `public private(set) int $x`. */`` |
|    4259 | 4555 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|    4259 | 4556 | `					if( nSetVis ){` |
|      15 | 4557 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|      15 | 4558 | `						pGen->pIn += nSetTok;` |
|       7 | 4559 | `					}` |
|       - | 4560 | `				}` |
|       - | 4561 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|       - | 4562 | ``				 * `public private(set) readonly int $x`. */`` |
|    4261 | 4563 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      38 | 4564 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      38 | 4565 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|      17 | 4566 | `				}` |
|    4256 | 4567 | `				if( pGen->pIn >= pGen->pEnd` |
|    4261 | 4568 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 | 4569 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4570 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 4571 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 4572 | `					if( rc == SXERR_ABORT ){` |
|       - | 4573 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 4574 | `						return SXERR_ABORT;` |
|       - | 4575 | `					}` |
|     ! 0 | 4576 | `					goto done;` |
|       - | 4577 | `				}` |
|    4261 | 4578 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 4579 | `					/* Attribute declaration (untyped) */` |
|    1027 | 4580 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    1027 | 4581 | `					if( rc != SXRET_OK ){` |
|      12 | 4582 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 4583 | `							return SXERR_ABORT;` |
|       - | 4584 | `						}` |
|      12 | 4585 | `						goto done;` |
|       - | 4586 | `					}` |
|    1264 | 4587 | `					continue;` |
|       - | 4588 | `				}` |
|    3239 | 4589 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - | 4590 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     501 | 4591 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     501 | 4592 | `					if( rc != SXRET_OK ){` |
|       8 | 4593 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 4594 | `							return SXERR_ABORT;` |
|       - | 4595 | `						}` |
|       8 | 4596 | `						goto done;` |
|       - | 4597 | `					}` |
|     495 | 4598 | `					continue;` |
|       - | 4599 | `				}` |
|       - | 4600 | `				/* Extract the keyword */` |
|    2743 | 4601 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    1369 | 4602 | `			}` |
|    3607 | 4603 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 4604 | `				/* Process constant declaration */` |
|     421 | 4605 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|     421 | 4606 | `				if( rc != SXRET_OK ){` |
|      15 | 4607 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4608 | `						return SXERR_ABORT;` |
|       - | 4609 | `					}` |
|      15 | 4610 | `					goto done;` |
|       - | 4611 | `				}` |
|     207 | 4612 | `			}else{` |
|    3191 | 4613 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 4614 | `					/* Static method or attribute,record that */` |
|     545 | 4615 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     545 | 4616 | `					pGen->pIn++; /* Jump the static keyword */` |
|     545 | 4617 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 4618 | `						int nSetTok;` |
|     387 | 4619 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|     387 | 4620 | `						if( nSetVis ){` |
|       - | 4621 | ``							/* `static private(set) int $x` — read side stays public */`` |
|       3 | 4622 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|       3 | 4623 | `							pGen->pIn += nSetTok;` |
|       2 | 4624 | `						}else{` |
|       - | 4625 | `							/* Extract the keyword */` |
|     385 | 4626 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     385 | 4627 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 4628 | `								iProtection = nKwrd;` |
|     ! 0 | 4629 | `								pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 | 4630 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|     ! 0 | 4631 | `								if( nSetVis ){` |
|     ! 0 | 4632 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|     ! 0 | 4633 | `									pGen->pIn += nSetTok;` |
|     ! 0 | 4634 | `								}` |
|     ! 0 | 4635 | `							}` |
|       - | 4636 | `						}` |
|     191 | 4637 | `					}` |
|       - | 4638 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|       - | 4639 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|       - | 4640 | `					 * than a generic "expecting method" parse error. */` |
|     545 | 4641 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 | 4642 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 | 4643 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|     ! 0 | 4644 | `					}` |
|     540 | 4645 | `					if( pGen->pIn >= pGen->pEnd` |
|     545 | 4646 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 | 4647 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4648 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 | 4649 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 4650 | `						if( rc == SXERR_ABORT ){` |
|       - | 4651 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 4652 | `							return SXERR_ABORT;` |
|       - | 4653 | `						}` |
|     ! 0 | 4654 | `						goto done;` |
|       - | 4655 | `					}` |
|     545 | 4656 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 4657 | `						/* Attribute declaration */` |
|     155 | 4658 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     155 | 4659 | `						if( rc != SXRET_OK ){` |
|       3 | 4660 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 4661 | `								return SXERR_ABORT;` |
|       - | 4662 | `							}` |
|       3 | 4663 | `							goto done;` |
|       - | 4664 | `						}` |
|     153 | 4665 | `						continue;` |
|       - | 4666 | `					}` |
|     395 | 4667 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - | 4668 | `						/* Typed static attribute declaration */` |
|      67 | 4669 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      67 | 4670 | `						if( rc != SXRET_OK ){` |
|       3 | 4671 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 4672 | `								return SXERR_ABORT;` |
|       - | 4673 | `							}` |
|       3 | 4674 | `							goto done;` |
|       - | 4675 | `						}` |
|      65 | 4676 | `						continue;` |
|       - | 4677 | `					}` |
|       - | 4678 | `					/* Extract the keyword */` |
|     333 | 4679 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    2815 | 4680 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - | 4681 | `					/* Abstract method,record that.` |
|       - | 4682 | `					 * PHL used to also mark the whole CLASS abstract here, silently` |
|       - | 4683 | ``					 * promoting `class C{abstract function m();}` -- which php rejects`` |
|       - | 4684 | `					 * outright -- into a valid abstract class. That promotion is why` |
|       - | 4685 | `					 * GenStateCheckAbstractMethods never fired for it: by the time the` |
|       - | 4686 | `					 * check ran, the class looked declared-abstract. The declaration is` |
|       - | 4687 | `					 * now diagnosed where the method name is known (see the install` |
|       - | 4688 | `					 * site), so the class flag stays what the SOURCE said. */` |
|      47 | 4689 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - | 4690 | `					/* Advance the stream cursor */` |
|      47 | 4691 | `					pGen->pIn++;` |
|      47 | 4692 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      47 | 4693 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      47 | 4694 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      45 | 4695 | `							iProtection = nKwrd;` |
|      45 | 4696 | `							pGen->pIn++; /* Jump the visibility token */` |
|      20 | 4697 | `						}` |
|      21 | 4698 | `					}` |
|      47 | 4699 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      42 | 4700 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 4701 | `							/* Static method */` |
|     ! 0 | 4702 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 4703 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 4704 | `					}` |
|      47 | 4705 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      42 | 4706 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       - | 4707 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|       - | 4708 | `							 * HOOKED property declaration. Route anything that is not a` |
|       - | 4709 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|       - | 4710 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|       - | 4711 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|       8 | 4712 | `							if( pGen->pIn < pGen->pEnd` |
|      10 | 4713 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|       4 | 4714 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|      10 | 4715 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      10 | 4716 | `								if( rc != SXRET_OK ){` |
|     ! 0 | 4717 | `									if( rc == SXERR_ABORT ){` |
|     ! 0 | 4718 | `										return SXERR_ABORT;` |
|       - | 4719 | `									}` |
|     ! 0 | 4720 | `									goto done;` |
|       - | 4721 | `								}` |
|      10 | 4722 | `								continue;` |
|       - | 4723 | `							}` |
|     ! 0 | 4724 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4725 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 | 4726 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 4727 | `							if( rc == SXERR_ABORT ){` |
|       - | 4728 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 4729 | `								return SXERR_ABORT;` |
|       - | 4730 | `							}` |
|     ! 0 | 4731 | `							goto done;` |
|       - | 4732 | `					}` |
|      39 | 4733 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|    2626 | 4734 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - | 4735 | `					/* final method ,record that */` |
|      34 | 4736 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|      34 | 4737 | `					pGen->pIn++; /* Jump the final keyword */` |
|      34 | 4738 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 4739 | `						/* Extract the keyword */` |
|      34 | 4740 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      34 | 4741 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      21 | 4742 | `							iProtection = nKwrd;` |
|      21 | 4743 | `							pGen->pIn++; /* Jump the visibility token */` |
|       9 | 4744 | `						}` |
|      15 | 4745 | `					}` |
|      34 | 4746 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      30 | 4747 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|       - | 4748 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|       - | 4749 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|       - | 4750 | `							 * child class is compiled (PH7_ClassInherit). */` |
|      20 | 4751 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      20 | 4752 | `							if( rc != SXRET_OK ){` |
|     ! 0 | 4753 | `								if( rc == SXERR_ABORT ){` |
|     ! 0 | 4754 | `									return SXERR_ABORT;` |
|       - | 4755 | `								}` |
|     ! 0 | 4756 | `								goto done;` |
|       - | 4757 | `							}` |
|      20 | 4758 | `							continue;` |
|       - | 4759 | `					}` |
|      15 | 4760 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      12 | 4761 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 4762 | `							/* Static method */` |
|       3 | 4763 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       3 | 4764 | `							pGen->pIn++; /* Jump the static keyword */` |
|       1 | 4765 | `					}` |
|      15 | 4766 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      12 | 4767 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 4768 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4769 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 | 4770 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 4771 | `							if( rc == SXERR_ABORT ){` |
|       - | 4772 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 4773 | `								return SXERR_ABORT;` |
|       - | 4774 | `							}` |
|     ! 0 | 4775 | `							goto done;` |
|       - | 4776 | `					}` |
|      15 | 4777 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       6 | 4778 | `				}` |
|    2953 | 4779 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 4780 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4781 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 | 4782 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 4783 | `						if( rc == SXERR_ABORT ){` |
|       - | 4784 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 4785 | `							return SXERR_ABORT;` |
|       - | 4786 | `						}` |
|     ! 0 | 4787 | `						goto done;` |
|       - | 4788 | `				}` |
|    2953 | 4789 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 | 4790 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 | 4791 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 | 4792 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4793 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 4794 | `						if( rc == SXERR_ABORT ){` |
|       - | 4795 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 4796 | `							return SXERR_ABORT;` |
|       - | 4797 | `						}` |
|     ! 0 | 4798 | `						goto done;` |
|       - | 4799 | `					}` |
|       - | 4800 | `					/* Attribute declaration */` |
|       7 | 4801 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 | 4802 | `				}else{` |
|       - | 4803 | `					/* Process method declaration */` |
|    2947 | 4804 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 4805 | `				}` |
|    2953 | 4806 | `				if( rc != SXRET_OK ){` |
|      20 | 4807 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4808 | `						return SXERR_ABORT;` |
|       - | 4809 | `					}` |
|      20 | 4810 | `					goto done;` |
|       - | 4811 | `				}` |
|       - | 4812 | `			}` |
|    1673 | 4813 | `		}else{` |
|       - | 4814 | `			/* Attribute declaration */` |
|     ! 0 | 4815 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 4816 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 4817 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4818 | `					return SXERR_ABORT;` |
|       - | 4819 | `				}` |
|     ! 0 | 4820 | `				goto done;` |
|       - | 4821 | `			}` |
|       - | 4822 | `		}` |
|       5 | 4823 | `	}` |
|       - | 4824 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - | 4825 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - | 4826 | `	 */` |
|    3295 | 4827 | `	rc = GenStateApplyTraitUses(&(*pGen),pClass,&aUseEntries);` |
|    3295 | 4828 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4829 | `		SySetRelease(&aUseEntries);` |
|     ! 0 | 4830 | `		SySetRelease(&aInterfaces);` |
|     ! 0 | 4831 | `		return SXERR_ABORT;` |
|       - | 4832 | `	}` |
|    3295 | 4833 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|       - | 4834 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|       - | 4835 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|      97 | 4836 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|      97 | 4837 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4838 | `			SySetRelease(&aUseEntries);` |
|     ! 0 | 4839 | `			SySetRelease(&aInterfaces);` |
|     ! 0 | 4840 | `			return SXERR_ABORT;` |
|       - | 4841 | `		}` |
|      46 | 4842 | `	}` |
|       - | 4843 | `	/* Reject a php-fatal redeclaration before hoisting the class */` |
|    3295 | 4844 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|       9 | 4845 | `		return SXERR_ABORT;` |
|       - | 4846 | `	}` |
|       - | 4847 | `	/* Install the class */` |
|    3289 | 4848 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    3289 | 4849 | `	if( rc == SXRET_OK ){` |
|       - | 4850 | `		ph7_class **apInterface;` |
|       - | 4851 | `		sxu32 n;` |
|    3289 | 4852 | `		if( pBase ){` |
|       - | 4853 | `			/* Inherit from base class and mark as a subclass */` |
|     561 | 4854 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|     278 | 4855 | `		}` |
|    3289 | 4856 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|    3567 | 4857 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 4858 | `			/* Implements one or more interface */` |
|     283 | 4859 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|     283 | 4860 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 4861 | `				break;` |
|       - | 4862 | `			}` |
|     144 | 4863 | `		}` |
|       - | 4864 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|       - | 4865 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|    3289 | 4866 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|      95 | 4867 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|      95 | 4868 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 4869 | `				pIntf = pIntf->pNextName;` |
|     ! 0 | 4870 | `			}` |
|      95 | 4871 | `			if( pIntf ){` |
|      95 | 4872 | `				PH7_ClassImplement(pClass,pIntf);` |
|      45 | 4873 | `			}` |
|      95 | 4874 | `			if( pClass->nEnumBacking != 0 ){` |
|      52 | 4875 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|      52 | 4876 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 4877 | `					pIntf = pIntf->pNextName;` |
|     ! 0 | 4878 | `				}` |
|      52 | 4879 | `				if( pIntf ){` |
|      52 | 4880 | `					PH7_ClassImplement(pClass,pIntf);` |
|      24 | 4881 | `				}` |
|      24 | 4882 | `			}` |
|      45 | 4883 | `		}` |
|       - | 4884 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - | 4885 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|    3284 | 4886 | `		if( rc == SXRET_OK` |
|    3284 | 4887 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|    3289 | 4888 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|     215 | 4889 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - | 4890 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|     215 | 4891 | `			if( pStringable ){` |
|     215 | 4892 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|     215 | 4893 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - | 4894 | `				sxu32 i;` |
|     215 | 4895 | `				int bAlready = 0;` |
|     219 | 4896 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|       8 | 4897 | `					if( apImpl[i] == pStringable ){` |
|       3 | 4898 | `						bAlready = 1;` |
|       3 | 4899 | `						break;` |
|       - | 4900 | `					}` |
|       3 | 4901 | `				}` |
|     215 | 4902 | `				if( !bAlready ){` |
|     213 | 4903 | `					PH7_ClassImplement(pClass,pStringable);` |
|     104 | 4904 | `				}` |
|     105 | 4905 | `			}` |
|     105 | 4906 | `		}` |
|       - | 4907 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|    3289 | 4908 | `		if( rc == SXRET_OK ){` |
|    3289 | 4909 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|    3289 | 4910 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 4911 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 4912 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 4913 | `				return SXERR_ABORT;` |
|       - | 4914 | `			}` |
|    1642 | 4915 | `		}` |
|       - | 4916 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|    3289 | 4917 | `		if( rc == SXRET_OK ){` |
|    3289 | 4918 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|    3289 | 4919 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 4920 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 4921 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 4922 | `				return SXERR_ABORT;` |
|       - | 4923 | `			}` |
|    1642 | 4924 | `		}` |
|    1642 | 4925 | `	}` |
|    3289 | 4926 | `	SySetRelease(&aUseEntries);` |
|    3289 | 4927 | `	SySetRelease(&aInterfaces);` |
|    3289 | 4928 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4929 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4930 | `		return SXERR_ABORT;` |
|       - | 4931 | `	}` |
|    1642 | 4932 | `done:` |
|    3339 | 4933 | `	pGen->pCurClass = pSavedCurClass;` |
|       - | 4934 | `	/* Point beyond the class body */` |
|    3339 | 4935 | `	pGen->pIn = &pEnd[1];` |
|    3339 | 4936 | `	pGen->pEnd = pTmp;` |
|    3339 | 4937 | `	return PH7_OK;` |
|    1684 | 4938 | `}` |
|       - | 4939 | `/* Compile a named class declaration (the common case). */` |
|    3290 | 4940 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 | 4941 | `{` |
|    3295 | 4942 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|       5 | 4943 | `}` |
|       - | 4944 | `/*` |
|       - | 4945 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|       - | 4946 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|       - | 4947 | ` * compile + install the class body once (at compile time, like every other` |
|       - | 4948 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|       - | 4949 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|       - | 4950 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|       - | 4951 | ` */` |
|      74 | 4952 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 4953 | `{` |
|       - | 4954 | `	char zName[128];         /* Synthesized class name */` |
|       - | 4955 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|       - | 4956 | `	SyString sName;` |
|       - | 4957 | `	SyToken *pArgStart,*pArgEnd;` |
|      79 | 4958 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|       - | 4959 | `	                              * is keyed to this 'class' token */` |
|       - | 4960 | `	ph7_value *pObj;` |
|      79 | 4961 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4962 | `	sxu32 nIdx,nLen;` |
|       - | 4963 | `	sxi32 nArg,rc;` |
|      37 | 4964 | `	SXUNUSED(iCompileFlag);` |
|      79 | 4965 | `	if( pGen->pVm->sDeferAnonName.nByte > 0 ){` |
|       - | 4966 | `		/* Deferred re-compile (VmExecDeferredClass): install under the SAME` |
|       - | 4967 | `		 * synthesized name the original site's OP_NEW loads. One-shot. */` |
|       5 | 4968 | `		sName = pGen->pVm->sDeferAnonName;` |
|       5 | 4969 | `		nLen = sName.nByte;` |
|       5 | 4970 | `		pGen->pVm->sDeferAnonName.zString = 0;` |
|       5 | 4971 | `		pGen->pVm->sDeferAnonName.nByte = 0;` |
|       3 | 4972 | `	}else{` |
|       - | 4973 | `		/* Generate a unique anonymous-class name (collision-checked) */` |
|      75 | 4974 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      75 | 4975 | `		while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 | 4976 | `			nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|     ! 0 | 4977 | `		}` |
|      75 | 4978 | `		SyStringInitFromBuf(&sName,zName,nLen);` |
|       - | 4979 | `	}` |
|       - | 4980 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|       - | 4981 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|       - | 4982 | `	 * delimited construct; GenStateCompileClassEx restores both on success.` |
|       - | 4983 | ``	 * Deferral gate: `new class extends \App\Child {}` where the`` |
|       - | 4984 | `	 * parent's autoloader has not RUN yet — capture the class for a runtime` |
|       - | 4985 | `	 * re-compile and keep only the site's argument/OP_NEW emission here. */` |
|      79 | 4986 | `	pArgStart = pArgEnd = 0;` |
|       - | 4987 | `	{` |
|       - | 4988 | `		SySet aMissing;` |
|      79 | 4989 | `		SyToken *pBody = 0;` |
|      79 | 4990 | `		SyToken *pBodyEnd = 0;` |
|       - | 4991 | `		SyBlob sSelfFqn;` |
|      79 | 4992 | `		int bDeferred = 0;` |
|      79 | 4993 | `		SySetInit(&aMissing,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|      79 | 4994 | `		SyBlobInit(&sSelfFqn,&pGen->pVm->sAllocator);` |
|      74 | 4995 | `		if( GenStateScanDeferDeps(pGen,1,PH7_DEFER_KIND_CLASS,&aMissing,&pBody,&pBodyEnd,&sSelfFqn) == SXRET_OK` |
|      79 | 4996 | `		 && SySetUsed(&aMissing) > 0 ){` |
|       8 | 4997 | `			if( &pTokKw[1] < pGen->pEnd && (pTokKw[1].nType & PH7_TK_LPAREN) ){` |
|       3 | 4998 | `				SyToken *pClose = 0;` |
|       3 | 4999 | `				PH7_DelimitNestedTokens(&pTokKw[2],pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|       3 | 5000 | `				if( pClose && pClose < pGen->pEnd ){` |
|       3 | 5001 | `					pArgStart = &pTokKw[2];` |
|       3 | 5002 | `					pArgEnd = pClose;` |
|       1 | 5003 | `				}` |
|       1 | 5004 | `			}` |
|       8 | 5005 | `			rc = GenStateEmitDeferredClass(pGen,0,1,&aMissing,pBodyEnd,0,&sName);` |
|       8 | 5006 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5007 | `				SySetRelease(&aMissing);` |
|     ! 0 | 5008 | `				SyBlobRelease(&sSelfFqn);` |
|     ! 0 | 5009 | `				return SXERR_ABORT;` |
|       - | 5010 | `			}` |
|       8 | 5011 | `			bDeferred = ( rc == SXRET_OK );` |
|       3 | 5012 | `		}` |
|      79 | 5013 | `		SySetRelease(&aMissing);` |
|      79 | 5014 | `		SyBlobRelease(&sSelfFqn);` |
|      79 | 5015 | `		if( !bDeferred ){` |
|      73 | 5016 | `			rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|      73 | 5017 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5018 | `				return rc;` |
|       - | 5019 | `			}` |
|       - | 5020 | `			{` |
|       - | 5021 | ``				/* Expression-position attributes (`new #[A] class {…}`) */`` |
|      73 | 5022 | `				ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,sName.zString,nLen,FALSE,0);` |
|      68 | 5023 | `				if( pAnonClass` |
|      73 | 5024 | `				 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|     ! 0 | 5025 | `					return SXERR_ABORT;` |
|       - | 5026 | `				}` |
|       - | 5027 | `			}` |
|      34 | 5028 | `		}` |
|       - | 5029 | `	}` |
|       - | 5030 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|       - | 5031 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|      79 | 5032 | `	nArg = 0;` |
|      79 | 5033 | `	if( pArgStart < pArgEnd ){` |
|      16 | 5034 | `		SyToken *pSavedIn = pGen->pIn;` |
|      16 | 5035 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|       - | 5036 | `		SyToken *pArgNext;` |
|      16 | 5037 | `		pGen->pIn = pArgStart;` |
|      16 | 5038 | `		pGen->pEnd = pArgEnd;` |
|      30 | 5039 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|      16 | 5040 | `			if( pGen->pIn < pArgNext ){` |
|      16 | 5041 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|      16 | 5042 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5043 | `					pGen->pIn = pSavedIn;` |
|     ! 0 | 5044 | `					pGen->pEnd = pSavedEnd;` |
|     ! 0 | 5045 | `					return SXERR_ABORT;` |
|       - | 5046 | `				}` |
|      16 | 5047 | `				nArg++;` |
|       7 | 5048 | `			}` |
|      16 | 5049 | `			pGen->pIn = &pArgNext[1];` |
|       2 | 5050 | `		}` |
|      16 | 5051 | `		pGen->pIn = pSavedIn;` |
|      16 | 5052 | `		pGen->pEnd = pSavedEnd;` |
|       7 | 5053 | `	}` |
|       - | 5054 | `	/* Load the synthesized class name */` |
|      79 | 5055 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      79 | 5056 | `	if( pObj == 0 ){` |
|     ! 0 | 5057 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 5058 | `		return SXERR_ABORT;` |
|       - | 5059 | `	}` |
|      79 | 5060 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      79 | 5061 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 5062 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|      79 | 5063 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      79 | 5064 | `	return SXRET_OK;` |
|      42 | 5065 | `}` |
|       - | 5066 | `/*` |
|       - | 5067 | ` * Compile a user-defined abstract class.` |
|       - | 5068 | ` *  According to the PHP language reference manual` |
|       - | 5069 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 5070 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 5071 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 5072 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 5073 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 5074 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 5075 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 5076 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 5077 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 5078 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 5079 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 5080 | ` *   could differ.` |
|       - | 5081 | ` */` |
|       - | 5082 | `/*` |
|       - | 5083 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|       - | 5084 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|       - | 5085 | ` * receives the corresponding PH7_CLASS_* bit.` |
|       - | 5086 | ` */` |
| 1582552 | 5087 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|       5 | 5088 | `{` |
| 1582557 | 5089 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  907047 | 5090 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  907047 | 5091 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  906985 | 5092 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  453407 | 5093 | `	}` |
| 1582329 | 5094 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
| 1582261 | 5095 | `	return FALSE;` |
|  791281 | 5096 | `}` |
|       - | 5097 | `/*` |
|       - | 5098 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|       - | 5099 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|       - | 5100 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|       - | 5101 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|       - | 5102 | ` */` |
| 1582256 | 5103 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|       5 | 5104 | `{` |
| 1582261 | 5105 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
| 1582261 | 5106 | `	sxi32 iFlags = 0,iFlag;` |
| 1582557 | 5107 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|     301 | 5108 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|       5 | 5109 | `			pDup = pIn;` |
|       2 | 5110 | `		}` |
|     301 | 5111 | `		iFlags \|= iFlag;` |
|     301 | 5112 | `		pIn++;` |
|       5 | 5113 | `	}` |
| 1582261 | 5114 | `	*ppIn = pIn;` |
| 1582261 | 5115 | `	if( ppDup ){ *ppDup = pDup; }` |
| 1582261 | 5116 | `	return iFlags;` |
|       5 | 5117 | `}` |
|       - | 5118 | `/*` |
|       - | 5119 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|       - | 5120 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|       - | 5121 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|       - | 5122 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|       - | 5123 | `` * `readonly`) to their existing handlers.`` |
|       - | 5124 | ` */` |
| 1582118 | 5125 | `PH7_PRIVATE int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|       5 | 5126 | `{` |
| 1582123 | 5127 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  791204 | 5128 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
| 1582189 | 5129 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|       5 | 5130 | `}` |
|       - | 5131 | `/*` |
|       - | 5132 | ` * Compile a class declaration carrying one or more leading modifiers` |
|       - | 5133 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|       - | 5134 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|       - | 5135 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|       - | 5136 | `` * `abstract`+`final` pair, like PHP.`` |
|       - | 5137 | ` */` |
|     138 | 5138 | `PH7_PRIVATE sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|       5 | 5139 | `{` |
|       - | 5140 | `	SyToken *pDup;` |
|     143 | 5141 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|       - | 5142 | `	sxi32 rc;` |
|     143 | 5143 | `	if( pDup ){` |
|       4 | 5144 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|       2 | 5145 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|       3 | 5146 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5147 | `			return SXERR_ABORT;` |
|       - | 5148 | `		}` |
|       1 | 5149 | `	}` |
|     138 | 5150 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|      74 | 5151 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|       3 | 5152 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5153 | `			"Cannot use the final modifier on an abstract class");` |
|       3 | 5154 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5155 | `			return SXERR_ABORT;` |
|       - | 5156 | `		}` |
|       1 | 5157 | `	}` |
|     143 | 5158 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|      74 | 5159 | `}` |
|       - | 5160 | `/*` |
|       - | 5161 | ` * Compile a user-defined trait.` |
|       - | 5162 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - | 5163 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - | 5164 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - | 5165 | ` */` |
|     184 | 5166 | `PH7_PRIVATE sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 | 5167 | `{` |
|     189 | 5168 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5169 | `	ph7_class *pClass;` |
|     189 | 5170 | `	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' */` |
|       - | 5171 | `	SyToken *pEnd,*pTmp;` |
|       - | 5172 | `	sxi32 iProtection;` |
|       - | 5173 | `	sxi32 iAttrflags;` |
|       - | 5174 | ``	SySet aUseEntries; /* trait-body `use` statements (incl. adaptation blocks) */`` |
|       - | 5175 | `	SyString *pName;` |
|       - | 5176 | `	sxi32 nKwrd;` |
|       - | 5177 | `	sxi32 rc;` |
|       - | 5178 | `	{` |
|       - | 5179 | `		/* Deferral gate: a used trait may need an autoloader that` |
|       - | 5180 | `		 * has not run yet. */` |
|       - | 5181 | `		sxi32 rcDefer;` |
|     189 | 5182 | `		if( GenStateMaybeDeferClass(pGen,0,PH7_DEFER_KIND_TRAIT,&rcDefer) ){` |
|       5 | 5183 | `			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 5184 | `		}` |
|       - | 5185 | `	}` |
|     185 | 5186 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - | 5187 | `	/* Jump the 'trait' keyword */` |
|     185 | 5188 | `	pGen->pIn++;` |
|     185 | 5189 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 5190 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 | 5191 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5192 | `			return SXERR_ABORT;` |
|       - | 5193 | `		}` |
|     ! 0 | 5194 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 5195 | `			pGen->pIn++;` |
|     ! 0 | 5196 | `		}` |
|     ! 0 | 5197 | `		return SXRET_OK;` |
|       - | 5198 | `	}` |
|       - | 5199 | `	/* Extract trait name */` |
|     185 | 5200 | `	pName = &pGen->pIn->sData;` |
|     185 | 5201 | `	pGen->pIn++;` |
|       - | 5202 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5203 | `		SyBlob sFQN;` |
|       - | 5204 | `		SyString sFQNStr;` |
|     185 | 5205 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     185 | 5206 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     185 | 5207 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       - | 5208 | ``		/* php refuses a declaration whose short name a local `use` already took. */`` |
|     185 | 5209 | `		if( GenStateGuardImportRedeclare(pGen,0,pName,&sFQNStr,nLine) == SXERR_ABORT ){` |
|     ! 0 | 5210 | `			SyBlobRelease(&sFQN);` |
|     ! 0 | 5211 | `			return SXERR_ABORT;` |
|       - | 5212 | `		}` |
|     185 | 5213 | `		GenStateRecordDeclaredName(pGen,0,&sFQNStr);` |
|     185 | 5214 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     185 | 5215 | `		SyBlobRelease(&sFQN);` |
|       - | 5216 | `	}` |
|     185 | 5217 | `	if( pClass == 0 ){` |
|     ! 0 | 5218 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5219 | `		return SXERR_ABORT;` |
|       - | 5220 | `	}` |
|     185 | 5221 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     185 | 5222 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|     ! 0 | 5223 | `		return SXERR_ABORT;` |
|       - | 5224 | `	}` |
|       - | 5225 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|     185 | 5226 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 5227 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 | 5228 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5229 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5230 | `			return SXERR_ABORT;` |
|       - | 5231 | `		}` |
|     ! 0 | 5232 | `		return SXRET_OK;` |
|       - | 5233 | `	}` |
|     185 | 5234 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     185 | 5235 | `	pEnd = 0;` |
|     185 | 5236 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|     185 | 5237 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 5238 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 5239 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5240 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5241 | `			return SXERR_ABORT;` |
|       - | 5242 | `		}` |
|     ! 0 | 5243 | `		return SXRET_OK;` |
|       - | 5244 | `	}` |
|       - | 5245 | `	/* The delimiter token is the trait body's closing brace */` |
|     185 | 5246 | `	pClass->nEndLine = pEnd->nLine;` |
|       - | 5247 | `	/* Swap token stream */` |
|     185 | 5248 | `	pTmp = pGen->pEnd;` |
|     185 | 5249 | `	pGen->pEnd = pEnd;` |
|       - | 5250 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|     185 | 5251 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|       - | 5252 | `	/* This trait is now the lexical class for its body, so a property/parameter` |
|       - | 5253 | `	 * default here resolves __TRAIT__ to it (see pCurClass). */` |
|     185 | 5254 | `	pGen->pCurClass = pClass;` |
|       - | 5255 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|     205 | 5256 | `	for(;;){` |
|     501 | 5257 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      49 | 5258 | `			pGen->pIn++;` |
|       5 | 5259 | `		}` |
|     457 | 5260 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     185 | 5261 | `			break;` |
|       - | 5262 | `		}` |
|       - | 5263 | `		/* Bind a directly-preceding docblock to this member */` |
|     277 | 5264 | `		GenStateSetPendingDoc(&(*pGen));` |
|     277 | 5265 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5266 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5267 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 5268 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5269 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5270 | `				return SXERR_ABORT;` |
|       - | 5271 | `			}` |
|     ! 0 | 5272 | `			goto done;` |
|       - | 5273 | `		}` |
|     277 | 5274 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|     277 | 5275 | `		iAttrflags = 0;` |
|     277 | 5276 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|     277 | 5277 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     277 | 5278 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 5279 | `				/* Trait uses another trait: use T[, T2] [{ resolution }]; A trait` |
|       - | 5280 | `				 * name is a full class reference — qualified or fully-qualified` |
|       - | 5281 | ``				 * (`use Foo\T;`, `use \Foo\T;`) — so parse it with the shared`` |
|       - | 5282 | `				 * class-reference reader like the CLASS body's trait-use does` |
|       - | 5283 | `				 * (the old single-identifier read choked on the leading '\'),` |
|       - | 5284 | `				 * and collect a TraitUseEntry so an adaptation block` |
|       - | 5285 | `				 * (insteadof/as) applies through the same shared machinery. */` |
|       - | 5286 | `				TraitUseEntry sUse;` |
|      19 | 5287 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      19 | 5288 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      19 | 5289 | `				pGen->pIn++; /* Jump 'use' */` |
|      11 | 5290 | `				for(;;){` |
|       - | 5291 | `					ph7_class *pUsedTrait;` |
|       - | 5292 | `					SyBlob sResolved;` |
|       - | 5293 | `					SyString sUsedName;` |
|      23 | 5294 | `					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|      23 | 5295 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      23 | 5296 | `					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 5297 | `						SyBlobRelease(&sResolved);` |
|     ! 0 | 5298 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|     ! 0 | 5299 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 5300 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5301 | `							return SXERR_ABORT;` |
|       - | 5302 | `						}` |
|     ! 0 | 5303 | `						break;` |
|       - | 5304 | `					}` |
|      41 | 5305 | `					pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|      18 | 5306 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      23 | 5307 | `					SyStringInitFromBuf(&sUsedName,` |
|       - | 5308 | `						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|      23 | 5309 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 5310 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 5311 | `					}` |
|      23 | 5312 | `					if( pUsedTrait == 0 ){` |
|     ! 0 | 5313 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|       - | 5314 | `							"'%z' is not a trait",&sUsedName);` |
|     ! 0 | 5315 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5316 | `							SyBlobRelease(&sResolved);` |
|     ! 0 | 5317 | `							return SXERR_ABORT;` |
|       - | 5318 | `						}` |
|     ! 0 | 5319 | `					}else{` |
|      23 | 5320 | `						SySetPut(&sUse.aTraits,(const void *)&pUsedTrait);` |
|       - | 5321 | `					}` |
|      23 | 5322 | `					SyBlobRelease(&sResolved);` |
|      23 | 5323 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      12 | 5324 | `						break;` |
|       - | 5325 | `					}` |
|       6 | 5326 | `					pGen->pIn++;` |
|       2 | 5327 | `				}` |
|       - | 5328 | `				/* Optional adaptation block (conflict resolution) */` |
|      19 | 5329 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 5330 | `					SyToken *pBlock;` |
|       5 | 5331 | `					pGen->pIn++; /* Jump '{' */` |
|       5 | 5332 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       5 | 5333 | `					sUse.pResolvStart = pGen->pIn;` |
|       5 | 5334 | `					sUse.pResolvEnd = pBlock;` |
|       5 | 5335 | `					if( pBlock < pGen->pEnd ){` |
|       5 | 5336 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       3 | 5337 | `					}else{` |
|     ! 0 | 5338 | `						pGen->pIn = pGen->pEnd;` |
|       - | 5339 | `					}` |
|       2 | 5340 | `				}` |
|      19 | 5341 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|      19 | 5342 | `				continue;` |
|       - | 5343 | `			}` |
|     263 | 5344 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     241 | 5345 | `				iProtection = nKwrd;` |
|     241 | 5346 | `				pGen->pIn++;` |
|       - | 5347 | ``				/* Optional `readonly` after the visibility (PHP 8.1): `private readonly T`` |
|       - | 5348 | ``				 * $x` — the generated PHPUnit runtime traits (StubApi, …) declare their`` |
|       - | 5349 | `				 * readonly state this way. Mirrors the class-body attribute parser. */` |
|     241 | 5350 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       5 | 5351 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       5 | 5352 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       2 | 5353 | `				}` |
|     236 | 5354 | `				if( pGen->pIn >= pGen->pEnd` |
|     241 | 5355 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 | 5356 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5357 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 5358 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 5359 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5360 | `						return SXERR_ABORT;` |
|       - | 5361 | `					}` |
|     ! 0 | 5362 | `					goto done;` |
|       - | 5363 | `				}` |
|     241 | 5364 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      23 | 5365 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      23 | 5366 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 5367 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5368 | `							return SXERR_ABORT;` |
|       - | 5369 | `						}` |
|     ! 0 | 5370 | `						goto done;` |
|       - | 5371 | `					}` |
|      23 | 5372 | `					continue;` |
|       - | 5373 | `				}` |
|     223 | 5374 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       9 | 5375 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       9 | 5376 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 5377 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5378 | `							return SXERR_ABORT;` |
|       - | 5379 | `						}` |
|     ! 0 | 5380 | `						goto done;` |
|       - | 5381 | `					}` |
|       9 | 5382 | `					continue;` |
|       - | 5383 | `				}` |
|     215 | 5384 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     105 | 5385 | `			}` |
|     237 | 5386 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 5387 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5388 | `					"Traits cannot have constants");` |
|     ! 0 | 5389 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5390 | `					return SXERR_ABORT;` |
|       - | 5391 | `				}` |
|     ! 0 | 5392 | `				goto done;` |
|     ! 0 | 5393 | `			}else{` |
|     237 | 5394 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|      19 | 5395 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      19 | 5396 | `					pGen->pIn++;` |
|      19 | 5397 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      17 | 5398 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      17 | 5399 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 5400 | `							iProtection = nKwrd;` |
|     ! 0 | 5401 | `							pGen->pIn++;` |
|     ! 0 | 5402 | `						}` |
|       7 | 5403 | `					}` |
|      16 | 5404 | `					if( pGen->pIn >= pGen->pEnd` |
|      19 | 5405 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 | 5406 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5407 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 5408 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5409 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5410 | `							return SXERR_ABORT;` |
|       - | 5411 | `						}` |
|     ! 0 | 5412 | `						goto done;` |
|       - | 5413 | `					}` |
|      19 | 5414 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 5415 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 5416 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 5417 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 5418 | `								return SXERR_ABORT;` |
|       - | 5419 | `							}` |
|     ! 0 | 5420 | `							goto done;` |
|       - | 5421 | `						}` |
|       3 | 5422 | `						continue;` |
|       - | 5423 | `					}` |
|      17 | 5424 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 | 5425 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 5426 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 5427 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 5428 | `								return SXERR_ABORT;` |
|       - | 5429 | `							}` |
|     ! 0 | 5430 | `							goto done;` |
|       - | 5431 | `						}` |
|     ! 0 | 5432 | `						continue;` |
|       - | 5433 | `					}` |
|      17 | 5434 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     228 | 5435 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       9 | 5436 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       9 | 5437 | `					pGen->pIn++;` |
|       9 | 5438 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       9 | 5439 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       9 | 5440 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       9 | 5441 | `							iProtection = nKwrd;` |
|       9 | 5442 | `							pGen->pIn++;` |
|       3 | 5443 | `						}` |
|       3 | 5444 | `					}` |
|       9 | 5445 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       6 | 5446 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5447 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5448 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 5449 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5450 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5451 | `							return SXERR_ABORT;` |
|       - | 5452 | `						}` |
|     ! 0 | 5453 | `						goto done;` |
|       - | 5454 | `					}` |
|       9 | 5455 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       3 | 5456 | `				}` |
|     235 | 5457 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 5458 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5459 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 5460 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 5461 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5462 | `						return SXERR_ABORT;` |
|       - | 5463 | `					}` |
|     ! 0 | 5464 | `					goto done;` |
|       - | 5465 | `				}` |
|     235 | 5466 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 5467 | `					pGen->pIn++;` |
|     ! 0 | 5468 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 5469 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5470 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 5471 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5472 | `							return SXERR_ABORT;` |
|       - | 5473 | `						}` |
|     ! 0 | 5474 | `						goto done;` |
|       - | 5475 | `					}` |
|     ! 0 | 5476 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 5477 | `				}else{` |
|     235 | 5478 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 5479 | `				}` |
|     235 | 5480 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 5481 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5482 | `						return SXERR_ABORT;` |
|       - | 5483 | `					}` |
|     ! 0 | 5484 | `					goto done;` |
|       - | 5485 | `				}` |
|       - | 5486 | `			}` |
|     120 | 5487 | `		}else{` |
|     ! 0 | 5488 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 5489 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5490 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5491 | `					return SXERR_ABORT;` |
|       - | 5492 | `				}` |
|     ! 0 | 5493 | `				goto done;` |
|       - | 5494 | `			}` |
|       - | 5495 | `		}` |
|       5 | 5496 | `	}` |
|       - | 5497 | ``	/* Apply the collected `use` entries (incl. adaptation blocks) through the`` |
|       - | 5498 | `	 * machinery shared with the class-body compiler. */` |
|     185 | 5499 | `	rc = GenStateApplyTraitUses(&(*pGen),pClass,&aUseEntries);` |
|     185 | 5500 | `	SySetRelease(&aUseEntries);` |
|     185 | 5501 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5502 | `		return SXERR_ABORT;` |
|       - | 5503 | `	}` |
|       - | 5504 | `	/* Reject a php-fatal redeclaration before hoisting the trait */` |
|     185 | 5505 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|       3 | 5506 | `		return SXERR_ABORT;` |
|       - | 5507 | `	}` |
|       - | 5508 | `	/* Install the trait */` |
|     183 | 5509 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     183 | 5510 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5511 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5512 | `		return SXERR_ABORT;` |
|       - | 5513 | `	}` |
|      89 | 5514 | `done:` |
|     183 | 5515 | `	pGen->pCurClass = pSavedCurClass;` |
|       - | 5516 | `	/* Point beyond the trait body */` |
|     183 | 5517 | `	pGen->pIn = &pEnd[1];` |
|     183 | 5518 | `	pGen->pEnd = pTmp;` |
|     183 | 5519 | `	return PH7_OK;` |
|      97 | 5520 | `}` |
|       - | 5521 | `/*` |
|       - | 5522 | ` * Compile a user-defined class.` |
|       - | 5523 | ` *  According to the PHP language reference manual` |
|       - | 5524 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 5525 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 5526 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 5527 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 5528 | ` *   and functions (called "methods").` |
|       - | 5529 | ` */` |
|    3054 | 5530 | `PH7_PRIVATE sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 | 5531 | `{` |
|       - | 5532 | `	sxi32 rc;` |
|    3059 | 5533 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|    3059 | 5534 | `	return rc;` |
|       5 | 5535 | `}` |
|       - | 5536 | `/*` |
|       - | 5537 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|       - | 5538 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|       - | 5539 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|       - | 5540 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|       - | 5541 | `` * meaning; `enum Name` can never start a valid expression.`` |
|       - | 5542 | ` */` |
| 1581980 | 5543 | `PH7_PRIVATE int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|       5 | 5544 | `{` |
| 1612524 | 5545 | `	return (pIn->nType & PH7_TK_ID)` |
|  821529 | 5546 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|   35601 | 5547 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
| 1612519 | 5548 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|       5 | 5549 | `}` |
|       - | 5550 | `/*` |
|       - | 5551 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|       - | 5552 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|       - | 5553 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|       - | 5554 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|       - | 5555 | ` */` |
|      98 | 5556 | `PH7_PRIVATE sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|       5 | 5557 | `{` |
|     103 | 5558 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|       5 | 5559 | `}` |
|       - | 5560 |  |
