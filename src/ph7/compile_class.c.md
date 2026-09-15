# src/ph7/compile_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2018/2696 lines (74.85%)

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
|   529292 |   27 | `static sxi32 GenStateGuardClassRedeclaration(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |   28 | `{` |
|        - |   29 | `	SyHashEntry *pEntry;` |
|   529297 |   30 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|       26 |   31 | `		return SXRET_OK; /* conditional/nested: keep hoisting */` |
|        - |   32 | `	}` |
|   529275 |   33 | `	pClass->iFlags \|= PH7_CLASS_BOUND;` |
|   529275 |   34 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|   527141 |   35 | `		return SXRET_OK; /* the prelude installs each builtin exactly once */` |
|        - |   36 | `	}` |
|     2139 |   37 | `	pEntry = SyHashGet(&pGen->pVm->hClass,(const void *)pClass->sName.zString,pClass->sName.nByte);` |
|     2139 |   38 | `	if( pEntry ){` |
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
|     2129 |   58 | `	return SXRET_OK;` |
|   264651 |   59 | `}` |
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
|  3824918 |   71 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|        5 |   72 | `{` |
|  3824923 |   73 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|   279303 |   74 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  3545625 |   75 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   221039 |   76 | `		return PH7_CLASS_PROT_PROTECTED;` |
|        - |   77 | `	}` |
|        - |   78 | `	/* Assume public by default */` |
|  3324591 |   79 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  1912464 |   80 | `}` |
|        - |   81 | `/*` |
|        - |   82 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|        - |   83 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|        - |   84 | ` * token immediately followed by '='. Anything else with a leading type token` |
|        - |   85 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|        - |   86 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|        - |   87 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|        - |   88 | ` */` |
|   341282 |   89 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|        5 |   90 | `{` |
|        - |   91 | `	SyToken *p0, *p1;` |
|   341287 |   92 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |   93 | `		return 0;` |
|        - |   94 | `	}` |
|   341287 |   95 | `	p0 = pGen->pIn;` |
|        - |   96 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|   341287 |   97 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|      ! 0 |   98 | `		return 1;` |
|        - |   99 | `	}` |
|   341287 |  100 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|        5 |  101 | `		return 1;` |
|        - |  102 | `	}` |
|        - |  103 | `	/* A name-like first token begins a type only when followed by another` |
|        - |  104 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|        - |  105 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|   341283 |  106 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|   341283 |  107 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|   341283 |  108 | `		if( p1 ){` |
|   341283 |  109 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|       34 |  110 | `				return 1;` |
|        - |  111 | `			}` |
|   341253 |  112 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|        5 |  113 | `				return 1;` |
|        - |  114 | `			}` |
|   170622 |  115 | `		}` |
|   170622 |  116 | `	}` |
|   341249 |  117 | `	return 0;` |
|   170646 |  118 | `}` |
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
|   721900 |  173 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|        5 |  174 | `{` |
|   721905 |  175 | `	SyToken *p = pGen->pIn;` |
|   721905 |  176 | `	int iDepth = 0;` |
|  1906619 |  177 | `	while( p < pGen->pEnd ){` |
|  1906619 |  178 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   721853 |  179 | `			break; /* end of this initializer */` |
|        - |  180 | `		}` |
|  1184766 |  181 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   600157 |  182 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|    15538 |  183 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  184 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|        - |  185 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|        - |  186 | `			 * expression. */` |
|        3 |  187 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|        3 |  188 | `			p++;` |
|        3 |  189 | `			if( bArrow ){` |
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
|      ! 0 |  211 | `				int iLocal = 0;` |
|      ! 0 |  212 | `				while( p < pGen->pEnd ){` |
|      ! 0 |  213 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|      ! 0 |  214 | `						break; /* body brace */` |
|        - |  215 | `					}` |
|      ! 0 |  216 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      ! 0 |  217 | `						iLocal++;` |
|      ! 0 |  218 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      ! 0 |  219 | `						if( iLocal > 0 ){` |
|      ! 0 |  220 | `							iLocal--;` |
|      ! 0 |  221 | `						}` |
|      ! 0 |  222 | `					}` |
|      ! 0 |  223 | `					p++;` |
|      ! 0 |  224 | `				}` |
|      ! 0 |  225 | `				if( p < pGen->pEnd ){` |
|      ! 0 |  226 | `					int iBrace = 0; /* p is on the body '{' */` |
|      ! 0 |  227 | `					while( p < pGen->pEnd ){` |
|      ! 0 |  228 | `						if( p->nType & PH7_TK_OCB ){` |
|      ! 0 |  229 | `							iBrace++;` |
|      ! 0 |  230 | `						}else if( p->nType & PH7_TK_CCB ){` |
|      ! 0 |  231 | `							iBrace--;` |
|      ! 0 |  232 | `							if( iBrace == 0 ){` |
|      ! 0 |  233 | `								p++;` |
|      ! 0 |  234 | `								break;` |
|        - |  235 | `							}` |
|      ! 0 |  236 | `						}` |
|      ! 0 |  237 | `						p++;` |
|      ! 0 |  238 | `					}` |
|      ! 0 |  239 | `				}` |
|        - |  240 | `			}` |
|        3 |  241 | `			continue;` |
|        - |  242 | `		}` |
|  1184769 |  243 | `		if( p->nType & PH7_TK_OCB ){` |
|       45 |  244 | `			if( iDepth == 0 ){` |
|        - |  245 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|        - |  246 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|        - |  247 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|        - |  248 | `				 * is legal — don't scan into it. */` |
|       45 |  249 | `				break;` |
|        - |  250 | `			}` |
|      ! 0 |  251 | `			iDepth++;` |
|  1184725 |  252 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|    50485 |  253 | `			iDepth++;` |
|  1159485 |  254 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|    50483 |  255 | `			if( iDepth > 0 ){` |
|    50483 |  256 | `				iDepth--;` |
|    25239 |  257 | `			}` |
|  1109006 |  258 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|   390601 |  259 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|        - |  260 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|        - |  261 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|        - |  262 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|       11 |  263 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|       11 |  264 | `				return 1;` |
|        - |  265 | `			}` |
|      ! 0 |  266 | `		}` |
|  1184717 |  267 | `		p++;` |
|        5 |  268 | `	}` |
|   721897 |  269 | `	return 0;` |
|   360955 |  270 | `}` |
|        - |  271 | `/*` |
|        - |  272 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|        - |  273 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|        - |  274 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|        - |  275 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|        - |  276 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|        - |  277 | ` * share the same backing.` |
|        - |  278 | ` */` |
|    15898 |  279 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|        - |  280 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|        5 |  281 | `{` |
|    15903 |  282 | `	pAttr->nType = nType;` |
|    15903 |  283 | `	pAttr->sClass = *pClass;` |
|    15903 |  284 | `	pAttr->sTypeName = *pTypeName;` |
|    15903 |  285 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  286 | `		sxu32 i;` |
|       73 |  287 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|       51 |  288 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|       51 |  289 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|       28 |  290 | `		}` |
|       11 |  291 | `	}` |
|    15903 |  292 | `}` |
|   341282 |  293 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  294 | `{` |
|   341287 |  295 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  296 | `	SySet *pInstrContainer;` |
|        - |  297 | `	ph7_class_attr *pCons;` |
|        - |  298 | `	SyString *pName;` |
|        - |  299 | `	sxi32 rc;` |
|   341287 |  300 | `	sxu32 nType = 0;` |
|        - |  301 | `	SyString sTypeClass;` |
|        - |  302 | `	SyString sTypeText;` |
|        - |  303 | `	SySet aUnionAlts;` |
|   341287 |  304 | `	sxi32 iTypeFlags = 0;` |
|   341287 |  305 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   341287 |  306 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   341287 |  307 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  308 | `	/* Extract visibility level */` |
|   341287 |  309 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  310 | `	/* Mark as constant */` |
|   341287 |  311 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|   341287 |  312 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        - |  313 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|        - |  314 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|   341306 |  315 | `	if( GenStateClassConstHasType(pGen) ){` |
|       61 |  316 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|       38 |  317 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|        - |  318 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|        - |  319 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|        - |  320 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|        - |  321 | `		 * and success paths release. */` |
|       42 |  322 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  323 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  324 | `			goto Synchronize;` |
|       42 |  325 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  326 | `			return SXERR_ABORT;` |
|       42 |  327 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 |  328 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  329 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|      ! 0 |  330 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  331 | `				return SXERR_ABORT;` |
|        - |  332 | `			}` |
|      ! 0 |  333 | `			goto Synchronize;` |
|        - |  334 | `		}` |
|       42 |  335 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       19 |  336 | `	}` |
|   170641 |  337 | `loop:` |
|   341289 |  338 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - |  339 | `		/* Invalid constant name */` |
|      ! 0 |  340 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|      ! 0 |  341 | `		if( rc == SXERR_ABORT ){` |
|        - |  342 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  343 | `			return SXERR_ABORT;` |
|        - |  344 | `		}` |
|      ! 0 |  345 | `		goto Synchronize;` |
|        - |  346 | `	}` |
|        - |  347 | `	/* Peek constant name */` |
|   341289 |  348 | `	pName = &pGen->pIn->sData;` |
|        - |  349 | `	/* Make sure the constant name isn't reserved */` |
|   341289 |  350 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  351 | `		/* Reserved constant name */` |
|      ! 0 |  352 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|      ! 0 |  353 | `		if( rc == SXERR_ABORT ){` |
|        - |  354 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  355 | `			return SXERR_ABORT;` |
|        - |  356 | `		}` |
|      ! 0 |  357 | `		goto Synchronize;` |
|        - |  358 | `	}` |
|        - |  359 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|   341289 |  360 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       61 |  361 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|       38 |  362 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       19 |  363 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|       42 |  364 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  365 | `			return SXERR_ABORT;` |
|       42 |  366 | `		}else if( rc != SXRET_OK ){` |
|        3 |  367 | `			goto Synchronize;` |
|        - |  368 | `		}` |
|       18 |  369 | `	}` |
|        - |  370 | `	/* Advance the stream cursor */` |
|   341287 |  371 | `	pGen->pIn++;` |
|   341287 |  372 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  373 | `		/* Invalid declaration */` |
|      ! 0 |  374 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|      ! 0 |  375 | `		if( rc == SXERR_ABORT ){` |
|        - |  376 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  377 | `			return SXERR_ABORT;` |
|        - |  378 | `		}` |
|      ! 0 |  379 | `		goto Synchronize;` |
|        - |  380 | `	}` |
|   341287 |  381 | `	pGen->pIn++; /* Jump the equal sign */` |
|        - |  382 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|        - |  383 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|        - |  384 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|        - |  385 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|   341282 |  386 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|       39 |  387 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|        8 |  388 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  389 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|        2 |  390 | `			&pClass->sName,pName,&sTypeText);` |
|        6 |  391 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  392 | `			return SXERR_ABORT;` |
|        - |  393 | `		}` |
|        6 |  394 | `		goto Synchronize;` |
|        - |  395 | `	}` |
|        - |  396 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|        - |  397 | `	 * constant initializer ("New expressions are not supported in this context").` |
|        - |  398 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|   341283 |  399 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|        5 |  400 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  401 | `			"New expressions are not supported in this context");` |
|        5 |  402 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  403 | `			return SXERR_ABORT;` |
|        - |  404 | `		}` |
|        5 |  405 | `		goto Synchronize;` |
|        - |  406 | `	}` |
|        - |  407 | `	/* Allocate a new class attribute */` |
|   341279 |  408 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   341279 |  409 | `	if( pCons ){` |
|   341279 |  410 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|   341279 |  411 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  412 | `			return SXERR_ABORT;` |
|        - |  413 | `		}` |
|   170637 |  414 | `	}` |
|   341279 |  415 | `	if( pCons == 0 ){` |
|      ! 0 |  416 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  417 | `		return SXERR_ABORT;` |
|        - |  418 | `	}` |
|   341279 |  419 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       35 |  420 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       16 |  421 | `	}` |
|        - |  422 | `	/* Swap bytecode container */` |
|   341279 |  423 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   341279 |  424 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|        - |  425 | `	/* Compile constant value.` |
|        - |  426 | `	 */` |
|   341279 |  427 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   341279 |  428 | `	if( rc == SXERR_EMPTY ){` |
|        3 |  429 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|        3 |  430 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  431 | `			return SXERR_ABORT;` |
|        - |  432 | `		}` |
|        1 |  433 | `	}` |
|        - |  434 | `	/* Emit the done instruction */` |
|   341279 |  435 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   341279 |  436 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   341279 |  437 | `	if( rc == SXERR_ABORT ){` |
|        - |  438 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  439 | `		return SXERR_ABORT;` |
|        - |  440 | `	}` |
|        - |  441 | `	/* All done,install the constant */` |
|   341279 |  442 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|   341279 |  443 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  444 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  445 | `		return SXERR_ABORT;` |
|        - |  446 | `	}` |
|   341279 |  447 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  448 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|        3 |  449 | `		pGen->pIn++; /* Jump the comma */` |
|        3 |  450 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 |  451 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 |  452 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 |  453 | `				pTok--;` |
|      ! 0 |  454 | `			}` |
|      ! 0 |  455 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  456 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|      ! 0 |  457 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 |  458 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  459 | `				return SXERR_ABORT;` |
|        - |  460 | `			}` |
|      ! 0 |  461 | `		}else{` |
|        3 |  462 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|        3 |  463 | `				goto loop;` |
|        - |  464 | `			}` |
|        - |  465 | `		}` |
|      ! 0 |  466 | `	}` |
|   341277 |  467 | `	SySetRelease(&aUnionAlts);` |
|   341277 |  468 | `	return SXRET_OK;` |
|        5 |  469 | `Synchronize:` |
|       13 |  470 | `	SySetRelease(&aUnionAlts);` |
|        - |  471 | `	/* Synchronize with the first semi-colon */` |
|       45 |  472 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       35 |  473 | `		pGen->pIn++;` |
|        3 |  474 | `	}` |
|       13 |  475 | `	return SXERR_CORRUPT;` |
|   170646 |  476 | `}` |
|        - |  477 | `/*` |
|        - |  478 | ` * complie a class attribute or Properties in the PHP jargon.` |
|        - |  479 | ` * According to the PHP language reference manual` |
|        - |  480 | ` *  Properties` |
|        - |  481 | ` *  Class member variables are called "properties". You may also see them referred` |
|        - |  482 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|        - |  483 | ` *  of this reference we will use "properties". They are defined by using one` |
|        - |  484 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|        - |  485 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|        - |  486 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|        - |  487 | ` *  and must not depend on run-time information in order to be evaluated.` |
|        - |  488 | ` * Symisc eXtension.` |
|        - |  489 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|        - |  490 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  491 | ` *  Example:` |
|        - |  492 | ` *   class Test{` |
|        - |  493 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - |  494 | ` *   };` |
|        - |  495 | ` *   var_dump(TEST::myVar);` |
|        - |  496 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - |  497 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - |  498 | ` */` |
|        - |  499 | `/*` |
|        - |  500 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|        - |  501 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|        - |  502 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|        - |  503 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|        - |  504 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|        - |  505 | ` */` |
|  2618284 |  506 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|        5 |  507 | `{` |
|  2618289 |  508 | `	SyToken *p = pStart;` |
|  2618289 |  509 | `	int bFirst = 1;` |
|  2618289 |  510 | `	if( p >= pEnd ) return 0;` |
|        - |  511 | ``	/* Optional nullable `?` shorthand. */`` |
|  2618289 |  512 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|       41 |  513 | `		p++;` |
|       41 |  514 | `		if( p >= pEnd ) return 0;` |
|       19 |  515 | `	}` |
|        - |  516 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|        - |  517 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|        - |  518 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|        - |  519 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|  1309142 |  520 | `	for(;;){` |
|  2618309 |  521 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|        - |  522 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|        3 |  523 | `			p++;` |
|        9 |  524 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|        3 |  525 | `			if( p >= pEnd ) return 0;` |
|        3 |  526 | `			p++; /* skip ')' */` |
|        2 |  527 | `		}else{` |
|        - |  528 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|        - |  529 | ``			 * then any `&`-joined intersection members. */`` |
|  2618307 |  530 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  2618307 |  531 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  532 | `				return 0;` |
|        - |  533 | `			}` |
|        - |  534 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|        - |  535 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|        - |  536 | `			 * may still appear at the initial dispatch site). */` |
|  2618307 |  537 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  2618251 |  538 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  2618246 |  539 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|   124534 |  540 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  2602429 |  541 | `					return 0;` |
|        - |  542 | `				}` |
|     7911 |  543 | `			}` |
|    15883 |  544 | `			p++;` |
|    15885 |  545 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  546 | `				p += 2;` |
|        1 |  547 | `			}` |
|    23820 |  548 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|    15886 |  549 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  550 | `				p++; /* skip '&' */` |
|        3 |  551 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|        3 |  552 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|        3 |  553 | `				p++;` |
|        3 |  554 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      ! 0 |  555 | `					p += 2;` |
|      ! 0 |  556 | `				}` |
|        1 |  557 | `			}` |
|        - |  558 | `		}` |
|    15885 |  559 | `		bFirst = 0;` |
|    15880 |  560 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       25 |  561 | `			&& p->sData.zString[0] == '\|' ){` |
|       25 |  562 | ``			p++; /* next `\|`-separated part */`` |
|       25 |  563 | `			continue;` |
|        - |  564 | `		}` |
|    15865 |  565 | `		break;` |
|      ! 0 |  566 | `	}` |
|    15865 |  567 | `	if( p >= pEnd ) return 0;` |
|    15865 |  568 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|  1309147 |  569 | `}` |
|        - |  570 |  |
|        - |  571 | `/*` |
|        - |  572 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|        - |  573 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|        - |  574 | ` * if not). Recognized forms:` |
|        - |  575 | ` *   ?Type, array, bool, int, float, string, object,` |
|        - |  576 | ` *   self, parent, \Ns\ClassName, ClassName` |
|        - |  577 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|        - |  578 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|        - |  579 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|        - |  580 | ` * on unrecoverable error.` |
|        - |  581 | ` *` |
|        - |  582 | ` * When a type is parsed:` |
|        - |  583 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|        - |  584 | ` *   *pClass is set to the class name (for class types)` |
|        - |  585 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|        - |  586 | ` *   *pTypeText is set to the original text span of the type` |
|        - |  587 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|        - |  588 | ` */` |
|    15870 |  589 | `static sxi32 GenStateParsePropertyType(` |
|        - |  590 | `	ph7_gen_state *pGen,` |
|        - |  591 | `	sxu32 *pnType,` |
|        - |  592 | `	SyString *pClass,` |
|        - |  593 | `	sxi32 *piTypeFlags,` |
|        - |  594 | `	SyString *pTypeText,` |
|        - |  595 | `	SySet *pAlts` |
|        5 |  596 | `){` |
|    15875 |  597 | `	sxi32 iFlags = 0;` |
|        - |  598 | `	sxi32 rc;` |
|    15875 |  599 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  600 | `		return SXRET_OK;` |
|        - |  601 | `	}` |
|        - |  602 | `	/* If the first token is '$', there's no type */` |
|    15875 |  603 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      ! 0 |  604 | `		return SXRET_OK;` |
|        - |  605 | `	}` |
|    15875 |  606 | `	rc = GenStateParseUnionTypeDecl(` |
|     7935 |  607 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|        - |  608 | `		PH7_CLASS_ATTR_NULLABLE,` |
|        - |  609 | `		PH7_CLASS_ATTR_UNION,` |
|        - |  610 | `		/* bAllowVoid */ 0,` |
|    15870 |  611 | `		pGen->pIn->nLine);` |
|    15875 |  612 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  613 | `		return rc;` |
|        - |  614 | `	}` |
|        - |  615 | `	/* Verify next token is '$' (start of property name) */` |
|    15875 |  616 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  617 | `		return SXERR_SYNTAX;` |
|        - |  618 | `	}` |
|    15875 |  619 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|    15875 |  620 | `	return SXRET_OK;` |
|     7940 |  621 | `}` |
|        - |  622 |  |
|        - |  623 | `/*` |
|        - |  624 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|        - |  625 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|        - |  626 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|        - |  627 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|        - |  628 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|        - |  629 | ` * by the type parser itself before reaching here.` |
|        - |  630 | ` *` |
|        - |  631 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|        - |  632 | ` * use in the error message.` |
|        - |  633 | ` */` |
|    16048 |  634 | `static int GenStateIsDisallowedPropertyAtom(` |
|        - |  635 | `	sxu32 nType,` |
|        - |  636 | `	const SyString *pClass,` |
|        - |  637 | `	const char **pzName,` |
|        - |  638 | `	sxu32 *pnName)` |
|        5 |  639 | `{` |
|        - |  640 | `	const char *z;` |
|        - |  641 | `	sxu32 n;` |
|    16053 |  642 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|    15987 |  643 | `		return 0;` |
|        - |  644 | `	}` |
|       70 |  645 | `	z = pClass->zString;` |
|       70 |  646 | `	n = pClass->nByte;` |
|       70 |  647 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|        8 |  648 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|        - |  649 | `	}` |
|        - |  650 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|        - |  651 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|        - |  652 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|       63 |  653 | `	return 0;` |
|     8029 |  654 | `}` |
|        - |  655 |  |
|        - |  656 | `/*` |
|        - |  657 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|        - |  658 | ` * constant) — the main atom plus any union alternatives — against the` |
|        - |  659 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|        - |  660 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|        - |  661 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|        - |  662 | ` * type T" vs "Class constant C::X cannot have type T").` |
|        - |  663 | ` *` |
|        - |  664 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|        - |  665 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|        - |  666 | ` */` |
|    15986 |  667 | `PH7_PRIVATE sxi32 GenStateValidateMemberType(` |
|        - |  668 | `	ph7_gen_state *pGen,` |
|        - |  669 | `	ph7_class *pClass,` |
|        - |  670 | `	const SyString *pMemberName,` |
|        - |  671 | `	sxu32 nType,` |
|        - |  672 | `	const SyString *pTypeClass,` |
|        - |  673 | `	const SyString *pTypeText,` |
|        - |  674 | `	SySet *pUnionAlts,` |
|        - |  675 | `	const char *zErrFmt,` |
|        - |  676 | `	sxu32 nLine)` |
|        5 |  677 | `{` |
|    15991 |  678 | `	const char *zBad = 0;` |
|    15991 |  679 | `	sxu32 nBad = 0;` |
|        - |  680 | `	SyString sFallback;` |
|        - |  681 | `	const SyString *pBad;` |
|        - |  682 | `	sxi32 rc;` |
|    15991 |  683 | `	int bDisallowed = 0;` |
|    15991 |  684 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|        5 |  685 | `		bDisallowed = 1;` |
|    15989 |  686 | `	}else if( pUnionAlts ){` |
|        - |  687 | `		sxu32 i;` |
|       95 |  688 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|       67 |  689 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|       67 |  690 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|        3 |  691 | `				bDisallowed = 1;` |
|        3 |  692 | `				break;` |
|        - |  693 | `			}` |
|       35 |  694 | `		}` |
|       15 |  695 | `	}` |
|    15991 |  696 | `	if( !bDisallowed ){` |
|    15985 |  697 | `		return SXRET_OK;` |
|        - |  698 | `	}` |
|        - |  699 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|        - |  700 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|        - |  701 | `	 * canonical spelling if the type text is unavailable. */` |
|        8 |  702 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|        8 |  703 | `		pBad = pTypeText;` |
|        5 |  704 | `	}else{` |
|      ! 0 |  705 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|      ! 0 |  706 | `		pBad = &sFallback;` |
|        - |  707 | `	}` |
|       11 |  708 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        3 |  709 | `		zErrFmt,` |
|        3 |  710 | `		&pClass->sName,pMemberName,pBad);` |
|        8 |  711 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  712 | `		return SXERR_ABORT;` |
|        - |  713 | `	}` |
|        8 |  714 | `	return SXERR_SYNTAX;` |
|     7998 |  715 | `}` |
|        - |  716 | `/*` |
|        - |  717 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|        - |  718 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|        - |  719 | ` * matched as a plain identifier in the class-member modifier position rather` |
|        - |  720 | ` * than promoted to a lexer keyword.` |
|        - |  721 | ` */` |
| 23498110 |  722 | `PH7_PRIVATE int GenStateIsReadonly(SyToken *pTok)` |
|        5 |  723 | `{` |
| 23723720 |  724 | `	return (pTok->nType & PH7_TK_ID)` |
| 11974660 |  725 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 23723715 |  726 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|        5 |  727 | `}` |
|        - |  728 | `/*` |
|        - |  729 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|        - |  730 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|        - |  731 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|        - |  732 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|        - |  733 | ` */` |
|  8437988 |  734 | `PH7_PRIVATE sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|        5 |  735 | `{` |
|  8437993 |  736 | `	*pnTok = 0;` |
|  8437988 |  737 | `	if( &pTok[3] < pEnd` |
|  7888687 |  738 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|  6433569 |  739 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|  2763884 |  740 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       16 |  741 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|       16 |  742 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|       21 |  743 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|       17 |  744 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|       17 |  745 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|       17 |  746 | `			*pnTok = 4;` |
|       17 |  747 | `			return nKw;` |
|        - |  748 | `		}` |
|      ! 0 |  749 | `	}` |
|  8437977 |  750 | `	return 0;` |
|  4218999 |  751 | `}` |
|        - |  752 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|       16 |  753 | `PH7_PRIVATE sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|        1 |  754 | `{` |
|       17 |  755 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|       13 |  756 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|        - |  757 | `	}` |
|        5 |  758 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|        3 |  759 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|        - |  760 | `	}` |
|        3 |  761 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|        9 |  762 | `}` |
|   590260 |  763 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  764 | `{` |
|   590265 |  765 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  766 | `	ph7_class_attr *pAttr;` |
|        - |  767 | `	SyString *pName;` |
|        - |  768 | `	sxi32 rc;` |
|   590265 |  769 | `	sxu32 nType = 0;` |
|        - |  770 | `	SyString sTypeClass;` |
|        - |  771 | `	SyString sTypeText;` |
|        - |  772 | `	SySet aUnionAlts;` |
|   590265 |  773 | `	sxi32 iTypeFlags = 0;` |
|   590265 |  774 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   590265 |  775 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   590265 |  776 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  777 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|        - |  778 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|        - |  779 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   590265 |  780 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|       21 |  781 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        9 |  782 | `	}` |
|        - |  783 | `	/* Extract visibility level */` |
|   590265 |  784 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  785 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   598200 |  786 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|    15875 |  787 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|    15875 |  788 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  789 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  790 | `			goto Synchronize;` |
|    15875 |  791 | `		}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  792 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  793 | `				"Invalid property type or declaration near '%z'",` |
|      ! 0 |  794 | `				&pGen->pIn->sData);` |
|      ! 0 |  795 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  796 | `				return SXERR_ABORT;` |
|        - |  797 | `			}` |
|      ! 0 |  798 | `			goto Synchronize;` |
|    15875 |  799 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  800 | `			return SXERR_ABORT;` |
|        - |  801 | `		}` |
|     7935 |  802 | `	}` |
|      ! 0 |  803 | `loop:` |
|   590269 |  804 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  805 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|      ! 0 |  806 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  807 | `			return SXERR_ABORT;` |
|        - |  808 | `		}` |
|      ! 0 |  809 | `		goto Synchronize;` |
|        - |  810 | `	}` |
|   590269 |  811 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   590269 |  812 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        - |  813 | `		/* Invalid attribute name */` |
|      ! 0 |  814 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|      ! 0 |  815 | `		if( rc == SXERR_ABORT ){` |
|        - |  816 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  817 | `			return SXERR_ABORT;` |
|        - |  818 | `		}` |
|      ! 0 |  819 | `		goto Synchronize;` |
|        - |  820 | `	}` |
|        - |  821 | `	/* Peek attribute name */` |
|   590269 |  822 | `	pName = &pGen->pIn->sData;` |
|        - |  823 | `	/* Advance the stream cursor */` |
|   590269 |  824 | `	pGen->pIn++;` |
|   590269 |  825 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|        - |  826 | `		/* Invalid declaration */` |
|        3 |  827 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|        3 |  828 | `		if( rc == SXERR_ABORT ){` |
|        - |  829 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  830 | `			return SXERR_ABORT;` |
|        - |  831 | `		}` |
|        3 |  832 | `		goto Synchronize;` |
|        - |  833 | `	}` |
|        - |  834 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|        - |  835 | `	 * the read visibility must not be narrower than the set visibility. */` |
|   590267 |  836 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|       13 |  837 | `		const char *zAvErr = 0;` |
|       19 |  838 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|       10 |  839 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|        2 |  840 | `			: PH7_CLASS_PROT_PUBLIC;` |
|       13 |  841 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  842 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|       13 |  843 | `		}else if( iProtection > iSetLevel ){` |
|      ! 0 |  844 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|      ! 0 |  845 | `		}` |
|       13 |  846 | `		if( zAvErr ){` |
|      ! 0 |  847 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|      ! 0 |  848 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  849 | `				return SXERR_ABORT;` |
|        - |  850 | `			}` |
|      ! 0 |  851 | `			goto Synchronize;` |
|        - |  852 | `		}` |
|        6 |  853 | `	}` |
|        - |  854 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|        - |  855 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   590267 |  856 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       51 |  857 | `		const char *zRoErr = 0;` |
|       51 |  858 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        3 |  859 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|       50 |  860 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        6 |  861 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|       47 |  862 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|        6 |  863 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|        2 |  864 | `		}` |
|       51 |  865 | `		if( zRoErr ){` |
|       13 |  866 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|       13 |  867 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  868 | `				return SXERR_ABORT;` |
|        - |  869 | `			}` |
|       13 |  870 | `			goto Synchronize;` |
|        - |  871 | `		}` |
|       18 |  872 | `	}` |
|        - |  873 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|        - |  874 | `	 * type atom or any union alternative. void/never are already rejected` |
|        - |  875 | `	 * by the type parser. */` |
|   590257 |  876 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|    23807 |  877 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|        - |  878 | `			&sTypeText,` |
|    15868 |  879 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|     7934 |  880 | `			"Property %z::$%z cannot have type %z",nLine);` |
|    15873 |  881 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  882 | `			return SXERR_ABORT;` |
|    15873 |  883 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 |  884 | `			goto Synchronize;` |
|        - |  885 | `		}` |
|     7934 |  886 | `	}` |
|        - |  887 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   590257 |  888 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|        4 |  889 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  890 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|        3 |  891 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  892 | `			return SXERR_ABORT;` |
|        - |  893 | `		}` |
|        3 |  894 | `		goto Synchronize;` |
|        - |  895 | `	}` |
|        - |  896 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|        - |  897 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|        - |  898 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|        - |  899 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|        - |  900 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|        - |  901 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|   590255 |  902 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|        6 |  903 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  904 | `			"New expressions are not supported in this context");` |
|        6 |  905 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  906 | `			return SXERR_ABORT;` |
|        - |  907 | `		}` |
|        6 |  908 | `		goto Synchronize;` |
|        - |  909 | `	}` |
|        - |  910 | `	/* Allocate a new class attribute */` |
|   590251 |  911 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   590251 |  912 | `	if( pAttr ){` |
|   590251 |  913 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|   590251 |  914 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  915 | `			return SXERR_ABORT;` |
|        - |  916 | `		}` |
|   295123 |  917 | `	}` |
|   590251 |  918 | `	if( pAttr == 0 ){` |
|      ! 0 |  919 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  920 | `		return SXERR_ABORT;` |
|        - |  921 | `	}` |
|   590251 |  922 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|    15871 |  923 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|     7933 |  924 | `	}` |
|   590251 |  925 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|        - |  926 | `		SySet *pInstrContainer;` |
|   380623 |  927 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|   380623 |  928 | `		pGen->pIn++; /*Jump the equal sign */` |
|        - |  929 | `		{` |
|        - |  930 | `			/* Delimit the default expression: it ends at the declaration's` |
|        - |  931 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|        - |  932 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|        - |  933 | `			 * compiler would otherwise run into the hook tokens. */` |
|   380623 |  934 | `			SyToken *pScan = pGen->pIn;` |
|   380623 |  935 | `			sxi32 iNest = 0;` |
|   839355 |  936 | `			while( pScan < pGen->pEnd ){` |
|   839355 |  937 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|    50479 |  938 | `					iNest++;` |
|   814118 |  939 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|    50479 |  940 | `					iNest--;` |
|   763644 |  941 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|   380623 |  942 | `					break;` |
|        - |  943 | `				}` |
|   458737 |  944 | `				pScan++;` |
|        5 |  945 | `			}` |
|   380623 |  946 | `			pGen->pEnd = pScan;` |
|        - |  947 | `		}` |
|        - |  948 | `		/* Swap bytecode container */` |
|   380623 |  949 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   380623 |  950 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|        - |  951 | `		/* Compile attribute value.` |
|        - |  952 | `		 */` |
|   380623 |  953 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   380623 |  954 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 |  955 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|      ! 0 |  956 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  957 | `				return SXERR_ABORT;` |
|        - |  958 | `			}` |
|      ! 0 |  959 | `		}` |
|        - |  960 | `		/* Emit the done instruction */` |
|   380623 |  961 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   380623 |  962 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   380623 |  963 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|   380623 |  964 | `		pGen->pEnd = pSavedDefEnd;` |
|   190309 |  965 | `	}` |
|        - |  966 | `	/* All done,install the attribute */` |
|   590251 |  967 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   590251 |  968 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  969 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  970 | `		return SXERR_ABORT;` |
|        - |  971 | `	}` |
|   590251 |  972 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|        - |  973 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|        - |  974 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|       95 |  975 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|       95 |  976 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  977 | `			return SXERR_ABORT;` |
|        - |  978 | `		}` |
|       95 |  979 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  980 | `			goto Synchronize;` |
|        - |  981 | `		}` |
|       95 |  982 | `		SySetRelease(&aUnionAlts);` |
|       95 |  983 | `		return SXRET_OK;` |
|        - |  984 | `	}` |
|   590157 |  985 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - |  986 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|        - |  987 | `		 * wording differs per declaration site) */` |
|      ! 0 |  988 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  989 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|        - |  990 | `				? "Interfaces may only include hooked properties"` |
|        - |  991 | `				: "Only hooked properties may be declared abstract");` |
|      ! 0 |  992 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  993 | `			return SXERR_ABORT;` |
|        - |  994 | `		}` |
|      ! 0 |  995 | `		goto Synchronize;` |
|        - |  996 | `	}` |
|   590157 |  997 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  998 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|        5 |  999 | `		pGen->pIn++; /* Jump the comma */` |
|        5 | 1000 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 | 1001 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 | 1002 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 | 1003 | `				pTok--;` |
|      ! 0 | 1004 | `			}` |
|      ! 0 | 1005 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 1006 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|      ! 0 | 1007 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 | 1008 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1009 | `				return SXERR_ABORT;` |
|        - | 1010 | `			}` |
|      ! 0 | 1011 | `		}else{` |
|        5 | 1012 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        5 | 1013 | `				goto loop;` |
|        - | 1014 | `			}` |
|        - | 1015 | `		}` |
|      ! 0 | 1016 | `	}` |
|   590153 | 1017 | `	SySetRelease(&aUnionAlts);` |
|   590153 | 1018 | `	return SXRET_OK;` |
|        9 | 1019 | `Synchronize:` |
|        - | 1020 | `	/* Synchronize with the first semi-colon */` |
|       56 | 1021 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       38 | 1022 | `		pGen->pIn++;` |
|        4 | 1023 | `	}` |
|       22 | 1024 | `	SySetRelease(&aUnionAlts);` |
|       22 | 1025 | `	return SXERR_CORRUPT;` |
|   295135 | 1026 | `}` |
|        - | 1027 | `/*` |
|        - | 1028 | ` * Compile a class method.` |
|        - | 1029 | ` *` |
|        - | 1030 | ` * Refer to the official documentation for more information` |
|        - | 1031 | ` * on the powerful extension introduced by the PH7 engine` |
|        - | 1032 | ` * to the OO subsystem such as full type hinting,method` |
|        - | 1033 | ` * overloading and many more.` |
|        - | 1034 | ` */` |
|  2893376 | 1035 | `static sxi32 GenStateCompileClassMethod(` |
|        - | 1036 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 1037 | `	sxi32 iProtection,   /* Visibility level */` |
|        - | 1038 | `	sxi32 iFlags,        /* Configuration flags */` |
|        - | 1039 | `	int doBody,          /* TRUE to process method body */` |
|        - | 1040 | `	ph7_class *pClass    /* Class this method belongs */` |
|        - | 1041 | `	)` |
|        5 | 1042 | `{` |
|  2893381 | 1043 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  2893381 | 1044 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|        - | 1045 | `	ph7_class_method *pMeth;` |
|        - | 1046 | `	sxi32 iFuncFlags;` |
|        - | 1047 | `	SyString *pName;` |
|        - | 1048 | `	SyToken *pEnd;` |
|        - | 1049 | `	sxi32 rc;` |
|        - | 1050 | `	/* Extract visibility level */` |
|  2893381 | 1051 | `	iProtection = GetProtectionLevel(iProtection);` |
|  2893381 | 1052 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  2893381 | 1053 | `	iFuncFlags = 0;` |
|  2893381 | 1054 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1055 | `		/* Invalid method name */` |
|      ! 0 | 1056 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|      ! 0 | 1057 | `		if( rc == SXERR_ABORT ){` |
|        - | 1058 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1059 | `			return SXERR_ABORT;` |
|        - | 1060 | `		}` |
|      ! 0 | 1061 | `		goto Synchronize;` |
|        - | 1062 | `	}` |
|  2893381 | 1063 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - | 1064 | `		/* Return by reference,remember that */` |
|      ! 0 | 1065 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|        - | 1066 | `		/* Jump the '&' token */` |
|      ! 0 | 1067 | `		pGen->pIn++;` |
|      ! 0 | 1068 | `	}` |
|  2893381 | 1069 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 1070 | `		/* Invalid method name */` |
|      ! 0 | 1071 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|      ! 0 | 1072 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1073 | `			return SXERR_ABORT;` |
|        - | 1074 | `		}` |
|      ! 0 | 1075 | `		goto Synchronize;` |
|        - | 1076 | `	}` |
|        - | 1077 | `	/* Peek method name */` |
|  2893381 | 1078 | `	pName = &pGen->pIn->sData;` |
|  2893381 | 1079 | `	nLine = pGen->pIn->nLine;` |
|        - | 1080 | `	/* Jump the method name */` |
|  2893381 | 1081 | `	pGen->pIn++;` |
|  2893381 | 1082 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - | 1083 | `		/* Abstract method */` |
|   139617 | 1084 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|      ! 0 | 1085 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1086 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|      ! 0 | 1087 | `				&pClass->sName,pName);` |
|      ! 0 | 1088 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1089 | `				return SXERR_ABORT;` |
|        - | 1090 | `			}` |
|      ! 0 | 1091 | `		}` |
|        - | 1092 | `		/* Assemble method signature only */` |
|   139617 | 1093 | `		doBody = FALSE;` |
|    69806 | 1094 | `	}` |
|  2893381 | 1095 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1096 | `		/* Syntax error */` |
|      ! 0 | 1097 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|      ! 0 | 1098 | `		if( rc == SXERR_ABORT ){` |
|        - | 1099 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1100 | `			return SXERR_ABORT;` |
|        - | 1101 | `		}` |
|      ! 0 | 1102 | `		goto Synchronize;` |
|        - | 1103 | `	}` |
|        - | 1104 | `	/* Allocate a new class_method instance */` |
|  2893381 | 1105 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  2893381 | 1106 | `	if( pMeth == 0 ){` |
|      ! 0 | 1107 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1108 | `		return SXERR_ABORT;` |
|        - | 1109 | `	}` |
|  2893381 | 1110 | `	pMeth->sFunc.nLine = nKwLine;` |
|  2893381 | 1111 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|  2893381 | 1112 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 1113 | `		return SXERR_ABORT;` |
|        - | 1114 | `	}` |
|        - | 1115 | `	/* Jump the left parenthesis '(' */` |
|  2893381 | 1116 | `	pGen->pIn++;` |
|  2893381 | 1117 | `	pEnd = 0; /* cc warning */` |
|        - | 1118 | `	/* Delimit the method signature */` |
|  2893381 | 1119 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  2893381 | 1120 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 1121 | `		/* Syntax error */` |
|        3 | 1122 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|        3 | 1123 | `		if( rc == SXERR_ABORT ){` |
|        - | 1124 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1125 | `			return SXERR_ABORT;` |
|        - | 1126 | `		}` |
|        3 | 1127 | `		goto Synchronize;` |
|        - | 1128 | `	}` |
|        - | 1129 | `	{` |
|  2893379 | 1130 | `		int bIsCtor = 0;` |
|  2893379 | 1131 | `		int bAbstractCtor = 0;` |
|  2893374 | 1132 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  1698798 | 1133 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  2790552 | 1134 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   205659 | 1135 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        3 | 1136 | `				bAbstractCtor = 1;` |
|        2 | 1137 | `			}else{` |
|   205657 | 1138 | `				bIsCtor = 1;` |
|        - | 1139 | `			}` |
|   102827 | 1140 | `		}` |
|  2893379 | 1141 | `		if( pGen->pIn < pEnd ){` |
|        - | 1142 | `			/* Collect method arguments */` |
|  1113157 | 1143 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|  1113157 | 1144 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1145 | `				return SXERR_ABORT;` |
|        - | 1146 | `			}` |
|   556576 | 1147 | `		}` |
|        - | 1148 | `	}` |
|        - | 1149 | `	/* Point past ')' and parse optional return type ': type' */` |
|  2893379 | 1150 | `	pGen->pIn = &pEnd[1];` |
|        - | 1151 | `	{` |
|  2893379 | 1152 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  2893379 | 1153 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 | 1154 | `			return SXERR_ABORT;` |
|  2893379 | 1155 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|      ! 0 | 1156 | `			goto Synchronize;` |
|        - | 1157 | `		}` |
|        - | 1158 | `	}` |
|        - | 1159 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|        - | 1160 | `	 * property init/typecheck is handled by the generic typed-property path` |
|        - | 1161 | `	 * since we mint real ph7_class_attr entries. */` |
|        - | 1162 | `	{` |
|  2893379 | 1163 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|        - | 1164 | `		sxu32 i;` |
|  4557117 | 1165 | `		for( i = 0; i < nArg; i++ ){` |
|  1663753 | 1166 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|        - | 1167 | `			ph7_class_attr *pAttr;` |
|  1663753 | 1168 | `			sxi32 iAttrFlags = 0;` |
|        - | 1169 | `			int bArgTyped;` |
|  1663753 | 1170 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  1663667 | 1171 | `				continue;` |
|        - | 1172 | `			}` |
|        - | 1173 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|        - | 1174 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|        - | 1175 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|       60 | 1176 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|       92 | 1177 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|       91 | 1178 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        3 | 1179 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1180 | `					"Cannot declare variadic promoted property");` |
|        3 | 1181 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1182 | `					return SXERR_ABORT;` |
|        - | 1183 | `				}` |
|        3 | 1184 | `				goto Synchronize;` |
|        - | 1185 | `			}` |
|        - | 1186 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|        - | 1187 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|        - | 1188 | `			 * appear as an alternative of a union type. */` |
|       89 | 1189 | `			if( bArgTyped ){` |
|      125 | 1190 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|       80 | 1191 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|       80 | 1192 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|       40 | 1193 | `					"Property %z::$%z cannot have type %z",nLine);` |
|       85 | 1194 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1195 | `					return SXERR_ABORT;` |
|       85 | 1196 | `				}else if( rc != SXRET_OK ){` |
|        6 | 1197 | `					goto Synchronize;` |
|        - | 1198 | `				}` |
|       38 | 1199 | `			}` |
|        - | 1200 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|       85 | 1201 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|        4 | 1202 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1203 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|        3 | 1204 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1205 | `					return SXERR_ABORT;` |
|        - | 1206 | `				}` |
|        3 | 1207 | `				goto Synchronize;` |
|        - | 1208 | `			}` |
|       83 | 1209 | `			if( bArgTyped ){` |
|       79 | 1210 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       37 | 1211 | `			}` |
|       83 | 1212 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|        3 | 1213 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|        1 | 1214 | `			}` |
|       83 | 1215 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|        8 | 1216 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|        3 | 1217 | `			}` |
|       83 | 1218 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|        - | 1219 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|        - | 1220 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|       26 | 1221 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        4 | 1222 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1223 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|        3 | 1224 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1225 | `						return SXERR_ABORT;` |
|        - | 1226 | `					}` |
|        3 | 1227 | `					goto Synchronize;` |
|        - | 1228 | `				}` |
|       24 | 1229 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       10 | 1230 | `			}` |
|       81 | 1231 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|        - | 1232 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|        5 | 1233 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 | 1234 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1235 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|      ! 0 | 1236 | `						&pClass->sName,&pArg->sName);` |
|      ! 0 | 1237 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1238 | `						return SXERR_ABORT;` |
|        - | 1239 | `					}` |
|      ! 0 | 1240 | `					goto Synchronize;` |
|        - | 1241 | `				}` |
|        5 | 1242 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|        2 | 1243 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|        2 | 1244 | `			}` |
|       81 | 1245 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|       81 | 1246 | `			if( pAttr == 0 ){` |
|      ! 0 | 1247 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1248 | `				return SXERR_ABORT;` |
|        - | 1249 | `			}` |
|       81 | 1250 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|       79 | 1251 | `				pAttr->nType = pArg->nType;` |
|       79 | 1252 | `				pAttr->sClass = pArg->sClass;` |
|       79 | 1253 | `				pAttr->sTypeName = pArg->sTypeName;` |
|       79 | 1254 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|        - | 1255 | `					sxu32 k;` |
|       20 | 1256 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|       14 | 1257 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|       14 | 1258 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|        8 | 1259 | `					}` |
|        3 | 1260 | `				}` |
|       37 | 1261 | `			}` |
|       81 | 1262 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|       81 | 1263 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1264 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1265 | `				return SXERR_ABORT;` |
|        - | 1266 | `			}` |
|       43 | 1267 | `		}` |
|        - | 1268 | `	}` |
|  2893369 | 1269 | `	if( doBody ){` |
|        - | 1270 | `		/* Compile method body */` |
|  2753757 | 1271 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  2753757 | 1272 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1273 | `			return SXERR_ABORT;` |
|        - | 1274 | `		}` |
|        - | 1275 | `		/* The cursor sits just past the body's closing brace */` |
|  2753757 | 1276 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|  1376881 | 1277 | `	}else{` |
|        - | 1278 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|   139617 | 1279 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|   139617 | 1280 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|    69806 | 1281 | `		}` |
|        - | 1282 | `		/* Only method signature is allowed */` |
|   139617 | 1283 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|      ! 0 | 1284 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 1285 | `				"Expected ';' after method signature '%z'",pName);` |
|      ! 0 | 1286 | `				if( rc == SXERR_ABORT ){` |
|        - | 1287 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 | 1288 | `					return SXERR_ABORT;` |
|        - | 1289 | `				}` |
|      ! 0 | 1290 | `				return SXERR_CORRUPT;` |
|        - | 1291 | `			}` |
|        - | 1292 | `	}` |
|        - | 1293 | `	/* All done,install the method */` |
|  2893369 | 1294 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  2893369 | 1295 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1296 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1297 | `		return SXERR_ABORT;` |
|        - | 1298 | `	}` |
|  2893369 | 1299 | `	return SXRET_OK;` |
|        6 | 1300 | `Synchronize:` |
|        - | 1301 | `	/* Synchronize with the first semi-colon */` |
|       40 | 1302 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       28 | 1303 | `		pGen->pIn++;` |
|        4 | 1304 | `	}` |
|       16 | 1305 | `	return SXERR_CORRUPT;` |
|  1446693 | 1306 | `}` |
|        - | 1307 | `/*` |
|        - | 1308 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|        - | 1309 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|        - | 1310 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|        - | 1311 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|        - | 1312 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|        - | 1313 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|        - | 1314 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|        - | 1315 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|        - | 1316 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|        - | 1317 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|        - | 1318 | `` * implicit `$value` formal.`` |
|        - | 1319 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|        - | 1320 | ` */` |
|        - | 1321 | `/*` |
|        - | 1322 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|        - | 1323 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|        - | 1324 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|        - | 1325 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|        - | 1326 | ` * allowed, excluded from the raw object surfaces.` |
|        - | 1327 | ` */` |
|       94 | 1328 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|        1 | 1329 | `{` |
|        - | 1330 | `	SyToken *p;` |
|      345 | 1331 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|      303 | 1332 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      223 | 1333 | `			continue;` |
|        - | 1334 | `		}` |
|        - | 1335 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|       80 | 1336 | `		if( p + 3 < pEnd` |
|       80 | 1337 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       80 | 1338 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|       73 | 1339 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|       66 | 1340 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|       66 | 1341 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       66 | 1342 | `		 && p[3].sData.nByte == pName->nByte` |
|       60 | 1343 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       51 | 1344 | `			return 1;` |
|        - | 1345 | `		}` |
|        - | 1346 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|        - | 1347 | `		 * hook operates on the shared per-instance backing store, so the` |
|        - | 1348 | `		 * property is backed (php compiles a default alongside it). */` |
|       30 | 1349 | `		if( p > pStart` |
|       26 | 1350 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|       12 | 1351 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        2 | 1352 | `		 && p[1].sData.nByte == pName->nByte` |
|        3 | 1353 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        3 | 1354 | `			return 1;` |
|        - | 1355 | `		}` |
|       15 | 1356 | `	}` |
|       43 | 1357 | `	return 0;` |
|       48 | 1358 | `}` |
|        - | 1359 | `/*` |
|        - | 1360 | ` * True when p opens php 8.4's parent-hook call form` |
|        - | 1361 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|        - | 1362 | ` */` |
|      990 | 1363 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|        1 | 1364 | `{` |
|     1167 | 1365 | `	return p + 6 < pEnd` |
|      671 | 1366 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|      250 | 1367 | `	 && p->sData.nByte == sizeof("parent")-1` |
|       81 | 1368 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|       11 | 1369 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|        8 | 1370 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|        8 | 1371 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        8 | 1372 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|        8 | 1373 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        8 | 1374 | `	 && p[5].sData.nByte == 3` |
|        8 | 1375 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|        6 | 1376 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|     1166 | 1377 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|        1 | 1378 | `}` |
|        - | 1379 | `/*` |
|        - | 1380 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|        - | 1381 | ` * hook body into calls of the parent class's synthesized hook method` |
|        - | 1382 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|        - | 1383 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|        - | 1384 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|        - | 1385 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|        - | 1386 | ` * or SXERR_MEM.` |
|        - | 1387 | ` */` |
|        4 | 1388 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|        - | 1389 | `	SyToken *pStart,SyToken *pEnd)` |
|        1 | 1390 | `{` |
|        5 | 1391 | `	SyToken *p = pStart;` |
|       35 | 1392 | `	while( p < pEnd ){` |
|       31 | 1393 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|        - | 1394 | `			SyToken sTok;` |
|        - | 1395 | `			char zName[384];` |
|        - | 1396 | `			sxu32 nName;` |
|        - | 1397 | `			char *zDup;` |
|        - | 1398 | ``			/* `parent` `::` */`` |
|        5 | 1399 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|        5 | 1400 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|        7 | 1401 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|        4 | 1402 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|        5 | 1403 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|        5 | 1404 | `			if( zDup == 0 ){` |
|      ! 0 | 1405 | `				return SXERR_MEM;` |
|        - | 1406 | `			}` |
|        5 | 1407 | `			sTok = p[3]; /* keep the line info of the property name */` |
|        5 | 1408 | `			sTok.nType = PH7_TK_ID;` |
|        5 | 1409 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|        5 | 1410 | `			sTok.pUserData = 0;` |
|        5 | 1411 | `			SySetPut(pCopy,(const void *)&sTok);` |
|        5 | 1412 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|        5 | 1413 | `			continue;` |
|        - | 1414 | `		}` |
|       27 | 1415 | `		SySetPut(pCopy,(const void *)p);` |
|       27 | 1416 | `		p++;` |
|        1 | 1417 | `	}` |
|        5 | 1418 | `	return SXRET_OK;` |
|        3 | 1419 | `}` |
|       94 | 1420 | `PH7_PRIVATE sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 | 1421 | `{` |
|       95 | 1422 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 1423 | `	sxi32 rc;` |
|       95 | 1424 | `	int bRefsSelf = 0;` |
|       95 | 1425 | `	pGen->pIn++; /* Jump '{' */` |
|      253 | 1426 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|        - | 1427 | `		char zHook[384];` |
|        - | 1428 | `		SyString sHookName;` |
|        - | 1429 | `		ph7_class_method *pMeth;` |
|        - | 1430 | `		int bGet;` |
|      159 | 1431 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|      159 | 1432 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       15 | 1433 | `			pGen->pIn++; /* stray ';' between hooks */` |
|       22 | 1434 | `			continue;` |
|        - | 1435 | `		}` |
|      145 | 1436 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - | 1437 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|      ! 0 | 1438 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 1439 | `				"By-reference property hooks are not supported for %z::$%z",` |
|      ! 0 | 1440 | `				&pClass->sName,&pAttr->sName);` |
|      ! 0 | 1441 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1442 | `				return SXERR_ABORT;` |
|        - | 1443 | `			}` |
|      ! 0 | 1444 | `			return SXERR_CORRUPT;` |
|        - | 1445 | `		}` |
|      145 | 1446 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 1447 | `			goto HookSyntax;` |
|        - | 1448 | `		}` |
|      144 | 1449 | `		if( pGen->pIn->sData.nByte == 3` |
|      145 | 1450 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|       79 | 1451 | `			bGet = 1;` |
|      106 | 1452 | `		}else if( pGen->pIn->sData.nByte == 3` |
|       67 | 1453 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|       67 | 1454 | `			bGet = 0;` |
|       34 | 1455 | `		}else{` |
|      ! 0 | 1456 | `			goto HookSyntax;` |
|        - | 1457 | `		}` |
|      145 | 1458 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|      145 | 1459 | `		sHookName.zString = zHook;` |
|      217 | 1460 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|       72 | 1461 | `			bGet ? "get" : "set",&pAttr->sName);` |
|      145 | 1462 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|        - | 1463 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|        - | 1464 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|        - | 1465 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|        - | 1466 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|        - | 1467 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|       14 | 1468 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|        8 | 1469 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 1470 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 1471 | `					"Non-abstract property hook must have a body");` |
|      ! 0 | 1472 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1473 | `					return SXERR_ABORT;` |
|        - | 1474 | `				}` |
|      ! 0 | 1475 | `				return SXERR_CORRUPT;` |
|        - | 1476 | `			}` |
|       15 | 1477 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|        - | 1478 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|       15 | 1479 | `			if( pMeth == 0 ){` |
|      ! 0 | 1480 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1481 | `				return SXERR_ABORT;` |
|        - | 1482 | `			}` |
|       15 | 1483 | `			pMeth->sFunc.nLine = nHLine;` |
|       15 | 1484 | `			if( !bGet ){` |
|        - | 1485 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|        - | 1486 | `				 * compatible with concrete set-hook implementations (which` |
|        - | 1487 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|        - | 1488 | `				 * type (php: the abstract set's parameter type IS the property` |
|        - | 1489 | `				 * type), so the override contravariance check accepts a typed` |
|        - | 1490 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|        - | 1491 | `				ph7_vm_func_arg sVArg;` |
|        7 | 1492 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        7 | 1493 | `				if( zVName == 0 ){` |
|      ! 0 | 1494 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1495 | `					return SXERR_ABORT;` |
|        - | 1496 | `				}` |
|        7 | 1497 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        7 | 1498 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        7 | 1499 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        7 | 1500 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        7 | 1501 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        7 | 1502 | `				sVArg.nType = pAttr->nType;` |
|        7 | 1503 | `				sVArg.sClass = pAttr->sClass;` |
|        7 | 1504 | `				sVArg.sTypeName = pAttr->sTypeName;` |
|        7 | 1505 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|      ! 0 | 1506 | `					sVArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|      ! 0 | 1507 | `				}` |
|        7 | 1508 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        3 | 1509 | `			}` |
|       15 | 1510 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       15 | 1511 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1512 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1513 | `				return SXERR_ABORT;` |
|        - | 1514 | `			}` |
|       15 | 1515 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|       15 | 1516 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|        - | 1517 | `		}` |
|      130 | 1518 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|      131 | 1519 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|        - | 1520 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|      ! 0 | 1521 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 1522 | `				"Abstract property hook cannot have body");` |
|      ! 0 | 1523 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1524 | `				return SXERR_ABORT;` |
|        - | 1525 | `			}` |
|      ! 0 | 1526 | `			return SXERR_CORRUPT;` |
|        - | 1527 | `		}` |
|      131 | 1528 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|        - | 1529 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|      131 | 1530 | `		if( pMeth == 0 ){` |
|      ! 0 | 1531 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1532 | `			return SXERR_ABORT;` |
|        - | 1533 | `		}` |
|      131 | 1534 | `		pMeth->sFunc.nLine = nHLine;` |
|      131 | 1535 | `		if( !bGet ){` |
|        - | 1536 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|       61 | 1537 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       17 | 1538 | `				SyToken *pRp = 0;` |
|       17 | 1539 | `				pGen->pIn++;` |
|       17 | 1540 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|       17 | 1541 | `				if( pRp >= pGen->pEnd ){` |
|      ! 0 | 1542 | `					goto HookSyntax;` |
|        - | 1543 | `				}` |
|       17 | 1544 | `				if( pGen->pIn < pRp ){` |
|       17 | 1545 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|       17 | 1546 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1547 | `						return SXERR_ABORT;` |
|        - | 1548 | `					}` |
|        8 | 1549 | `				}` |
|       17 | 1550 | `				pGen->pIn = &pRp[1];` |
|        8 | 1551 | `			}` |
|       61 | 1552 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|        - | 1553 | `				/* Implicit $value formal */` |
|        - | 1554 | `				ph7_vm_func_arg sVArg;` |
|       45 | 1555 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|       45 | 1556 | `				if( zVName == 0 ){` |
|      ! 0 | 1557 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1558 | `					return SXERR_ABORT;` |
|        - | 1559 | `				}` |
|       45 | 1560 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|       45 | 1561 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|       45 | 1562 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       45 | 1563 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       45 | 1564 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|       45 | 1565 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|       45 | 1566 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|       22 | 1567 | `			}` |
|       30 | 1568 | `		}` |
|      165 | 1569 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - | 1570 | `			/* Block body */` |
|       69 | 1571 | `			SyToken *pBodyStart = pGen->pIn;` |
|       69 | 1572 | `			SyToken *pCloser = 0;` |
|       69 | 1573 | `			int bParentCall = 0;` |
|       69 | 1574 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|       69 | 1575 | `			if( pCloser < pGen->pEnd ){` |
|        - | 1576 | `				SyToken *pScan;` |
|      753 | 1577 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|      687 | 1578 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|        3 | 1579 | `						bParentCall = 1;` |
|        3 | 1580 | `						break;` |
|        - | 1581 | `					}` |
|      343 | 1582 | `				}` |
|       34 | 1583 | `			}` |
|       69 | 1584 | `			if( bParentCall ){` |
|        - | 1585 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|        - | 1586 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|        - | 1587 | `				 * hook method), then continue past the original body. */` |
|        - | 1588 | `				SySet sBody;` |
|        3 | 1589 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|        3 | 1590 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        3 | 1591 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|        3 | 1592 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1593 | `					SySetRelease(&sBody);` |
|      ! 0 | 1594 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1595 | `					return SXERR_ABORT;` |
|        - | 1596 | `				}` |
|        3 | 1597 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|        3 | 1598 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|        3 | 1599 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        3 | 1600 | `				pGen->pIn = &pCloser[1];` |
|        3 | 1601 | `				pGen->pEnd = pSavedEnd;` |
|        3 | 1602 | `				SySetRelease(&sBody);` |
|        3 | 1603 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1604 | `					return SXERR_ABORT;` |
|        - | 1605 | `				}` |
|        3 | 1606 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|        2 | 1607 | `			}else{` |
|       67 | 1608 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|       67 | 1609 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1610 | `					return SXERR_ABORT;` |
|        - | 1611 | `				}` |
|       67 | 1612 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|        - | 1613 | `			}` |
|       69 | 1614 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|       17 | 1615 | `				bRefsSelf = 1;` |
|        9 | 1616 | `			}` |
|      128 | 1617 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|        - | 1618 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|        - | 1619 | `			GenBlock *pBlock;` |
|        - | 1620 | `			SySet *pInstrContainer;` |
|        - | 1621 | `			SyToken *pBodyStart;` |
|        - | 1622 | `			SyToken *pExprEnd;` |
|       63 | 1623 | `			SyToken *pSavedEnd = 0;` |
|        - | 1624 | `			SySet sBody;` |
|       63 | 1625 | `			int bParentCall = 0;` |
|       63 | 1626 | `			pGen->pIn++; /* Jump '=>' */` |
|       63 | 1627 | `			pBodyStart = pGen->pIn;` |
|        - | 1628 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|        - | 1629 | `			 * would end the enclosing hook list) and rewrite any` |
|        - | 1630 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|        - | 1631 | `			 * method on a token copy. */` |
|        - | 1632 | `			{` |
|       63 | 1633 | `				sxi32 iNest = 0;` |
|       63 | 1634 | `				pExprEnd = pBodyStart;` |
|      355 | 1635 | `				while( pExprEnd < pGen->pEnd ){` |
|      355 | 1636 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        9 | 1637 | `						iNest++;` |
|      351 | 1638 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        9 | 1639 | `						if( iNest <= 0 ){` |
|      ! 0 | 1640 | `							break;` |
|        - | 1641 | `						}` |
|        9 | 1642 | `						iNest--;` |
|      343 | 1643 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|       63 | 1644 | `						break;` |
|        - | 1645 | `					}` |
|      293 | 1646 | `					pExprEnd++;` |
|        1 | 1647 | `				}` |
|        - | 1648 | `			}` |
|        - | 1649 | `			{` |
|        - | 1650 | `				SyToken *pScan;` |
|      335 | 1651 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|      275 | 1652 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|        3 | 1653 | `						bParentCall = 1;` |
|        3 | 1654 | `						break;` |
|        - | 1655 | `					}` |
|      137 | 1656 | `				}` |
|        - | 1657 | `			}` |
|       63 | 1658 | `			if( bParentCall ){` |
|        3 | 1659 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        3 | 1660 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|        3 | 1661 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1662 | `					SySetRelease(&sBody);` |
|      ! 0 | 1663 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1664 | `					return SXERR_ABORT;` |
|        - | 1665 | `				}` |
|        3 | 1666 | `				pSavedEnd = pGen->pEnd;` |
|        3 | 1667 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|        3 | 1668 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|        1 | 1669 | `			}` |
|       94 | 1670 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       62 | 1671 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|       63 | 1672 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1673 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|      ! 0 | 1674 | `				return SXERR_ABORT;` |
|        - | 1675 | `			}` |
|       63 | 1676 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       63 | 1677 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|       63 | 1678 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|       63 | 1679 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       63 | 1680 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       63 | 1681 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       63 | 1682 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       63 | 1683 | `			GenStateLeaveBlock(&(*pGen),0);` |
|       63 | 1684 | `			if( bParentCall ){` |
|        3 | 1685 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|        3 | 1686 | `				pGen->pEnd = pSavedEnd;` |
|        3 | 1687 | `				SySetRelease(&sBody);` |
|        1 | 1688 | `			}` |
|       63 | 1689 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1690 | `				return SXERR_ABORT;` |
|        - | 1691 | `			}` |
|       63 | 1692 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|       63 | 1693 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|       37 | 1694 | `				bRefsSelf = 1;` |
|       18 | 1695 | `			}` |
|       63 | 1696 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       63 | 1697 | `				pGen->pIn++; /* Jump ';' */` |
|       31 | 1698 | `			}` |
|       63 | 1699 | `			if( !bGet ){` |
|        - | 1700 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|        - | 1701 | `				 * the dispatcher consumes the implicit return value — which` |
|        - | 1702 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|        - | 1703 | ``				 * for `$this->NAME = expr`). */`` |
|        3 | 1704 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|        3 | 1705 | `				bRefsSelf = 1;` |
|        1 | 1706 | `			}` |
|       32 | 1707 | `		}else{` |
|      ! 0 | 1708 | `			goto HookSyntax;` |
|        - | 1709 | `		}` |
|      131 | 1710 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|      131 | 1711 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1712 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1713 | `			return SXERR_ABORT;` |
|        - | 1714 | `		}` |
|      131 | 1715 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        1 | 1716 | `	}` |
|       95 | 1717 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|      ! 0 | 1718 | `		goto HookSyntax;` |
|        - | 1719 | `	}` |
|       95 | 1720 | `	pGen->pIn++; /* Jump '}' */` |
|       95 | 1721 | `	if( !bRefsSelf ){` |
|        - | 1722 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|        - | 1723 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|        - | 1724 | `		 * a default value (compile fatal, php's exact wording). */` |
|       41 | 1725 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|       41 | 1726 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      ! 0 | 1727 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1728 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|      ! 0 | 1729 | `				&pClass->sName,&pAttr->sName);` |
|      ! 0 | 1730 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1731 | `				return SXERR_ABORT;` |
|        - | 1732 | `			}` |
|      ! 0 | 1733 | `			return SXERR_CORRUPT;` |
|        - | 1734 | `		}` |
|       20 | 1735 | `	}` |
|       95 | 1736 | `	return SXRET_OK;` |
|      ! 0 | 1737 | `HookSyntax:` |
|      ! 0 | 1738 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1739 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|      ! 0 | 1740 | `		&pClass->sName,&pAttr->sName);` |
|      ! 0 | 1741 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1742 | `		return SXERR_ABORT;` |
|        - | 1743 | `	}` |
|      ! 0 | 1744 | `	return SXERR_CORRUPT;` |
|       48 | 1745 | `}` |
|        - | 1746 | `/*` |
|        - | 1747 | ` * Compile an object interface.` |
|        - | 1748 | ` *  According to the PHP language reference manual` |
|        - | 1749 | ` *   Object Interfaces:` |
|        - | 1750 | ` *   Object interfaces allow you to create code which specifies which methods` |
|        - | 1751 | ` *   a class must implement, without having to define how these methods are handled.` |
|        - | 1752 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - | 1753 | ` *   class, but without any of the methods having their contents defined.` |
|        - | 1754 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|        - | 1755 | ` */` |
|    69892 | 1756 | `PH7_PRIVATE sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|        5 | 1757 | `{` |
|    69897 | 1758 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 1759 | `	ph7_class *pClass,*pBase;` |
|        - | 1760 | `	SyToken *pEnd,*pTmp;` |
|        - | 1761 | `	SyString *pName;` |
|        - | 1762 | `	sxi32 nKwrd;` |
|        - | 1763 | `	sxi32 rc;` |
|        - | 1764 | `	/* Jump the 'interface' keyword */` |
|    69897 | 1765 | `	pGen->pIn++;` |
|        - | 1766 | `	/* Extract interface name */` |
|    69897 | 1767 | `	pName = &pGen->pIn->sData;` |
|        - | 1768 | `	/* Advance the stream cursor */` |
|    69897 | 1769 | `	pGen->pIn++;` |
|        - | 1770 | `	/* Build FQN and obtain a raw class */ {` |
|        - | 1771 | `		SyBlob sFQN;` |
|        - | 1772 | `		SyString sFQNStr;` |
|    69897 | 1773 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    69897 | 1774 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    69897 | 1775 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    69897 | 1776 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    69897 | 1777 | `		SyBlobRelease(&sFQN);` |
|        - | 1778 | `	}` |
|    69897 | 1779 | `	if( pClass == 0 ){` |
|      ! 0 | 1780 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1781 | `		return SXERR_ABORT;` |
|        - | 1782 | `	}` |
|    69897 | 1783 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    69897 | 1784 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 1785 | `		return SXERR_ABORT;` |
|        - | 1786 | `	}` |
|        - | 1787 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|    69897 | 1788 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|        - | 1789 | `	/* Assume no base class is given */` |
|    69897 | 1790 | `	pBase = 0;` |
|    69897 | 1791 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    27149 | 1792 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    27149 | 1793 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a, c, … */ ){` |
|        - | 1794 | `			/* php lets an interface extend SEVERAL parent interfaces. The first` |
|        - | 1795 | `			 * becomes pBase (single-inheritance chain, hDerived); every extra one` |
|        - | 1796 | `			 * is recorded via PH7_ClassImplement so it lands in aInterface — the` |
|        - | 1797 | `			 * runtime subtype walk (VmInterfaceReaches) follows both. */` |
|    27149 | 1798 | `			pGen->pIn++;` |
|    13573 | 1799 | `			for(;;){` |
|        - | 1800 | `				SyBlob sResolved;` |
|        - | 1801 | `				SyString sBaseName;` |
|        - | 1802 | `				sxu32 nRefLine;` |
|        - | 1803 | `				ph7_class *pParent;` |
|    27151 | 1804 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    27151 | 1805 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    27151 | 1806 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 1807 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 1808 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1809 | `						"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|      ! 0 | 1810 | `						pName);` |
|      ! 0 | 1811 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 1812 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1813 | `						return SXERR_ABORT;` |
|        - | 1814 | `					}` |
|      ! 0 | 1815 | `					return SXRET_OK;` |
|        - | 1816 | `				}` |
|    40724 | 1817 | `				pParent = PH7_VmExtractClass(pGen->pVm,` |
|    27146 | 1818 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    27151 | 1819 | `				SyStringInitFromBuf(&sBaseName,` |
|        - | 1820 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 1821 | `				/* Only interfaces is allowed */` |
|    27151 | 1822 | `				while( pParent && (pParent->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 1823 | `					pParent = pParent->pNextName;` |
|      ! 0 | 1824 | `				}` |
|    27151 | 1825 | `				if( pParent == 0 ){` |
|      ! 0 | 1826 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 1827 | `						"Nonexistent base interface '%z'",&sBaseName);` |
|      ! 0 | 1828 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1829 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 1830 | `						return SXERR_ABORT;` |
|      ! 0 | 1831 | `					}` |
|    27151 | 1832 | `				}else if( pBase == 0 ){` |
|        - | 1833 | `					/* First parent → single-inheritance base */` |
|    27149 | 1834 | `					pBase = pParent;` |
|    13577 | 1835 | `				}else{` |
|        - | 1836 | `					/* Additional parent → record it in aInterface (+ copy its` |
|        - | 1837 | `					 * constants/method stubs) so instanceof reaches it too. */` |
|        3 | 1838 | `					PH7_ClassImplement(pClass,pParent);` |
|        - | 1839 | `				}` |
|    27151 | 1840 | `				SyBlobRelease(&sResolved);` |
|        - | 1841 | `				/* Continue on a comma-separated list */` |
|    27151 | 1842 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 | 1843 | `					pGen->pIn++;` |
|        3 | 1844 | `					continue;` |
|        - | 1845 | `				}` |
|    27149 | 1846 | `				break;` |
|      ! 0 | 1847 | `			}` |
|    13572 | 1848 | `		}` |
|    13572 | 1849 | `	}` |
|    69897 | 1850 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - | 1851 | `		/* Syntax error */` |
|      ! 0 | 1852 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|      ! 0 | 1853 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 1854 | `		if( rc == SXERR_ABORT ){` |
|        - | 1855 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1856 | `			return SXERR_ABORT;` |
|        - | 1857 | `		}` |
|      ! 0 | 1858 | `		return SXRET_OK;` |
|        - | 1859 | `	}` |
|    69897 | 1860 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    69897 | 1861 | `	pEnd = 0; /* cc warning */` |
|        - | 1862 | `	/* Delimit the interface body */` |
|    69897 | 1863 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    69897 | 1864 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 1865 | `		/* Syntax error */` |
|      ! 0 | 1866 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|      ! 0 | 1867 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 1868 | `		if( rc == SXERR_ABORT ){` |
|        - | 1869 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1870 | `			return SXERR_ABORT;` |
|        - | 1871 | `		}` |
|      ! 0 | 1872 | `		return SXRET_OK;` |
|        - | 1873 | `	}` |
|        - | 1874 | `	/* The delimiter token is the interface body's closing brace */` |
|    69897 | 1875 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 1876 | `	/* Swap token stream */` |
|    69897 | 1877 | `	pTmp = pGen->pEnd;` |
|    69897 | 1878 | `	pGen->pEnd = pEnd;` |
|        - | 1879 | `	/* Start the parse process` |
|        - | 1880 | `	 * Note (According to the PHP reference manual):` |
|        - | 1881 | `	 *  Only constants and function signatures(without body) are allowed.` |
|        - | 1882 | `	 *  Only 'public' visibility is allowed.` |
|        - | 1883 | `	 */` |
|   128003 | 1884 | `	for(;;){` |
|        - | 1885 | `		/* Jump leading/trailing semi-colons */` |
|   442129 | 1886 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   186119 | 1887 | `			pGen->pIn++;` |
|        5 | 1888 | `		}` |
|   256015 | 1889 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1890 | `			/* End of interface body */` |
|    69893 | 1891 | `			break;` |
|        - | 1892 | `		}` |
|        - | 1893 | `		/* Bind a directly-preceding docblock to this member */` |
|   186127 | 1894 | `		GenStateSetPendingDoc(&(*pGen));` |
|   186127 | 1895 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 1896 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 1897 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|      ! 0 | 1898 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 1899 | `			if( rc == SXERR_ABORT ){` |
|        - | 1900 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1901 | `				return SXERR_ABORT;` |
|        - | 1902 | `			}` |
|      ! 0 | 1903 | `			goto done;` |
|        - | 1904 | `		}` |
|        - | 1905 | `		/* Extract the current keyword */` |
|   186127 | 1906 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   186127 | 1907 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        - | 1908 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|        - | 1909 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|        3 | 1910 | `			const char *zKind = "member";` |
|        3 | 1911 | `			SyString *pMemberName = 0;` |
|        3 | 1912 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|        3 | 1913 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|        3 | 1914 | `				if( nNext == PH7_TKWRD_CONST ){` |
|        3 | 1915 | `					zKind = "constant";` |
|        3 | 1916 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|        3 | 1917 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|        2 | 1918 | `					}` |
|        1 | 1919 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 1920 | `					zKind = "method";` |
|      ! 0 | 1921 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|      ! 0 | 1922 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|      ! 0 | 1923 | `					}` |
|      ! 0 | 1924 | `				}` |
|        1 | 1925 | `			}` |
|        3 | 1926 | `			if( pMemberName ){` |
|        4 | 1927 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        1 | 1928 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|        2 | 1929 | `			}else{` |
|      ! 0 | 1930 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 1931 | `					"Access type for interface %s must be public",zKind);` |
|        - | 1932 | `			}` |
|        3 | 1933 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1934 | `				return SXERR_ABORT;` |
|        - | 1935 | `			}` |
|        3 | 1936 | `			goto done;` |
|        - | 1937 | `		}` |
|   186125 | 1938 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|      ! 0 | 1939 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 1940 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 | 1941 | `			if( rc == SXERR_ABORT ){` |
|        - | 1942 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1943 | `				return SXERR_ABORT;` |
|        - | 1944 | `			}` |
|      ! 0 | 1945 | `			goto done;` |
|        - | 1946 | `		}` |
|   186125 | 1947 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|        - | 1948 | `			/* Advance the stream cursor */` |
|   131843 | 1949 | `			pGen->pIn++;` |
|   131838 | 1950 | `			if( pGen->pIn < pGen->pEnd` |
|   131843 | 1951 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|   131838 | 1952 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|        - | 1953 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|        - | 1954 | `				 * requirement. The attribute compiler + hook parser handle it` |
|        - | 1955 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|        - | 1956 | `				 * property without hooks is ITS "Interfaces may only include` |
|        - | 1957 | `				 * hooked properties" error). */` |
|      ! 0 | 1958 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|      ! 0 | 1959 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|      ! 0 | 1960 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1961 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1962 | `						return SXERR_ABORT;` |
|        - | 1963 | `					}` |
|      ! 0 | 1964 | `					goto done;` |
|        - | 1965 | `				}` |
|      ! 0 | 1966 | `				continue;` |
|        - | 1967 | `			}` |
|   131843 | 1968 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|        - | 1969 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|        - | 1970 | `				 * '$' also opens a hooked-property requirement. */` |
|      ! 0 | 1971 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|      ! 0 | 1972 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|      ! 0 | 1973 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|      ! 0 | 1974 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|      ! 0 | 1975 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|      ! 0 | 1976 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 1977 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 1978 | `							return SXERR_ABORT;` |
|        - | 1979 | `						}` |
|      ! 0 | 1980 | `						goto done;` |
|        - | 1981 | `					}` |
|      ! 0 | 1982 | `					continue;` |
|        - | 1983 | `				}` |
|      ! 0 | 1984 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 1985 | `					"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 | 1986 | `				if( rc == SXERR_ABORT ){` |
|        - | 1987 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 | 1988 | `					return SXERR_ABORT;` |
|        - | 1989 | `				}` |
|      ! 0 | 1990 | `				goto done;` |
|        - | 1991 | `			}` |
|   131843 | 1992 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   131843 | 1993 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|        - | 1994 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|        - | 1995 | `				 * hooked-property requirement (PHP 8.4). */` |
|        4 | 1996 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|        5 | 1997 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|        7 | 1998 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|        2 | 1999 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|        5 | 2000 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 2001 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 2002 | `							return SXERR_ABORT;` |
|        - | 2003 | `						}` |
|      ! 0 | 2004 | `						goto done;` |
|        - | 2005 | `					}` |
|        5 | 2006 | `					continue;` |
|        - | 2007 | `				}` |
|      ! 0 | 2008 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2009 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 | 2010 | `				if( rc == SXERR_ABORT ){` |
|        - | 2011 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 | 2012 | `					return SXERR_ABORT;` |
|        - | 2013 | `				}` |
|      ! 0 | 2014 | `				goto done;` |
|        - | 2015 | `			}` |
|    65917 | 2016 | `		}` |
|   186121 | 2017 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|        - | 2018 | `			/* Parse constant */` |
|    54283 | 2019 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|    54283 | 2020 | `			if( rc != SXRET_OK ){` |
|        3 | 2021 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2022 | `					return SXERR_ABORT;` |
|        - | 2023 | `				}` |
|        3 | 2024 | `				goto done;` |
|        - | 2025 | `			}` |
|    27143 | 2026 | `		}else{` |
|   131843 | 2027 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   131843 | 2028 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - | 2029 | `				/* Static method,record that */` |
|    11633 | 2030 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|        - | 2031 | `				/* Advance the stream cursor */` |
|    11633 | 2032 | `				pGen->pIn++;` |
|    11628 | 2033 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    11633 | 2034 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 2035 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2036 | `							"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 | 2037 | `						if( rc == SXERR_ABORT ){` |
|        - | 2038 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 2039 | `							return SXERR_ABORT;` |
|        - | 2040 | `						}` |
|      ! 0 | 2041 | `						goto done;` |
|        - | 2042 | `				}` |
|     5814 | 2043 | `			}` |
|        - | 2044 | `			/* Process method signature (no body for interface methods) */` |
|   131843 | 2045 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   131843 | 2046 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2047 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2048 | `					return SXERR_ABORT;` |
|        - | 2049 | `				}` |
|      ! 0 | 2050 | `				goto done;` |
|        - | 2051 | `			}` |
|        - | 2052 | `		}` |
|        5 | 2053 | `	}` |
|        - | 2054 | `	/* Reject a php-fatal redeclaration before hoisting the interface */` |
|    69893 | 2055 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|        3 | 2056 | `		return SXERR_ABORT;` |
|        - | 2057 | `	}` |
|        - | 2058 | `	/* Install the interface */` |
|    69891 | 2059 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    69891 | 2060 | `	if( rc == SXRET_OK && pBase ){` |
|        - | 2061 | `		/* Inherit from the base interface */` |
|    27149 | 2062 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    13572 | 2063 | `	}` |
|    69891 | 2064 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2065 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2066 | `		return SXERR_ABORT;` |
|        - | 2067 | `	}` |
|    34943 | 2068 | `done:` |
|        - | 2069 | `	/* Point beyond the interface body */` |
|    69895 | 2070 | `	pGen->pIn  = &pEnd[1];` |
|    69895 | 2071 | `	pGen->pEnd = pTmp;` |
|    69895 | 2072 | `	return PH7_OK;` |
|    34951 | 2073 | `}` |
|        - | 2074 | `/*` |
|        - | 2075 | ` * Compile a user-defined class.` |
|        - | 2076 | ` * According to the PHP language reference manual` |
|        - | 2077 | ` *  class` |
|        - | 2078 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|        - | 2079 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|        - | 2080 | ` *  of the properties and methods belonging to the class.` |
|        - | 2081 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|        - | 2082 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|        - | 2083 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|        - | 2084 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - | 2085 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|        - | 2086 | ` *  (called "methods").` |
|        - | 2087 | ` */` |
|        - | 2088 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|        - | 2089 | `typedef struct TraitUseEntry TraitUseEntry;` |
|        - | 2090 | `struct TraitUseEntry {` |
|        - | 2091 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|        - | 2092 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|        - | 2093 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|        - | 2094 | `};` |
|        - | 2095 | `/*` |
|        - | 2096 | ` * Validate that methods implementing interface contracts have compatible` |
|        - | 2097 | ` * signatures: public visibility and at least as many parameters as declared.` |
|        - | 2098 | ` */` |
|   451546 | 2099 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 2100 | `{` |
|        - | 2101 | `	ph7_class **apIface;` |
|        - | 2102 | `	sxu32 nIface,i;` |
|        - | 2103 | `	sxi32 rc;` |
|   451551 | 2104 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      ! 0 | 2105 | `		return SXRET_OK;` |
|        - | 2106 | `	}` |
|   451551 | 2107 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   451551 | 2108 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   874389 | 2109 | `	for(i = 0; i < nIface; i++){` |
|   422843 | 2110 | `		ph7_class *pIface = apIface[i];` |
|        - | 2111 | `		SyHashEntry *pEntry;` |
|   422843 | 2112 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  1245161 | 2113 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   822323 | 2114 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - | 2115 | `			ph7_class_method *pImplMeth;` |
|   822323 | 2116 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|        - | 2117 | `			/* Find the implementing method in the class */` |
|   822323 | 2118 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   822323 | 2119 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       23 | 2120 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|        - | 2121 | `			}` |
|        - | 2122 | `			/* Check visibility: interface methods must be implemented as public */` |
|   822305 | 2123 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        4 | 2124 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - | 2125 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|        1 | 2126 | `					&pClass->sName,pMName,&pIface->sName);` |
|        3 | 2127 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2128 | `					return SXERR_ABORT;` |
|        - | 2129 | `				}` |
|        1 | 2130 | `			}` |
|        - | 2131 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|        - | 2132 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|        - | 2133 | `			 */` |
|        - | 2134 | `			{` |
|   822305 | 2135 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   822305 | 2136 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   822305 | 2137 | `				int sigError = 0;` |
|   822305 | 2138 | `				if( nImplArgs < nIfaceArgs ){` |
|        3 | 2139 | `					sigError = 1;` |
|   822304 | 2140 | `				}else if( nImplArgs > nIfaceArgs ){` |
|        - | 2141 | `					/* Extra parameters must all have default values */` |
|     3885 | 2142 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        - | 2143 | `					sxu32 k;` |
|     7763 | 2144 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|     3885 | 2145 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|        3 | 2146 | `							sigError = 1;` |
|        3 | 2147 | `							break;` |
|        - | 2148 | `						}` |
|     1944 | 2149 | `					}` |
|     1940 | 2150 | `				}` |
|   822305 | 2151 | `				if( sigError ){` |
|        - | 2152 | `					SyBlob sImplSig, sIfaceSig;` |
|        - | 2153 | `					ph7_vm_func_arg *aArgs;` |
|        - | 2154 | `					sxu32 j;` |
|        6 | 2155 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|        6 | 2156 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|        - | 2157 | `					/* Build implementing method signature */` |
|        6 | 2158 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       12 | 2159 | `					for(j = 0; j < nImplArgs; j++){` |
|        8 | 2160 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|        8 | 2161 | `						SyBlobAppend(&sImplSig,"$",1);` |
|        8 | 2162 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 | 2163 | `					}` |
|        - | 2164 | `					/* Build interface method signature */` |
|        6 | 2165 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|       12 | 2166 | `					for(j = 0; j < nIfaceArgs; j++){` |
|        8 | 2167 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|        8 | 2168 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|        8 | 2169 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 | 2170 | `					}` |
|        8 | 2171 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - | 2172 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|        2 | 2173 | `						&pClass->sName,pMName,` |
|        4 | 2174 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|        2 | 2175 | `						&pIface->sName,pMName,` |
|        4 | 2176 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|        6 | 2177 | `					SyBlobRelease(&sImplSig);` |
|        6 | 2178 | `					SyBlobRelease(&sIfaceSig);` |
|        6 | 2179 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2180 | `						return SXERR_ABORT;` |
|        - | 2181 | `					}` |
|        2 | 2182 | `				}` |
|        - | 2183 | `			}` |
|        5 | 2184 | `		}` |
|   211424 | 2185 | `	}` |
|   451551 | 2186 | `	return SXRET_OK;` |
|   225778 | 2187 | `}` |
|        - | 2188 | `/*` |
|        - | 2189 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|        - | 2190 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|        - | 2191 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|        - | 2192 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|        - | 2193 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|        - | 2194 | ` * means that specific hook is still missing.` |
|        - | 2195 | ` */` |
|       38 | 2196 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|        5 | 2197 | `{` |
|        - | 2198 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        - | 2199 | `	ph7_class_attr *pProp;` |
|       38 | 2200 | `	if( pMName->nByte <= nPfx` |
|       27 | 2201 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|        4 | 2202 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|       36 | 2203 | `		return 0; /* not a hook stub */` |
|        - | 2204 | `	}` |
|        7 | 2205 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|        7 | 2206 | `	return pProp != 0` |
|        6 | 2207 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|        3 | 2208 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|       24 | 2209 | `}` |
|        - | 2210 | `/*` |
|        - | 2211 | ` * Append an abstract member's display name to the message blob, translating a` |
|        - | 2212 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|        - | 2213 | ` */` |
|       16 | 2214 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|        4 | 2215 | `{` |
|        - | 2216 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|       16 | 2217 | `	if( pMName->nByte > nPfx` |
|       12 | 2218 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|      ! 0 | 2219 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|      ! 0 | 2220 | `		SyBlobAppend(pMsg,"$",1);` |
|      ! 0 | 2221 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|      ! 0 | 2222 | `		SyBlobAppend(pMsg,"::",2);` |
|      ! 0 | 2223 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|      ! 0 | 2224 | `		return;` |
|        - | 2225 | `	}` |
|       20 | 2226 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|       12 | 2227 | `}` |
|        - | 2228 | `/*` |
|        - | 2229 | ` * Check that a concrete class has no remaining abstract methods.` |
|        - | 2230 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|        - | 2231 | ` */` |
|   451546 | 2232 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 2233 | `{` |
|        - | 2234 | `	ph7_class_method *pMeth;` |
|        - | 2235 | `	SyHashEntry *pEntry;` |
|        - | 2236 | `	sxu32 nAbstract;` |
|        - | 2237 | `	SyBlob sMsg;` |
|        - | 2238 | `	sxi32 rc;` |
|        - | 2239 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   451551 | 2240 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|    19439 | 2241 | `		return SXRET_OK;` |
|        - | 2242 | `	}` |
|        - | 2243 | `	/* Count abstract methods */` |
|   432117 | 2244 | `	nAbstract = 0;` |
|   432117 | 2245 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  6457249 | 2246 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  5809081 | 2247 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  5809081 | 2248 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       27 | 2249 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|        7 | 2250 | `				continue; /* hook requirement met by a plain property (php) */` |
|        - | 2251 | `			}` |
|       20 | 2252 | `			nAbstract++;` |
|        8 | 2253 | `		}` |
|        5 | 2254 | `	}` |
|   432117 | 2255 | `	if( nAbstract == 0 ){` |
|   432103 | 2256 | `		return SXRET_OK;` |
|        - | 2257 | `	}` |
|        - | 2258 | `	/* Build the error message listing all abstract methods with origins */` |
|       18 | 2259 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       18 | 2260 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|        - | 2261 | `		"be declared abstract or implement the remaining method%s (",` |
|        7 | 2262 | `		&pClass->sName,nAbstract,` |
|        7 | 2263 | `		(nAbstract > 1 ? "s" : ""),` |
|        7 | 2264 | `		(nAbstract > 1 ? "s" : ""));` |
|        - | 2265 | `	/* Second pass: list methods with origins */` |
|        - | 2266 | `	{` |
|       18 | 2267 | `		sxu32 nListed = 0;` |
|       18 | 2268 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|       36 | 2269 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|       22 | 2270 | `			ph7_class *pOrigin = 0;` |
|        - | 2271 | `			SyString *pMName;` |
|       22 | 2272 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|       22 | 2273 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        3 | 2274 | `				continue;` |
|        - | 2275 | `			}` |
|       20 | 2276 | `			pMName = &pMeth->sFunc.sName;` |
|       20 | 2277 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|      ! 0 | 2278 | `				continue; /* hook requirement met by a plain property (php) */` |
|        - | 2279 | `			}` |
|       20 | 2280 | `			if( nListed > 0 ){` |
|        3 | 2281 | `				SyBlobAppend(&sMsg,", ",2);` |
|        1 | 2282 | `			}` |
|        - | 2283 | `			/* Find the origin of this abstract method.` |
|        - | 2284 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|        - | 2285 | `			 * inheritance chains) take precedence for interface-declared` |
|        - | 2286 | `			 * methods. Abstract class methods only win when the class` |
|        - | 2287 | `			 * itself declared the abstract method (not inherited from` |
|        - | 2288 | `			 * an interface). Trait methods are adopted into the using` |
|        - | 2289 | `			 * class's namespace.` |
|        - | 2290 | `			 */` |
|        - | 2291 | `			{` |
|        - | 2292 | `				ph7_class **apIface;` |
|        - | 2293 | `				ph7_class **apTrait;` |
|        - | 2294 | `				ph7_class *pWalk;` |
|        - | 2295 | `				sxu32 i;` |
|        - | 2296 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|        - | 2297 | `				 * (one that was written in the class body, not inherited from an` |
|        - | 2298 | `				 * interface). PHP attributes origin to the declaring class.` |
|        - | 2299 | `				 */` |
|       20 | 2300 | `				if( pClass->pBase ){` |
|       11 | 2301 | `					pWalk = pClass->pBase;` |
|       19 | 2302 | `					while( pWalk ){` |
|        - | 2303 | `						ph7_class_method *pParentMeth;` |
|       13 | 2304 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|       13 | 2305 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        - | 2306 | `							/* Exclude methods that came from an interface anywhere` |
|        - | 2307 | `							 * in this class's ancestor chain.` |
|        - | 2308 | `							 */` |
|       13 | 2309 | `							int fromIface = 0;` |
|       13 | 2310 | `							ph7_class *pAnc = pWalk;` |
|       17 | 2311 | `							while( pAnc ){` |
|        - | 2312 | `								ph7_class **apPI;` |
|        - | 2313 | `								sxu32 j;` |
|       15 | 2314 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|       15 | 2315 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       10 | 2316 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       10 | 2317 | `										fromIface = 1;` |
|       10 | 2318 | `										break;` |
|        - | 2319 | `									}` |
|      ! 0 | 2320 | `								}` |
|       15 | 2321 | `								if( fromIface ) break;` |
|        6 | 2322 | `								pAnc = pAnc->pBase;` |
|        2 | 2323 | `							}` |
|       13 | 2324 | `							if( !fromIface ){` |
|        3 | 2325 | `								pOrigin = pWalk;` |
|        3 | 2326 | `								break;` |
|        - | 2327 | `							}` |
|        4 | 2328 | `						}` |
|       10 | 2329 | `						pWalk = pWalk->pBase;` |
|        2 | 2330 | `					}` |
|        4 | 2331 | `				}` |
|        - | 2332 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|        - | 2333 | `				 * each interface's own parent chain for the deepest origin.` |
|        - | 2334 | `				 */` |
|       20 | 2335 | `				if( !pOrigin ){` |
|       18 | 2336 | `					pWalk = pClass;` |
|       40 | 2337 | `					while( pWalk && !pOrigin ){` |
|       26 | 2338 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|       26 | 2339 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|       16 | 2340 | `							ph7_class *pIface = apIface[i];` |
|       16 | 2341 | `							ph7_class *pDeepest = 0;` |
|       28 | 2342 | `							while( pIface ){` |
|       16 | 2343 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|       16 | 2344 | `									pDeepest = pIface;` |
|        6 | 2345 | `								}` |
|       16 | 2346 | `								pIface = pIface->pBase;` |
|        4 | 2347 | `							}` |
|       16 | 2348 | `							if( pDeepest ){` |
|       16 | 2349 | `								pOrigin = pDeepest;` |
|       16 | 2350 | `								break;` |
|        - | 2351 | `							}` |
|      ! 0 | 2352 | `						}` |
|       26 | 2353 | `						pWalk = pWalk->pBase;` |
|        4 | 2354 | `					}` |
|        7 | 2355 | `				}` |
|        - | 2356 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|       20 | 2357 | `				if( !pOrigin ){` |
|        3 | 2358 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        3 | 2359 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|        3 | 2360 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|        3 | 2361 | `							pOrigin = pClass;` |
|        3 | 2362 | `							break;` |
|        - | 2363 | `						}` |
|      ! 0 | 2364 | `					}` |
|        1 | 2365 | `				}` |
|        - | 2366 | `			}` |
|       20 | 2367 | `			if( pOrigin ){` |
|       20 | 2368 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|       12 | 2369 | `			}else{` |
|        - | 2370 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|      ! 0 | 2371 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|        - | 2372 | `			}` |
|       20 | 2373 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|       20 | 2374 | `			nListed++;` |
|        4 | 2375 | `		}` |
|        - | 2376 | `	}` |
|       18 | 2377 | `	SyBlobAppend(&sMsg,")",1);` |
|       25 | 2378 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|       14 | 2379 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|       18 | 2380 | `	SyBlobRelease(&sMsg);` |
|       18 | 2381 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2382 | `		return SXERR_ABORT;` |
|        - | 2383 | `	}` |
|       18 | 2384 | `	return SXRET_OK;` |
|   225778 | 2385 | `}` |
|        - | 2386 | `/*` |
|        - | 2387 | ` * Parse a class/interface name reference from the current token stream.` |
|        - | 2388 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|        - | 2389 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|        - | 2390 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|        - | 2391 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|        - | 2392 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|        - | 2393 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|        - | 2394 | ` */` |
|   513966 | 2395 | `PH7_PRIVATE sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|        5 | 2396 | `{` |
|   513971 | 2397 | `	int isAbsolute = 0;` |
|   513971 | 2398 | `	SyToken *pStart = pGen->pIn;` |
|        - | 2399 | `	SyBlob sName;` |
|   513971 | 2400 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|     4531 | 2401 | `		isAbsolute = 1;` |
|     4531 | 2402 | `		pGen->pIn++;` |
|     2263 | 2403 | `	}` |
|   513971 | 2404 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        8 | 2405 | `		pGen->pIn = pStart;` |
|        8 | 2406 | `		return SXERR_INVALID;` |
|        - | 2407 | `	}` |
|   513965 | 2408 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   513965 | 2409 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   513965 | 2410 | `	pGen->pIn++;` |
|   770979 | 2411 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   257024 | 2412 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       28 | 2413 | `		SyBlobAppend(&sName,"\\",1);` |
|       28 | 2414 | `		pGen->pIn++;` |
|       28 | 2415 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       28 | 2416 | `		pGen->pIn++;` |
|        2 | 2417 | `	}` |
|   513965 | 2418 | `	if( isAbsolute ){` |
|     4529 | 2419 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     2267 | 2420 | `	}else{` |
|        - | 2421 | `		SyString sRaw;` |
|   509441 | 2422 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   509441 | 2423 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|        - | 2424 | `	}` |
|   513965 | 2425 | `	SyBlobRelease(&sName);` |
|   513965 | 2426 | `	return SXRET_OK;` |
|   256988 | 2427 | `}` |
|        - | 2428 | `/*` |
|        - | 2429 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|        - | 2430 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|        - | 2431 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|        - | 2432 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|        - | 2433 | ` * either direction cannot run unbounded.` |
|        - | 2434 | ` */` |
|        - | 2435 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|   197888 | 2436 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|        5 | 2437 | `{` |
|        - | 2438 | `	ph7_class **apParent;` |
|        - | 2439 | `	sxu32 n;` |
|   523729 | 2440 | `	while( pInterface ){` |
|   333603 | 2441 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|      ! 0 | 2442 | `			return FALSE;` |
|        - | 2443 | `		}` |
|   372383 | 2444 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    77560 | 2445 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|     7767 | 2446 | `			return TRUE;` |
|        - | 2447 | `		}` |
|   325841 | 2448 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|   325843 | 2449 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|        3 | 2450 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|      ! 0 | 2451 | `				return TRUE;` |
|        - | 2452 | `			}` |
|        2 | 2453 | `		}` |
|   325841 | 2454 | `		pInterface = pInterface->pBase;` |
|   325841 | 2455 | `		iDepth++;` |
|        5 | 2456 | `	}` |
|   190131 | 2457 | `	return FALSE;` |
|    98949 | 2458 | `}` |
|   197886 | 2459 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|        5 | 2460 | `{` |
|   197891 | 2461 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|        5 | 2462 | `}` |
|        - | 2463 | `/*` |
|        - | 2464 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|        - | 2465 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|        - | 2466 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|        - | 2467 | ` */` |
|     7762 | 2468 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|        5 | 2469 | `{` |
|     7771 | 2470 | `	while( pBase ){` |
|       10 | 2471 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|        2 | 2472 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|        3 | 2473 | `			return TRUE;` |
|        - | 2474 | `		}` |
|       10 | 2475 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|        6 | 2476 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|        3 | 2477 | `			return TRUE;` |
|        - | 2478 | `		}` |
|        5 | 2479 | `		pBase = pBase->pBase;` |
|        1 | 2480 | `	}` |
|     7763 | 2481 | `	return FALSE;` |
|     3886 | 2482 | `}` |
|        - | 2483 | `/*` |
|        - | 2484 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|        - | 2485 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|        - | 2486 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|        - | 2487 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|        - | 2488 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|        - | 2489 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|        - | 2490 | ` * pClass->aEnumCases for cases().` |
|        - | 2491 | ` */` |
|     7806 | 2492 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 2493 | `{` |
|     7811 | 2494 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2495 | `	SySet *pInstrContainer;` |
|        - | 2496 | `	ph7_class_attr *pCase;` |
|        - | 2497 | `	SyString *pName;` |
|        - | 2498 | `	sxi32 rc;` |
|     7811 | 2499 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|     7811 | 2500 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2501 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 2502 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|      ! 0 | 2503 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2504 | `			return SXERR_ABORT;` |
|        - | 2505 | `		}` |
|      ! 0 | 2506 | `		goto Synchronize;` |
|        - | 2507 | `	}` |
|     7811 | 2508 | `	pName = &pGen->pIn->sData;` |
|        - | 2509 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|     7811 | 2510 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      ! 0 | 2511 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2512 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|      ! 0 | 2513 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2514 | `			return SXERR_ABORT;` |
|        - | 2515 | `		}` |
|      ! 0 | 2516 | `		goto Synchronize;` |
|        - | 2517 | `	}` |
|     7811 | 2518 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 2519 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|     7811 | 2520 | `	if( pCase == 0 ){` |
|      ! 0 | 2521 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2522 | `		return SXERR_ABORT;` |
|        - | 2523 | `	}` |
|     7811 | 2524 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|     7811 | 2525 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 2526 | `		return SXERR_ABORT;` |
|        - | 2527 | `	}` |
|     7811 | 2528 | `	pGen->pIn++; /* Jump the case name */` |
|     7811 | 2529 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|     7797 | 2530 | `		if( pClass->nEnumBacking == 0 ){` |
|        8 | 2531 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        2 | 2532 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|        6 | 2533 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2534 | `				return SXERR_ABORT;` |
|        - | 2535 | `			}` |
|        6 | 2536 | `			goto Synchronize;` |
|        - | 2537 | `		}` |
|     7793 | 2538 | `		pGen->pIn++; /* Jump the equal sign */` |
|        - | 2539 | `		/* Compile the backing value expression into the case's own container` |
|        - | 2540 | `		 * (same technique as class constants). */` |
|     7793 | 2541 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     7793 | 2542 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|     7793 | 2543 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     7793 | 2544 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2545 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 2546 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|      ! 0 | 2547 | `		}` |
|     7793 | 2548 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|     7793 | 2549 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     7793 | 2550 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2551 | `			return SXERR_ABORT;` |
|        - | 2552 | `		}` |
|     3899 | 2553 | `	}else{` |
|       17 | 2554 | `		if( pClass->nEnumBacking != 0 ){` |
|      ! 0 | 2555 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 2556 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|      ! 0 | 2557 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2558 | `				return SXERR_ABORT;` |
|        - | 2559 | `			}` |
|      ! 0 | 2560 | `			goto Synchronize;` |
|        - | 2561 | `		}` |
|        - | 2562 | `	}` |
|     7807 | 2563 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|     7807 | 2564 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2565 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2566 | `		return SXERR_ABORT;` |
|        - | 2567 | `	}` |
|     7807 | 2568 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|     7807 | 2569 | `	return SXRET_OK;` |
|        2 | 2570 | `Synchronize:` |
|        - | 2571 | `	/* Synchronize with the first semi-colon */` |
|       14 | 2572 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       10 | 2573 | `		pGen->pIn++;` |
|        2 | 2574 | `	}` |
|        6 | 2575 | `	return SXERR_CORRUPT;` |
|     3908 | 2576 | `}` |
|        - | 2577 | `/*` |
|        - | 2578 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|        - | 2579 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|        - | 2580 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|        - | 2581 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|        - | 2582 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|        - | 2583 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|        - | 2584 | ` * pointers into it (see the constructor-promotion precedent above).` |
|        - | 2585 | ` */` |
|     3908 | 2586 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 2587 | `{` |
|        - | 2588 | `	SyToken *pSaveIn,*pSaveEnd;` |
|        - | 2589 | `	const char *zBack;` |
|        - | 2590 | `	SySet sToken;` |
|        - | 2591 | `	char *zSrc;` |
|        - | 2592 | `	sxu32 nSrc,nMax;` |
|     3913 | 2593 | `	sxi32 rc = SXRET_OK;` |
|     3913 | 2594 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|     3908 | 2595 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|     3913 | 2596 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|     3913 | 2597 | `	if( zSrc == 0 ){` |
|      ! 0 | 2598 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2599 | `		return SXERR_ABORT;` |
|        - | 2600 | `	}` |
|     3913 | 2601 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|     3913 | 2602 | `	if( pClass->nEnumBacking != 0 ){` |
|     5840 | 2603 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        - | 2604 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|        - | 2605 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|        - | 2606 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|     1945 | 2607 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|     1950 | 2608 | `	}else{` |
|       30 | 2609 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        9 | 2610 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|        - | 2611 | `	}` |
|     3913 | 2612 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|     3913 | 2613 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|     3913 | 2614 | `	pSaveIn = pGen->pIn;` |
|     3913 | 2615 | `	pSaveEnd = pGen->pEnd;` |
|     3913 | 2616 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|     3913 | 2617 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|    15601 | 2618 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|    11693 | 2619 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|        5 | 2620 | `	}` |
|     3913 | 2621 | `	pGen->pIn = pSaveIn;` |
|     3913 | 2622 | `	pGen->pEnd = pSaveEnd;` |
|     3913 | 2623 | `	SySetRelease(&sToken);` |
|     3913 | 2624 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|     1959 | 2625 | `}` |
|        - | 2626 | `/*` |
|        - | 2627 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|        - | 2628 | ` * __call/__callStatic/__invoke stay allowed).` |
|        - | 2629 | ` */` |
|        - | 2630 | `static const char *azEnumBannedMagic[] = {` |
|        - | 2631 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|        - | 2632 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|        - | 2633 | `};` |
|        - | 2634 | `/*` |
|        - | 2635 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|        - | 2636 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|        - | 2637 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|        - | 2638 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|        - | 2639 | ` * and before the class is installed.` |
|        - | 2640 | ` */` |
|     3908 | 2641 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|        5 | 2642 | `{` |
|        - | 2643 | `	SyHashEntry *pEntry;` |
|        - | 2644 | `	sxi32 rc;` |
|        - | 2645 | `	sxu32 n;` |
|        - | 2646 | `	/* php: "Enum %s cannot include properties" */` |
|     3913 | 2647 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    11721 | 2648 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     7815 | 2649 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     7815 | 2650 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        3 | 2651 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|        1 | 2652 | `				"Enum %z cannot include properties",&pClass->sName);` |
|        3 | 2653 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2654 | `				return SXERR_ABORT;` |
|        - | 2655 | `			}` |
|        3 | 2656 | `			break;` |
|        - | 2657 | `		}` |
|        5 | 2658 | `	}` |
|        - | 2659 | `	/* php: "Enum %s cannot include magic method %s" */` |
|    54717 | 2660 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|    76206 | 2661 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|    50809 | 2662 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|      ! 0 | 2663 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 2664 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|      ! 0 | 2665 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2666 | `				return SXERR_ABORT;` |
|        - | 2667 | `			}` |
|      ! 0 | 2668 | `		}` |
|    25407 | 2669 | `	}` |
|        - | 2670 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|        - | 2671 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|        - | 2672 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|        - | 2673 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|        - | 2674 | `	{` |
|        - | 2675 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|        - | 2676 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|        - | 2677 | `		ph7_class_attr *pAttr;` |
|     3913 | 2678 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 2679 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|     3913 | 2680 | `		if( pAttr == 0 ){` |
|      ! 0 | 2681 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2682 | `			return SXERR_ABORT;` |
|        - | 2683 | `		}` |
|     3913 | 2684 | `		pAttr->nType = MEMOBJ_STRING;` |
|     3913 | 2685 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|     3913 | 2686 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|     3913 | 2687 | `		if( pClass->nEnumBacking != 0 ){` |
|     3895 | 2688 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 2689 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|     3895 | 2690 | `			if( pAttr == 0 ){` |
|      ! 0 | 2691 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2692 | `				return SXERR_ABORT;` |
|        - | 2693 | `			}` |
|     3895 | 2694 | `			pAttr->nType = pClass->nEnumBacking;` |
|     3895 | 2695 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        7 | 2696 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|        4 | 2697 | `			}else{` |
|     3889 | 2698 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|        - | 2699 | `			}` |
|     3895 | 2700 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|     1945 | 2701 | `		}` |
|        - | 2702 | `	}` |
|     3913 | 2703 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|     1959 | 2704 | `}` |
|        - | 2705 | `/*` |
|        - | 2706 | ` * Compile a class declaration, named or anonymous.` |
|        - | 2707 | ` *` |
|        - | 2708 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|        - | 2709 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|        - | 2710 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|        - | 2711 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|        - | 2712 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|        - | 2713 | ` * implements, body, install) is shared by both paths.` |
|        - | 2714 | ` */` |
|   451596 | 2715 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|        - | 2716 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|        5 | 2717 | `{` |
|   451601 | 2718 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2719 | `	ph7_class *pClass,*pBase;` |
|        - | 2720 | `	SyToken *pEnd,*pTmp;` |
|        - | 2721 | `	sxi32 iProtection;` |
|        - | 2722 | `	SySet aInterfaces;` |
|        - | 2723 | `	SySet aUseEntries;` |
|        - | 2724 | `	sxi32 iAttrflags;` |
|        - | 2725 | `	SyString *pName;` |
|        - | 2726 | `	sxi32 nKwrd;` |
|        - | 2727 | `	sxi32 rc;` |
|        - | 2728 | `	/* Jump the 'class' keyword */` |
|   451601 | 2729 | `	pGen->pIn++;` |
|   451601 | 2730 | `	if( pAnonName ){` |
|        - | 2731 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|        - | 2732 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|        - | 2733 | `		 * then use the synthesized name. */` |
|       34 | 2734 | `		*ppArgStart = *ppArgEnd = 0;` |
|       34 | 2735 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        7 | 2736 | `			pGen->pIn++; /* Jump '(' */` |
|        7 | 2737 | `			*ppArgStart = pGen->pIn;` |
|       10 | 2738 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|        3 | 2739 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|        7 | 2740 | `			pGen->pIn = *ppArgEnd;` |
|        7 | 2741 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|        3 | 2742 | `		}` |
|       34 | 2743 | `		pName = pAnonName;` |
|       34 | 2744 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|       19 | 2745 | `	}else{` |
|   451571 | 2746 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - | 2747 | `			/* Syntax error */` |
|      ! 0 | 2748 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|      ! 0 | 2749 | `			if( rc == SXERR_ABORT ){` |
|        - | 2750 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 2751 | `				return SXERR_ABORT;` |
|        - | 2752 | `			}` |
|        - | 2753 | `			/* Synchronize with the first semi-colon or curly braces */` |
|      ! 0 | 2754 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|      ! 0 | 2755 | `				pGen->pIn++;` |
|      ! 0 | 2756 | `			}` |
|      ! 0 | 2757 | `			return SXRET_OK;` |
|        - | 2758 | `		}` |
|        - | 2759 | `		/* Extract class name */` |
|   451571 | 2760 | `		pName = &pGen->pIn->sData;` |
|        - | 2761 | `		/* Advance the stream cursor */` |
|   451571 | 2762 | `		pGen->pIn++;` |
|        - | 2763 | `		/* Build FQN and obtain a raw class */ {` |
|        - | 2764 | `			SyBlob sFQN;` |
|        - | 2765 | `			SyString sFQNStr;` |
|   451571 | 2766 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   451571 | 2767 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|   451571 | 2768 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   451571 | 2769 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   451571 | 2770 | `			SyBlobRelease(&sFQN);` |
|        - | 2771 | `		}` |
|        - | 2772 | `	}` |
|   451601 | 2773 | `	if( pClass == 0 ){` |
|      ! 0 | 2774 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2775 | `		return SXERR_ABORT;` |
|        - | 2776 | `	}` |
|   451596 | 2777 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|     3917 | 2778 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|        - | 2779 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|     3897 | 2780 | `		pGen->pIn++; /* Jump ':' */` |
|     3892 | 2781 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     3897 | 2782 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|        7 | 2783 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|        7 | 2784 | `			pGen->pIn++;` |
|     3890 | 2785 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     3891 | 2786 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|     3889 | 2787 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|     3889 | 2788 | `			pGen->pIn++;` |
|     1947 | 2789 | `		}else{` |
|        3 | 2790 | `			SyToken *pTok = pGen->pIn;` |
|        3 | 2791 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|        4 | 2792 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|        1 | 2793 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|        3 | 2794 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2795 | `				return SXERR_ABORT;` |
|        - | 2796 | `			}` |
|        3 | 2797 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|        3 | 2798 | `				pGen->pIn++; /* Skip the bogus type token */` |
|        1 | 2799 | `			}` |
|        - | 2800 | `		}` |
|     1946 | 2801 | `	}` |
|   451601 | 2802 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|   451601 | 2803 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 2804 | `		return SXERR_ABORT;` |
|        - | 2805 | `	}` |
|        - | 2806 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   451601 | 2807 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   451601 | 2808 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|        - | 2809 | `	/* Assume a standalone class */` |
|   451601 | 2810 | `	pBase = 0;` |
|   451601 | 2811 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   368715 | 2812 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   368715 | 2813 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|        - | 2814 | `			SyBlob sResolved;` |
|        - | 2815 | `			SyString sBaseName;` |
|        - | 2816 | `			sxu32 nRefLine;` |
|   248379 | 2817 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|        - | 2818 | `				/* php parse-fatals here (enums have no inheritance) */` |
|      ! 0 | 2819 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 2820 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|      ! 0 | 2821 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2822 | `					return SXERR_ABORT;` |
|        - | 2823 | `				}` |
|      ! 0 | 2824 | `			}` |
|   248379 | 2825 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   248379 | 2826 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   248379 | 2827 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   248379 | 2828 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        3 | 2829 | `				SyBlobRelease(&sResolved);` |
|        4 | 2830 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2831 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|        1 | 2832 | `					pName);` |
|        3 | 2833 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|        3 | 2834 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2835 | `					return SXERR_ABORT;` |
|        - | 2836 | `				}` |
|        3 | 2837 | `				return SXRET_OK;` |
|        - | 2838 | `			}` |
|   372563 | 2839 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   248372 | 2840 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   248377 | 2841 | `			SyStringInitFromBuf(&sBaseName,` |
|        - | 2842 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 2843 | `			/* Interfaces are not allowed */` |
|   248377 | 2844 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|      ! 0 | 2845 | `				pBase = pBase->pNextName;` |
|      ! 0 | 2846 | `			}` |
|   248377 | 2847 | `			if( pBase == 0 ){` |
|      ! 0 | 2848 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 2849 | `					"Nonexistent base class '%z'",&sBaseName);` |
|      ! 0 | 2850 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2851 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 2852 | `					return SXERR_ABORT;` |
|        - | 2853 | `				}` |
|      ! 0 | 2854 | `			}else{` |
|   248377 | 2855 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|        4 | 2856 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 2857 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|        3 | 2858 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2859 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 2860 | `						return SXERR_ABORT;` |
|        - | 2861 | `					}` |
|        3 | 2862 | `					pBase = 0; /* Never inherit from an enum */` |
|   248376 | 2863 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|      ! 0 | 2864 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 2865 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|      ! 0 | 2866 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2867 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 2868 | `						return SXERR_ABORT;` |
|        - | 2869 | `					}` |
|      ! 0 | 2870 | `				}` |
|        - | 2871 | `			}` |
|   248377 | 2872 | `			SyBlobRelease(&sResolved);` |
|   248377 | 2873 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 | 2874 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|      ! 0 | 2875 | `			}` |
|   124186 | 2876 | `		}` |
|   368713 | 2877 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|        - | 2878 | `			ph7_class *pInterface;` |
|        - | 2879 | `			/* Interface implementation */` |
|   135861 | 2880 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   129958 | 2881 | `			for(;;){` |
|        - | 2882 | `				SyBlob sResolved;` |
|        - | 2883 | `				SyString sIntName;` |
|        - | 2884 | `				sxu32 nRefLine;` |
|   197891 | 2885 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   197891 | 2886 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   197891 | 2887 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 2888 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 2889 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2890 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|      ! 0 | 2891 | `						pName);` |
|      ! 0 | 2892 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2893 | `						return SXERR_ABORT;` |
|        - | 2894 | `					}` |
|      ! 0 | 2895 | `					break;` |
|        - | 2896 | `				}` |
|   395777 | 2897 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|   197886 | 2898 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   197891 | 2899 | `				SyStringInitFromBuf(&sIntName,` |
|        - | 2900 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 2901 | `				/* Only interfaces are allowed */` |
|   197891 | 2902 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 2903 | `					pInterface = pInterface->pNextName;` |
|      ! 0 | 2904 | `				}` |
|   197891 | 2905 | `				if( pInterface == 0 ){` |
|      ! 0 | 2906 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 2907 | `						"Nonexistent base interface '%z'",&sIntName);` |
|      ! 0 | 2908 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2909 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 2910 | `						return SXERR_ABORT;` |
|        - | 2911 | `					}` |
|      ! 0 | 2912 | `				}else{` |
|        - | 2913 | `					/* Reject user classes that try to implement Throwable` |
|        - | 2914 | `					 * directly (or via an interface that extends Throwable)` |
|        - | 2915 | `					 * unless they already extend Exception or Error.` |
|        - | 2916 | `					 * Exception and Error themselves are compiled from the` |
|        - | 2917 | `					 * built-in library and are exempt by FQN — a namespaced` |
|        - | 2918 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|   197891 | 2919 | `					SyString *pFqn = &pClass->sName;` |
|   197891 | 2920 | `					int bIsExceptionOrError =` |
|   102823 | 2921 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   298771 | 2922 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|   195955 | 2923 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|     3890 | 2924 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   201767 | 2925 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    11646 | 2926 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|     3879 | 2927 | `						!bIsExceptionOrError ){` |
|       12 | 2928 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 2929 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|        3 | 2930 | `							&pClass->sName);` |
|        9 | 2931 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 2932 | `							SyBlobRelease(&sResolved);` |
|      ! 0 | 2933 | `							return SXERR_ABORT;` |
|        - | 2934 | `						}` |
|        - | 2935 | `						/* Skip registration so the follow-up abstract-method` |
|        - | 2936 | `						 * check does not produce a duplicate fatal. */` |
|        6 | 2937 | `					}else{` |
|   197885 | 2938 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|        - | 2939 | `					}` |
|        - | 2940 | `				}` |
|   197891 | 2941 | `				SyBlobRelease(&sResolved);` |
|   197891 | 2942 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    67933 | 2943 | `					break;` |
|        - | 2944 | `				}` |
|    62035 | 2945 | `				pGen->pIn++;/* Jump the comma */` |
|        5 | 2946 | `			}` |
|    67928 | 2947 | `		}` |
|   184354 | 2948 | `	}` |
|   451599 | 2949 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - | 2950 | `		/* Syntax error */` |
|      ! 0 | 2951 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|      ! 0 | 2952 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 2953 | `		if( rc == SXERR_ABORT ){` |
|        - | 2954 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 2955 | `			return SXERR_ABORT;` |
|        - | 2956 | `		}` |
|      ! 0 | 2957 | `		return SXRET_OK;` |
|        - | 2958 | `	}` |
|   451599 | 2959 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   451599 | 2960 | `	pEnd = 0; /* cc warning */` |
|        - | 2961 | `	/* Delimit the class body */` |
|   451599 | 2962 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   451599 | 2963 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 2964 | `		/* Syntax error */` |
|      ! 0 | 2965 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|      ! 0 | 2966 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 2967 | `		if( rc == SXERR_ABORT ){` |
|        - | 2968 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 2969 | `			return SXERR_ABORT;` |
|        - | 2970 | `		}` |
|      ! 0 | 2971 | `		return SXRET_OK;` |
|        - | 2972 | `	}` |
|        - | 2973 | `	/* The delimiter token is the class body's closing brace */` |
|   451599 | 2974 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 2975 | `	/* Swap token stream */` |
|   451599 | 2976 | `	pTmp = pGen->pEnd;` |
|   451599 | 2977 | `	pGen->pEnd = pEnd;` |
|        - | 2978 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|   451599 | 2979 | `	pClass->iFlags \|= iFlags;` |
|        - | 2980 | `	/* Start the parse process */` |
|  1691834 | 2981 | `	for(;;){` |
|        - | 2982 | `		/* Jump leading/trailing semi-colons */` |
|  4859051 | 2983 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   885023 | 2984 | `			pGen->pIn++;` |
|        5 | 2985 | `		}` |
|  3974033 | 2986 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 2987 | `			/* End of class body */` |
|   451557 | 2988 | `			break;` |
|        - | 2989 | `		}` |
|        - | 2990 | `		/* Bind a directly-preceding docblock to this member */` |
|  3522481 | 2991 | `		GenStateSetPendingDoc(&(*pGen));` |
|  3522476 | 2992 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  1761243 | 2993 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|      ! 0 | 2994 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 2995 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 | 2996 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 2997 | `			if( rc == SXERR_ABORT ){` |
|        - | 2998 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 2999 | `				return SXERR_ABORT;` |
|        - | 3000 | `			}` |
|      ! 0 | 3001 | `			goto done;` |
|        - | 3002 | `		}` |
|        - | 3003 | `		/* Assume public visibility */` |
|  3522481 | 3004 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  3522481 | 3005 | `		iAttrflags = 0;` |
|        - | 3006 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|        - | 3007 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|        - | 3008 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|        - | 3009 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  3522481 | 3010 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 3011 | `			int bMod = 0;` |
|      ! 0 | 3012 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 | 3013 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        - | 3014 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|        - | 3015 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|        - | 3016 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|        - | 3017 | `			 * that the generic keyword dispatch would misread as a method. */` |
|      ! 0 | 3018 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      ! 0 | 3019 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 | 3020 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|      ! 0 | 3021 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|      ! 0 | 3022 | `			}` |
|      ! 0 | 3023 | `			if( !bMod ){` |
|      ! 0 | 3024 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 3025 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 3026 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3027 | `						return SXERR_ABORT;` |
|        - | 3028 | `					}` |
|      ! 0 | 3029 | `					goto done;` |
|        - | 3030 | `				}` |
|      ! 0 | 3031 | `				continue;` |
|        - | 3032 | `			}` |
|      ! 0 | 3033 | `		}` |
|  3522481 | 3034 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 3035 | `			/* Extract the current keyword */` |
|  3522481 | 3036 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  3522481 | 3037 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|        - | 3038 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|     7811 | 3039 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|     7811 | 3040 | `				if( rc != SXRET_OK ){` |
|        6 | 3041 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3042 | `						return SXERR_ABORT;` |
|        - | 3043 | `					}` |
|        6 | 3044 | `					goto done;` |
|        - | 3045 | `				}` |
|     7807 | 3046 | `				continue;` |
|        - | 3047 | `			}` |
|  3514675 | 3048 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 3049 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|        - | 3050 | `				TraitUseEntry sUse;` |
|    15599 | 3051 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    15599 | 3052 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|    15599 | 3053 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|     7805 | 3054 | `				for(;;){` |
|        - | 3055 | `					ph7_class *pTrait;` |
|        - | 3056 | `					SyBlob sResolved;` |
|        - | 3057 | `					SyString sTraitName;` |
|    15607 | 3058 | `					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|        - | 3059 | `					/* A trait name is a full class reference: it may be qualified or` |
|        - | 3060 | ``					 * fully-qualified (`use Foo\Bar\T;`, `use \Foo\Bar\T;`) — the generated`` |
|        - | 3061 | `					 * PHPUnit mocks name their traits absolutely. Parse it with the shared` |
|        - | 3062 | `					 * class-reference reader (handles the leading '\', every '\'-segment and` |
|        - | 3063 | `					 * namespace/import resolution) instead of a single-identifier read, which` |
|        - | 3064 | `					 * choked on the first '\'. */` |
|    15607 | 3065 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    15607 | 3066 | `					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 3067 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 3068 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|      ! 0 | 3069 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|      ! 0 | 3070 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3071 | `							return SXERR_ABORT;` |
|        - | 3072 | `						}` |
|      ! 0 | 3073 | `						break;` |
|        - | 3074 | `					}` |
|    31209 | 3075 | `					pTrait = PH7_VmExtractClass(pGen->pVm,` |
|    15602 | 3076 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    15607 | 3077 | `					SyStringInitFromBuf(&sTraitName,` |
|        - | 3078 | `						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 3079 | `					/* Only traits are allowed */` |
|    15607 | 3080 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 3081 | `						pTrait = pTrait->pNextName;` |
|      ! 0 | 3082 | `					}` |
|    15607 | 3083 | `					if( pTrait == 0 ){` |
|      ! 0 | 3084 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|        - | 3085 | `							"'%z' is not a trait",&sTraitName);` |
|      ! 0 | 3086 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3087 | `							SyBlobRelease(&sResolved);` |
|      ! 0 | 3088 | `							return SXERR_ABORT;` |
|        - | 3089 | `						}` |
|      ! 0 | 3090 | `					}else{` |
|    15607 | 3091 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|        - | 3092 | `					}` |
|    15607 | 3093 | `					SyBlobRelease(&sResolved);` |
|        - | 3094 | `					/* GenStateParseClassReference already advanced past the whole name —` |
|        - | 3095 | `					 * continue only across a comma-separated trait list. */` |
|    15607 | 3096 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     7802 | 3097 | `						break;` |
|        - | 3098 | `					}` |
|       10 | 3099 | `					pGen->pIn++; /* Jump the comma */` |
|        2 | 3100 | `				}` |
|        - | 3101 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|    15599 | 3102 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - | 3103 | `					SyToken *pBlock;` |
|       13 | 3104 | `					pGen->pIn++; /* Jump '{' */` |
|       13 | 3105 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       13 | 3106 | `					sUse.pResolvStart = pGen->pIn;` |
|       13 | 3107 | `					sUse.pResolvEnd = pBlock;` |
|       13 | 3108 | `					if( pBlock < pGen->pEnd ){` |
|       13 | 3109 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|        8 | 3110 | `					}else{` |
|      ! 0 | 3111 | `						pGen->pIn = pGen->pEnd;` |
|        - | 3112 | `					}` |
|        5 | 3113 | `				}` |
|    15599 | 3114 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|        - | 3115 | `				/* The semicolon will be consumed by the outer loop */` |
|    15599 | 3116 | `				continue;` |
|        - | 3117 | `			}` |
|  3499081 | 3118 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        - | 3119 | `				int nSetTok;` |
|  2955925 | 3120 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|  2955925 | 3121 | `				if( nSetVis ){` |
|        - | 3122 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|        - | 3123 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|        3 | 3124 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        3 | 3125 | `					pGen->pIn += nSetTok;` |
|        2 | 3126 | `				}else{` |
|  2955923 | 3127 | `					iProtection = nKwrd;` |
|  2955923 | 3128 | `					pGen->pIn++; /* Jump the visibility token */` |
|        - | 3129 | `					/* Optional asymmetric set-visibility after the read` |
|        - | 3130 | ``					 * visibility: `public private(set) int $x`. */`` |
|  2955923 | 3131 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|  2955923 | 3132 | `					if( nSetVis ){` |
|        9 | 3133 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        9 | 3134 | `						pGen->pIn += nSetTok;` |
|        4 | 3135 | `					}` |
|        - | 3136 | `				}` |
|        - | 3137 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|        - | 3138 | ``				 * `public private(set) readonly int $x`. */`` |
|  2955925 | 3139 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       28 | 3140 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       28 | 3141 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       12 | 3142 | `				}` |
|  2955920 | 3143 | `				if( pGen->pIn >= pGen->pEnd` |
|  2955925 | 3144 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 3145 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3146 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 | 3147 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 3148 | `					if( rc == SXERR_ABORT ){` |
|        - | 3149 | `						/* Error count limit reached,abort immediately */` |
|      ! 0 | 3150 | `						return SXERR_ABORT;` |
|        - | 3151 | `					}` |
|      ! 0 | 3152 | `					goto done;` |
|        - | 3153 | `				}` |
|  2955925 | 3154 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 3155 | `					/* Attribute declaration (untyped) */` |
|   523947 | 3156 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   523947 | 3157 | `					if( rc != SXRET_OK ){` |
|       11 | 3158 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3159 | `							return SXERR_ABORT;` |
|        - | 3160 | `						}` |
|       11 | 3161 | `						goto done;` |
|        - | 3162 | `					}` |
|   531854 | 3163 | `					continue;` |
|        - | 3164 | `				}` |
|  2431983 | 3165 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 3166 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|    15841 | 3167 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    15841 | 3168 | `					if( rc != SXRET_OK ){` |
|        9 | 3169 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3170 | `							return SXERR_ABORT;` |
|        - | 3171 | `						}` |
|        9 | 3172 | `						goto done;` |
|        - | 3173 | `					}` |
|    15835 | 3174 | `					continue;` |
|        - | 3175 | `				}` |
|        - | 3176 | `				/* Extract the keyword */` |
|  2416147 | 3177 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1208071 | 3178 | `			}` |
|  2959303 | 3179 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|        - | 3180 | `				/* Process constant declaration */` |
|   286997 | 3181 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|   286997 | 3182 | `				if( rc != SXRET_OK ){` |
|       11 | 3183 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3184 | `						return SXERR_ABORT;` |
|        - | 3185 | `					}` |
|       11 | 3186 | `					goto done;` |
|        - | 3187 | `				}` |
|   143497 | 3188 | `			}else{` |
|  2672311 | 3189 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - | 3190 | `					/* Static method or attribute,record that */` |
|   100975 | 3191 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|   100975 | 3192 | `					pGen->pIn++; /* Jump the static keyword */` |
|   100975 | 3193 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 3194 | `						int nSetTok;` |
|    73801 | 3195 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|    73801 | 3196 | `						if( nSetVis ){` |
|        - | 3197 | ``							/* `static private(set) int $x` — read side stays public */`` |
|        3 | 3198 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        3 | 3199 | `							pGen->pIn += nSetTok;` |
|        2 | 3200 | `						}else{` |
|        - | 3201 | `							/* Extract the keyword */` |
|    73799 | 3202 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    73799 | 3203 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 3204 | `								iProtection = nKwrd;` |
|      ! 0 | 3205 | `								pGen->pIn++; /* Jump the visibility token */` |
|      ! 0 | 3206 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|      ! 0 | 3207 | `								if( nSetVis ){` |
|      ! 0 | 3208 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|      ! 0 | 3209 | `									pGen->pIn += nSetTok;` |
|      ! 0 | 3210 | `								}` |
|      ! 0 | 3211 | `							}` |
|        - | 3212 | `						}` |
|    36898 | 3213 | `					}` |
|        - | 3214 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|        - | 3215 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|        - | 3216 | `					 * than a generic "expecting method" parse error. */` |
|   100975 | 3217 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 3218 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 | 3219 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|      ! 0 | 3220 | `					}` |
|   100970 | 3221 | `					if( pGen->pIn >= pGen->pEnd` |
|   100975 | 3222 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 3223 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3224 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|      ! 0 | 3225 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 3226 | `						if( rc == SXERR_ABORT ){` |
|        - | 3227 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 3228 | `							return SXERR_ABORT;` |
|        - | 3229 | `						}` |
|      ! 0 | 3230 | `						goto done;` |
|        - | 3231 | `					}` |
|   100975 | 3232 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 3233 | `						/* Attribute declaration */` |
|    27175 | 3234 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    27175 | 3235 | `						if( rc != SXRET_OK ){` |
|        3 | 3236 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 3237 | `								return SXERR_ABORT;` |
|        - | 3238 | `							}` |
|        3 | 3239 | `							goto done;` |
|        - | 3240 | `						}` |
|    27173 | 3241 | `						continue;` |
|        - | 3242 | `					}` |
|    73805 | 3243 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 3244 | `						/* Typed static attribute declaration */` |
|       19 | 3245 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       19 | 3246 | `						if( rc != SXRET_OK ){` |
|        3 | 3247 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 3248 | `								return SXERR_ABORT;` |
|        - | 3249 | `							}` |
|        3 | 3250 | `							goto done;` |
|        - | 3251 | `						}` |
|       17 | 3252 | `						continue;` |
|        - | 3253 | `					}` |
|        - | 3254 | `					/* Extract the keyword */` |
|    73789 | 3255 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  2608233 | 3256 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        - | 3257 | `					/* Abstract method,record that */` |
|     7779 | 3258 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        - | 3259 | `					/* Mark the whole class as abstract */` |
|     7779 | 3260 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|        - | 3261 | `					/* Advance the stream cursor */` |
|     7779 | 3262 | `					pGen->pIn++;` |
|     7779 | 3263 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     7779 | 3264 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     7779 | 3265 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     7777 | 3266 | `							iProtection = nKwrd;` |
|     7777 | 3267 | `							pGen->pIn++; /* Jump the visibility token */` |
|     3886 | 3268 | `						}` |
|     3887 | 3269 | `					}` |
|     7779 | 3270 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     7774 | 3271 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 3272 | `							/* Static method */` |
|      ! 0 | 3273 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 3274 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 3275 | `					}` |
|     7779 | 3276 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|     7774 | 3277 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|        - | 3278 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|        - | 3279 | `							 * HOOKED property declaration. Route anything that is not a` |
|        - | 3280 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|        - | 3281 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|        - | 3282 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|        6 | 3283 | `							if( pGen->pIn < pGen->pEnd` |
|        7 | 3284 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|        3 | 3285 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|        7 | 3286 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        7 | 3287 | `								if( rc != SXRET_OK ){` |
|      ! 0 | 3288 | `									if( rc == SXERR_ABORT ){` |
|      ! 0 | 3289 | `										return SXERR_ABORT;` |
|        - | 3290 | `									}` |
|      ! 0 | 3291 | `									goto done;` |
|        - | 3292 | `								}` |
|        7 | 3293 | `								continue;` |
|        - | 3294 | `							}` |
|      ! 0 | 3295 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3296 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|      ! 0 | 3297 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 3298 | `							if( rc == SXERR_ABORT ){` |
|        - | 3299 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 3300 | `								return SXERR_ABORT;` |
|        - | 3301 | `							}` |
|      ! 0 | 3302 | `							goto done;` |
|        - | 3303 | `					}` |
|     7773 | 3304 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  2567451 | 3305 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|        - | 3306 | `					/* final method ,record that */` |
|       21 | 3307 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       21 | 3308 | `					pGen->pIn++; /* Jump the final keyword */` |
|       21 | 3309 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 3310 | `						/* Extract the keyword */` |
|       21 | 3311 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       21 | 3312 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       11 | 3313 | `							iProtection = nKwrd;` |
|       11 | 3314 | `							pGen->pIn++; /* Jump the visibility token */` |
|        4 | 3315 | `						}` |
|        9 | 3316 | `					}` |
|       21 | 3317 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       18 | 3318 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|        - | 3319 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|        - | 3320 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|        - | 3321 | `							 * child class is compiled (PH7_ClassInherit). */` |
|       14 | 3322 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|       14 | 3323 | `							if( rc != SXRET_OK ){` |
|      ! 0 | 3324 | `								if( rc == SXERR_ABORT ){` |
|      ! 0 | 3325 | `									return SXERR_ABORT;` |
|        - | 3326 | `								}` |
|      ! 0 | 3327 | `								goto done;` |
|        - | 3328 | `							}` |
|       14 | 3329 | `							continue;` |
|        - | 3330 | `					}` |
|        9 | 3331 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        6 | 3332 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 3333 | `							/* Static method */` |
|      ! 0 | 3334 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 3335 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 3336 | `					}` |
|        9 | 3337 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        6 | 3338 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 3339 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3340 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|      ! 0 | 3341 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 3342 | `							if( rc == SXERR_ABORT ){` |
|        - | 3343 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 3344 | `								return SXERR_ABORT;` |
|        - | 3345 | `							}` |
|      ! 0 | 3346 | `							goto done;` |
|        - | 3347 | `					}` |
|        9 | 3348 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        3 | 3349 | `				}` |
|  2645107 | 3350 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 3351 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3352 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|      ! 0 | 3353 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 3354 | `						if( rc == SXERR_ABORT ){` |
|        - | 3355 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 3356 | `							return SXERR_ABORT;` |
|        - | 3357 | `						}` |
|      ! 0 | 3358 | `						goto done;` |
|        - | 3359 | `				}` |
|  2645107 | 3360 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|        7 | 3361 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|        7 | 3362 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|      ! 0 | 3363 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3364 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 3365 | `						if( rc == SXERR_ABORT ){` |
|        - | 3366 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 3367 | `							return SXERR_ABORT;` |
|        - | 3368 | `						}` |
|      ! 0 | 3369 | `						goto done;` |
|        - | 3370 | `					}` |
|        - | 3371 | `					/* Attribute declaration */` |
|        7 | 3372 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        4 | 3373 | `				}else{` |
|        - | 3374 | `					/* Process method declaration */` |
|  2645101 | 3375 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 3376 | `				}` |
|  2645107 | 3377 | `				if( rc != SXRET_OK ){` |
|       16 | 3378 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3379 | `						return SXERR_ABORT;` |
|        - | 3380 | `					}` |
|       16 | 3381 | `					goto done;` |
|        - | 3382 | `				}` |
|        - | 3383 | `			}` |
|  1466042 | 3384 | `		}else{` |
|        - | 3385 | `			/* Attribute declaration */` |
|      ! 0 | 3386 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 3387 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 3388 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3389 | `					return SXERR_ABORT;` |
|        - | 3390 | `				}` |
|      ! 0 | 3391 | `				goto done;` |
|        - | 3392 | `			}` |
|        - | 3393 | `		}` |
|        5 | 3394 | `	}` |
|        - | 3395 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|        - | 3396 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|        - | 3397 | `	 */` |
|        - | 3398 | `	{` |
|        - | 3399 | `		TraitUseEntry *apUse;` |
|        - | 3400 | `		sxu32 nU;` |
|   451557 | 3401 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   467151 | 3402 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|    15599 | 3403 | `			TraitUseEntry *pUse = &apUse[nU];` |
|    15599 | 3404 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|    15599 | 3405 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|    15599 | 3406 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|        - | 3407 | `			sxu32 nT;` |
|    15599 | 3408 | `			if( !hasResolution ){` |
|        - | 3409 | `				/* No conflict resolution block: use standard trait application */` |
|    31179 | 3410 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|    15595 | 3411 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|    15595 | 3412 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 3413 | `						break;` |
|        - | 3414 | `					}` |
|     7800 | 3415 | `				}` |
|     7797 | 3416 | `			}else{` |
|        - | 3417 | `				/* With resolution block: copy attributes, record traits,` |
|        - | 3418 | `				 * then use the block to resolve method conflicts.` |
|        - | 3419 | `				 */` |
|        - | 3420 | `				SyToken *pR;` |
|       25 | 3421 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       15 | 3422 | `					ph7_class *pTR = apTrait[nT];` |
|        - | 3423 | `					ph7_class_attr *pAR;` |
|        - | 3424 | `					SyHashEntry *pER;` |
|        - | 3425 | `					SyString *pNR;` |
|       15 | 3426 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|       21 | 3427 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|      ! 0 | 3428 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|      ! 0 | 3429 | `						pNR = &pAR->sName;` |
|      ! 0 | 3430 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      ! 0 | 3431 | `							SyHashInsertTail(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|      ! 0 | 3432 | `						}` |
|      ! 0 | 3433 | `					}` |
|       15 | 3434 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|        9 | 3435 | `				}` |
|        - | 3436 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       13 | 3437 | `				pR = pUse->pResolvStart;` |
|       27 | 3438 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 3439 | `					SyString sTrait,sMethod;` |
|        - | 3440 | `					ph7_class *pSrcTrait;` |
|        - | 3441 | `					ph7_class_method *pMeth;` |
|        - | 3442 | `					sxi32 nRKwrd;` |
|       41 | 3443 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       27 | 3444 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       17 | 3445 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       17 | 3446 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       17 | 3447 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       17 | 3448 | `					sMethod = pR->sData;` |
|       17 | 3449 | `					pR++;` |
|       17 | 3450 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 3451 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 3452 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 3453 | `							sTrait = sMethod;` |
|        7 | 3454 | `							pR++;` |
|        7 | 3455 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 3456 | `							sMethod = pR->sData;` |
|        7 | 3457 | `							pR++;` |
|        3 | 3458 | `						}` |
|        3 | 3459 | `					}` |
|       17 | 3460 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 3461 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 3462 | `						continue;` |
|        - | 3463 | `					}` |
|       17 | 3464 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       17 | 3465 | `					pR++;` |
|       17 | 3466 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|        5 | 3467 | `						pSrcTrait = 0;` |
|        7 | 3468 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        7 | 3469 | `							SyString *pTN = &apTrait[nT]->sName;` |
|       10 | 3470 | `							if( pTN->nByte >= sTrait.nByte &&` |
|        6 | 3471 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        5 | 3472 | `								pSrcTrait = apTrait[nT];` |
|        5 | 3473 | `								break;` |
|        - | 3474 | `							}` |
|        2 | 3475 | `						}` |
|        5 | 3476 | `						if( pSrcTrait ){` |
|        5 | 3477 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        5 | 3478 | `							if( pMeth ){` |
|        5 | 3479 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|        5 | 3480 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|        5 | 3481 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|        2 | 3482 | `								}` |
|        2 | 3483 | `							}` |
|        2 | 3484 | `						}` |
|        2 | 3485 | `					}` |
|       35 | 3486 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        3 | 3487 | `				}` |
|        - | 3488 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|       25 | 3489 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        - | 3490 | `					ph7_class_method *pMR;` |
|        - | 3491 | `					SyHashEntry *pER;` |
|        - | 3492 | `					SyString *pNR;` |
|       15 | 3493 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|       41 | 3494 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|       23 | 3495 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|       23 | 3496 | `						pNR = &pMR->sFunc.sName;` |
|       23 | 3497 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       14 | 3498 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|        6 | 3499 | `						}` |
|        3 | 3500 | `					}` |
|        9 | 3501 | `				}` |
|        - | 3502 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       13 | 3503 | `				pR = pUse->pResolvStart;` |
|       27 | 3504 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 3505 | `					SyString sTrait,sMethod,sAlias;` |
|        - | 3506 | `					ph7_class *pSrcTrait;` |
|        - | 3507 | `					ph7_class_method *pMeth;` |
|       27 | 3508 | `					int hasQual = 0;` |
|        - | 3509 | `					sxi32 nRKwrd;` |
|       41 | 3510 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       27 | 3511 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       17 | 3512 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       17 | 3513 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       17 | 3514 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|       17 | 3515 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       17 | 3516 | `					sMethod = pR->sData;` |
|       17 | 3517 | `					pR++;` |
|       17 | 3518 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 3519 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 3520 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 3521 | `							sTrait = sMethod;` |
|        7 | 3522 | `							hasQual = 1;` |
|        7 | 3523 | `							pR++;` |
|        7 | 3524 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 3525 | `							sMethod = pR->sData;` |
|        7 | 3526 | `							pR++;` |
|        3 | 3527 | `						}` |
|        3 | 3528 | `					}` |
|       17 | 3529 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 3530 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 3531 | `						continue;` |
|        - | 3532 | `					}` |
|       17 | 3533 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       17 | 3534 | `					pR++;` |
|       17 | 3535 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       13 | 3536 | `						sxi32 iNewVis = -1;` |
|       13 | 3537 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|        7 | 3538 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|        7 | 3539 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|        7 | 3540 | `								iNewVis = nAK;` |
|        7 | 3541 | `								pR++;` |
|        3 | 3542 | `							}` |
|        3 | 3543 | `						}` |
|       13 | 3544 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       11 | 3545 | `							sAlias = pR->sData;` |
|       11 | 3546 | `							pR++;` |
|        4 | 3547 | `						}` |
|       13 | 3548 | `						pMeth = 0;` |
|       13 | 3549 | `						if( hasQual ){` |
|        3 | 3550 | `							pSrcTrait = 0;` |
|        5 | 3551 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        5 | 3552 | `								SyString *pTN = &apTrait[nT]->sName;` |
|        7 | 3553 | `								if( pTN->nByte >= sTrait.nByte &&` |
|        4 | 3554 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        3 | 3555 | `									pSrcTrait = apTrait[nT];` |
|        3 | 3556 | `									break;` |
|        - | 3557 | `								}` |
|        2 | 3558 | `							}` |
|        3 | 3559 | `							if( pSrcTrait ){` |
|        3 | 3560 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        1 | 3561 | `							}` |
|        2 | 3562 | `						}else{` |
|       10 | 3563 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|        - | 3564 | `						}` |
|       13 | 3565 | `						if( pMeth ){` |
|       13 | 3566 | `							if( sAlias.nByte > 0 ){` |
|        - | 3567 | `								/* Create a shallow copy of the method struct for the alias` |
|        - | 3568 | `								 * so it can carry its own visibility without affecting the original.` |
|        - | 3569 | `								 */` |
|        - | 3570 | `								ph7_class_method *pAlias;` |
|        - | 3571 | `								char *zAliasDup;` |
|       11 | 3572 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       11 | 3573 | `								if( pAlias ){` |
|       11 | 3574 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       11 | 3575 | `									if( iNewVis >= 0 ){` |
|        5 | 3576 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 3577 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 3578 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        2 | 3579 | `									}` |
|       11 | 3580 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       11 | 3581 | `									if( zAliasDup ){` |
|       11 | 3582 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|        4 | 3583 | `									}` |
|        7 | 3584 | `								}` |
|        7 | 3585 | `							}else if( iNewVis >= 0 ){` |
|        - | 3586 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|        - | 3587 | `								ph7_class_method *pCopy;` |
|        3 | 3588 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        3 | 3589 | `								if( pCopy ){` |
|        3 | 3590 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|        3 | 3591 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|        3 | 3592 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 3593 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 3594 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        - | 3595 | `									/* Replace the method in the class hash */` |
|        3 | 3596 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|        3 | 3597 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|        1 | 3598 | `								}` |
|        1 | 3599 | `							}` |
|        5 | 3600 | `						}` |
|        5 | 3601 | `						SXUNUSED(hasQual);` |
|        5 | 3602 | `					}` |
|       21 | 3603 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        3 | 3604 | `				}` |
|        - | 3605 | `			}` |
|    15599 | 3606 | `			SySetRelease(&pUse->aTraits);` |
|     7802 | 3607 | `		}` |
|        - | 3608 | `	}` |
|   451557 | 3609 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - | 3610 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|        - | 3611 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|     3913 | 3612 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|     3913 | 3613 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3614 | `			SySetRelease(&aUseEntries);` |
|      ! 0 | 3615 | `			SySetRelease(&aInterfaces);` |
|      ! 0 | 3616 | `			return SXERR_ABORT;` |
|        - | 3617 | `		}` |
|     1954 | 3618 | `	}` |
|        - | 3619 | `	/* Reject a php-fatal redeclaration before hoisting the class */` |
|   451557 | 3620 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|        9 | 3621 | `		return SXERR_ABORT;` |
|        - | 3622 | `	}` |
|        - | 3623 | `	/* Install the class */` |
|   451551 | 3624 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   451551 | 3625 | `	if( rc == SXRET_OK ){` |
|        - | 3626 | `		ph7_class **apInterface;` |
|        - | 3627 | `		sxu32 n;` |
|   451551 | 3628 | `		if( pBase ){` |
|        - | 3629 | `			/* Inherit from base class and mark as a subclass */` |
|   248375 | 3630 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   124185 | 3631 | `		}` |
|   451551 | 3632 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   649431 | 3633 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|        - | 3634 | `			/* Implements one or more interface */` |
|   197885 | 3635 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|   197885 | 3636 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 3637 | `				break;` |
|        - | 3638 | `			}` |
|    98945 | 3639 | `		}` |
|        - | 3640 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|        - | 3641 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|   451551 | 3642 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|     3911 | 3643 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|     3911 | 3644 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 3645 | `				pIntf = pIntf->pNextName;` |
|      ! 0 | 3646 | `			}` |
|     3911 | 3647 | `			if( pIntf ){` |
|     3911 | 3648 | `				PH7_ClassImplement(pClass,pIntf);` |
|     1953 | 3649 | `			}` |
|     3911 | 3650 | `			if( pClass->nEnumBacking != 0 ){` |
|     3895 | 3651 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|     3895 | 3652 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 3653 | `					pIntf = pIntf->pNextName;` |
|      ! 0 | 3654 | `				}` |
|     3895 | 3655 | `				if( pIntf ){` |
|     3895 | 3656 | `					PH7_ClassImplement(pClass,pIntf);` |
|     1945 | 3657 | `				}` |
|     1945 | 3658 | `			}` |
|     1953 | 3659 | `		}` |
|        - | 3660 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|        - | 3661 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|   451546 | 3662 | `		if( rc == SXRET_OK` |
|   451546 | 3663 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|   451551 | 3664 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   228797 | 3665 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|        - | 3666 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   228797 | 3667 | `			if( pStringable ){` |
|   228797 | 3668 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   228797 | 3669 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|        - | 3670 | `				sxu32 i;` |
|   228797 | 3671 | `				int bAlready = 0;` |
|   275313 | 3672 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    58151 | 3673 | `					if( apImpl[i] == pStringable ){` |
|    11635 | 3674 | `						bAlready = 1;` |
|    11635 | 3675 | `						break;` |
|        - | 3676 | `					}` |
|    23263 | 3677 | `				}` |
|   228797 | 3678 | `				if( !bAlready ){` |
|   217167 | 3679 | `					PH7_ClassImplement(pClass,pStringable);` |
|   108581 | 3680 | `				}` |
|   114396 | 3681 | `			}` |
|   114396 | 3682 | `		}` |
|        - | 3683 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   451551 | 3684 | `		if( rc == SXRET_OK ){` |
|   451551 | 3685 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   451551 | 3686 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 3687 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 3688 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 3689 | `				return SXERR_ABORT;` |
|        - | 3690 | `			}` |
|   225773 | 3691 | `		}` |
|        - | 3692 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   451551 | 3693 | `		if( rc == SXRET_OK ){` |
|   451551 | 3694 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   451551 | 3695 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 3696 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 3697 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 3698 | `				return SXERR_ABORT;` |
|        - | 3699 | `			}` |
|   225773 | 3700 | `		}` |
|   225773 | 3701 | `	}` |
|   451551 | 3702 | `	SySetRelease(&aUseEntries);` |
|   451551 | 3703 | `	SySetRelease(&aInterfaces);` |
|   451551 | 3704 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3705 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3706 | `		return SXERR_ABORT;` |
|        - | 3707 | `	}` |
|   225773 | 3708 | `done:` |
|        - | 3709 | `	/* Point beyond the class body */` |
|   451593 | 3710 | `	pGen->pIn = &pEnd[1];` |
|   451593 | 3711 | `	pGen->pEnd = pTmp;` |
|   451593 | 3712 | `	return PH7_OK;` |
|   225803 | 3713 | `}` |
|        - | 3714 | `/* Compile a named class declaration (the common case). */` |
|   451566 | 3715 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|        5 | 3716 | `{` |
|   451571 | 3717 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|        5 | 3718 | `}` |
|        - | 3719 | `/*` |
|        - | 3720 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|        - | 3721 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|        - | 3722 | ` * compile + install the class body once (at compile time, like every other` |
|        - | 3723 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|        - | 3724 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|        - | 3725 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|        - | 3726 | ` */` |
|       30 | 3727 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 | 3728 | `{` |
|        - | 3729 | `	char zName[128];         /* Synthesized class name */` |
|        - | 3730 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|        - | 3731 | `	SyString sName;` |
|        - | 3732 | `	SyToken *pArgStart,*pArgEnd;` |
|       34 | 3733 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|        - | 3734 | `	                              * is keyed to this 'class' token */` |
|        - | 3735 | `	ph7_value *pObj;` |
|       34 | 3736 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3737 | `	sxu32 nIdx,nLen;` |
|        - | 3738 | `	sxi32 nArg,rc;` |
|       15 | 3739 | `	SXUNUSED(iCompileFlag);` |
|        - | 3740 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|       34 | 3741 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       34 | 3742 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 | 3743 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      ! 0 | 3744 | `	}` |
|       34 | 3745 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - | 3746 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|        - | 3747 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|        - | 3748 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|       34 | 3749 | `	pArgStart = pArgEnd = 0;` |
|       34 | 3750 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|       34 | 3751 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3752 | `		return rc;` |
|        - | 3753 | `	}` |
|        - | 3754 | `	{` |
|        - | 3755 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|       34 | 3756 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|       30 | 3757 | `		if( pAnonClass` |
|       34 | 3758 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 3759 | `			return SXERR_ABORT;` |
|        - | 3760 | `		}` |
|        - | 3761 | `	}` |
|        - | 3762 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|        - | 3763 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|       34 | 3764 | `	nArg = 0;` |
|       34 | 3765 | `	if( pArgStart < pArgEnd ){` |
|        7 | 3766 | `		SyToken *pSavedIn = pGen->pIn;` |
|        7 | 3767 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 3768 | `		SyToken *pArgNext;` |
|        7 | 3769 | `		pGen->pIn = pArgStart;` |
|        7 | 3770 | `		pGen->pEnd = pArgEnd;` |
|       13 | 3771 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|        7 | 3772 | `			if( pGen->pIn < pArgNext ){` |
|        7 | 3773 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|        7 | 3774 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3775 | `					pGen->pIn = pSavedIn;` |
|      ! 0 | 3776 | `					pGen->pEnd = pSavedEnd;` |
|      ! 0 | 3777 | `					return SXERR_ABORT;` |
|        - | 3778 | `				}` |
|        7 | 3779 | `				nArg++;` |
|        3 | 3780 | `			}` |
|        7 | 3781 | `			pGen->pIn = &pArgNext[1];` |
|        1 | 3782 | `		}` |
|        7 | 3783 | `		pGen->pIn = pSavedIn;` |
|        7 | 3784 | `		pGen->pEnd = pSavedEnd;` |
|        3 | 3785 | `	}` |
|        - | 3786 | `	/* Load the synthesized class name */` |
|       34 | 3787 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       34 | 3788 | `	if( pObj == 0 ){` |
|      ! 0 | 3789 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 3790 | `		return SXERR_ABORT;` |
|        - | 3791 | `	}` |
|       34 | 3792 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       34 | 3793 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - | 3794 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|       34 | 3795 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       34 | 3796 | `	return SXRET_OK;` |
|       19 | 3797 | `}` |
|        - | 3798 | `/*` |
|        - | 3799 | ` * Compile a user-defined abstract class.` |
|        - | 3800 | ` *  According to the PHP language reference manual` |
|        - | 3801 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|        - | 3802 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|        - | 3803 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|        - | 3804 | ` *   the method's signature - they cannot define the implementation.` |
|        - | 3805 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|        - | 3806 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|        - | 3807 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|        - | 3808 | ` *   method is defined as protected, the function implementation must be defined as either` |
|        - | 3809 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|        - | 3810 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|        - | 3811 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|        - | 3812 | ` *   could differ.` |
|        - | 3813 | ` */` |
|        - | 3814 | `/*` |
|        - | 3815 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|        - | 3816 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|        - | 3817 | ` * receives the corresponding PH7_CLASS_* bit.` |
|        - | 3818 | ` */` |
| 14431548 | 3819 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|        5 | 3820 | `{` |
| 14431553 | 3821 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  8637723 | 3822 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  8637723 | 3823 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  8583413 | 3824 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  4272271 | 3825 | `	}` |
| 14338377 | 3826 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
| 14338317 | 3827 | `	return FALSE;` |
|  7215779 | 3828 | `}` |
|        - | 3829 | `/*` |
|        - | 3830 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|        - | 3831 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|        - | 3832 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|        - | 3833 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|        - | 3834 | ` */` |
| 14338312 | 3835 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|        5 | 3836 | `{` |
| 14338317 | 3837 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
| 14338317 | 3838 | `	sxi32 iFlags = 0,iFlag;` |
| 14431553 | 3839 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    93241 | 3840 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|        5 | 3841 | `			pDup = pIn;` |
|        2 | 3842 | `		}` |
|    93241 | 3843 | `		iFlags \|= iFlag;` |
|    93241 | 3844 | `		pIn++;` |
|        5 | 3845 | `	}` |
| 14338317 | 3846 | `	*ppIn = pIn;` |
| 14338317 | 3847 | `	if( ppDup ){ *ppDup = pDup; }` |
| 14338317 | 3848 | `	return iFlags;` |
|        5 | 3849 | `}` |
|        - | 3850 | `/*` |
|        - | 3851 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|        - | 3852 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|        - | 3853 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|        - | 3854 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|        - | 3855 | `` * `readonly`) to their existing handlers.`` |
|        - | 3856 | ` */` |
| 14295580 | 3857 | `PH7_PRIVATE int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|        5 | 3858 | `{` |
| 14295585 | 3859 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  7198281 | 3860 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
| 14320824 | 3861 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|        5 | 3862 | `}` |
|        - | 3863 | `/*` |
|        - | 3864 | ` * Compile a class declaration carrying one or more leading modifiers` |
|        - | 3865 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|        - | 3866 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|        - | 3867 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|        - | 3868 | `` * `abstract`+`final` pair, like PHP.`` |
|        - | 3869 | ` */` |
|    42732 | 3870 | `PH7_PRIVATE sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|        5 | 3871 | `{` |
|        - | 3872 | `	SyToken *pDup;` |
|    42737 | 3873 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|        - | 3874 | `	sxi32 rc;` |
|    42737 | 3875 | `	if( pDup ){` |
|        4 | 3876 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|        2 | 3877 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|        3 | 3878 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3879 | `			return SXERR_ABORT;` |
|        - | 3880 | `		}` |
|        1 | 3881 | `	}` |
|    42732 | 3882 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    21371 | 3883 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|        3 | 3884 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3885 | `			"Cannot use the final modifier on an abstract class");` |
|        3 | 3886 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3887 | `			return SXERR_ABORT;` |
|        - | 3888 | `		}` |
|        1 | 3889 | `	}` |
|    42737 | 3890 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    21371 | 3891 | `}` |
|        - | 3892 | `/*` |
|        - | 3893 | ` * Compile a user-defined trait.` |
|        - | 3894 | ` *  Traits are similar to classes, but only intended to group functionality` |
|        - | 3895 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|        - | 3896 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|        - | 3897 | ` */` |
|     7852 | 3898 | `PH7_PRIVATE sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|        5 | 3899 | `{` |
|     7857 | 3900 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3901 | `	ph7_class *pClass;` |
|        - | 3902 | `	SyToken *pEnd,*pTmp;` |
|        - | 3903 | `	sxi32 iProtection;` |
|        - | 3904 | `	sxi32 iAttrflags;` |
|        - | 3905 | `	SyString *pName;` |
|        - | 3906 | `	sxi32 nKwrd;` |
|        - | 3907 | `	sxi32 rc;` |
|        - | 3908 | `	/* Jump the 'trait' keyword */` |
|     7857 | 3909 | `	pGen->pIn++;` |
|     7857 | 3910 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 3911 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|      ! 0 | 3912 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3913 | `			return SXERR_ABORT;` |
|        - | 3914 | `		}` |
|      ! 0 | 3915 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|      ! 0 | 3916 | `			pGen->pIn++;` |
|      ! 0 | 3917 | `		}` |
|      ! 0 | 3918 | `		return SXRET_OK;` |
|        - | 3919 | `	}` |
|        - | 3920 | `	/* Extract trait name */` |
|     7857 | 3921 | `	pName = &pGen->pIn->sData;` |
|     7857 | 3922 | `	pGen->pIn++;` |
|        - | 3923 | `	/* Build FQN and obtain a raw class */ {` |
|        - | 3924 | `		SyBlob sFQN;` |
|        - | 3925 | `		SyString sFQNStr;` |
|     7857 | 3926 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     7857 | 3927 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     7857 | 3928 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|     7857 | 3929 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     7857 | 3930 | `		SyBlobRelease(&sFQN);` |
|        - | 3931 | `	}` |
|     7857 | 3932 | `	if( pClass == 0 ){` |
|      ! 0 | 3933 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3934 | `		return SXERR_ABORT;` |
|        - | 3935 | `	}` |
|     7857 | 3936 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     7857 | 3937 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 3938 | `		return SXERR_ABORT;` |
|        - | 3939 | `	}` |
|        - | 3940 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|     7857 | 3941 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 | 3942 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|      ! 0 | 3943 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 3944 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3945 | `			return SXERR_ABORT;` |
|        - | 3946 | `		}` |
|      ! 0 | 3947 | `		return SXRET_OK;` |
|        - | 3948 | `	}` |
|     7857 | 3949 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     7857 | 3950 | `	pEnd = 0;` |
|     7857 | 3951 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|     7857 | 3952 | `	if( pEnd >= pGen->pEnd ){` |
|      ! 0 | 3953 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|      ! 0 | 3954 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 3955 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3956 | `			return SXERR_ABORT;` |
|        - | 3957 | `		}` |
|      ! 0 | 3958 | `		return SXRET_OK;` |
|        - | 3959 | `	}` |
|        - | 3960 | `	/* The delimiter token is the trait body's closing brace */` |
|     7857 | 3961 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 3962 | `	/* Swap token stream */` |
|     7857 | 3963 | `	pTmp = pGen->pEnd;` |
|     7857 | 3964 | `	pGen->pEnd = pEnd;` |
|        - | 3965 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|     7857 | 3966 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|        - | 3967 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|    56303 | 3968 | `	for(;;){` |
|   159185 | 3969 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|    23295 | 3970 | `			pGen->pIn++;` |
|        5 | 3971 | `		}` |
|   135895 | 3972 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     7857 | 3973 | `			break;` |
|        - | 3974 | `		}` |
|        - | 3975 | `		/* Bind a directly-preceding docblock to this member */` |
|   128043 | 3976 | `		GenStateSetPendingDoc(&(*pGen));` |
|   128043 | 3977 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|      ! 0 | 3978 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3979 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 3980 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 3981 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3982 | `				return SXERR_ABORT;` |
|        - | 3983 | `			}` |
|      ! 0 | 3984 | `			goto done;` |
|        - | 3985 | `		}` |
|   128043 | 3986 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   128043 | 3987 | `		iAttrflags = 0;` |
|   128043 | 3988 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   128043 | 3989 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   128043 | 3990 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 3991 | `				/* Trait uses another trait: use OtherTrait; */` |
|        5 | 3992 | `				pGen->pIn++; /* Jump 'use' */` |
|        2 | 3993 | `				for(;;){` |
|        - | 3994 | `					ph7_class *pUsedTrait;` |
|        - | 3995 | `					SyString *pUsedName;` |
|        5 | 3996 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 3997 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 3998 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|      ! 0 | 3999 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4000 | `							return SXERR_ABORT;` |
|        - | 4001 | `						}` |
|      ! 0 | 4002 | `						break;` |
|        - | 4003 | `					}` |
|        5 | 4004 | `					pUsedName = &pGen->pIn->sData;` |
|        - | 4005 | `					{` |
|        - | 4006 | `						SyBlob sResolved;` |
|        5 | 4007 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        5 | 4008 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|        7 | 4009 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|        4 | 4010 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|        5 | 4011 | `						SyBlobRelease(&sResolved);` |
|        - | 4012 | `					}` |
|        5 | 4013 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 4014 | `						pUsedTrait = pUsedTrait->pNextName;` |
|      ! 0 | 4015 | `					}` |
|        5 | 4016 | `					if( pUsedTrait == 0 ){` |
|        4 | 4017 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        1 | 4018 | `							"'%z' is not a trait",pUsedName);` |
|        3 | 4019 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4020 | `							return SXERR_ABORT;` |
|        - | 4021 | `						}` |
|        2 | 4022 | `					}else{` |
|        3 | 4023 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|        - | 4024 | `					}` |
|        5 | 4025 | `					pGen->pIn++;` |
|        5 | 4026 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|        3 | 4027 | `						break;` |
|        - | 4028 | `					}` |
|      ! 0 | 4029 | `					pGen->pIn++;` |
|      ! 0 | 4030 | `				}` |
|        5 | 4031 | `				continue;` |
|        - | 4032 | `			}` |
|   128039 | 4033 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|   128021 | 4034 | `				iProtection = nKwrd;` |
|   128021 | 4035 | `				pGen->pIn++;` |
|        - | 4036 | ``				/* Optional `readonly` after the visibility (PHP 8.1): `private readonly T`` |
|        - | 4037 | ``				 * $x` — the generated PHPUnit runtime traits (StubApi, …) declare their`` |
|        - | 4038 | `				 * readonly state this way. Mirrors the class-body attribute parser. */` |
|   128021 | 4039 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|        5 | 4040 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|        5 | 4041 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        2 | 4042 | `				}` |
|   128016 | 4043 | `				if( pGen->pIn >= pGen->pEnd` |
|   128021 | 4044 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 4045 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4046 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 4047 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 4048 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 4049 | `						return SXERR_ABORT;` |
|        - | 4050 | `					}` |
|      ! 0 | 4051 | `					goto done;` |
|        - | 4052 | `				}` |
|   128021 | 4053 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|    23275 | 4054 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    23275 | 4055 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 4056 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4057 | `							return SXERR_ABORT;` |
|        - | 4058 | `						}` |
|      ! 0 | 4059 | `						goto done;` |
|        - | 4060 | `					}` |
|    23275 | 4061 | `					continue;` |
|        - | 4062 | `				}` |
|   104751 | 4063 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        9 | 4064 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        9 | 4065 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 4066 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4067 | `							return SXERR_ABORT;` |
|        - | 4068 | `						}` |
|      ! 0 | 4069 | `						goto done;` |
|        - | 4070 | `					}` |
|        9 | 4071 | `					continue;` |
|        - | 4072 | `				}` |
|   104743 | 4073 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    52369 | 4074 | `			}` |
|   104761 | 4075 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|      ! 0 | 4076 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4077 | `					"Traits cannot have constants");` |
|      ! 0 | 4078 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 4079 | `					return SXERR_ABORT;` |
|        - | 4080 | `				}` |
|      ! 0 | 4081 | `				goto done;` |
|      ! 0 | 4082 | `			}else{` |
|   104761 | 4083 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|     7767 | 4084 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     7767 | 4085 | `					pGen->pIn++;` |
|     7767 | 4086 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     7765 | 4087 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     7765 | 4088 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 4089 | `							iProtection = nKwrd;` |
|      ! 0 | 4090 | `							pGen->pIn++;` |
|      ! 0 | 4091 | `						}` |
|     3880 | 4092 | `					}` |
|     7762 | 4093 | `					if( pGen->pIn >= pGen->pEnd` |
|     7767 | 4094 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 4095 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4096 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|      ! 0 | 4097 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 4098 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4099 | `							return SXERR_ABORT;` |
|        - | 4100 | `						}` |
|      ! 0 | 4101 | `						goto done;` |
|        - | 4102 | `					}` |
|     7767 | 4103 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        3 | 4104 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        3 | 4105 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 4106 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 4107 | `								return SXERR_ABORT;` |
|        - | 4108 | `							}` |
|      ! 0 | 4109 | `							goto done;` |
|        - | 4110 | `						}` |
|        3 | 4111 | `						continue;` |
|        - | 4112 | `					}` |
|     7765 | 4113 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|      ! 0 | 4114 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 4115 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 4116 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 4117 | `								return SXERR_ABORT;` |
|        - | 4118 | `							}` |
|      ! 0 | 4119 | `							goto done;` |
|        - | 4120 | `						}` |
|      ! 0 | 4121 | `						continue;` |
|        - | 4122 | `					}` |
|     7765 | 4123 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   100879 | 4124 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        9 | 4125 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        9 | 4126 | `					pGen->pIn++;` |
|        9 | 4127 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        9 | 4128 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        9 | 4129 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        9 | 4130 | `							iProtection = nKwrd;` |
|        9 | 4131 | `							pGen->pIn++;` |
|        3 | 4132 | `						}` |
|        3 | 4133 | `					}` |
|        9 | 4134 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        6 | 4135 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 4136 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4137 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|      ! 0 | 4138 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 4139 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4140 | `							return SXERR_ABORT;` |
|        - | 4141 | `						}` |
|      ! 0 | 4142 | `						goto done;` |
|        - | 4143 | `					}` |
|        9 | 4144 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        3 | 4145 | `				}` |
|   104759 | 4146 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 4147 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4148 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|      ! 0 | 4149 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 4150 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 4151 | `						return SXERR_ABORT;` |
|        - | 4152 | `					}` |
|      ! 0 | 4153 | `					goto done;` |
|        - | 4154 | `				}` |
|   104759 | 4155 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|      ! 0 | 4156 | `					pGen->pIn++;` |
|      ! 0 | 4157 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 4158 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 4159 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 4160 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 4161 | `							return SXERR_ABORT;` |
|        - | 4162 | `						}` |
|      ! 0 | 4163 | `						goto done;` |
|        - | 4164 | `					}` |
|      ! 0 | 4165 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 4166 | `				}else{` |
|   104759 | 4167 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 4168 | `				}` |
|   104759 | 4169 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 4170 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 4171 | `						return SXERR_ABORT;` |
|        - | 4172 | `					}` |
|      ! 0 | 4173 | `					goto done;` |
|        - | 4174 | `				}` |
|        - | 4175 | `			}` |
|    52382 | 4176 | `		}else{` |
|      ! 0 | 4177 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 4178 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 4179 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 4180 | `					return SXERR_ABORT;` |
|        - | 4181 | `				}` |
|      ! 0 | 4182 | `				goto done;` |
|        - | 4183 | `			}` |
|        - | 4184 | `		}` |
|        5 | 4185 | `	}` |
|        - | 4186 | `	/* Reject a php-fatal redeclaration before hoisting the trait */` |
|     7857 | 4187 | `	if( GenStateGuardClassRedeclaration(pGen,pClass) == SXERR_ABORT ){` |
|        3 | 4188 | `		return SXERR_ABORT;` |
|        - | 4189 | `	}` |
|        - | 4190 | `	/* Install the trait */` |
|     7855 | 4191 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     7855 | 4192 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 4193 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4194 | `		return SXERR_ABORT;` |
|        - | 4195 | `	}` |
|     3925 | 4196 | `done:` |
|        - | 4197 | `	/* Point beyond the trait body */` |
|     7855 | 4198 | `	pGen->pIn = &pEnd[1];` |
|     7855 | 4199 | `	pGen->pEnd = pTmp;` |
|     7855 | 4200 | `	return PH7_OK;` |
|     3931 | 4201 | `}` |
|        - | 4202 | `/*` |
|        - | 4203 | ` * Compile a user-defined class.` |
|        - | 4204 | ` *  According to the PHP language reference manual` |
|        - | 4205 | ` *   Basic class definitions begin with the keyword class, followed` |
|        - | 4206 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|        - | 4207 | ` *   the definitions of the properties and methods belonging to the class.` |
|        - | 4208 | ` *   A class may contain its own constants, variables (called "properties")` |
|        - | 4209 | ` *   and functions (called "methods").` |
|        - | 4210 | ` */` |
|   404922 | 4211 | `PH7_PRIVATE sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|        5 | 4212 | `{` |
|        - | 4213 | `	sxi32 rc;` |
|   404927 | 4214 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   404927 | 4215 | `	return rc;` |
|        5 | 4216 | `}` |
|        - | 4217 | `/*` |
|        - | 4218 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|        - | 4219 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|        - | 4220 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|        - | 4221 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|        - | 4222 | `` * meaning; `enum Name` can never start a valid expression.`` |
|        - | 4223 | ` */` |
| 14245096 | 4224 | `PH7_PRIVATE int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|        5 | 4225 | `{` |
| 14460896 | 4226 | `	return (pIn->nType & PH7_TK_ID)` |
|  7338343 | 4227 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|   225632 | 4228 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
| 14460891 | 4229 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|        5 | 4230 | `}` |
|        - | 4231 | `/*` |
|        - | 4232 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|        - | 4233 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|        - | 4234 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|        - | 4235 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|        - | 4236 | ` */` |
|     3912 | 4237 | `PH7_PRIVATE sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|        5 | 4238 | `{` |
|     3917 | 4239 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|        5 | 4240 | `}` |
|        - | 4241 |  |
